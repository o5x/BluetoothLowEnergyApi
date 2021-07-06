using System;
using System.Numerics;

namespace BluetoothLowEnergyApiTest
{
    internal class IMU
    {
        public int id;
        public Vector3 m;
        public Vector3 a;
        public Vector3 g;
        public Vector3 euler;
        public float time;
        public float lasttime;
        public Quaternion q;
        //public bool first;

        public IMU(int id)
        {
            this.id = id;
            m = a = g = Vector3.Zero;
            time = lasttime = 0.0f;
            q = Quaternion.Identity;
            //first = true;
        }

        private static Vector3 decodeVector3(byte[] buf, int offset, float scale)
        {
            float x = BitConverter.ToInt16(buf, 0 + offset) * scale;
            float y = BitConverter.ToInt16(buf, 2 + offset) * scale;
            float z = BitConverter.ToInt16(buf, 4 + offset) * scale;
            return new Vector3(x, y, z);
        }

        public void update(byte[] buf, int offset)
        {
            m = decodeVector3(buf, offset, 0.000061f);
            g = decodeVector3(buf, offset + 8, 0.070000f * MathF.PI / 180f);
            a = decodeVector3(buf, offset + 14, 0.3f);

            time = (((UInt32)buf[21 + offset] << 0 | (UInt32)buf[22 + offset] << 8 | (UInt32)buf[23 + offset] << 16)) / 39.0f;

            //if(first)
            //{
            //    for (int i = 0; i < 500; i++)
            //    {
            //        MadgwickAHRS.update(g, a, m, ref q, 0.01f);
            //    }
            //    first = false;
            //}
            //else
            //{
            MadgwickAHRS.update(g, a, m, ref q, 0.01f);
            //}
        }

        override public string ToString()
        {
            euler = MadgwickAHRS.toEuler(q);

            return " IMU" + id
            + "; " + String.Format("{0,6:0.0}", euler.X)
            + "; " + String.Format("{0,6:0.0}", euler.Y)
            + "; " + String.Format("{0,6:0.0}", euler.Z)
            + "; " + String.Format("{0,6:0.0}", time)
            + ";     ";
        }
    }
}