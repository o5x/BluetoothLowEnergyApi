using System;
using System.Linq;
using System.Runtime.InteropServices;

public delegate bool CallbackEvent(BleApi.Event ev);
public class BleApi
{
    // Enumerations
    public enum What
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
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Device
    {
        [MarshalAs(UnmanagedType.U8)]
        public UInt64 address;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 50)]
        public string name;

        private string strAdress(UInt64 macAddress)
        {
            return string.Join(":", BitConverter.GetBytes(macAddress).Reverse().Select(b => b.ToString("X2"))).Substring(6);
        }
        override public string ToString()
        {
            if (name.Length > 0)
            {
                return strAdress(address) + "(" + name + ")";
            }
            return strAdress(address);
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Characteristic
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 100)]
        public string uuid;
        public bool isService;
        override public string ToString()
        {
            if (uuid.Length > 0)
            {
                return (isService ? "S" : "C") + uuid;
            }
            return (isService ? "S" : "C") + "{INVALID_UUID}";
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Event
    {
        public What what;
        public Device who;
        public Characteristic characteristic;
        override public string ToString()
        {
            if (who.address == 0)
            {
                return what.ToString();
            }
            else if (characteristic.uuid.Length == 0)
            {
                return who + " " + what;
            }
            else
            {
                return who + " " + characteristic + " " + what;
            }
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Data
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
        public byte[] buf;
        [MarshalAs(UnmanagedType.I2)]
        public short size;
    }

    // Functions
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool PollErrors(out Data err);


    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool PollEvents(out Event ev);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool setEventCallback(CallbackEvent callback);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool removeEventCallback();


    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool IsBluetoothEnabled();


    // Device scan functions
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern int StartDeviceScan();

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern int StopDeviceScan();

    //[DllImport("BluetoothLowEnergyApi.dll")]
    //public static extern bool PollAdvertisedDevices(out Device advertisedDevice, bool first);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void CleanAdvertisedDevices();

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern bool IsDeviceConnectable(string deviceName);

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern UInt64 GetDeviceAddress(string deviceName);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool GetDevice(out Device advertisedDevice, UInt64 addr);


    // connect functions
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool ConnectDevice(UInt64 deviceAddress, bool block = false);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void DisconnectDevice(UInt64 deviceAddress);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool IsDeviceConnected(UInt64 deviceAddress);


    // characteristics
    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern bool DeviceWriteEx(UInt64 deviceAddress, string uuid, string data, bool block = false);

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern bool DeviceWrite(UInt64 deviceAddress, string uuid, ref Data data, bool block = false);

    public static bool DeviceWriteString(UInt64 deviceAddress, string uuid, string data, bool block = false)
    {
        Data btData;
        btData.buf = new byte[512];
        System.Text.Encoding.ASCII.GetBytes(data).CopyTo(btData.buf, 0);
        btData.size = (short)data.Length;
        //Array.Resize(ref btData.buf, 512);
        return DeviceWrite(deviceAddress, uuid, ref btData, block);
    }

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern bool DeviceRead(UInt64 deviceAddress, string uuid, ref Data data, bool block = false);

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern bool DeviceSubscribe(UInt64 deviceAddress, string uuid, bool block = false);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool PollNotifications(out Data n, UInt64 deviceAddress, string uuid);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void ScanCharacteristics(UInt64 deviceAddress, bool services = false, bool characteristics = true);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool PollCharacteristics(out Characteristic c, UInt64 deviceAddress, bool first = false);


    // Library clean
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void Clean();


    // Consttuctor
    BleApi()
    {
        Clean();
    }

    // Destructor
    ~BleApi()
    {
        Clean();
    }
}