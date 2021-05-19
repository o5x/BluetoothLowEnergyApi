using System;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    class Program
    {
        static readonly bool  filterDevices = false;
        static bool run = true;
        static readonly float degScale = 180 / MathF.PI;

        static string devName = "NV-arm-main";

        static DateTime startTime;
        static DateTime lastTime;

        //static readonly string devWriteCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        //static readonly string devReadCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";
        //static string devWriteCharacteristic = "{569a2001-b87f-490c-92cb-11ba5ea5167c}";
        static string devReadCharacteristic = "{569a2000-b87f-490c-92cb-11ba5ea5167c}";
        //static string devUartCharacteristic = "{0000ffe1-0000-1000-8000-00805f9b34fb}";

        static readonly int deltatimesLen = 20;
        static double[] deltatimes = new double[deltatimesLen];
        static int indexDeltaTimes = 0;
        static double avgDeltaTime = 0;

        public static void decodeIMU(byte[] buf, int id0)
        {
            int offset = id0 * 20;

            if (buf[0+offset] != '#')
            {
                Console.Write("Incorrect start ");
                return;
            }
            if (buf[0 + offset] != '#')
            {
                Console.Write("Incorrect stop ");
                return;
            }

            int id = buf[1 + offset] & 0b11;
            int state = buf[1 + offset] >> 2;

            float w = BitConverter.ToSingle(buf, 2 + offset);    // q3 
            float x = BitConverter.ToSingle(buf, 6 + offset);    // q0
            float y = BitConverter.ToSingle(buf, 10 + offset);    // q1
            float z = BitConverter.ToSingle(buf, 14 + offset);    // q2

            //Console.WriteLine(" w " + w + " x " + x + " y = " + y + " z = " + z );

            float ya = MathF.Atan2((2 * x * y - 2 * w * z), (2 * w * w + 2 * x * x - 1)) * degScale;
            float p = -MathF.Asin(2 * x * z + 2 * w * y) * degScale;
            float r = MathF.Atan2((2 * y * z - 2 * w * x), (2 * w * w + 2 * z * z - 1)) * degScale;

            Console.Write(" IMU" + id + "; " + state);
            Console.Write("; " + String.Format("{0,6:0.0}", r));
            Console.Write("; " + String.Format("{0,6:0.0}", p));
            Console.Write("; " + String.Format("{0,6:0.0}", ya));
            Console.Write(";     ;");
        }

        public static void decodeIMU16(byte[] buf, int id)
        {
            int offset = 1 + id * 8;

            /*if (buf[0 + offset] != '#')
            {
                Console.Write("Incorrect start ");
                return;
            }
            if (buf[0 + offset] != '#' && buf[0 + offset] != '^')
            {
                Console.Write("Incorrect stop ");
                return;
            }*/

            int state = 2;

            float w = (float)BitConverter.ToInt16(buf, 0 + offset) / 16384f;
            float x = (float)BitConverter.ToInt16(buf, 2 + offset) / 16384f;
            float y = (float)BitConverter.ToInt16(buf, 4 + offset) / 16384f;
            float z = (float)BitConverter.ToInt16(buf, 6 + offset) / 16384f;

            //Console.WriteLine(" w " + w + " x " + x + " y = " + y + " z = " + z );

            float ya = MathF.Atan2((2 * x * y - 2 * w * z), (2 * w * w + 2 * x * x - 1)) * degScale;
            float p = -MathF.Asin(2 * x * z + 2 * w * y) * degScale;
            float r = MathF.Atan2((2 * y * z - 2 * w * x), (2 * w * w + 2 * z * z - 1)) * degScale;

            Console.Write(" IMU" + id + "; " + state);
            Console.Write("; " + String.Format("{0,6:0.0}", r));
            Console.Write("; " + String.Format("{0,6:0.0}", p));
            Console.Write("; " + String.Format("{0,6:0.0}", ya));
            Console.Write(";     ;");
        }

        public static void ProcessEvent(BleApi.Event btEvent)
        {
            if (filterDevices && btEvent.who.address != 0 && btEvent.who.name != devName) return;
            
            if(btEvent.what == BleApi.What.CHARACTERISTIC_NOTIFICATION)
            {
                //long milliseconds = DateTimeOffset.Now.ToUnixTimeMilliseconds();
                //Console.WriteLine(milliseconds + " : " + System.Text.Encoding.Default.GetString(btEvent.data.buf));
                //return;

                if (btEvent.data.size != 18) // 40
                {
                    Console.WriteLine("Incorrect Length " + btEvent.data.size);
                    return;
                }

                decodeIMU16(btEvent.data.buf, 0);
                decodeIMU16(btEvent.data.buf, 1);

                DateTime now = DateTime.Now;
                TimeSpan dt = now - lastTime;
                TimeSpan time = now - startTime;
                lastTime = now;

                if(avgDeltaTime == 0)
                {
                    for (int i = 0; i < deltatimesLen; i++)
                    {
                        deltatimes[i] = dt.TotalMilliseconds;
                    }
                }
                else
                {
                    deltatimes[indexDeltaTimes++ % deltatimesLen] = dt.TotalMilliseconds;
                }

                // calculate avg delta time from array
                double sum = 0;
                for (int i = 0; i < deltatimesLen; i++)
                {
                    sum += deltatimes[i];
                }
                avgDeltaTime = sum / deltatimesLen;


                Console.WriteLine(String.Format("{0,10:0.00}", time.TotalMilliseconds) + ";" + String.Format("{0,10:0.00}", dt.TotalMilliseconds) + ";" + String.Format("{0,10:0.00}", avgDeltaTime) + ";");

                return;
            }


            string timestamp = DateTime.UtcNow.ToString("HH:mm:ss.fff");

            Console.WriteLine(" - Event " + timestamp + " " + btEvent);

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
                    startTime = DateTime.Now;
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
            //BleApi.SetEventCallback(ProcessEvent);

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
                while (BleApi.PollEvents(ref btEvent))
                {
                    Console.Write(" - Poll");
                    ProcessEvent(btEvent);
                }

                Thread.Sleep(5);
            }

            // Automatically release all the connected devices
            BleApi.Clean();
            return;
        }
    }
}
