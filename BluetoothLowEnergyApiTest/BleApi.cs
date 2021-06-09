////////////////////////////////////////////////////////////////////////////////////////////////////
/// <file>  BluetoothLowEnergyApiTest\BleApi.cs </file>
///
/// <copyright file="BleApi.cs" company="arrol.fr">
/// Copyright (c) 2021 arrol.fr. All rights reserved.
/// </copyright>
///
/// <summary>   Implements the ble API class. </summary>
////////////////////////////////////////////////////////////////////////////////////////////////////

using System;
using System.Linq;
using System.Runtime.InteropServices;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>   Callback event. </summary>
///
/// <remarks>   Arrol, 13/04/2021. </remarks>
///
/// <param name="ev">   The ev. </param>
////////////////////////////////////////////////////////////////////////////////////////////////////

public delegate void CallbackEvent(BleApi.Event ev);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>   A ble api. </summary>
///
/// <remarks>   Arrol, 13/04/2021. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

public class BleApi
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Enumerations. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    public enum What
    {
        /// <summary>   An enum constant representing the library error option. </summary>
        LIBRARY_ERROR,
        /// <summary>   An enum constant representing the library nothing option. </summary>
        LIBRARY_NOTHING,
        /// <summary>   An enum constant representing the library callback setup option. </summary>
        LIBRARY_CALLBACK_SETUP,
        /// <summary>   An enum constant representing the discovered started option. </summary>
        DISCOVERED_STARTED,
        /// <summary>   An enum constant representing the discovered stopped option. </summary>
        DISCOVERED_STOPPED,
        /// <summary>   An enum constant representing the discovered new option. </summary>
        DISCOVERED_NEW,
        /// <summary>   An enum constant representing the discovered updated option. </summary>
        DISCOVERED_UPDATED,
        /// <summary>   An enum constant representing the discovered lost option. </summary>
        DISCOVERED_LOST,
        /// <summary>   An enum constant representing the discovered removed option. </summary>
        DISCOVERED_REMOVED,
        /// <summary>   An enum constant representing the device connected option. </summary>
        DEVICE_CONNECTED,
        /// <summary>   An enum constant representing the device disconnected option. </summary>
        DEVICE_DISCONNECTED,
        /// <summary>   An enum constant representing the device lost option. </summary>
        DEVICE_LOST,
        /// <summary>   An enum constant representing the characteristic scan new option. </summary>
        CHARACTERISTIC_SCAN_NEW,
        /// <summary>   An enum constant representing the characteristic scan over option. </summary>
        CHARACTERISTIC_SCAN_OVER,
        /// <summary>   An enum constant representing the characteristic written option. </summary>
        CHARACTERISTIC_WRITTEN,
        /// <summary>   An enum constant representing the characteristic not written option. </summary>
        CHARACTERISTIC_NOT_WRITTEN,
        /// <summary>   An enum constant representing the characteristic readed option. </summary>
        CHARACTERISTIC_READED,
        /// <summary>   An enum constant representing the characteristic not readed option. </summary>
        CHARACTERISTIC_NOT_READED,
        /// <summary>   An enum constant representing the characteristic subscribed option. </summary>
        CHARACTERISTIC_SUBSCRIBED,
        /// <summary>   An enum constant representing the characteristic not subscribed option. </summary>
        CHARACTERISTIC_NOT_SUBSCRIBED,
        /// <summary>   An enum constant representing the characteristic notification option. </summary>
        CHARACTERISTIC_NOTIFICATION
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Structures. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Device
    {
        /// <summary>   The address. </summary>
        [MarshalAs(UnmanagedType.U8)]
        public UInt64 address;
        /// <summary>   The name. </summary>
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 50)]
        public string name;

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        /// <summary>   Adress the given MAC address. </summary>
        ///
        /// <remarks>   Arrol, 13/04/2021. </remarks>
        ///
        /// <param name="macAddress">   The MAC address. </param>
        ///
        /// <returns>   A string. </returns>
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        private string StrAdress(UInt64 macAddress)
        {
            return string.Join(":", BitConverter.GetBytes(macAddress).Reverse().Select(b => b.ToString("X2"))).Substring(6);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        /// <summary>   Returns the fully qualified type name of this instance. </summary>
        ///
        /// <remarks>   Arrol, 13/04/2021. </remarks>
        ///
        /// <returns>   The fully qualified type name. </returns>
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        override public string ToString()
        {
            if (name.Length > 0)
            {
                return StrAdress(address) + "(" + name + ")";
            }
            return StrAdress(address);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   A characteristic. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Characteristic
    {
        /// <summary>   True if is service, false if not. </summary>
        [MarshalAs(UnmanagedType.U1)]
        public bool isService;
        /// <summary>   The uuid. </summary>
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 50)]
        public string uuid;

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        /// <summary>   Returns the fully qualified type name of this instance. </summary>
        ///
        /// <remarks>   Arrol, 13/04/2021. </remarks>
        ///
        /// <returns>   The fully qualified type name. </returns>
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        override public string ToString()
        {
            if (uuid.Length > 0)
            {
                return (isService ? "S" : "C") + uuid;
            }
            return (isService ? "S" : "C") + "{INVALID_UUID}";
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   A data. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Data
    {
        /// <summary>   The buffer. </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
        public byte[] buf;
        /// <summary>   The size. </summary>
        [MarshalAs(UnmanagedType.U2)]
        public UInt16 size;

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        /// <summary>   Returns the fully qualified type name of this instance. </summary>
        ///
        /// <remarks>   Arrol, 13/04/2021. </remarks>
        ///
        /// <returns>   The fully qualified type name. </returns>
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        override public string ToString()
        {
            return System.Text.Encoding.UTF8.GetString(buf).Substring(0, size).Trim();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        /// <summary>   Converts this object to the bytes. </summary>
        ///
        /// <remarks>   Arrol, 13/04/2021. </remarks>
        ///
        /// <returns>   This object as a string. </returns>
        ////////////////////////////////////////////////////////////////////////////////////////////////////

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

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   An event. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct Event
    {
        /// <summary>   The what. </summary>
        public What what;
        /// <summary>   The who. </summary>
        public Device who;
        /// <summary>   The characteristic. </summary>
        public Characteristic characteristic;
        /// <summary>   The data. </summary>
        public Data data;

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        /// <summary>   Returns the fully qualified type name of this instance. </summary>
        ///
        /// <remarks>   Arrol, 13/04/2021. </remarks>
        ///
        /// <returns>   The fully qualified type name. </returns>
        ////////////////////////////////////////////////////////////////////////////////////////////////////

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

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Functions. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="ev">   [out] The ev. </param>
    ///
    /// <returns>   True if it succeeds, false if it fails. </returns>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool PollEvents(ref Event ev);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Callback, called when the set event. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="callback"> The callback. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void SetEventCallback(CallbackEvent callback);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Removes the event callback. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void RemoveEventCallback();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Queries if the bluetooth is enabled. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <returns>   True if the bluetooth is enabled, false if not. </returns>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool IsBluetoothEnabled();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Device scan functions. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void StartDeviceScan();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Stops device scan. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void StopDeviceScan();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Clean advertised devices. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void CleanAdvertisedDevices();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   connect functions. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void ConnectDevice(UInt64 deviceAddress);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Disconnects the device described by deviceAddress. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void DisconnectDevice(UInt64 deviceAddress);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Query if 'deviceAddress' is device connected. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    ///
    /// <returns>   True if device connected, false if not. </returns>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern bool IsDeviceConnected(UInt64 deviceAddress);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   characteristics. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    /// <param name="uuid">             The uuid. </param>
    /// <param name="data">             [in,out] The data. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern void DeviceWrite(UInt64 deviceAddress, string uuid, ref Data data);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Device write string. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    /// <param name="uuid">             The uuid. </param>
    /// <param name="data">             The data. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    public static void DeviceWriteString(UInt64 deviceAddress, string uuid, string data)
    {
        Data btData;
        btData.buf = new byte[512];
        System.Text.Encoding.ASCII.GetBytes(data).CopyTo(btData.buf, 0);
        btData.size = (UInt16)data.Length;
        DeviceWrite(deviceAddress, uuid, ref btData);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Device read. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    /// <param name="uuid">             The uuid. </param>
    /// <param name="data">             [in,out] The data. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern void DeviceRead(UInt64 deviceAddress, string uuid);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Device subscribe. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ///
    /// <param name="deviceAddress">    The device address. </param>
    /// <param name="uuid">             The uuid. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll", CharSet = CharSet.Unicode)]
    public static extern void DeviceSubscribe(UInt64 deviceAddress, string uuid);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Library clean. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void Clean();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Disconnect all devices. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    [DllImport("BluetoothLowEnergyApi.dll")]
    public static extern void DisconnectAll();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Consttuctor. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    BleApi()
    {
        Clean();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   Destructor. </summary>
    ///
    /// <remarks>   Arrol, 13/04/2021. </remarks>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ~BleApi()
    {
        Clean();
    }
}