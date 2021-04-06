# BluetoothLowEnergyApi Dll

![Console screenshot](screen.png)

Simple Dynamic Linked Library (DLL) allowing to communicate with bluetooth devices. Initially created for Unity.

This project is based on [UWP BLE API](https://docs.microsoft.com/de-de/windows/uwp/devices-sensors/bluetooth-low-energy-overview).

## Features

- DLL threaded API
- Continuous device advertisment scan
- Connect Bluetooth devices
- Disconnect Bluetooth devices
- Write data to characteristics
- Subscribe / read from characteristics
- Poll or Callback on event
- Detect if device is disconnected (lost)
- Check if the device is bluetooth enabled
- Error management

## How to use

The interface is done in C#, you can easily translate to any other language.

Copy [BluetoothLowEnergyApiTest/BleApi.cs](BluetoothLowEnergyApiTest/BleApi.cs) and [BluetoothLowEnergyApiTest/BluetoothLowEnergyApi.dll](BluetoothLowEnergyApiTest/BluetoothLowEnergyApi.dll), and use the BleApi class as static to interface with bluetooth devices (sample code available in [BluetoothLowEnergyApiTest/Program.cs](BluetoothLowEnergyApiTest/Program.cs)) used in [Test](#Test) section

## Code 

### Events

When polling data events we gather two variables : Who (`struct Device`) and What (`enum What`), who as the device responsible for this event and what the kind of event trigerred, you can find the list of events below:

```CS
enum What
{
    LIBRARY_ERROR,                      // Error
    LIBRARY_NOTHING,                    // Nothing
    DISCOVERED_NEW,                     // Device Advertisment : new device
    DISCOVERED_UPDATED,                 // Device Advertisment name updated
    DISCOVERED_LOST,                    // Device Advertisment lost (not advertisment for 10s)
    DISCOVERED_REMOVED,                 // Device Advertisment removed (when connecting or clearing list)
    DEVICE_CONNECTED,                   // Device connected
    DEVICE_DISCONNECTED,                // Device disconnected (from code) - also triggered when not DEVICE_CONNECTED
    DEVICE_LOST,                        // Device disconnected (external cause)
    CHARACTERISTIC_WRITTEN,             // Characteristic write successful
    CHARACTERISTIC_NOT_WRITTEN,         // Characteristic write error
    CHARACTERISTIC_SUBSCRIBED,          // Characteristic subscribe successful
    CHARACTERISTIC_NOT_SUBSCRIBED,      // Characteristic subscribe successful
    CHARACTERISTIC_NOTIFICATION         // Notification recieved
};
```

### Structures

```CS
struct Device
{
    public UInt64 address;
    public string name;
}

struct Event
{
    public What what;
    public Device who;
}

struct Data
{
    public byte[] buf;
    public short size;
}
```

### Interface functions

```CS
bool PollErrors(out Data err);
bool PollEvents(out Event ev);
bool IsBluetoothEnabled();

// Device scan functions    
int StartDeviceScan();
int StopDeviceScan();
bool PollAdvertisedDevices(out Device advertisedDevice, bool first);
void CleanAdvertisedDevices();
bool IsDeviceConnectable(string deviceName);
UInt64 GetDeviceAddress(string deviceName);
bool GetDevice(out Device advertisedDevice, UInt64 addr);

// Connect functions    
bool ConnectDevice(UInt64 deviceAddress, bool block = false);
void DisconnectDevice(UInt64 deviceAddress);
bool IsDeviceConnected(UInt64 deviceAddress);

// Characteristics    
bool DeviceWrite(UInt64 deviceAddress, string uuid, string data, bool block = false);
bool DeviceSubscribe(UInt64 deviceAddress, string uuid, bool block = false);
bool PollNotifications(out Data n, UInt64 uuid);

// Library clean    
void Clean();
```

## Build

There is a prebuild dll included [BluetoothLowEnergyApiTest/BluetoothLowEnergyApi.dll](BluetoothLowEnergyApiTest/BluetoothLowEnergyApi.dll).
You also build it yourself by following these steps :
- Open the solution file BluetoothLowEnergyApi.sln with VisualStudio (tested with Community 2019). You may be asked install VisualStudio components when you open the project. The needed components are C++ Desktop and UWP.
- Choose configuration "Release" and "x64" (to match your machine architecture).
- In the project explorer, right-click the project "BluetoothLowEnergyApi" and choose "Compile".
- Wait until the compilation finishes successfully.

Now you find the file BluetoothLowEnergyApi.dll in the folder x64/Release. You can copy this dll where you need it. It also automatically copy the dll in `BluetoothLowEnergyApiTest/`

## Test

The BluetoothLowEnergyTest project depends on the BluetoothLowEnergy and will be built automatically if there is any change in the dll, it allows to quickly test changes.

To test the API follow these steps :
- Open the solution file BluetoothLowEnergyApi.sln with VisualStudio. You may be asked install VisualStudio components when you open the project. The needed components are C++ Desktop and UWP.
- Set BluetoothLowEnergyApiTest as **Startup Project** (right click -> Set as Startup Project)
- Choose configuration "Release" and "x64"
- Compile and run, F5 (with debugger) or Ctrl+F5 (without debugger)
- Wait until the compilation finishes successfully and the program to run.

```CS
static string devName = "NV-Command";
static string devCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
```
***Important Note*** : The Characteristics need to be surrounded with braces : `{}`

This Test program is looking for a device `devName` to write and subscribe to the characteristic `devCharacteristic`. When started it will tell the user if the bluetooth is ready to be used (available + enabled), start scanning the devices, connect, write and subscribe, and show de notification data string in the console. When the first notification is recieved it disconnect the device and close the program, Otherwise you can cancel with Ctrl+C (will disconnect and exit).

If the device is lost, it will automatically reconnect when available.

## Issues 

Please feel free to submit issues with any bugs or other unforseen issues you experience. If you experience problems, open an issue describing the problem and how to reproduce it, and I'll be sure to take a look at it.

## Licence

[MIT](https://choosealicense.com/licenses/mit/)

## Author 

Arrol - [arrol.fr](https://arrol.fr)