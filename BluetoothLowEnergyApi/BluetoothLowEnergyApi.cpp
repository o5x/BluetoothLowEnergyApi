////////////////////////////////////////////////////////////////////////////////////////////////////
/// <file>	BluetoothLowEnergyApi\BluetoothLowEnergyApi.cpp </file>
///
/// <copyright file="BluetoothLowEnergyApi.cpp" company="arrol.fr">
/// Copyright (c) 2021 arrol.fr. All rights reserved.
/// </copyright>
///
/// <summary>	Implements the bluetooth low energy API class. </summary>
////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Local structures and prototypes. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedCharacteristic : Characteristic 
{
	/// <summary>	The characteristic. </summary>
	GattCharacteristic characteristic = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A connected subscription. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedSubscription
{
	/// <summary>	The characteristic. </summary>
	ConnectedCharacteristic* characteristic = nullptr;
	/// <summary>	The revoker. </summary>
	GattCharacteristic::ValueChanged_revoker revoker;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	An advertised device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct AdvertisedDevice : Device
{
	/// <summary>	The last advertisment. </summary>
	chrono::steady_clock::time_point lastAdvertisment;
};

wstring addrToString(uint64_t addr);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A connected device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectedDevice : Device
{
	/// <summary>	The le development. </summary>
	BluetoothLEDevice leDev = nullptr;
	/// <summary>	The result. </summary>
	GattDeviceServicesResult result = nullptr;
	/// <summary>	The revoker. </summary>
	BluetoothLEDevice::ConnectionStatusChanged_revoker revoker;
	/// <summary>	The subscriptions. </summary>
	list<ConnectedSubscription*> subscriptions;
	/// <summary>	The characteristics. </summary>
	list<ConnectedCharacteristic*> characteristics;
	/// <summary>	True if is connected, false if not. </summary>
	bool isConnected = false;
	wstring print()
	{
		return addrToString(address) + L" " + (leDev.Name().size() ? wstring(leDev.Name()) : L"");
	}
};

void ScanCharacteristicsAsync(ConnectedDevice* device);
bool DisconnectDevice(ConnectedDevice* device);
void AddEvent(What what, Device* device, Characteristic* c, Data* d);

/// <summary>	Variables. </summary>
mutex eventsLock;
/// <summary>	The events. </summary>
queue<Event> events;

/// <summary>	The callback lock. </summary>
mutex callbackLock;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Callback, called when the event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="ev">	The ev. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void (*eventCallback)(Event ev) = nullptr;

/// <summary>	The advertised lock. </summary>
mutex advertisedLock;
/// <summary>	The advertised devices. </summary>
list<AdvertisedDevice*> advertisedDevices;

/// <summary>	The watcher. </summary>
BluetoothLEAdvertisementWatcher watcher = BluetoothLEAdvertisementWatcher();
/// <summary>	True to watcher setup. </summary>
bool watcherSetup = false;
/// <summary>	True to thread run. </summary>
bool threadRun = false;
/// <summary>	True to thread running. </summary>
bool threadRunning = false;
//thread;

/// <summary>	The connected lock. </summary>
mutex connectedLock;
/// <summary>	The devices. </summary>
list<ConnectedDevice*> devices;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Init / destroy objects. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

class BluetoothLowEnergyApi
{
public:
	BluetoothLowEnergyApi()
	{
		Clean();
	}

	~BluetoothLowEnergyApi()
	{
		Clean();
	}
}BluetoothLowEnergyApiInstance;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Function - Convert address to string. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="addr">	The address. </param>
///
/// <returns>	A wstring. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Function - Save and print error. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="message">	The message. </param>
/// <param name="dev">	  	[in,out] If non-null, the development. </param>
/// <param name="c">	  	[in,out] If non-null, a Characteristic to process. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void AddError(const char* message, Device * dev , Characteristic *c )
{
	try
	{
		Data error;
		error.size = (uint16_t)strlen(message);
		memcpy_s(error.buf, sizeof(error.buf) / sizeof(uint8_t), message, error.size);
		//cout << "ERROR [" << error.size << "]: " << (char*)error.buf << endl;
		AddEvent(What::LIBRARY_ERROR, dev, c, &error);
	}
	catch (...)
	{
		//AddError("AddError error: exception caught");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Function - Events. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="ev">	[in,out] If non-null, the ev. </param>
///
/// <returns>	True if it succeeds, false if it fails. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool PollEvents(Event* ev)
{
	if (ev == nullptr)return false;
	ev->what = What::LIBRARY_NOTHING;

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
		AddError("PollEvents error: exception caught", nullptr, nullptr);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Event callback asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="ev">	The ev. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void eventCallbackAsync(Event ev)
{
	eventCallback(ev);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Adds an event asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="cb">	The cb. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void AddEventAsync(Event cb)
{
	try
	{
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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Callback, called when the set event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="callback">	[in,out] If non-null, the callback. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void SetEventCallback(void (*callback)(Event ev))
{
	try
	{
		lock_guard lock(callbackLock);
		eventCallback = callback;
		AddEvent(What::LIBRARY_CALLBACK_SETUP, nullptr, nullptr, nullptr);
	}
	catch (...)
	{
		AddError("SetEventCallback error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Removes the event callback. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void RemoveEventCallback()
{
	try
	{
		lock_guard lock(callbackLock);
		eventCallback = nullptr;
	}
	catch (...)
	{
		AddError("RemoveEventCallback error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Adds an event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="what">  	The what. </param>
/// <param name="device">	[in,out] If non-null, the device. </param>
/// <param name="c">	 	[in,out] If non-null, a Characteristic to process. </param>
/// <param name="d">	 	[in,out] If non-null, a Data to process. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void AddEvent(What what, Device *device, Characteristic *c, Data *d)
{
	try
	{
		Event cb;
		cb.what = what;
		if (device != nullptr)cb.who = *device;
		if (c != nullptr)cb.characteristic = *c;
		if (d != nullptr)cb.data = *d;
		thread(AddEventAsync, cb).detach();
	}
	catch (...)
	{
		//AddError("AddEvent(Device device, What what) error: exception caught");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Function - Check if bluetooth is enabled. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <returns>	True if the bluetooth is enabled, false if not. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

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
		AddError("IsBluetoothEnabled error: exception caught", nullptr, nullptr);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Functions - Advertised devices. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
///
/// <returns>	Null if it fails, else the advertised device. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

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
		AddError("GetAdvertisedDevice error: exception caught", nullptr, nullptr);
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Raises the bluetooth le advertisement received event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="watcher">  	The watcher. </param>
/// <param name="eventArgs">	Event information to send to registered event handlers. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{
	if (eventArgs.AdvertisementType() == BluetoothLEAdvertisementType::NonConnectableUndirected) return;	

	AdvertisedDevice* advertisedDevice = nullptr;

	try
	{
		wstring localname = (wstring)eventArgs.Advertisement().LocalName();

		lock_guard lock(advertisedLock);

		advertisedDevice = GetAdvertisedDevice(eventArgs.BluetoothAddress());

		if (advertisedDevice != nullptr)
		{
			if (localname.length() && wstring(advertisedDevice->name) != localname)
			{
				wcscpy_s(advertisedDevice->name, sizeof(advertisedDevice->name) / sizeof(wchar_t), localname.c_str());
				//wcout << L" - update " << advertisedDevice.print() << endl;
				AddEvent(What::DISCOVERED_UPDATED, advertisedDevice, nullptr, nullptr);
			}
			advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
			return;
		}

		advertisedDevice = new AdvertisedDevice;
		advertisedDevice->address = eventArgs.BluetoothAddress();
		wcscpy_s(advertisedDevice->name, sizeof(advertisedDevice->name) / sizeof(wchar_t), localname.c_str());
		//wcout << L" - add " << advertisedDevice->print() << endl;
		advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
		AddEvent(What::DISCOVERED_NEW, advertisedDevice, nullptr, nullptr);

		advertisedDevices.push_back(advertisedDevice);
	}
	catch (...)
	{
		AddError("OnAdvertisementReceived error: exception caught", advertisedDevice, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Timeout thread. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

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
					advertisedDevices.remove(advertisedDevice);
					delete advertisedDevice;
					advertisedDevice = nullptr;
					break;
				}
			}
		}
		catch (...)
		{
			AddError("timeoutThread error: exception caught", nullptr, nullptr);
		}

		for (int i = 0; i < 10 && threadRun; i++)
		{
			std::this_thread::sleep_for(10ms);
		}
	}
	threadRunning = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Starts device scan asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void StartDeviceScanAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		if (!watcherSetup)
		{
			//watcher = BluetoothLEAdvertisementWatcher();

			watcher.ScanningMode(BluetoothLEScanningMode::Active);
			watcher.Received(OnAdvertisementReceived);

			watcher.SignalStrengthFilter().InRangeThresholdInDBm(-70);
			watcher.SignalStrengthFilter().OutOfRangeThresholdInDBm(-75);
			watcher.SignalStrengthFilter().OutOfRangeTimeout(TimeSpan::duration(2000));

			//if(false) // TODO Filter Advertisment
			//{
			//	BluetoothLEManufacturerData manufacturerData;
			//	manufacturerData.CompanyId(0xFFFE);
			//	//DataWriter dw;
			//	//dw.WriteString(L"Neurinnov");
			//	//manufacturerData.Data(dw.DetachBuffer());
			//	watcher.watcher.AdvertisementFilter().Advertisement().ManufacturerData().Append(manufacturerData);
			//}
			watcherSetup = true;
		}

		for (AdvertisedDevice* advertisedDevice : advertisedDevices)
		{
			AddEvent(What::DISCOVERED_REMOVED, advertisedDevice, nullptr, nullptr);
			delete advertisedDevice;
			advertisedDevice = nullptr;
		}

		advertisedDevices.clear();

		if (threadRunning == false)
		{
			threadRun = true;
			threadRunning = true;
			thread(timeoutThread).detach();
		}

		watcher.Start();

		AddEvent(What::DISCOVERED_STARTED, nullptr, nullptr, nullptr);
	}
	catch (...)
	{
		AddError("StartDeviceScanAsync error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Starts device scan. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void StartDeviceScan()
{
	try
	{
		thread(StartDeviceScanAsync).detach();
	}
	catch (...)
	{
		AddError("StartDeviceScan error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Stops device scan asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void StopDeviceScanAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		if (watcherSetup && threadRun)
		{
			watcher.Stop();
			threadRun = false;
			AddEvent(What::DISCOVERED_STOPPED, nullptr, nullptr, nullptr);
		}
	}
	catch (...)
	{
		AddError("StopDeviceScanAsync error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Stops device scan. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void StopDeviceScan()
{
	try
	{
		thread(StopDeviceScanAsync).detach();
	}
	catch (...)
	{
		AddError("StopDeviceScan error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean advertised devices asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void CleanAdvertisedDevicesAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		for (AdvertisedDevice* advDev : advertisedDevices)
		{
			AddEvent(What::DISCOVERED_REMOVED, advDev, nullptr, nullptr);
			delete advDev;
			advDev = nullptr;
		}
		advertisedDevices.clear();
	}
	catch (...)
	{
		AddError("CleanAdvertisedDevicesAsync error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean advertised devices. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void CleanAdvertisedDevices()
{
	try
	{
		thread(CleanAdvertisedDevices).detach();
	}
	catch (...)
	{
		AddError("CleanAdvertisedDevices error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Functions - Connected devices. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
///
/// <returns>	Null if it fails, else the device. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

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
		AddError("GetDevice error: exception caught", nullptr, nullptr);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connection status changed event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="dev">	The development. </param>
/// <param name="obj">	The object. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

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
				AddEvent(What::DEVICE_LOST, device, nullptr, nullptr);
				devices.remove(device);
				delete device;
				device = nullptr;
			}
			else
			{
				AddEvent(What::DEVICE_CONNECTED, device, nullptr, nullptr);
				lock_guard lock2(advertisedLock);
				AdvertisedDevice* advDev = GetAdvertisedDevice(device->address);
				if (advDev != nullptr)
				{
					AddEvent(What::DISCOVERED_REMOVED, advDev, nullptr, nullptr);
					advertisedDevices.remove(advDev);
					delete advDev;
					advDev = nullptr;
				}
			}
		}
	}
	catch (...)
	{
		AddError("ConnectionStatusChangedEvent error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connects a device asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void ConnectDeviceAsync(uint64_t deviceAddress)
{
	if (deviceAddress == 0) return;

	ConnectedDevice* device = nullptr;

	try
	{
		lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);

		if (device != nullptr) return;

		device = new ConnectedDevice();
		device->address = deviceAddress;
		device->leDev = BluetoothLEDevice::FromBluetoothAddressAsync(device->address).get();
		device->revoker = device->leDev.ConnectionStatusChanged(auto_revoke, ConnectionStatusChangedEvent);

		/*if (device->leDev.DeviceInformation().Pairing().CanPair())
		{
			cout << "can pair" << endl;
			device->leDev.DeviceInformation().Pairing().PairAsync(DevicePairingProtectionLevel::Encryption).get();
			if (device->leDev.DeviceInformation().Pairing().IsPaired())
			{
				cout << "is paired" << endl;
			}
			else
			{
				cout << "is NOT paired" << endl;
			}

		}*/
		
		//

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
		}
		else
		{
			AddEvent(What::DEVICE_DISCONNECTED, device, nullptr, nullptr);
			DisconnectDevice(device);
			delete device;
			device = nullptr;
		}
	}
	catch (...)
	{
		AddError("ConnectDeviceAsync error: exception caught", device, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connects a device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void ConnectDevice(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return;
	try
	{
		thread(ConnectDeviceAsync, deviceAddress).detach();
	}
	catch (...)
	{
		AddError("ConnectDevice error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnects the device described by device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="device">	[in,out] If non-null, the device. </param>
///
/// <returns>	True if it succeeds, false if it fails. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

bool DisconnectDevice(ConnectedDevice* device)
{
	if (device == nullptr) return false;
	try
	{
		device->isConnected = false;
		device->leDev.Close();
		device->revoker.revoke();

		for (ConnectedSubscription* sub : device->subscriptions)
		{
			sub->revoker.revoke();
			delete sub;
			sub = nullptr;
		}

		for (ConnectedCharacteristic* sub : device->characteristics)
		{
			delete sub;
			sub = nullptr;
		}

		device->subscriptions.clear();
		device->characteristics.clear();
		return true;
	}
	catch (...)
	{
		AddError("ConnectionStatusChangedEvent error: exception caught", device, nullptr);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnects the device asynchronous described by deviceAddress. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void DisconnectDeviceAsync(uint64_t deviceAddress)
{
	if (deviceAddress == 0) return;
	ConnectedDevice* device = nullptr;
	try
	{
		lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);
		if (device != nullptr)
		{
			if (DisconnectDevice(device))
			{
				AddEvent(What::DEVICE_DISCONNECTED, device, nullptr, nullptr);
			}
			devices.remove(device);
			delete device;
			device = nullptr;
			return;
		}
	}
	catch (...)
	{
		AddError("DisconnectDeviceAsync error: exception caught", device, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnects the device described by deviceAddress. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DisconnectDevice(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return;
	try
	{
		thread(DisconnectDeviceAsync, deviceAddress).detach();
	}
	catch (...)
	{
		AddError("DisconnectDevice error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Query if 'deviceAddress' is device connected. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
///
/// <returns>	True if device connected, false if not. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool IsDeviceConnected(uint64_t deviceAddress)
{
	if (deviceAddress == 0)return false;
	ConnectedDevice* device = nullptr;
	try
	{
		lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);
		if (device != nullptr)
		{
			return device->isConnected;
		}
	}
	catch (...)
	{
		AddError("IsDeviceConnected error: exception caught", device, nullptr);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Functions - Characteristics (for connected devices) </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="dev">   	[in,out] If non-null, the development. </param>
/// <param name="t_uuid">	The uuid. </param>
///
/// <returns>	Null if it fails, else the found characteristic. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

ConnectedCharacteristic* FindCharacteristic(ConnectedDevice* dev, wstring t_uuid)
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
	}
	catch (...)
	{
		AddError("FindCharacteristic error: exception caught", dev, nullptr);
		return nullptr;
	}

	AddError("FindCharacteristic error: characteristic not found !", dev, nullptr);
	return nullptr;
}

//ConnectedCharacteristic* FindCharacteristicEx(ConnectedDevice* dev, wstring t_uuid)
//{
//	const wchar_t* str = t_uuid.c_str();
//
//	ConnectedCharacteristic* current = nullptr;
//
//	if (dev == nullptr) return current;
//
//	try
//	{
//		IVectorView<GattDeviceService> services = dev->result.Services();
//		for (GattDeviceService service : services)
//		{
//			IVectorView<GattCharacteristic> characteristics = service.GetCharacteristicsAsync().get().Characteristics();
//
//			for (GattCharacteristic characteristic : characteristics)
//			{
//				wstring uuidStr = (wstring)to_hstring(characteristic.Uuid());
//				//wcout << "c" << uuidStr << endl;
//				current = new ConnectedCharacteristic();
//				current->isService = false;
//				current->characteristic = characteristic;
//				wcscpy_s(current->uuid, sizeof(current->uuid) / sizeof(wchar_t), uuidStr.c_str());
//
//				if (!wcscmp(current->uuid, str))
//				{
//					return current;
//				}
//
//				delete current;
//			}
//		}
//		AddEvent(What::CHARACTERISTIC_SCAN_OVER, dev, current, nullptr);
//	}
//	catch (...)
//	{
//		AddError("FindCharacteristicEx error: exception caught", dev, current);
//	}
//
//}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device write asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
/// <param name="data">				The data. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceWriteAsync(uint64_t deviceAddress, wchar_t* uuid, Data data)
{
	if (deviceAddress == 0)return;
	if (uuid == nullptr) return;
	if (data.buf == nullptr)return;

	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		{
			lock_guard lock(connectedLock);
			device = GetDevice(deviceAddress);

			if (device == nullptr)
			{
				AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
				return;
			}
		}
		
		characteristic = FindCharacteristic(device, uuid);
		if (characteristic == nullptr)
		{
			AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
			return;
		}

		array_view<uint8_t> av(data.buf, data.size);
		auto writer = DataWriter();
		writer.WriteBytes(av);
			
		auto status = characteristic->characteristic.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse).get();
		if (status.Status() == GattCommunicationStatus::Success)
		{
			AddEvent(What::CHARACTERISTIC_WRITTEN, device, characteristic, &data);
			return;
		}
		else
		{
			AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
			return;
		}
	}
	catch (...)
	{
		AddError("DeviceWriteAsync error: exception caught", device, characteristic);
	}

	AddEvent(What::CHARACTERISTIC_NOT_WRITTEN, device, characteristic, &data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device write. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
/// <param name="data">				[in,out] If non-null, the data. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, Data* data)
{
	if (deviceAddress == 0)return;
	try
	{
		thread(DeviceWriteAsync, deviceAddress, uuid, *data).detach();
	}
	catch (...)
	{
		AddError("DeviceWrite error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device read asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceReadAsync(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)return;
	if (uuid == nullptr) return;

	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		{
			//lock_guard lock(connectedLock);
			device = GetDevice(deviceAddress);
			if (device == nullptr) return;
		}

		characteristic = FindCharacteristic(device, uuid);
		if (characteristic == nullptr)
		{
			AddEvent(What::CHARACTERISTIC_NOT_READED, device, characteristic, nullptr);
			return;
		}

		//std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		//	std::chrono::system_clock::now().time_since_epoch()
		//	);
		//std::cout << "DATA at ;" << ms.count() << ";";

		//auto start = std::chrono::high_resolution_clock::now();
		auto val = characteristic->characteristic.ReadValueAsync(BluetoothCacheMode::Uncached).get();
		//auto stop = std::chrono::high_resolution_clock::now();
		//auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
		//std::cout << "DATA at ;" << duration.count() << ";" << std::endl;


		//std::chrono::milliseconds ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(
		//	std::chrono::system_clock::now().time_since_epoch()
		//	);
		//std::cout << ";" << ms2.count() << ";" << std::endl;

		if (val.Status() != GattCommunicationStatus::Success)
		{
			AddEvent(What::CHARACTERISTIC_NOT_READED, device, characteristic, nullptr);
			return;
		}

		Data data;
		data.size = val.Value().Length();

		memcpy(data.buf, val.Value().data(), data.size);
		data.buf[data.size] = '\0';


		//std::cout << data.buf << " " << data.size << std::endl;

		AddEvent(What::CHARACTERISTIC_READED, device, characteristic, &data);
		return;
	}
	catch (...)
	{
		AddError("DeviceReadAsync error: exception caught", device, characteristic);
	}

	AddEvent(What::CHARACTERISTIC_NOT_READED, device, characteristic, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device read. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceRead(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)return;
	try
	{
		thread(DeviceReadAsync, deviceAddress, uuid).detach();
	}
	catch (...)
	{
		AddError("DeviceRead error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device subscribe asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceSubscribeAsync(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0) return;
	ConnectedDevice* device = nullptr;
	ConnectedCharacteristic* characteristic = nullptr;

	try
	{
		lock_guard lock(connectedLock);
		device = GetDevice(deviceAddress);

		if (device != nullptr)
		{
			characteristic = FindCharacteristic(device, uuid);

			if (characteristic != nullptr)
			{
				// go trough subscriptions to check if existing
				for (ConnectedSubscription* sub : device->subscriptions)
				{
					if (sub->characteristic->characteristic.Uuid() == characteristic->characteristic.Uuid())
					{
						return;
					}
				}

				ConnectedSubscription* sub = new ConnectedSubscription();
				sub->characteristic = characteristic;

				// TODO test 
				//sub->characteristic->characteristic.ProtectionLevel(GattProtectionLevel::EncryptionRequired);

				GattCommunicationStatus status = sub->characteristic->characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

				if (status == GattCommunicationStatus::Success)
				{
					sub->revoker = sub->characteristic->characteristic.ValueChanged(auto_revoke, [device, sub](const GattCharacteristic&, const GattValueChangedEventArgs& args)
						{
							try
							{
								//std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
								//	std::chrono::system_clock::now().time_since_epoch()
								//	);
								//std::cout << "DATA at ;" << ms.count() << ";" << args.CharacteristicValue().Length() <<  std::endl;
								Data cd;
								cd.size = args.CharacteristicValue().Length();
								memcpy(cd.buf, args.CharacteristicValue().data(), cd.size);
								cd.buf[cd.size] = '\0';
								AddEvent(What::CHARACTERISTIC_NOTIFICATION, device, sub->characteristic, &cd);
							}
							catch (...)
							{
								AddError("DeviceSubscribeAsync ValueChanged error: exception caught", device, sub->characteristic);
							}

							return;
						});

					device->subscriptions.push_back(sub);
					AddEvent(What::CHARACTERISTIC_SUBSCRIBED, device, characteristic, nullptr);
					return;
				}
				delete sub;
				sub = nullptr;
				AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED, device, characteristic, nullptr);
				return;
			}
			AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED, device, characteristic, nullptr);
			return;
		}
	}
	catch (...)
	{
		AddError("DeviceSubscribeAsync error: exception caught", device, characteristic);
	}
	AddEvent(What::CHARACTERISTIC_NOT_SUBSCRIBED, device, characteristic, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device subscribe. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid)
{
	if (deviceAddress == 0)return;
	if (uuid == nullptr)return;
	try
	{
		thread(DeviceSubscribeAsync, deviceAddress, uuid).detach();
	}
	catch (...)
	{
		AddError("DeviceSubscribe error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Scans the characteristics asynchronous. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="dev">	[in,out] If non-null, the development. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

void ScanCharacteristicsAsync(ConnectedDevice * dev)
{
	if (dev == nullptr) return;

	ConnectedCharacteristic* current = nullptr;

	try
	{		
		IVectorView<GattDeviceService> services = dev->result.Services();
		for (GattDeviceService service : services)
		{
			{
				wstring uuidStr = (wstring)to_hstring(service.Uuid());
				//wcout << "s" << uuidStr << endl;
				current = new ConnectedCharacteristic();
				current->isService = true;
				//current.characteristic = 0;
				wcscpy_s(current->uuid, sizeof(current->uuid) / sizeof(wchar_t) , uuidStr.c_str());
				lock_guard lock(connectedLock);
				dev->characteristics.push_back(current);
				AddEvent(What::CHARACTERISTIC_SCAN_NEW, dev, current, nullptr);
			}
			
			IVectorView<GattCharacteristic> characteristics = service.GetCharacteristicsAsync().get().Characteristics();

			for (GattCharacteristic characteristic : characteristics)
			{
				wstring uuidStr = (wstring)to_hstring(characteristic.Uuid());
				//wcout << "c" << uuidStr << endl;
				current = new ConnectedCharacteristic();
				current->isService = false;
				current->characteristic = characteristic;
				wcscpy_s(current->uuid, sizeof(current->uuid) / sizeof(wchar_t), uuidStr.c_str());
				lock_guard lock(connectedLock);
				dev->characteristics.push_back(current);
				AddEvent(What::CHARACTERISTIC_SCAN_NEW, dev, current, nullptr);
			}
		}
		AddEvent(What::CHARACTERISTIC_SCAN_OVER, dev, current, nullptr);
	}
	catch (...)
	{
		AddError("ScanCharacteristicsAsync error: exception caught", dev, current);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Function - Clean quit / init. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

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
				advDevice = nullptr;
			}
			advertisedDevices.clear();
		}
		{
			lock_guard lock(connectedLock);
			for (ConnectedDevice* device : devices)
			{
				DisconnectDevice(device);
				delete device;
				device = nullptr;
			}
			devices.clear();
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
		AddError("Clean error: exception caught", nullptr, nullptr);
	}
}