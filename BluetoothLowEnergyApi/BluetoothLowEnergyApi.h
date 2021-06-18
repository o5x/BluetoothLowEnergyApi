
#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <file>	BluetoothLowEnergyApi\BluetoothLowEnergyApi.h </file>
///
/// <copyright file="BluetoothLowEnergyApi.h" company="arrol.fr">
/// Copyright (c) 2021 arrol.fr. All rights reserved.
/// </copyright>
///
/// <summary>	Declares the bluetooth low energy API class. </summary>
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "pch.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A macro that defines DLL export. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

#define DllExport extern "C" __declspec( dllexport )

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Enumerations. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

enum class What
{
    /// <summary>	An enum constant representing the library error option. </summary>
    LIBRARY_ERROR,
    /// <summary>	An enum constant representing the library nothing option. </summary>
    LIBRARY_NOTHING,
    /// <summary>	An enum constant representing the library callback setup option. </summary>
    LIBRARY_CALLBACK_SETUP,
    /// <summary>	An enum constant representing the discovered started option. </summary>
    DISCOVERED_STARTED,
    /// <summary>	An enum constant representing the discovered stopped option. </summary>
    DISCOVERED_STOPPED,
    /// <summary>	An enum constant representing the discovered new option. </summary>
    DISCOVERED_NEW,
    /// <summary>	An enum constant representing the discovered updated option. </summary>
    DISCOVERED_UPDATED,
    /// <summary>	An enum constant representing the discovered lost option. </summary>
    DISCOVERED_LOST,
    /// <summary>	An enum constant representing the discovered removed option. </summary>
    DISCOVERED_REMOVED,
    /// <summary>	An enum constant representing the device connected option. </summary>
    DEVICE_CONNECTED,
    /// <summary>	An enum constant representing the device disconnected option. </summary>
    DEVICE_DISCONNECTED,
    /// <summary>	An enum constant representing the device lost option. </summary>
    DEVICE_LOST,
    /// <summary>	An enum constant representing the characteristic scan new option. </summary>
    CHARACTERISTIC_SCAN_NEW,
    /// <summary>	An enum constant representing the characteristic scan over option. </summary>
    CHARACTERISTIC_SCAN_OVER,
    /// <summary>	An enum constant representing the characteristic written option. </summary>
    CHARACTERISTIC_WRITTEN,
    /// <summary>	An enum constant representing the characteristic not written option. </summary>
    CHARACTERISTIC_NOT_WRITTEN,
    /// <summary>	An enum constant representing the characteristic readed option. </summary>
    CHARACTERISTIC_READED,
    /// <summary>	An enum constant representing the characteristic not readed option. </summary>
    CHARACTERISTIC_NOT_READED,
    /// <summary>	An enum constant representing the characteristic subscribed option. </summary>
    CHARACTERISTIC_SUBSCRIBED,
    /// <summary>	An enum constant representing the characteristic not subscribed option. </summary>
    CHARACTERISTIC_NOT_SUBSCRIBED,
    /// <summary>	An enum constant representing the characteristic un subscribed option. </summary>
    CHARACTERISTIC_UNSUBSCRIBED,
    /// <summary>	An enum constant representing the characteristic not un subscribed option. </summary>
    CHARACTERISTIC_NOT_UNSUBSCRIBED,
    /// <summary>	An enum constant representing the characteristic notification option. </summary>
    CHARACTERISTIC_NOTIFICATION
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Structures. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Device
{
	/// <summary>	The address. </summary>
	uint64_t address = 0;
	/// <summary>	The name[ 50]. </summary>
	wchar_t name[50] = L"";
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A characteristic. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Characteristic
{
    /// <summary>	True if is service, false if not. </summary>
    bool isService = false;
    /// <summary>	The uuid[ 50]. </summary>
    wchar_t uuid[50] = L"";
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A data. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Data
{
	/// <summary>	The buf[ 512]. </summary>
	uint8_t buf[512] = "";
	/// <summary>	The size. </summary>
	uint16_t size = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	An event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Event
{
	/// <summary>	The what. </summary>
	What what = What::LIBRARY_NOTHING;
	/// <summary>	The who. </summary>
	Device who;
	/// <summary>	The characteristic. </summary>
	Characteristic characteristic;
    /// <summary>	The data. </summary>
    Data data;
};


// Local only 

// Scan 
void RemoveAdvertisedDevice(uint64_t);
bool GetNameFromAdvertisment(uint64_t deviceAddress, wchar_t* devName);
void CleanScan();

// Connect
void CleanConnect();

// base
std::wstring addrToString(uint64_t addr);
void AddEvent(What what, Device* device, Characteristic* c, Data* d);
void AddError(const char* message, Device* dev, Characteristic* c);


////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Functions. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="ev">	[in,out] If non-null, the ev. </param>
///
/// <returns>	True if it succeeds, false if it fails. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool PollEvents(Event* ev);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Callback, called when the set event. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="callback">	[in,out] If non-null, the callback. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void SetEventCallback(void (*callback)(Event ev));

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Removes the event callback. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void RemoveEventCallback();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Queries if the bluetooth is enabled. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <returns>	True if the bluetooth is enabled, false if not. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool IsBluetoothEnabled();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void Clean();








////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device scan function. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void StartDeviceScan();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Stops device scan. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void StopDeviceScan();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Clean advertised devices. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void CleanAdvertisedDevices();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Remove all advertisement filters. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void RemoveAdertisementFilters();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Add advertisement filter. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="key">	            The advertisement key. </param>
/// <param name="data">				The advertisement data. </param>
/// <param name="len">				The advertisement data length. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void AddAdertisementFilter(uint8_t key, uint8_t* data, uint16_t len);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Remove advertisement filter. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="key">	            The advertisement key. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void RemoveAdertisementFilter(uint8_t key);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Set Advertisement device lost timeout. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="key">	            The advertisement key. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool SetAdvertisementDeviceLostTimeout(int32_t timeout_ms);






////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Connect functions. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void ConnectDevice(uint64_t deviceAddress);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnects the device described by deviceAddress. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DisconnectDevice(uint64_t deviceAddress);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Query if 'deviceAddress' is device connected. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
///
/// <returns>	True if device connected, false if not. </returns>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport bool IsDeviceConnected(uint64_t deviceAddress);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Get the number of devices connected. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport int16_t ConnectedCount();

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Characteristics. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
/// <param name="data">				[in,out] If non-null, the data. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, Data* data);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device read. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceRead(uint64_t deviceAddress, wchar_t* uuid);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device subscribe. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Device unsubscribe. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
///
/// <param name="deviceAddress">	The device address. </param>
/// <param name="uuid">				[in,out] If non-null, the uuid. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DeviceUnSubscribe(uint64_t deviceAddress, wchar_t* uuid);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	Disconnect All devices. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

DllExport void DisconnectAll();

