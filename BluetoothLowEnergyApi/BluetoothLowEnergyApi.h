#pragma once

#include "pch.h"
#define DllExport extern "C" __declspec( dllexport )

// Library content & configuration 
#define LOST_TIMEOUT_MS 10000

// add auto read charactristics (to avoid latency on read / write)
// add notification UUID
// fix isservice 
// add data into event ? (see memory size and structure hierarchy)
	// reove pollNotification
	// remove read block delay
	// remove error polling ? 
	// remove advertised device pollint 
	// if all by pointer ? ref / out check 

// Enum
enum class What
{
	LIBRARY_ERROR,
	LIBRARY_NOTHING,
	LIBRARY_CALLBACK_SETUP,
	DISCOVERED_NEW,
	DISCOVERED_UPDATED,
	DISCOVERED_LOST,
	DISCOVERED_REMOVED,
	DEVICE_CONNECTED,
	DEVICE_DISCONNECTED,
	DEVICE_LOST,
	CHARACTERISTIC_WRITTEN,
	CHARACTERISTIC_NOT_WRITTEN,
	CHARACTERISTIC_READED,
	CHARACTERISTIC_NOT_READED,
	CHARACTERISTIC_SUBSCRIBED,
	CHARACTERISTIC_NOT_SUBSCRIBED,
	CHARACTERISTIC_NOTIFICATION,
	CHARACTERISTIC_SCANNED
};

// Structures
struct Device
{
	uint64_t address = 0;
	wchar_t name[50] = L"";
};

struct Characteristic
{
	wchar_t uuid[100] = L"";
	bool isService = false;
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
DllExport bool PollErrors(Data* e);
DllExport bool PollEvents(Event* ev);

DllExport bool setEventCallback(bool (*callback)(Event ev));
DllExport bool removeEventCallback();

DllExport bool IsBluetoothEnabled();

// Device scan function
DllExport void StartDeviceScan();
DllExport void StopDeviceScan();
//DllExport bool PollAdvertisedDevices(Device* device, bool first = false);
DllExport void CleanAdvertisedDevices();
DllExport bool IsDeviceConnectable(const wchar_t* deviceName);
DllExport uint64_t GetDeviceAddress(const wchar_t* deviceName);
DllExport bool GetDevice(Device* dev, uint64_t deviceAddress);

// Connect functions
DllExport bool ConnectDevice(uint64_t deviceAddress, bool block = false);
DllExport void DisconnectDevice(uint64_t deviceAddress);
DllExport bool IsDeviceConnected(uint64_t deviceAddress);

// Characteristics
DllExport bool DeviceWrite(uint64_t deviceAddress, wchar_t* uuid, Data* data, bool block = false);
DllExport bool DeviceRead(uint64_t deviceAddress, wchar_t* uuid, Data* data);

DllExport bool DeviceSubscribe(uint64_t deviceAddress, wchar_t* uuid, bool block = false);
DllExport bool PollNotifications(Data* n, uint64_t deviceAddress, wchar_t* uuid); // TODO add ref to notificatioun uuid as out variable
DllExport bool PollCharacteristics(Characteristic* c, uint64_t deviceAddress, bool first = false);

// Clean
DllExport void Clean();