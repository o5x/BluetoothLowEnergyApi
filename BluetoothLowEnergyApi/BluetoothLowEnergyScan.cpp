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
	std::map<uint8_t, std::vector<uint8_t>> advertisedData;
	bool filtered = true;
};

/// <summary>	The advertised lock. </summary>
//mutex advertisedLock;

int32_t AdvertisementDeviceLostTimeoutMs = 15000;

//mutex mtxtest;
/// <summary>	The advertised devices. </summary>
list<AdvertisedDevice*> advertisedDevices;

std::map<uint8_t, vector<uint8_t>> advertisementFilters;

/// <summary>	The watcher. </summary>
BluetoothLEAdvertisementWatcher watcher = BluetoothLEAdvertisementWatcher();
/// <summary>	True to watcher setup. </summary>
bool watcherSetup = false;
/// <summary>	True to thread run. </summary>
bool threadRun = false;
/// <summary>	True to thread running. </summary>
bool threadRunning = false;
//thread;

void updateFiltered(AdvertisedDevice* advertisedDevice);

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

bool AdvUpdate(AdvertisedDevice* advertisedDevice, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{
	bool updated = false;
	advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();

	IVector<BluetoothLEAdvertisementDataSection> dataSections = eventArgs.Advertisement().DataSections();

	for (BluetoothLEAdvertisementDataSection dataSection : dataSections)
	{
		uint8_t key = dataSection.DataType();
		uint8_t dataLen = dataSection.Data().Length();
		vector<uint8_t> data;
		for (uint8_t i = 0; i < dataLen; i++)
		{
			data.push_back(dataSection.Data().data()[i]);
		}

		if (data != advertisedDevice->advertisedData[key])
		{
			advertisedDevice->advertisedData[key] = data;
			updated = true;
		}
	}

	updateFiltered(advertisedDevice);
	return updated;
}

void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{
	if (eventArgs.AdvertisementType() == BluetoothLEAdvertisementType::NonConnectableUndirected) return;

	AdvertisedDevice* advertisedDevice = nullptr;

	try
	{
		wstring localname = (wstring)eventArgs.Advertisement().LocalName();

		//lock_guard lock(advertisedLock);

		advertisedDevice = GetAdvertisedDevice(eventArgs.BluetoothAddress());

		if (advertisedDevice != nullptr)
		{
			if (localname.length() && wstring(advertisedDevice->name) != localname)
			{
				wcscpy_s(advertisedDevice->name, sizeof(advertisedDevice->name) / sizeof(wchar_t), localname.c_str());
				//wcout << L" - update " << advertisedDevice.print() << endl;
			}

			bool updated = AdvUpdate(advertisedDevice, eventArgs);

			if (!advertisedDevice->filtered && updated)
			{
				AddEvent(What::DISCOVERED_UPDATED, advertisedDevice, nullptr, nullptr);
			}

			advertisedDevice->lastAdvertisment = std::chrono::steady_clock::now();
			return;
		}

		advertisedDevice = new AdvertisedDevice;
		advertisedDevice->address = eventArgs.BluetoothAddress();
		wcscpy_s(advertisedDevice->name, sizeof(advertisedDevice->name) / sizeof(wchar_t), localname.c_str());
		//wcout << L" - add " << advertisedDevice->print() << endl;
		AdvUpdate(advertisedDevice, eventArgs);
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
	if (AdvertisementDeviceLostTimeoutMs < 0)
	{
		return;
	}

	std::chrono::steady_clock::time_point t;
	while (threadRun)
	{
		try
		{
			t = std::chrono::steady_clock::now();
			//lock_guard lock(advertisedLock);
			for (AdvertisedDevice* advertisedDevice : advertisedDevices)
			{
				if (std::chrono::duration_cast<std::chrono::milliseconds>(t - advertisedDevice->lastAdvertisment).count() > AdvertisementDeviceLostTimeoutMs)
				{
					advertisedDevices.remove(advertisedDevice);
					if (!advertisedDevice->filtered)
					{
						AddEvent(What::DISCOVERED_LOST, advertisedDevice, nullptr, nullptr);
					}
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
/// <summary>	Starts device scan. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

bool filtered(std::map<uint8_t, vector<uint8_t>> dev)
{
	std::map<uint8_t, vector<uint8_t>>::iterator it;

	for (it = advertisementFilters.begin(); it != advertisementFilters.end(); ++it)
	{
		if (advertisementFilters[it->first] != dev[it->first])
		{
			return true;
		}
	}

	return false; // we don't filter
}

void updateFiltered(AdvertisedDevice* advertisedDevice)
{
	bool f = filtered(advertisedDevice->advertisedData);

	if (advertisedDevice->filtered && !f)
	{
		advertisedDevice->filtered = false;
		AddEvent(What::DISCOVERED_NEW, advertisedDevice, nullptr, nullptr);
	}
	else if (!advertisedDevice->filtered && f)
	{
		advertisedDevice->filtered = true;
		AddEvent(What::DISCOVERED_LOST, advertisedDevice, nullptr, nullptr);
	}
}

void ReMatchAdvertisements()
{
	for (AdvertisedDevice* advertisedDevice : advertisedDevices)
	{
		updateFiltered(advertisedDevice);
	}
}

DllExport void RemoveAdertisementFilters()
{
	advertisementFilters.clear();
}

DllExport void AddAdertisementFilter(uint8_t key, uint8_t* data, uint16_t len)
{
	vector<uint8_t> advd;
	for (size_t i = 0; i < len; i++)
	{
		advd.push_back(data[i]);
	}
	advertisementFilters[key] = advd;
	ReMatchAdvertisements();
}

DllExport void RemoveAdertisementFilter(uint8_t key)
{
	try
	{
		advertisementFilters.erase(key);
	}
	catch (...)
	{
	}
	ReMatchAdvertisements();
}

DllExport bool SetAdvertisementDeviceLostTimeout(int32_t timeout_ms)
{
	if (0 <= timeout_ms && timeout_ms < 1000)
	{
		return false;
	}
	AdvertisementDeviceLostTimeoutMs = timeout_ms;
	return true;
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
		//lock_guard lock(advertisedLock);
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
		AddError("StartDeviceScan error: exception caught", nullptr, nullptr);
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
		//lock_guard lock(advertisedLock);
		if (watcherSetup && threadRun)
		{
			watcher.Stop();
			threadRun = false;
			AddEvent(What::DISCOVERED_STOPPED, nullptr, nullptr, nullptr);
		}
	}
	catch (...)
	{
		AddError("StopDeviceScan error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean advertised devices. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void CleanAdvertisedDevices()
{
	try
	{
		//lock_guard lock(advertisedLock);
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
		AddError("CleanAdvertisedDevices error: exception caught", nullptr, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Remove advertised device from list </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void RemoveAdvertisedDevice(uint64_t devAddress)
{
	//lock_guard lock2(advertisedLock);
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
/// <summary>	Get Name from advertisement </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

bool GetNameFromAdvertisment(uint64_t deviceAddress, wchar_t* devName)
{
	//lock_guard lock2(advertisedLock);
	AdvertisedDevice* advDev = GetAdvertisedDevice(deviceAddress);
	if (advDev != nullptr)
	{
		wcscpy_s(devName, 50, advDev->name);
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

void CleanScan()
{
	StopDeviceScan();

	try
	{
		//mtxtest.lock();
		//lock_guard lock(advertisedLock);

		if (advertisedDevices.size() > 0)
		{
			for (AdvertisedDevice* advDevice : advertisedDevices)
			{
				delete advDevice;
				advDevice = nullptr;
			}
			advertisedDevices.clear();
		}

		RemoveAdertisementFilters();
	}
	catch (...)
	{
		AddError("Clean error: exception caught", nullptr, nullptr);
	}
}