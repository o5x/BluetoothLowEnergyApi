using System;
using System.Threading;

namespace BluetoothLowEnergyApiTest
{
    internal class Program
    {
        // Global exit condition
        private static bool run = true;

        // Target device
        private static string deviceName = "NV-arm";

        // Characteristics
        private static string UART_RX = "{6e400002-b5a3-f393-e0a9-e50e24dcca9e}";

        private static string UART_TX = "{6e400003-b5a3-f393-e0a9-e50e24dcca9e}";

        private static string BAT_LVL = "{00002a19-0000-1000-8000-00805f9b34fb}";
        private static string BAT_VOLTAGE = "{00002b18-0000-1000-8000-00805f9b34fb}";
        private static string BAT_TEMP = "{00002a6e-0000-1000-8000-00805f9b34fb}";

        private static string RSSI_PWR = "{00002a07-0000-1000-8000-00805f9b34fb}";

        // Compute times
        private static DateTime startTime;

        private static DateTime lastTime;

        private static readonly int deltatimesLen = 20;
        private static double[] deltatimes = new double[deltatimesLen];
        private static int indexDeltaTimes = 0;
        private static double avgDeltaTime = 0;

        // IMUs
        private static IMU imu1 = new IMU(1);

        private static IMU imu2 = new IMU(2);

        public static void ProcessEvent(BleApi.Event btEvent)
        {
            if (btEvent.what == BleApi.What.CHARACTERISTIC_NOTIFICATION)
            {
                if (btEvent.characteristic.uuid == BAT_LVL)
                {
                    Int16 bat = BitConverter.ToInt16(btEvent.data.buf, 0);
                    Console.WriteLine(" Battery level = " + bat + " %");
                }
                if (btEvent.characteristic.uuid == BAT_VOLTAGE)
                {
                    Int16 v = BitConverter.ToInt16(btEvent.data.buf, 0);
                    Console.WriteLine(" Battery voltage = " + v + " mv");
                }
                if (btEvent.characteristic.uuid == BAT_TEMP)
                {
                    Int16 t = BitConverter.ToInt16(btEvent.data.buf, 0);
                    Console.WriteLine(" Battery temp = " + t / 10.0 + " deg");
                }
                else if (btEvent.characteristic.uuid == RSSI_PWR)
                {
                    Int16 rssi = BitConverter.ToInt16(btEvent.data.buf, 0);
                    Console.WriteLine(" RSSI = " + rssi + " dbm");
                }
                else if (btEvent.characteristic.uuid == UART_TX)
                {
                    if (btEvent.data.size == 240)
                    {
                        for (int i = 0; i < 5; i++)
                        {
                            imu1.update(btEvent.data.buf, 0 + i * 48);
                            imu2.update(btEvent.data.buf, 24 + i * 48);
                        }

                        Console.Write(imu1.ToString());
                        Console.Write(imu2.ToString());
                    }

                    // Compute Average time
                    DateTime now = DateTime.Now;
                    TimeSpan dt = now - lastTime;
                    TimeSpan time = now - startTime;

                    if (lastTime.Millisecond == 0)
                    {
                        dt = TimeSpan.Zero;
                    }
                    lastTime = now;

                    if (avgDeltaTime == 0)
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
                }
                return;
            }

            string timestamp = DateTime.UtcNow.ToString("HH:mm:ss.fff");

            Console.WriteLine(" - Event " + timestamp + " " + btEvent);

            // Handle desired events
            switch (btEvent.what)
            {
                case BleApi.What.DISCOVERED_NEW:
                case BleApi.What.DISCOVERED_UPDATED:
                    if (btEvent.who.name == deviceName)
                    {
                        BleApi.ConnectDevice(btEvent.who.address);
                    }
                    break;

                case BleApi.What.DEVICE_CONNECTED:
                    BleApi.StopDeviceScan();
                    break;

                case BleApi.What.DEVICE_DISCONNECTED:
                    run = false;
                    break;

                case BleApi.What.DEVICE_LOST:
                    run = false;
                    break;

                case BleApi.What.CHARACTERISTIC_SCAN_OVER:
                    startTime = DateTime.Now;

                    BleApi.DeviceWriteString(btEvent.who.address, UART_RX, "+");
                    //BleApi.DeviceWriteString(btEvent.who.address, UART_RX, "Hello Bluetooth ");
                    //BleApi.DeviceWriteString(btEvent.who.address, UART_RX, "it is " + timestamp + "\r\n");

                    BleApi.DeviceSubscribe(btEvent.who.address, UART_TX);
                    //BleApi.DeviceSubscribe(btEvent.who.address, BAT_LVL);
                    //BleApi.DeviceSubscribe(btEvent.who.address, BAT_VOLTAGE);
                    //BleApi.DeviceSubscribe(btEvent.who.address, BAT_TEMP);
                    //BleApi.DeviceSubscribe(btEvent.who.address, RSSI_PWR);
                    break;
            }
        }

        private static void Main()
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

            BleApi.RemoveAdertisementFilters();
            byte[] filter = { 0xFF, 0xFF, 0x4E, 0x65, 0x75, 0x72, 0x69, 0x6E, 0x6E, 0x6F, 0x76 }; // 0xFFFF Neurinnov
            BleApi.AddAdertisementFilter(0xFF, filter, (byte)filter.Length);
            BleApi.StartDeviceScan();

            BleApi.Event btEvent = new BleApi.Event();

            Console.CancelKeyPress += (sender, eventArgs) =>
            {
                Console.WriteLine("Cancel event triggered");
                if (BleApi.ConnectedCount() == 0)
                {
                    run = false;
                }
                else
                {
                    BleApi.DisconnectAll();
                }
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

            BleApi.Clean(); // Make sure to clean before leaving (unity)
            return;
        }
    }
}