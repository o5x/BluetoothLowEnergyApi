#pragma once

#include "pch.h"
#define DllExport extern "C" __declspec( dllexport )

// Library content & configuration 
#define LOST_TIMEOUT_MS 10000

// Enum
enum class What
{
	LIBRARY_ERROR,
	LIBRARY_NOTHING,
	DISCOVERED_NEW,
	DISCOVERED_UPDATED,
	DISCOVERED_LOST,
	DISCOVERED_REMOVED,
	DEVICE_CONNECTED,
	DEVICE_DISCONNECTED,
	DEVICE_LOST,
	CHARACTERISTIC_WRITTEN,
	CHARACTERISTIC_NOT_WRITTEN,
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

struct Event
{
	What what = What::LIBRARY_NOTHING;
	Device who;
};

struct Data
{
	uint8_t buf[512];
	uint16_t size;
};

// Functions
DllExport bool PollErrors(Data* e);

DllExport bool PollEvents(Event* ev);

DllExport bool IsBluetoothEnabled();

// Device scan function
DllExport void StartDeviceScan();
DllExport void StopDeviceScan();
DllExport bool PollAdvertisedDevices(Device* device, bool first = false);
DllExport void CleanAdvertisedDevices();
DllExport bool IsDeviceConnectable(const wchar_t* deviceName);
DllExport uint64_t GetDeviceAddress(const wchar_t* deviceName);
DllExport bool GetDevice(Device* dev, uint64_t deviceAddress);

// Connect functions
DllExport bool ConnectDevice(uint64_t deviceAddress, bool block = false);
DllExport void DisconnectDevice(uint64_t deviceAddress);
DllExport bool IsDeviceConnected(uint64_t deviceAddress);

// Characteristics
DllExport bool DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, wchar_t* string, bool block = false);
DllExport bool DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid, bool block = false);
DllExport bool PollNotifications(Data* n, uint64_t deviceAddress);

// Clean
DllExport void Clean();