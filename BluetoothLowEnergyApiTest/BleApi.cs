using System;
using System.Linq;
using System.Runtime.InteropServices;

public delegate void CallbackEvent(BleApi.Event ev);
public class BleApi
{
    // Enumerations
    public enum What
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
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Device
    {
        [MarshalAs(UnmanagedType.U8)]
        public UInt64 address;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 50)]
        public string name;

        private string StrAdress(UInt64 macAddress)
        {
            return string.Join(":", BitConverter.GetBytes(macAddress).Reverse().Select(b => b.ToString("X2"))).Substring(6);
        }
        override public string ToString()
        {
            if (name.Length > 0)
            {
                return StrAdress(address) + "(" + name + ")";
            }
            return StrAdress(address);
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Characteristic
    {
        [MarshalAs(UnmanagedType.U1)]
        public bool isService;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 50)]
        public string uuid;
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
    public struct Data
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
        public byte[] buf;
        [MarshalAs(UnmanagedType.U2)]
        public UInt16 size;

        override public string ToString()
        {
            return System.Text.Encoding.UTF8.GetString(buf).Substring(0, size).Trim();
        }
        public string ToBytes()
        {
            if(size > 0)
            {
                string os = string.Format("{0,3:D3}", size) + "[";
                for (int i = 0; i < size - 1; i++) os += buf[i].ToString("X2") + "-";
                os += buf[size - 1].ToString("X2") + "]";
                return os + this;
            }
            return "{}";
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Event
    {
        public What what;
        public Device who;
        public Characteristic characteristic;
        public Data data;
        override public string ToString()
        {
            if(what == What.LIBRARY_NOTHING)
            {
                return what.ToString();
            }

            string os = "";

            if(who.address != 0) os += who + " ";
            if(characteristic.uuid.Length != 0) os += characteristic + " ";
            os += what + " ";
            if (data.size != 0)
            {
                if (what == What.LIBRARY_ERROR) os += data + " ";
                else os += data.ToBytes() + " ";
            }
            return os;
        }
    }

    // Functions
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool PollEvents(out Event ev);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void SetEventCallback(CallbackEvent callback);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void RemoveEventCallback();


    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool IsBluetoothEnabled();


    // Device scan functions
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void StartDeviceScan();

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void StopDeviceScan();

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void CleanAdvertisedDevices();


    // connect functions
    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void ConnectDevice(UInt64 deviceAddress);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void DisconnectDevice(UInt64 deviceAddress);

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool IsDeviceConnected(UInt64 deviceAddress);


    // characteristics
    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern void DeviceWrite(UInt64 deviceAddress, string uuid, ref Data data);

    public static void DeviceWriteString(UInt64 deviceAddress, string uuid, string data)
    {
        Data btData;
        btData.buf = new byte[512];
        System.Text.Encoding.ASCII.GetBytes(data).CopyTo(btData.buf, 0);
        btData.size = (UInt16)data.Length;
        DeviceWrite(deviceAddress, uuid, ref btData);
    }

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern void DeviceRead(UInt64 deviceAddress, string uuid, ref Data data);

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern void DeviceSubscribe(UInt64 deviceAddress, string uuid);


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