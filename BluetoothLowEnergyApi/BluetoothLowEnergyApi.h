#pragma once

#include "pch.h"
#define DllExport extern "C" __declspec( dllexport )

/*
    Fix Delay to close when discovery is running
    Find out why 20 is max packet length
*/

// Library content & configuration 
#define LOST_TIMEOUT_MS 10000

// Enumerations
enum class What
{
    LIBRARY_ERROR,
    LIBRARY_NOTHING,
    LIBRARY_CALLBACK_SETUP,
    DISCOVERED_STARTED,
    DISCOVERED_STOPPED,
    DISCOVERED_NEW,
    DISCOVERED_UPDATED,
    DISCOVERED_LOST,
    DISCOVERED_REMOVED,
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    DEVICE_LOST,
    CHARACTERISTIC_SCAN_NEW,
    CHARACTERISTIC_SCAN_OVER,
    CHARACTERISTIC_WRITTEN,
    CHARACTERISTIC_NOT_WRITTEN,
    CHARACTERISTIC_READED,
    CHARACTERISTIC_NOT_READED,
    CHARACTERISTIC_SUBSCRIBED,
    CHARACTERISTIC_NOT_SUBSCRIBED,
    CHARACTERISTIC_NOTIFICATION
};

// Structures
struct Device
{
	uint64_t address = 0;
	wchar_t name[50] = L"";
};

struct Characteristic
{
    bool isService = false;
	wchar_t uuid[50] = L"";
};

struct Data
{
	uint8_t buf[512] = "";
	uint16_t size = 0;
};

struct Event
{
	What what = What::LIBRARY_NOTHING;
	Device who;
	Characteristic characteristic;
    Data data;
};


// Functions
DllExport bool PollEvents(Event* ev);

DllExport void SetEventCallback(void (*callback)(Event ev));
DllExport void RemoveEventCallback();

DllExport bool IsBluetoothEnabled();

// Device scan function
DllExport void StartDeviceScan();
DllExport void StopDeviceScan();
DllExport void CleanAdvertisedDevices();

// Connect functions
DllExport void ConnectDevice(uint64_t deviceAddress);
DllExport void DisconnectDevice(uint64_t deviceAddress);
DllExport bool IsDeviceConnected(uint64_t deviceAddress);

// Characteristics
DllExport void DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, Data* data);
DllExport void DeviceRead(uint64_t deviceAddress, wchar_t* uuid);
DllExport void DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid);

// Clean
DllExport void Clean();