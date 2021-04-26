using System;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    class Program
    {
        static readonly bool  filterDevices = false;
        static bool run = true;
        static readonly float qScale = 1f / 16384f;
        static readonly float degScale = 180 / MathF.PI;

        static string devName = "NV-arm";

        //static readonly string devWriteCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        //static readonly string devReadCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        //static string devWriteCharacteristic = "{569a2001-b87f-490c-92cb-11ba5ea5167c}";
        static string devReadCharacteristic = "{569a2000-b87f-490c-92cb-11ba5ea5167c}";

        public static void ProcessEvent(BleApi.Event btEvent)
        {
            if (filterDevices && btEvent.who.address != 0 && btEvent.who.name != devName) return;
            
            if(btEvent.what == BleApi.What.CHARACTERISTIC_NOTIFICATION)
            {
                var arr = btEvent.data.buf;
                float w = BitConverter.ToInt16(arr, 1) * qScale;    // q3 
                float x = BitConverter.ToInt16(arr, 3) * qScale;    // q0
                float y = BitConverter.ToInt16(arr, 5) * qScale;    // q1
                float z = BitConverter.ToInt16(arr, 7) * qScale;    // q2

                //Console.WriteLine(" w " + w + " x " + x + " y = " + y + " z = " + z );

                float r = MathF.Atan2(2f * x * y + 2f * z * w, 1 - 2f * (y * y + z * z)) * degScale;       // Roll 
                float p = MathF.Asin(2f * (x * z - w * y)) * degScale;                                   // Pitch 
                float ya = MathF.Atan2(2f * x * w + 2f * y * z, 1 - 2f * (z * z + w * w)) * degScale;      // Yaw 

                Console.Write(" r " + String.Format("{0,7:0.00}", r));
                Console.Write(" p " + String.Format("{0,7:0.00}", p));
                Console.Write(" y " + String.Format("{0,7:0.00}", ya));

                w = BitConverter.ToInt16(arr, 9) * qScale;    // q3 
                x = BitConverter.ToInt16(arr, 11) * qScale;    // q0
                y = BitConverter.ToInt16(arr, 13) * qScale;    // q1
                z = BitConverter.ToInt16(arr, 15) * qScale;    // q2

                //Console.WriteLine(" w " + w + " x " + x + " y = " + y + " z = " + z );

                r = MathF.Atan2(2f * x * y + 2f * z * w, 1 - 2f * (y * y + z * z)) * degScale;       // Roll 
                p = MathF.Asin(2f * (x * z - w * y)) * degScale;                                   // Pitch 
                ya = MathF.Atan2(2f * x * w + 2f * y * z, 1 - 2f * (z * z + w * w)) * degScale;      // Yaw 

                Console.Write("    r " + String.Format("{0,7:0.00}", r));
                Console.Write(" p " + String.Format("{0,7:0.00}", p));
                Console.WriteLine(" y " + String.Format("{0,7:0.00}", ya));

                return;
            }

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
                    break;
                case BleApi.What.DEVICE_DISCONNECTED:
                    //run = false;
                    break;
                case BleApi.What.DEVICE_LOST:
                    BleApi.StartDeviceScan();
                    break;
                case BleApi.What.CHARACTERISTIC_SCAN_OVER:
                    //BleApi.DeviceWriteString(btEvent.who.address, devWriteCharacteristic, "+");
                    //BleApi.DeviceWriteString(btEvent.who.address, devWriteCharacteristic, "Hello Bluetooth ");
                    //BleApi.DeviceWriteString(btEvent.who.address, devWriteCharacteristic, "it is " + timestamp + "\r\n"); 
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
                Thread.Sleep(10);
            }

            // Automatically release all the connected devices
            BleApi.Clean();
            return;
        }
    }
}
