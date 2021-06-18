////////////////////////////////////////////////////////////////////////////////////////////////////
/// <file>	BluetoothLowEnergyApi\BluetoothLowEnergyApi.cpp </file>
///
/// <copyright file="BluetoothLowEnergyApi.cpp" company="arrol.fr">
/// Copyright (c) 2021 arrol.fr. All rights reserved.
/// </copyright>
///
/// <summary>	Implements the bluetooth low energy API class. </summary>
////////////////////////////////////////////////////////////////////////////////////////////////////

/*
TODO : check encryption
check MTU size detection
update characteristics on changed ?
secure LE device with single semaphore
unsubscribe from all characteristics / devices (putting null or empty value)
check ressouce protection with mutexes ?
fix unsubscribe after disconnection
*/
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
	if (eventCallback)
	{
		eventCallback(ev);
	}
	// missed event
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
		//AddEventAsync(cb);
		AddEventAsync(cb);
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
		//wcout << "classicSecure = " << btAdapter.AreClassicSecureConnectionsSupported() << endl;
		//wcout << "LeSecure = " << btAdapter.AreLowEnergySecureConnectionsSupported() << endl;
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
/// <summary>	Function - Clean quit / init. </summary>
///
/// <remarks>	Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////


DllExport void Clean()
{
	CleanScan();
	CleanConnect();
	
	try
	{
		{
			lock_guard lock3(eventsLock);

			while (!events.empty())
			{
				events.pop();
			}
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
