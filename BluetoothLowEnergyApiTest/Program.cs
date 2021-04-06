using System;
using System.Text;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    class Program
    {
        static string devName = "NV-Command";
        static bool filterDevices = true;
                
        //static string devName = "LAIRD BL652";
        //static string devName = "BL652-F6362ACBB4A8";
        static string devWriteCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        static string devReadCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        //static string devWriteCharacteristic = "{569a2001-b87f-490c-92cb-11ba5ea5167c}";
        //static string devReadCharacteristic = "{569a2000-b87f-490c-92cb-11ba5ea5167c}";
        static bool run = true;

        static BleApi.Data btData = new BleApi.Data();
        static BleApi.Characteristic btCharacteristic = new BleApi.Characteristic();

        public static bool ProcessEvent(BleApi.Event btEvent)
        {
            if (filterDevices && btEvent.who.address != 0 && btEvent.who.name != devName) return true;

            Console.WriteLine(" - Event = " + btEvent);


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
                case BleApi.What.DEVICE_LOST:
                    if (BleApi.IsDeviceConnectable(btEvent.who.name))
                    {
                        BleApi.ConnectDevice(btEvent.who.address); // Hello World !
                        BleApi.StartDeviceScan();
                    }
                    break;
                case BleApi.What.DEVICE_CONNECTED:      // When device is connected (need to wait characteristic scan to be over)
                    break;
                case BleApi.What.CHARACTERISTIC_SCANNED:
                    while (BleApi.PollCharacteristics(out btCharacteristic, btEvent.who.address)) // don't forget first if you want to poll again
                    {
                        Console.WriteLine("characteristic = " + btCharacteristic);    
                    }

                    // new way to write data
                    string d = "hello";
                    btData.buf = new byte[512];
                    Encoding.ASCII.GetBytes(d).CopyTo(btData.buf, 0);
                    btData.size = (short)d.Length;
                    Array.Resize(ref btData.buf, 512);
                    BleApi.DeviceWrite(btEvent.who.address, devWriteCharacteristic, ref btData);

                    BleApi.DeviceWriteString(btEvent.who.address, devWriteCharacteristic, "HelloString");

                    BleApi.DeviceSubscribe(btEvent.who.address, devReadCharacteristic);

                    BleApi.StopDeviceScan();

                    break;
                case BleApi.What.DEVICE_DISCONNECTED:   // When device is disconnected
                    run = false;
                    BleApi.StartDeviceScan();
                    break;
                case BleApi.What.CHARACTERISTIC_NOTIFICATION:   // On notification recieve
                    while (BleApi.PollNotifications(out btData, btEvent.who.address, ""/* btEvent.characteristic.uuid*/))
                    {
                        //string result = System.Text.Encoding.UTF8.GetString(btData.buf).Substring(0, btData.size);
                        Console.Write("notification[" + btData.size + "] = ");
                        for (int i = 0; i < btData.size; i++)
                        {
                            Console.Write(btData.buf[i].ToString("X2") + "");// + "(" + (Char.IsControl((char)btData.buf[i]) ? ((char)btData.buf[i]).ToString() : " ") + ") ");
                        }
                        Console.WriteLine();
                        //BleApi.DisconnectDevice(btEvent.who.address);
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

            return true;
        }

        static void Main(string[] args)
        {

            // tests 

            //bool tret = true;
            //bool* ptr = &tret;
            // Make sure there is no previous instance data (if keeping dll loaded (e.g., Unity))
            BleApi.Clean();

            if (!BleApi.IsBluetoothEnabled())
            {
                Console.WriteLine("Bluetooh not available");
                return;
            }

            // Disable polling and use callback instead - comment to use polling instead
            BleApi.setEventCallback(ProcessEvent);

            BleApi.StartDeviceScan();

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
                Thread.Sleep(10);
            }

            // Automatically release all the connected devices
            BleApi.Clean();
            return;
        }
    }
}
