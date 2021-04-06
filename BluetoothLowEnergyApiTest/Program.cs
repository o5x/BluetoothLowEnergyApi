using System;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    class Program
    {
        static readonly bool  filterDevices = true;
        static bool run = true;
        static UInt64 connAddr = 0;

        static readonly string devName = "NV-Command";
        //static string devName = "LAIRD BL652";
        //static string devName = "BL652-F6362ACBB4A8";

        static readonly string devWriteCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        static readonly string devReadCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        //static string devWriteCharacteristic = "{569a2001-b87f-490c-92cb-11ba5ea5167c}";
        //static string devReadCharacteristic = "{569a2000-b87f-490c-92cb-11ba5ea5167c}";

        public static void ProcessEvent(BleApi.Event btEvent)
        {
            if (filterDevices && btEvent.who.address != 0 && btEvent.who.name != devName) return;

            string timestamp = DateTime.UtcNow.ToString("HH:mm:ss.fff");


            Console.WriteLine(" - Event " + timestamp + " = " + btEvent);

            // Handle desired events
            switch (btEvent.what)
            {
                case BleApi.What.DISCOVERED_NEW:
                case BleApi.What.DISCOVERED_UPDATED:
                    if (btEvent.who.name == devName)
                    {
                        BleApi.ConnectDevice(btEvent.who.address);
                    }
                    break;
                case BleApi.What.DEVICE_CONNECTED:
                    BleApi.StopDeviceScan();
                    connAddr = btEvent.who.address;
                    break;
                case BleApi.What.DEVICE_DISCONNECTED:
                    //run = false;
                    break;
                case BleApi.What.DEVICE_LOST:
                    BleApi.StartDeviceScan();
                    break;
                case BleApi.What.CHARACTERISTIC_SCAN_OVER:
                    BleApi.DeviceWriteString(btEvent.who.address, devWriteCharacteristic, "Hello Bluetooth ");
                    BleApi.DeviceWriteString(btEvent.who.address, devWriteCharacteristic, "it is " + timestamp + "\r\n"); 
                    BleApi.DeviceSubscribe(btEvent.who.address, devReadCharacteristic);
                    break;
            }
        }

        static void Main()
        {
            // Make sure there is no previous instance data (if keeping dll loaded (e.g., Unity))
            BleApi.Clean();

            if (!BleApi.IsBluetoothEnabled())
            {
                Console.WriteLine("Bluetooh not available");
                return;
            }

            // Disable polling and use callback instead - comment to use polling instead
            BleApi.SetEventCallback(ProcessEvent);

            BleApi.StartDeviceScan();
            //Thread.Sleep(100);
            //BleApi.StopDeviceScan();
            //while (true) Thread.Sleep(10);

            BleApi.Event btEvent = new BleApi.Event();
            
            Console.CancelKeyPress += (sender, eventArgs) =>
            {
                Console.WriteLine("Cancel event triggered");
                run = false;
                eventArgs.Cancel = true;
            };

            // Main loop
            while (run)
            {
                // Poll bluetooth events - not used if callback enabled
                while (BleApi.PollEvents(out btEvent))
                {
                    Console.Write(" - Poll");
                    ProcessEvent(btEvent);
                }

                if(BleApi.IsDeviceConnected(connAddr))
                {
                    Thread.Sleep(1000);
                    BleApi.DisconnectDevice(connAddr);
                }
                Thread.Sleep(10);
            }

            // Automatically release all the connected devices
            BleApi.Clean();
            return;
        }
    }
}
