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
struct Subscription
{
	GattCharacteristic characteristic = nullptr;
	GattCharacteristic::ValueChanged_revoker revoker;
};

struct AdvertisedDevice
{
	Device dev;
	chrono::steady_clock::time_point lastAdvertisment;
};

wstring addrToString(uint64_t addr);

struct ConnectedDevice
{
	Device dev;
	BluetoothLEDevice leDev = nullptr;
	GattDeviceServicesResult result = nullptr;
	BluetoothLEDevice::ConnectionStatusChanged_revoker revoker;
	list<Subscription*> subscriptions;
	bool isConnected = false;
	queue<Data> notificationQueue;
	wstring print()
	{
		return addrToString(dev.address) + L" " + (leDev.Name().size() ? wstring(leDev.Name()) : L"");
	}
};

void DisconnectDevice(ConnectedDevice* device);
void AddEvent(Device device, What what);
void AddEvent(What what);

// Variables
mutex errorLock;
queue<Data> errors;

mutex eventsLock;
queue<Event> events;

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
		//AddError("AddErrorAsync error: exception catched");
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
		//AddError("AddError error: exception catched");
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
		AddError("PollErrors error: exception catched");
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
		AddError("PollEvents error: exception catched");
	}
	
	ev->who = Device();
	ev->what = What::LIBRARY_NOTHING;
	
	return false;
}

void AddEventAsync(Device device, What what)
{
	try 
	{
		Event cb;
		cb.who = device;
		cb.what = what;

		lock_guard lock(eventsLock);
		events.push(cb);
	}
	catch (...) 
	{
		//AddError("AddEventAsync error: exception catched");
	}
}

void AddEvent(Device device, What what)
{
	try
	{
		thread(AddEventAsync, device, what).detach();
	}
	catch (...)
	{
		//AddError("AddEvent(Device device, What what) error: exception catched");
	}
}

void AddEvent(What what)
{
	try 
	{
		Device odev;
		odev.address = 0;
		odev.name[0] = 0;
		thread(AddEventAsync, odev, what).detach();
	}
	catch (...)
	{
		//AddError("AddEvent(What what)  error: exception catched");
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
		AddError("IsBluetoothEnabled error: exception catched");
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
			if (device->dev.address == deviceAddress)
			{
				return device;
			}
		}
	}
	catch (...)
	{
		AddError("GetAdvertisedDevice error: exception catched");
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
			if (localname.length() && wstring(advertisedDevice->dev.name) != localname)
			{
				wcscpy_s(advertisedDevice->dev.name, sizeof(localname) / sizeof(wchar_t), localname.c_str());
				//wcout << L" - update " << advertisedDevice.print() << endl;
				AddEvent(advertisedDevice->dev, What::DISCOVERED_UPDATED);
			}
			advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
			return;
		}

		advertisedDevice = new AdvertisedDevice;
		advertisedDevice->dev.address = eventArgs.BluetoothAddress();
		wcscpy_s(advertisedDevice->dev.name, sizeof(localname) / sizeof(wchar_t), localname.c_str());
		//wcout << L" - add " << advertisedDevice->print() << endl;
		advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
		AddEvent(advertisedDevice->dev, What::DISCOVERED_NEW);

		advertisedDevices.push_back(advertisedDevice);
	}
	catch (...)
	{
		AddError("OnAdvertisementReceived error: exception catched");
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
					AddEvent(advertisedDevice->dev, What::DISCOVERED_LOST);
					advertisedDevices.remove(advertisedDevice);
					delete advertisedDevice;
					break;
				}
			}
		}
		catch (...)
		{
			AddError("timeoutThread error: exception catched");
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
		}

		for (AdvertisedDevice* advertisedDevice : advertisedDevices)
		{
			AddEvent(advertisedDevice->dev, What::DISCOVERED_REMOVED);
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
		AddError("StartDeviceScanAsync error: exception catched");
	}
}

DllExport void StartDeviceScan()
{
	try
	{
		thread(StartDeviceScanAsync).detach();
	}
	catch(...)
	{
		AddError("StartDeviceScan error: exception catched");
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
		AddError("StopDeviceScanAsync error: exception catched");
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
		AddError("StopDeviceScan error: exception catched");
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
				wcscpy_s(device->name, sizeof((*it)->dev.name) / sizeof(wchar_t), (*it)->dev.name);
				device->address = (*it)->dev.address;
				currentIndex++;
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("PollAdvertisedDevices error: exception catched");
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
			AddEvent(advDev->dev, What::DISCOVERED_REMOVED);
			delete advDev;
		}
		advertisedDevices.clear();
	}
	catch (...)
	{
		AddError("CleanAdvertisedDevicesAsync error: exception catched");
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
		AddError("CleanAdvertisedDevices error: exception catched");
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
			if (!wcscmp(advDev->dev.name, deviceName))
			{
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("IsDeviceConnectable error: exception catched");
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
			if (!wcscmp(advDev->dev.name, deviceName))
			{
				return advDev->dev.address;
			}
		}
	}
	catch (...)
	{
		AddError("GetDeviceAddress error: exception catched");
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
			if (advDev->dev.address == deviceAddress)
			{
				*odev = advDev->dev;
				advDev->dev.address = deviceAddress;
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("GetDeviceAddress error: exception catched in advertisment");
	}

	try
	{
		lock_guard lock(connectedLock);
		for (ConnectedDevice* dev : devices)
		{
			if (dev->dev.address == deviceAddress)
			{
				odev->address = deviceAddress;
				int size = sizeof(dev->dev.name) / sizeof(wchar_t);
				wcscpy_s(odev->name, size, dev->dev.name);
				return true;
			}
		}
	}
	catch (...)
	{
		AddError("GetDeviceAddress error: exception catched in devices");
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
			if (device->dev.address == deviceAddress)
			{
				return device;
			}
		}
	}
	catch (...)
	{
		AddError("GetDevice error: exception catched");
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
				AddEvent(device->dev, What::DEVICE_LOST);
				devices.remove(device);
				delete device;
			}
			else
			{
				AddEvent(device->dev, What::DEVICE_CONNECTED);
				lock_guard lock2(advertisedLock);
				AdvertisedDevice* advDev = GetAdvertisedDevice(device->dev.address);
				if (advDev != nullptr)
				{
					AddEvent(advDev->dev, What::DISCOVERED_REMOVED);
					advertisedDevices.remove(advDev);
					delete advDev;
				}
			}
		}
	}
	catch (...)
	{
		AddError("ConnectionStatusChangedEvent error: exception catched");
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
		device->dev.address = deviceAddress;
		device->leDev = BluetoothLEDevice::FromBluetoothAddressAsync(device->dev.address).get();
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
				wcscpy_s(device->dev.name, sizeof(advDev->dev.name) / sizeof(wchar_t), advDev->dev.name);
			}
			else
			{
				wstring w = (wstring)device->leDev.Name();
				wcscpy_s(device->dev.name, sizeof(w.c_str()) / sizeof(wchar_t), w.c_str());
			}
		}
		else
		{
			AddEvent(device->dev, What::DEVICE_DISCONNECTED);
			DisconnectDevice(device);
			delete device;
		}

		return device->isConnected;
	}
	catch (...)
	{
		AddError("ConnectDeviceAsync error: exception catched");
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
		AddError("ConnectDevice error: exception catched");
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

		for (Subscription* sub : device->subscriptions)
		{
			sub->revoker.revoke();
			delete sub;
		}
		device->subscriptions.clear();		
	}
	catch (...) 
	{
		AddError("ConnectionStatusChangedEvent error: exception catched");
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
			AddEvent(device->dev, What::DEVICE_DISCONNECTED);
			devices.remove(device);
			delete device;
			return;
		}
	}
	catch(...)
	{
		AddError("DisconnectDeviceAsync error: exception catched");
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
		AddError("DisconnectDevice error: exception catched");
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
		AddError("IsDeviceConnected error: exception catched");
	}
	
	return false;
}

// Functions - Characteristics (for connected devices)

GattCharacteristic FindCharacteristic(ConnectedDevice* dev, wstring t_uuid)
{
	if (dev == nullptr)return nullptr;
	try 
	{
		transform(t_uuid.begin(), t_uuid.end(), t_uuid.begin(), towlower);

		wchar_t uuid[100];
		IVectorView<GattDeviceService> services = dev->result.Services();
		for (GattDeviceService service : services)
		{
			//wcscpy_s(uuid, sizeof(uuid) / sizeof(wchar_t), to_hstring(service.Uuid()).c_str());
			//wcout << uuid << endl;

			IVectorView<GattCharacteristic> characteristics = service.GetCharacteristicsAsync().get().Characteristics();

			for (GattCharacteristic characteristic : characteristics)
			{
				wcscpy_s(uuid, sizeof(uuid) / sizeof(wchar_t), to_hstring(characteristic.Uuid()).c_str());
				if (wcslen(uuid) != t_uuid.length()) continue;
				//wcout << "\t" << uuid << endl;

				if (!wcscmp(t_uuid.c_str(), uuid))
				{
					return characteristic;
				}
			}
		}
	}
	catch (...)
	{
		AddError("FindCharacteristic error: exception catched");
		return nullptr;
	}

	AddError("FindCharacteristic error: characteristic not found !");
	return nullptr;
}

bool DeviceWriteAsync(uint64_t deviceAddress, wchar_t* uuid, wchar_t* string)
{
	if (deviceAddress == 0)return false;
	if (uuid == nullptr)return false;
	try 
	{
		lock_guard lock(connectedLock);
		ConnectedDevice* device = GetDevice(deviceAddress);

		if (device != nullptr)
		{
			GattCharacteristic characteristic = FindCharacteristic(device, uuid);
			if (characteristic == nullptr)
			{
				AddEvent(device->dev, What::CHARACTERISTIC_NOT_WRITTEN);
				return false;
			}
			auto writer = DataWriter();
			writer.WriteString(string);
			auto status = characteristic.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse).get();
			if (status.Status() == GattCommunicationStatus::Success)
			{
				AddEvent(device->dev, What::CHARACTERISTIC_WRITTEN);
				return true;
			}
			AddEvent(device->dev, What::CHARACTERISTIC_NOT_WRITTEN);
			return false;
		}
	}
	catch (...)
	{
		AddError("DeviceWriteAsync error: exception catched");
	}

	AddEvent(What::CHARACTERISTIC_NOT_WRITTEN);
	return false;
}

DllExport bool DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, wchar_t* string, bool block)
{
	if (deviceAddress == 0)return false;
	try
	{
		if (block)
		{
			return DeviceWriteAsync(deviceAddress, uuid, string);
		}
		else
		{
			thread(DeviceWriteAsync, deviceAddress, uuid, string).join();
		}
	}
	catch (...)
	{
		AddError("DeviceWrite error: exception catched");
	}
	
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
			GattCharacteristic characteristic = FindCharacteristic(device, uuid);

			if (characteristic != nullptr)
			{
				// go trough subscriptions to check if existing
				for (Subscription* sub : device->subscriptions)
				{
					if (sub->characteristic.Uuid() == characteristic.Uuid())
					{
						return true;
					}
				}

				Subscription* sub = new Subscription();

				sub->characteristic = characteristic;
				GattCommunicationStatus status = sub->characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

				if (status == GattCommunicationStatus::Success)
				{
					sub->revoker = sub->characteristic.ValueChanged(auto_revoke, [device](const GattCharacteristic&, const GattValueChangedEventArgs& args)
						{
							try 
							{
								Data cd;
								cd.size = args.CharacteristicValue().Length();
								memcpy(cd.buf, args.CharacteristicValue().data(), cd.size);
								cd.buf[cd.size] = '\0';

								device->notificationQueue.push(cd);
								AddEvent(device->dev, What::CHARACTERISTIC_NOTIFICATION);
							}
							catch (...)
							{
								AddError("DeviceSubscribeAsync ValueChanged error: exception catched");
							}
							
							return;
						});

					device->subscriptions.push_back(sub);
					AddEvent(device->dev, What::CHARACTERISTIC_SUBSCRIBED);
					return true;
				}
				delete sub;
				AddEvent(device->dev, What::CHARACTERISTIC_NOT_SUBSCRIBED);
				return false;
			}
			AddEvent(device->dev, What::CHARACTERISTIC_NOT_SUBSCRIBED);
			return false;
		}
	}
	catch (...)
	{
		AddError("DeviceSubscribeAsync error: exception catched");
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
		AddError("DeviceSubscribe error: exception catched");
	}
	return false;
}

DllExport bool PollNotifications(Data* n, uint64_t deviceAddress)
{
	if (deviceAddress == 0) return false;
	if (n == nullptr) return false;
	
	try 
	{
		n->size = 0;

		lock_guard lock(connectedLock);

		ConnectedDevice* dev = GetDevice(deviceAddress);

		if (dev == nullptr) return false;

		if (!dev->notificationQueue.empty())
		{
			Data nn = dev->notificationQueue.front();

			memcpy(n->buf, nn.buf, nn.size);
			n->size = nn.size;

			dev->notificationQueue.pop();
			return true;
		}
	}
	catch (...)
	{
		AddError("PollNotifications error: exception catched");
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
				//AddEvent(advDevice->dev, What::DISCOVERED_REMOVED);
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
	}
	catch (...)
	{
		AddError("Clean error: exception catched");
	}
}