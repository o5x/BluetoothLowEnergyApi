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
/// <summary>	An advertised device. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct AdvertisedDevice : Device
{
	/// <summary>	The last advertisment. </summary>
	chrono::steady_clock::time_point lastAdvertisment;
};

/// <summary>	The advertised lock. </summary>
mutex advertisedLock;

//mutex mtxtest;
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

	//if (eventArgs.Advertisement().LocalName().size())
	//{
	//	wcout << " + Advertisement = " << (wstring)eventArgs.Advertisement().LocalName() << "  " << eventArgs.Advertisement().ManufacturerData().Size() << endl;

	//	for (size_t i = 0; i < eventArgs.Advertisement().ManufacturerData().Size(); i++)
	//	{
	//		wcout << " | ADVdata " << i << " " << eventArgs.Advertisement().ManufacturerData().GetAt(i).CompanyId() << "  " << eventArgs.Advertisement().ManufacturerData().GetAt(i).Data().Length() << endl;
	//	}
	//}

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

DllExport void RemoveAdertisementFilter()
{
	watcher.AdvertisementFilter().Advertisement().ManufacturerData().Clear();
}

DllExport void SetAdertisementFilter(uint16_t CompanyId, wchar_t *data)
{
	RemoveAdertisementFilter();

	BluetoothLEManufacturerData manufacturerData;
	manufacturerData.CompanyId(CompanyId); // look for "Nv"
	DataWriter dw;
	dw.WriteString(hstring(data));	// look for content Neurinnov
	manufacturerData.Data(dw.DetachBuffer());
	watcher.AdvertisementFilter().Advertisement().ManufacturerData().Clear();
	watcher.AdvertisementFilter().Advertisement().ManufacturerData().Append(manufacturerData);
}

void StartDeviceScanAsync()
{
	try
	{
		lock_guard lock(advertisedLock);
		if (!watcherSetup)
		{
			watcher.ScanningMode(BluetoothLEScanningMode::Active);
			watcher.Received(OnAdvertisementReceived);

			watcher.SignalStrengthFilter().InRangeThresholdInDBm(-70);
			watcher.SignalStrengthFilter().OutOfRangeThresholdInDBm(-75);
			watcher.SignalStrengthFilter().OutOfRangeTimeout(TimeSpan::duration(2000));

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

void RemoveAdvertisedDevice(uint64_t devAddress)
{
	lock_guard lock2(advertisedLock);
	AdvertisedDevice* advDev = GetAdvertisedDevice(devAddress);
	if (advDev != nullptr)
	{
		AddEvent(What::DISCOVERED_REMOVED, advDev, nullptr, nullptr);
		advertisedDevices.remove(advDev);
		delete advDev;
		advDev = nullptr;
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

bool GetNameFromAdvertisment(uint64_t deviceAddress, wchar_t * devName)
{
	lock_guard lock2(advertisedLock);
	AdvertisedDevice* advDev = GetAdvertisedDevice(deviceAddress);
	if (advDev != nullptr)
	{
		wcscpy_s(devName, 50, advDev->name);
		return true;
	}

	return false;
}


void CleanScan()
{
	StopDeviceScanAsync();

	try
	{
		{
			//mtxtest.lock();
			lock_guard lock(advertisedLock);
			//watcher = nullptr;

			if (advertisedDevices.size() > 0)
			{
				for (AdvertisedDevice* advDevice : advertisedDevices)
				{
					delete advDevice;
					advDevice = nullptr;
				}
				advertisedDevices.clear();
			}			
		}
	}
	catch (...)
	{
		AddError("Clean error: exception caught", nullptr, nullptr);
	}
}