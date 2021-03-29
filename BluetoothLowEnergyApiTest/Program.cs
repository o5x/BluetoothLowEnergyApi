using System;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    class Program
    {
        static string devName = "NV-Command";
        static UInt64 devAddr = 0;
        static string devCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";

        static void Main(string[] args)
        {
            BleApi.Clean();

            if (!BleApi.IsBluetoothEnabled())
            {
                Console.WriteLine("Bluetooh not available");
                return;
            }

            BleApi.StartDeviceScan();

            BleApi.Event ev = new BleApi.Event();
            BleApi.Data data = new BleApi.Data();
            bool run = true;

            Console.CancelKeyPress += delegate 
            {
                BleApi.Clean();
                Environment.Exit(0);
            };

            while (run)
            {
                while (BleApi.PollEvents(out ev))
                {
                    Console.WriteLine(" - PollEvent = " + ev);
                    
                    switch (ev.what)
                    {
                        case BleApi.What.DISCOVERED_NEW:
                        case BleApi.What.DISCOVERED_UPDATED:
                        case BleApi.What.DISCOVERED_LOST:
                        case BleApi.What.DISCOVERED_REMOVED:
                        case BleApi.What.DEVICE_LOST:
                            if (!BleApi.IsDeviceConnected(devAddr))
                            {
                                if (BleApi.IsDeviceConnectable(devName))
                                {
                                    devAddr = BleApi.GetDeviceAddress(devName);
                                    BleApi.ConnectDevice(devAddr);
                                }
                            }
                            break;
                        case BleApi.What.DEVICE_CONNECTED:
                            BleApi.DeviceWrite(devAddr, devCharacteristic, "hello !");
                            BleApi.DeviceSubscribe(devAddr, devCharacteristic);
                            break;
                        case BleApi.What.DEVICE_DISCONNECTED:
                            run = false;
                            break;
                        case BleApi.What.CHARACTERISTIC_NOTIFICATION:
                            while(BleApi.PollNotifications(out data, devAddr))
                            {
                                string result = System.Text.Encoding.UTF8.GetString(data.buf).Substring(0, data.size);
                                Console.WriteLine("notification[" + data.size + "] = " + result);
                            }
                            break;
                        case BleApi.What.LIBRARY_ERROR:
                            while (BleApi.PollErrors(out data))
                            {
                                string result = System.Text.Encoding.UTF8.GetString(data.buf).Substring(0, data.size);
                                Console.WriteLine("Error[" + data.size + "] = " + result);
                            }
                            break;
                    }
                }
                Thread.Sleep(10);
            }
            BleApi.Clean();
            return;
        }
    }
}
