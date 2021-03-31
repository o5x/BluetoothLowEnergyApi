using System;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    class Program
    {
        static string devName = "NV-Command";
        static string devCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";

        static void Main(string[] args)
        {
            // Make sure there is no previous instance data (if keeping dll loaded (e.g., Unity))
            BleApi.Clean();

            if (!BleApi.IsBluetoothEnabled())
            {
                Console.WriteLine("Bluetooh not available");
                return;
            }

            BleApi.StartDeviceScan();

            BleApi.Event btEvent = new BleApi.Event();
            BleApi.Data btData = new BleApi.Data();
            bool run = true;

            Console.CancelKeyPress += (sender, eventArgs) =>
            {
                Console.WriteLine("Cancel event triggered");
                run = false;
                eventArgs.Cancel = true;
            };

            // Main loop
            while (run)
            {
                // Poll bluetooth events
                while (BleApi.PollEvents(out btEvent))
                {
                    Console.WriteLine(" - PollEvent = " + btEvent);

                    // handle desired events
                    switch (btEvent.what)
                    {
                        case BleApi.What.DISCOVERED_NEW:        // When new device is found
                        case BleApi.What.DISCOVERED_UPDATED:    // When device name is updated
                            if (btEvent.who.name == devName)
                            {
                                BleApi.ConnectDevice(btEvent.who.address);
                            }
                            break;
                        case BleApi.What.DEVICE_CONNECTED:      // When device is connected
                            BleApi.DeviceWrite(btEvent.who.address, devCharacteristic, "@");
                            BleApi.DeviceSubscribe(btEvent.who.address, devCharacteristic);
                            break;
                        case BleApi.What.DEVICE_DISCONNECTED:   // When device is disconnected
                            run = false;
                            break;
                        case BleApi.What.CHARACTERISTIC_NOTIFICATION:   // On notification recieve
                            while (BleApi.PollNotifications(out btData, btEvent.who.address))
                            {
                                string result = System.Text.Encoding.UTF8.GetString(btData.buf).Substring(0, btData.size);
                                Console.WriteLine("notification[" + btData.size + "] = " + result);
                                BleApi.DisconnectDevice(btEvent.who.address);
                            }
                            break;
                        case BleApi.What.LIBRARY_ERROR:         // On error
                            while (BleApi.PollErrors(out btData))
                            {
                                string result = System.Text.Encoding.UTF8.GetString(btData.buf).Substring(0, btData.size);
                                Console.WriteLine("Error[" + btData.size + "] = " + result);
                            }
                            break;
                    }
                }
                Thread.Sleep(10);
            }

            // Automatically release all the connected devices
            BleApi.Clean();
            return;
        }
    }
}
