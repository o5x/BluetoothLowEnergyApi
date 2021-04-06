#include "pch.h"
#include "BluetoothLowEnergyApi.h"

// Namespaces
using namespace std;
using namespace winrt;
using namespace winrt::Windows::Devices::Radios;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;

// Local structures and prototypes
struct ConnectedCharacteristic : Characteristic {
	GattCharacteristic characteristic = nullptr;
};

struct ConnectedSubscription
{
	ConnectedCharacteristic* characteristic = nullptr;
	GattCharacteristic::ValueChanged_revoker revoker;
	queue<Data> notificationQueue;
};

struct AdvertisedDevice : Device
{
	chrono::steady_clock::time_point lastAdvertisment;
};

wstring addrToString(uint64_t addr);



struct ConnectedDevice : Device
{
	BluetoothLEDevice leDev = nullptr;
	GattDeviceServicesResult result = nullptr;
	BluetoothLEDevice::ConnectionStatusChanged_revoker revoker;
	list<ConnectedSubscription*> subscriptions;
	list<ConnectedCharacteristic*> characteristics;
	int characteristicPollIndex = 0;
	bool isConnected = false;
	wstring print()
	{
		return addrToString(address) + L" " + (leDev.Name().size() ? wstring(leDev.Name()) : L"");
	}
};

void ScanCharacteristicsAsync(ConnectedDevice* device);
void DisconnectDevice(ConnectedDevice* device);
void AddEvent(Device device, What what, Characteristic *c);
void AddEvent(Device device, What what);
void AddEvent(What what);

// Variables
mutex errorLock;
queue<Data> errors;

mutex eventsLock;
queue<Event> events;

mutex callbackLock;
bool (*eventCallback)(Event ev) = nullptr;

mutex advertisedLock;
list<AdvertisedDevice*> advertisedDevices;
BluetoothLEAdvertisementWatcher watcher = nullptr;
BluetoothLEAdvertisementWatcher::Received_revoker watcherRevoker;
bool threadRun = false;
bool threadRunning = false;
thread _timeoutThread;

mutex connectedLock;
list<ConnectedDevice*> devices;


// Function - Convert address to string

wstring addrToString(uint64_t addr)
{
	wostringstream ret;
	ret << hex << setfill(L'0')
		<< setw(2) << ((addr >> (5 * 8)) & 0xff) << ":"
		<< setw(2) << ((addr >> (4 * 8)) & 0xff) << ":"
		<< setw(2) << ((addr >> (3 * 8)) & 0xff) << ":"
		<< setw(2) << ((addr >> (2 * 8)) & 0xff) << ":"
		<< setw(2) << ((addr >> (1 * 8)) & 0xff) << ":"
		<< setw(2) << ((addr >> (0 * 8)) & 0xff);
	return ret.str();
}

// Function - Save and print error

void AddErrorAsync(Data error)
{
	try
	{
		lock_guard error_lock(errorLock);
		errors.push(error);
		AddEvent(What::LIBRARY_ERROR);
	}
	catch (...)
	{
		//AddError("AddErrorAsync error: exception caught");
	}
}

void AddError(const char* message, ...)
{
	try
	{
		Data error;
		va_list args;
		va_start(args, message);
		error.size = snprintf((char*)error.buf, 512, message, args);
		va_end(args);
		cout << "ERROR [" << error.size << "]: " << (char*)error.buf << endl;
		thread(AddErrorAsync, error).detach();
	}
	catch (...)
	{
		//AddError("AddError error: exception caught");
	}
}

DllExport bool PollErrors(Data* e)
{
	if (e == nullptr)return false;
	try
	{
		lock_guard lock(errorLock);

		e->size = 0;

		if (!errors.empty())
		{
			*e = errors.front();
			errors.pop();
			return true;
		}
	}
	catch (...)
	{
		AddError("PollErrors error: exception caught");
	}

	return false;
}

// Function - Events

DllExport bool PollEvents(Event* ev)
{
	if (ev == nullptr)return false;
	lock_guard lock(eventsLock); // TODO check why it needs to be here
	try
	{

		if (!events.empty())
		{
			*ev = events.front();
			events.pop();
			return true;
		}
	}
	catch (...)
	{
		AddError("PollEvents error: exception caught");
	}

	ev->who = Device();
	ev->what = What::LIBRARY_NOTHING;

	return false;
}

void eventCallbackAsync(Event ev)
{
	eventCallback(ev);
}

void AddEventAsync(Device device, What what, Characteristic * c)
{
	try
	{
		Event cb;
		cb.who = device;
		cb.what = what;

		if (c != nullptr)
		{
			cb.characteristic = *c;


			//wcscpy_s(cb.characteristic.uuid, sizeof(cb.characteristic.uuid) / sizeof(wchar_t), c->uuid);
			//cb.characteristic.isService = c->isService;
			//wcout << "thisisfrom" << c->isService << endl;
			//wcout << "thisisfrom" << cb.characteristic.isService << endl;
		}
		

		lock_guard lock(callbackLock);
		lock_guard lock2(eventsLock);
		if (eventCallback)	// if callback setup use it
		{	
			thread(eventCallbackAsync, cb).detach();
			while (!events.empty()) // if there was events waiting before setting up the callback
			{
				thread(eventCallbackAsync, events.front()).detach();
				events.pop();
			}
		}
		else
		{
			//otherwise store in queue for polling
			events.push(cb);
		}
	}
	catch (...)
	{
		//AddError("AddEventAsync error: exception caught");
	}
}

DllExport bool setEventCallback(bool (*callback)(Event ev))
{
	try
	{
		{
			lock_guard lock(callbackLock);
			eventCallback = callback;
		}
		AddEvent(What::LIBRARY_CALLBACK_SETUP); // used to callback all previous events not polled
	}
	catch (...)
	{
		AddError("setEventCallback error: exception caught");
	}

	return false;
}

DllExport bool removeEventCallback()
{
	try
	{
		lock_guard lock(callbackLock);
		eventCallback = nullptr;
		return true;
	}
	catch (...)
	{
		AddError("setEventCallback error: exception caught");
	}
	return false;
}

void AddEvent(Device device, What what, Characteristic *c)
{
	try
	{
		thread(AddEventAsync, device, what, c).detach();
	}
	catch (...)
	{
		//AddError("AddEvent(Device device, What what) error: exception caught");
	}
}

void AddEvent(Device device, What what)
{
	try
	{
		thread(AddEventAsync, device, what, nullptr).detach();
	}
	catch (...)
	{
		//AddError("AddEvent(Device device, What what) error: exception caught");
	}
}

void AddEvent(What what)
{
	try
	{
		Device odev;
		odev.address = 0;
		odev.name[0] = 0;
		thread(AddEventAsync, odev, what, nullptr).detach();
	}
	catch (...)
	{
		//AddError("AddEvent(What what)  error: exception caught");
	}
}

// Function - Check if bluetooth is enabled

DllExport bool IsBluetoothEnabled()
{
	try
	{
		BluetoothAdapter btAdapter = BluetoothAdapter::GetDefaultAsync().get();
		if (btAdapter == nullptr)
			return false;

		//wcout << "c = " << btAdapter.IsClassicSupported() << endl;
		//wcout << "c = " << btAdapter.IsLowEnergySupported() << endl;
		//wcout << "c = " << btAdapter.IsCentralRoleSupported() << endl;
		//wcout << "c = " << btAdapter.IsPeripheralRoleSupported() << endl;
		//wcout << "c = " << btAdapter.AreClassicSecureConnectionsSupported() << endl;
		//wcout << "c = " << btAdapter.AreLowEnergySecureConnectionsSupported() << endl;
		//wcout << "c = " << btAdapter.BluetoothAddress() << endl;
		//wcout << "d_id = " << btAdapter.DeviceId().c_str() << endl;

		Radio radios = btAdapter.GetRadioAsync().get();
		if (radios == nullptr)
			return false;

		//radios.SetStateAsync(RadioState::On).get();

		return radios.State() == RadioState::On;
	}
	catch (...)
	{
		AddError("IsBluetoothEnabled error: exception caught");
	}
	return false;
}

// Functions - Advertised devices

AdvertisedDevice* GetAdvertisedDevice(uint64_t deviceAddress)
{
	if (deviceAddress == 0) return nullptr;
	try
	{
		for (AdvertisedDevice* device : advertisedDevices)
		{
			if (device->address == deviceAddress)
			{
				return device;
			}
		}
	}
	catch (...)
	{
		AddError("GetAdvertisedDevice error: exception caught");
	}

	return nullptr;
}

void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{
	if (eventArgs.AdvertisementType() == BluetoothLEAdvertisementType::NonConnectableUndirected) return;	

	try
	{
		wstring localname = (wstring)eventArgs.Advertisement().LocalName();

		lock_guard lock(advertisedLock);

		AdvertisedDevice* advertisedDevice = GetAdvertisedDevice(eventArgs.BluetoothAddress());

		if (advertisedDevice != nullptr)
		{
			if (localname.length() && wstring(advertisedDevice->name) != localname)
			{
				wcscpy_s(advertisedDevice->name, sizeof(advertisedDevice->name) / sizeof(wchar_t), localname.c_str());
				//wcout << L" - update " << advertisedDevice.print() << endl;
				AddEvent(*advertisedDevice, What::DISCOVERED_UPDATED);
			}
			advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
			return;
		}

		advertisedDevice = new AdvertisedDevice;
		advertisedDevice->address = eventArgs.BluetoothAddress();
		wcscpy_s(advertisedDevice->name, sizeof(advertisedDevice->name) / sizeof(wchar_t), localname.c_str());
		//wcout << L" - add " << advertisedDevice->print() << endl;
		advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
		AddEvent(*advertisedDevice, What::DISCOVERED_NEW);

		advertisedDevices.push_back(advertisedDevice);
	}
	catch (...)
	{
		AddError("OnAdvertisementReceived error: exception caught");
	}
}

void timeoutThread()
{
	std::chrono::steady_clock::time_point t;
	while (threadRun)
	{
		try
		{
			t = std::chrono::steady_clock::now();
			lock_guard lock(advertisedLock);
			for (AdvertisedDevice* advertisedDevice : advertisedDevices)
			{
				if (std::chrono::duration_cast<std::chrono::milliseconds>(t - advertisedDevice->lastAdvertisment).count() > LOST_TIMEOUT_MS)
				{
					AddEvent(*advertisedDevice, What::DISCOVERED_LOST);
					advertisedDevices.remove(advertisedDevice);
					delete advertisedDevice;
					break;
				}
			}
		}
		catch (...)
		{
			AddError("timeoutThread error: exception caught");
		}

		for (int i = 0; i < 10 && threadRun; i++)
		{
			std::this_thread::sleep_for(10ms);
		}
	}
	threadRunning = false;
}

void StartDeviceScanAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		if (watcher == nullptr)
		{
			watcher = BluetoothLEAdvertisementWatcher();

			watcher.ScanningMode(BluetoothLEScanningMode::Active);
			watcherRevoker = watcher.Received(auto_revoke, OnAdvertisementReceived);

			watcher.SignalStrengthFilter().InRangeThresholdInDBm(-70);
			watcher.SignalStrengthFilter().OutOfRangeThresholdInDBm(-75);
			watcher.SignalStrengthFilter().OutOfRangeTimeout(TimeSpan::duration(2000));

			if(0) // Filter Advertisment
			{
				BluetoothLEManufacturerData manufacturerData;
				manufacturerData.CompanyId(0xFFFE);
				//DataWriter dw;
				//dw.WriteString(L"Neurinnov");
				//manufacturerData.Data(dw.DetachBuffer());
				watcher.AdvertisementFilter().Advertisement().ManufacturerData().Append(manufacturerData);
			}
		}

		for (AdvertisedDevice* advertisedDevice : advertisedDevices)
		{
			AddEvent(*advertisedDevice, What::DISCOVERED_REMOVED);
			delete advertisedDevice;
		}
		advertisedDevices.clear();

		if (threadRunning == false)
		{
			threadRun = true;
			threadRunning = true;
			_timeoutThread = thread(timeoutThread);
		}

		watcher.Start();
	}
	catch (...)
	{
		AddError("StartDeviceScanAsync error: exception caught");
	}
}

DllExport void StartDeviceScan()
{
	try
	{
		thread(StartDeviceScanAsync).detach();
	}
	catch (...)
	{
		AddError("StartDeviceScan error: exception caught");
	}
}

void StopDeviceScanAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		if (watcher != nullptr)
		{
			watcher.Stop();

			threadRun = false;
			_timeoutThread.join();
		}
	}
	catch (...)
	{
		AddError("StopDeviceScanAsync error: exception caught");
	}
}

DllExport void StopDeviceScan()
{
	try
	{
		thread(StopDeviceScan).detach();
	}
	catch (...)
	{
		AddError("StopDeviceScan error: exception caught");
	}
}

DllExport bool PollAdvertisedDevices(Device* device, bool first)
{
	if (device == nullptr)return false;
	try
	{
		lock_guard lock(advertisedLock);
		static uint16_t currentIndex = 0;

		if (first) currentIndex = 0;

		for (size_t i = 0; i < advertisedDevices.size(); i++)
		{
			if (i == currentIndex)
			{
				list<AdvertisedDevice*>::iterator it = advertisedDevices.begin();
				advance(it, currentIndex);
				wcscpy_s(device->name, sizeof(device->name) / sizeof(wchar_t), (*it)->name);
				device->address = (*it)->address;
				currentIndex++;
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("PollAdvertisedDevices error: exception caught");
		return false;
	}

	return false;
}

void CleanAdvertisedDevicesAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		for (AdvertisedDevice* advDev : advertisedDevices)
		{
			AddEvent(*advDev, What::DISCOVERED_REMOVED);
			delete advDev;
		}
		advertisedDevices.clear();
	}
	catch (...)
	{
		AddError("CleanAdvertisedDevicesAsync error: exception caught");
	}
}

DllExport void CleanAdvertisedDevices()
{
	try
	{
		thread(CleanAdvertisedDevices).detach();
	}
	catch (...)
	{
		AddError("CleanAdvertisedDevices error: exception caught");
	}
}

DllExport bool IsDeviceConnectable(const wchar_t* deviceName)
{
	if (deviceName == nullptr)return false;

	try
	{
		lock_guard lock(advertisedLock);
		for (AdvertisedDevice* advDev : advertisedDevices)
		{
			if (!wcscmp(advDev->name, deviceName))
			{
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("IsDeviceConnectable error: exception caught");
	}
	return false;
}

DllExport uint64_t GetDeviceAddress(const wchar_t* deviceName)
{
	if (deviceName == nullptr)return 0;
	try
	{
		lock_guard lock(advertisedLock);
		for (AdvertisedDevice* advDev : advertisedDevices)
		{
			if (!wcscmp(advDev->name, deviceName))
			{
				return advDev->address;
			}
		}
	}
	catch (...)
	{
		AddError("GetDeviceAddress error: exception caught");
	}
	return 0;
}

DllExport bool GetDevice(Device* odev, uint64_t deviceAddress)
{
	if (deviceAddress == 0)return false;
	if (odev == nullptr)return false;

	try
	{
		odev->address = 0;
		odev->name[0] = 0;

		lock_guard lock(advertisedLock);
		for (AdvertisedDevice* advDev : advertisedDevices)
		{
			if (advDev->address == deviceAddress)
			{
				*odev = *advDev;
				advDev->address = deviceAddress;
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("GetDeviceAddress error: exception caught in advertisment");
	}

	try
	{
		lock_guard lock(connectedLock);
		for (ConnectedDevice* dev : devices)
		{
			if (dev->address == deviceAddress)
			{
				odev->address = deviceAddress;
				int size = sizeof(dev->name) / sizeof(wchar_t);
				wcscpy_s(odev->name, sizeof(odev->name) / sizeof(wchar_t), dev->name);
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("GetDeviceAddress error: exception caught in devices");
	}

	return false;
}

// Functions - Connected devices

ConnectedDevice* GetDevice(uint64_t deviceAddress)
{
	try
	{
		for (ConnectedDevice* device : devices)
		{
			if (device->address == deviceAddress)
			{
				return device;
			}
		}
	}
	catch (...)
	{
		AddError("GetDevice error: exception caught");
	}
	return nullptr;
}

void ConnectionStatusChangedEvent(BluetoothLEDevice dev, const IInspectable& obj)
{
	try
	{
		lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(dev.BluetoothAddress());

		if (device != nullptr && device->isConnected)
		{
			if (dev.ConnectionStatus() != BluetoothConnectionStatus::Connected)
			{
				DisconnectDevice(device);
				AddEvent(*device, What::DEVICE_LOST);
				devices.remove(device);
				delete device;
			}
			else
			{
				AddEvent(*device, What::DEVICE_CONNECTED);
				lock_guard lock2(advertisedLock);
				AdvertisedDevice* advDev = GetAdvertisedDevice(device->address);
				if (advDev != nullptr)
				{
					AddEvent(*advDev, What::DISCOVERED_REMOVED);
					advertisedDevices.remove(advDev);
					delete advDev;
				}
			}
		}
	}
	catch (...)
	{
		AddError("ConnectionStatusChangedEvent error: exception caught");
	}
}

bool ConnectDeviceAsync(uint64_t deviceAddress)
{
	if (deviceAddress == 0) return false;

	try
	{
		lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(deviceAddress);

		if (device != nullptr) return device->isConnected;

		device = new ConnectedDevice();
		device->address = deviceAddress;
		device->leDev = BluetoothLEDevice::FromBluetoothAddressAsync(device->address).get();
		device->revoker = device->leDev.ConnectionStatusChanged(auto_revoke, ConnectionStatusChangedEvent);

		device->result = device->leDev.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
		device->isConnected = device->result.Status() == GattCommunicationStatus::Success;

		if (device->isConnected)
		{
			devices.push_back(device);

			// gather name Remove from advertisments
			lock_guard lock2(advertisedLock);
			AdvertisedDevice* advDev = GetAdvertisedDevice(deviceAddress);
			if (advDev != nullptr)
			{
				wcscpy_s(device->name, sizeof(device->name) / sizeof(wchar_t), advDev->name);
			}
			else
			{
				wstring w = (wstring)device->leDev.Name();
				wcscpy_s(device->name, sizeof(device->name) / sizeof(wchar_t), w.c_str());
			}
			thread(ScanCharacteristicsAsync, device).detach();
			return true;
		}
		else
		{
			AddEvent(*device, What::DEVICE_DISCONNECTED);
			DisconnectDevice(device);
			delete device;
			return false;
		}
	}
	catch (...)
	{
		AddError("ConnectDeviceAsync error: exception caught");
	}
	return false;
}

DllExport bool ConnectDevice(uint64_t deviceAddress, bool block)
{
	if (deviceAddress == 0)return false;
	try
	{
		if (block)
		{
			return ConnectDeviceAsync(deviceAddress);
		}
		else
		{
			thread(ConnectDeviceAsync, deviceAddress).detach();
		}
	}
	catch (...)
	{
		AddError("ConnectDevice error: exception caught");
	}

	return true;
}

void DisconnectDevice(ConnectedDevice* device)
{
	if (device == nullptr) return;
	try
	{
		device->isConnected = false;
		device->leDev.Close();
		device->revoker.revoke();

		for (ConnectedSubscription* sub : device->subscriptions)
		{
			sub->revoker.revoke();
			delete sub;
		}
		device->subscriptions.clear();
	}
	catch (...)
	{
		AddError("ConnectionStatusChangedEvent error: exception caught");
	}
}

void DisconnectDeviceAsync(uint64_t deviceAddress)
{
	if (deviceAddress == 0) return;
	try
	{
		lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(deviceAddress);
		if (device != nullptr)
		{
			DisconnectDevice(device);
			AddEvent(*device, What::DEVICE_DISCONNECTED);
			devices.remove(device);
			delete device;
			return;
		}
	}
	catch (...)
	{
		AddError("DisconnectDeviceAsync error: exception caught");
	}
}

DllExport void DisconnectDevice(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return;
	try
	{
		thread(DisconnectDeviceAsync, deviceAddress).detach();
	}
	catch (...)
	{
		AddError("DisconnectDevice error: exception caught");
	}
}

DllExport bool IsDeviceConnected(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return false;
	try
	{
		lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(deviceAddress);
		if (device != nullptr)
		{
			return device->isConnected;
		}
	}
	catch (...)
	{
		AddError("IsDeviceConnected error: exception caught");
	}

	return false;
}

// Functions - Characteristics (for connected devices)

ConnectedCharacteristic *FindCharacteristic(ConnectedDevice* dev, wstring t_uuid)
{
	if (dev == nullptr)return nullptr;
	try
	{
		transform(t_uuid.begin(), t_uuid.end(), t_uuid.begin(), towlower);
		const wchar_t* str = t_uuid.c_str();
		
		for (list<ConnectedCharacteristic*>::iterator it = dev->characteristics.begin(); it != dev->characteristics.end(); it++)
		{
			if (!wcscmp(str, (*it)->uuid))
			{
				return (*it);
			}
		}

		//wchar_t uuid[100];
		//IVectorView<GattDeviceService> services = dev->result.Services();
		//for (GattDeviceService service : services)
		//{
		//	//wcscpy_s(uuid, sizeof(uuid) / sizeof(wchar_t), to_hstring(service.Uuid()).c_str());
		//	//wcout << uuid << endl;

		//	IVectorView<GattCharacteristic> characteristics = service.GetCharacteristicsAsync().get().Characteristics();

		//	for (GattCharacteristic characteristic : characteristics)
		//	{
		//		wcscpy_s(uuid, sizeof(uuid) / sizeof(wchar_t), to_hstring(characteristic.Uuid()).c_str());
		//		if (wcslen(uuid) != t_uuid.length()) continue;
		//		//wcout << "\t" << uuid << endl;

		//		if (!wcscmp(t_uuid.c_str(), uuid))
		//		{
		//			return characteristic;
		//		}
		//	}
		//}
	}
	catch (...)
	{
		AddError("FindCharacteristic error: exception caught");
		return nullptr;
	}

	AddError("FindCharacteristic error: characteristic not found !");
	return nullptr;
}

bool DeviceWriteAsync(uint64_t deviceAddress, wchar_t* uuid, Data data)
{
	if (deviceAddress == 0)return false;
	if (uuid == nullptr) return false;
	if (data.buf == nullptr)return false;

	try
	{
		ConnectedDevice* device = nullptr;
		{
			lock_guard lock(connectedLock);
			device = GetDevice(deviceAddress);
			if (device == nullptr) return false;
		}
		
		ConnectedCharacteristic* characteristic = FindCharacteristic(device, uuid);
		if (characteristic == nullptr)
		{
			AddEvent(*device, What::CHARACTERISTIC_NOT_WRITTEN);
			return false;
		}

		array_view<uint8_t> av(data.buf, data.size);

		auto writer = DataWriter();
		writer.WriteBytes(av);
			
		auto status = characteristic->characteristic.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse).get();
		if (status.Status() == GattCommunicationStatus::Success)
		{
			AddEvent(*device, What::CHARACTERISTIC_WRITTEN, characteristic);
			return true;
		}
		AddEvent(*device, What::CHARACTERISTIC_NOT_WRITTEN);
		return false;
		
	}
	catch (...)
	{
		AddError("DeviceWriteAsync error: exception caught");
	}

	AddEvent(What::CHARACTERISTIC_NOT_WRITTEN);
	return false;
}

DllExport bool DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, Data* data, bool block)
{
	if (deviceAddress == 0)return false;
	try
	{
		// Sall we copy data ? -> probably (check if copy is enough)
		if (block)
		{
			return DeviceWriteAsync(deviceAddress, uuid, *data);
		}
		else
		{
			thread(DeviceWriteAsync, deviceAddress, uuid, *data).join();
		}
	}
	catch (...)
	{
		AddError("DeviceWrite error: exception caught");
	}

	return false;
}

DllExport bool DeviceRead(uint64_t deviceAddress, wchar_t* uuid, Data* data)
{
	if (deviceAddress == 0)return false;
	if (uuid == nullptr) return false;
	if (data->buf == nullptr)return false;

	try
	{
		ConnectedDevice* device = nullptr;
		{
			lock_guard lock(connectedLock);
			device = GetDevice(deviceAddress);
			if (device == nullptr) return false;
		}

		ConnectedCharacteristic* characteristic = FindCharacteristic(device, uuid);
		if (characteristic == nullptr)
		{
			AddEvent(*device, What::CHARACTERISTIC_NOT_READED);
			return false;
		}

		auto val = characteristic->characteristic.ReadValueAsync().get();

		if (val.Status() != GattCommunicationStatus::Success)
		{
			return false;
			AddEvent(*device, What::CHARACTERISTIC_NOT_READED);
		}

		data->size = val.Value().Length();

		memcpy(data->buf, val.Value().data(), data->size);
		data->buf[data->size] = '\0';

		AddEvent(*device, What::CHARACTERISTIC_READED);
		return true;
	}
	catch (...)
	{
		AddError("DeviceReadAsync error: exception caught");
	}

	AddEvent(What::CHARACTERISTIC_NOT_READED);
	return false;
}


bool DeviceSubscribeAsync(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0) return false;
	try
	{
		lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(deviceAddress);

		if (device != nullptr)
		{
			ConnectedCharacteristic* characteristic = FindCharacteristic(device, uuid);

			if (characteristic != nullptr)
			{
				// go trough subscriptions to check if existing
				for (ConnectedSubscription* sub : device->subscriptions)
				{
					if (sub->characteristic->characteristic.Uuid() == characteristic->characteristic.Uuid())
					{
						return true;
					}
				}


				ConnectedSubscription* sub = new ConnectedSubscription();


				sub->characteristic = characteristic;
	

				// TODO test 
				// sub->characteristic->characteristic.ProtectionLevel(GattProtectionLevel::EncryptionRequired);

				GattCommunicationStatus status = sub->characteristic->characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

				if (status == GattCommunicationStatus::Success)
				{
					sub->revoker = sub->characteristic->characteristic.ValueChanged(auto_revoke, [device, sub](const GattCharacteristic&, const GattValueChangedEventArgs& args)
						{
							try
							{
								Data cd;
								cd.size = args.CharacteristicValue().Length();
								memcpy(cd.buf, args.CharacteristicValue().data(), cd.size);
								cd.buf[cd.size] = '\0';

								sub->notificationQueue.push(cd);
								AddEvent(*device, What::CHARACTERISTIC_NOTIFICATION, sub->characteristic);
							}
							catch (...)
							{
								AddError("DeviceSubscribeAsync ValueChanged error: exception caught");
							}

							return;
						});

					device->subscriptions.push_back(sub);
					AddEvent(*device, What::CHARACTERISTIC_SUBSCRIBED, sub->characteristic);
					return true;
				}
				delete sub;
				AddEvent(*device, What::CHARACTERISTIC_NOT_SUBSCRIBED);
				return false;
			}
			AddEvent(*device, What::CHARACTERISTIC_NOT_SUBSCRIBED);
			return false;
		}
	}
	catch (...)
	{
		AddError("DeviceSubscribeAsync error: exception caught");
	}
	AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED);
	return false;
}

DllExport bool DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid, bool block)
{
	if (deviceAddress == 0)return false;
	if (uuid == nullptr)return false;
	try
	{
		if (block)
		{
			return DeviceSubscribeAsync(deviceAddress, uuid);
		}
		else
		{
			thread(DeviceSubscribeAsync, deviceAddress, uuid).join();
		}
	}
	catch (...)
	{
		AddError("DeviceSubscribe error: exception caught");
	}
	return false;
}

DllExport bool PollNotifications(Data* n, uint64_t deviceAddress, wchar_t * uuid)
{
	if (deviceAddress == 0) return false;
	if (n == nullptr) return false;

	try
	{
		n->size = 0;

		lock_guard lock(connectedLock);

		ConnectedDevice* dev = GetDevice(deviceAddress);

		if (dev == nullptr) return false;

		for (list<ConnectedSubscription*>::iterator i = dev->subscriptions.begin(); i != dev->subscriptions.end(); i++)
		{
			if (!wcscmp((*i)->characteristic->uuid, uuid))
			{
				if (!(*i)->notificationQueue.empty())
				{
					Data nn = (*i)->notificationQueue.front();

					memcpy(n->buf, nn.buf, nn.size);
					n->size = nn.size;

					(*i)->notificationQueue.pop();
					return true;
				}

			}
		}	
	}
	catch (...)
	{
		AddError("PollNotifications error: exception caught");
	}

	return false;
}

void ScanCharacteristicsAsync(ConnectedDevice * dev)
{
	if (dev == nullptr) return;
	try
	{
		{
			lock_guard lock(connectedLock);
			//std::queue<ConnectedCharacteristic> empty; // empty and clear pointers
			//std::swap(dev->characteristicQueue, empty);
		}
		
		IVectorView<GattDeviceService> services = dev->result.Services();
		for (GattDeviceService service : services)
		{
			{
				wstring uuidStr = (wstring)to_hstring(service.Uuid());
				//wcout << "s" << uuidStr << endl;
				ConnectedCharacteristic *current = new ConnectedCharacteristic(); 
				current->isService = true;
				//current.characteristic = 0;
				wcscpy_s(current->uuid, sizeof(current->uuid) / sizeof(wchar_t) , uuidStr.c_str());
				lock_guard lock(connectedLock);
				dev->characteristics.push_back(current);
			}
			
			IVectorView<GattCharacteristic> characteristics = service.GetCharacteristicsAsync().get().Characteristics();

			for (GattCharacteristic characteristic : characteristics)
			{
				wstring uuidStr = (wstring)to_hstring(characteristic.Uuid());
				//wcout << "c" << uuidStr << endl;
				ConnectedCharacteristic *current = new ConnectedCharacteristic(); 
				current->isService = false;
				current->characteristic = characteristic;
				wcscpy_s(current->uuid, sizeof(current->uuid) / sizeof(wchar_t), uuidStr.c_str());
				lock_guard lock(connectedLock);
				dev->characteristics.push_back(current);
			}
		}
		AddEvent(*dev, What::CHARACTERISTIC_SCANNED);
	}
	catch (...)
	{
		AddError("ScanCharacteristicsAsync error: exception caught");
	}
}

bool PollCharacteristics(Characteristic* c, uint64_t deviceAddress, bool first)
{
	if (deviceAddress == 0) return false;
	if (c == nullptr) return false;

	try
	{
		lock_guard lock(connectedLock);

		ConnectedDevice* dev = GetDevice(deviceAddress);

		if (dev == nullptr) return false;

		if (first) dev->characteristicPollIndex = 0;

		for (size_t i = 0; i < dev->characteristics.size(); i++)
		{
			if (i == dev->characteristicPollIndex)
			{
				list<ConnectedCharacteristic*>::iterator it = dev->characteristics.begin();
				advance(it, dev->characteristicPollIndex);

				wcscpy_s(c->uuid, sizeof(c->uuid) / sizeof(wchar_t), (*it)->uuid);
				c->isService = (*it)->isService;

				dev->characteristicPollIndex++;
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("PollCharacteristics error: exception caught");
	}

	return false;
}

// Function - Clean quit / init

DllExport void Clean()
{
	StopDeviceScanAsync();
	try
	{
		{
			lock_guard lock(advertisedLock);
			//watcher = nullptr;
			//watcherRevoker.revoke();
			for (AdvertisedDevice* advDevice : advertisedDevices)
			{
				delete advDevice;
			}
			advertisedDevices.clear();
		}
		{
			lock_guard lock(connectedLock);
			for (ConnectedDevice* device : devices)
			{
				DisconnectDevice(device);
				delete device;
			}
			devices.clear();
		}
		{
			lock_guard lock(errorLock);
			std::queue<Data> empty;
			std::swap(errors, empty);
		}
		{
			lock_guard lock3(eventsLock);
			std::queue<Event> empty;
			std::swap(events, empty);
		}
		{
			lock_guard lock4(callbackLock);
			eventCallback = nullptr;
		}
	}
	catch (...)
	{
		AddError("Clean error: exception caught");
	}
}