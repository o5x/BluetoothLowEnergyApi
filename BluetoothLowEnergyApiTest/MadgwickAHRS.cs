using System;
using System.Numerics;

namespace BluetoothLowEnergyApiTest
{
    internal class MadgwickAHRS
    {
        private static float gyroMeasError = 3.14159265358979f * (5.0f / 180.0f);   // gyroscope measurement error in rad/s (shown as 5 deg/s)
        private static float beta = MathF.Sqrt(3.0f / 4.0f) * gyroMeasError;        // 2 * proportional gain (Kp)

        public static Vector3 toEuler(Quaternion q)
        {
            float yaw = MathF.Atan2((2 * q.X * q.Y - 2 * q.W * q.Z), (2 * q.W * q.W + 2 * q.X * q.X - 1));
            float pitch = -MathF.Asin(2 * q.X * q.Z + 2 * q.W * q.Y);
            float roll = MathF.Atan2((2 * q.Y * q.Z - 2 * q.W * q.X), (2 * q.W * q.W + 2 * q.Z * q.Z - 1));
            return (new Vector3(roll, pitch, yaw)) * 180 / MathF.PI;
        }

        static public void update(Vector3 g, Vector3 a, Vector3 m, ref Quaternion q, float dt)
        {
            float recipNorm;
            float s0, s1, s2, s3;
            float qDot1, qDot2, qDot3, qDot4;
            float hx, hy;
            float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz, _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3, q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

            // Use IMU algorithm if magnetometer measurement invalid (avoids NaN in magnetometer normalisation)
            if ((m.X == 0.0f) && (m.Y == 0.0f) && (m.Z == 0.0f))
            {
                updateIMU(g, a, ref q, dt);
                return;
            }

            // Rate of change of quaternion from gyroscope
            qDot1 = 0.5f * (-q.X * g.X - q.Y * g.Y - q.Z * g.Z);
            qDot2 = 0.5f * (q.W * g.X + q.Y * g.Z - q.Z * g.Y);
            qDot3 = 0.5f * (q.W * g.Y - q.X * g.Z + q.Z * g.X);
            qDot4 = 0.5f * (q.W * g.Z + q.X * g.Y - q.Y * g.X);

            // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
            if (!((a.X == 0.0f) && (a.Y == 0.0f) && (a.Z == 0.0f)))
            {
                // Normalise accelerometer measurement
                recipNorm = invSqrt(a.X * a.X + a.Y * a.Y + a.Z * a.Z);
                a.X *= recipNorm;
                a.Y *= recipNorm;
                a.Z *= recipNorm;

                // Normalise magnetometer measurement
                recipNorm = invSqrt(m.X * m.X + m.Y * m.Y + m.Z * m.Z);
                m.X *= recipNorm;
                m.Y *= recipNorm;
                m.Z *= recipNorm;

                // Auxiliary variables to avoid repeated arithmetic
                _2q0mx = 2.0f * q.W * m.X;
                _2q0my = 2.0f * q.W * m.Y;
                _2q0mz = 2.0f * q.W * m.Z;
                _2q1mx = 2.0f * q.X * m.X;
                _2q0 = 2.0f * q.W;
                _2q1 = 2.0f * q.X;
                _2q2 = 2.0f * q.Y;
                _2q3 = 2.0f * q.Z;
                _2q0q2 = 2.0f * q.W * q.Y;
                _2q2q3 = 2.0f * q.Y * q.Z;
                q0q0 = q.W * q.W;
                q0q1 = q.W * q.X;
                q0q2 = q.W * q.Y;
                q0q3 = q.W * q.Z;
                q1q1 = q.X * q.X;
                q1q2 = q.X * q.Y;
                q1q3 = q.X * q.Z;
                q2q2 = q.Y * q.Y;
                q2q3 = q.Y * q.Z;
                q3q3 = q.Z * q.Z;

                // Reference direction of Earth's magnetic field
                hx = m.X * q0q0 - _2q0my * q.Z + _2q0mz * q.Y + m.X * q1q1 + _2q1 * m.Y * q.Y + _2q1 * m.Z * q.Z - m.X * q2q2 - m.X * q3q3;
                hy = _2q0mx * q.Z + m.Y * q0q0 - _2q0mz * q.X + _2q1mx * q.Y - m.Y * q1q1 + m.Y * q2q2 + _2q2 * m.Z * q.Z - m.Y * q3q3;
                _2bx = MathF.Sqrt(hx * hx + hy * hy);
                _2bz = -_2q0mx * q.Y + _2q0my * q.X + m.Z * q0q0 + _2q1mx * q.Z - m.Z * q1q1 + _2q2 * m.Y * q.Z - m.Z * q2q2 + m.Z * q3q3;
                _4bx = 2.0f * _2bx;
                _4bz = 2.0f * _2bz;

                // Gradient decent algorithm corrective step
                s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - a.X) + _2q1 * (2.0f * q0q1 + _2q2q3 - a.Y) - _2bz * q.Y * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - m.X) + (-_2bx * q.Z + _2bz * q.X) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - m.Y) + _2bx * q.Y * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - m.Z);
                s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - a.X) + _2q0 * (2.0f * q0q1 + _2q2q3 - a.Y) - 4.0f * q.X * (1 - 2.0f * q1q1 - 2.0f * q2q2 - a.Z) + _2bz * q.Z * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - m.X) + (_2bx * q.Y + _2bz * q.W) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - m.Y) + (_2bx * q.Z - _4bz * q.X) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - m.Z);
                s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - a.X) + _2q3 * (2.0f * q0q1 + _2q2q3 - a.Y) - 4.0f * q.Y * (1 - 2.0f * q1q1 - 2.0f * q2q2 - a.Z) + (-_4bx * q.Y - _2bz * q.W) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - m.X) + (_2bx * q.X + _2bz * q.Z) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - m.Y) + (_2bx * q.W - _4bz * q.Y) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - m.Z);
                s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - a.X) + _2q2 * (2.0f * q0q1 + _2q2q3 - a.Y) + (-_4bx * q.Z + _2bz * q.X) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - m.X) + (-_2bx * q.W + _2bz * q.Y) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - m.Y) + _2bx * q.X * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - m.Z);
                recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
                s0 *= recipNorm;
                s1 *= recipNorm;
                s2 *= recipNorm;
                s3 *= recipNorm;

                // Apply feedback step
                qDot1 -= beta * s0;
                qDot2 -= beta * s1;
                qDot3 -= beta * s2;
                qDot4 -= beta * s3;
            }

            // Integrate rate of change of quaternion to yield quaternion
            q.W += qDot1 * dt;
            q.X += qDot2 * dt;
            q.Y += qDot3 * dt;
            q.Z += qDot4 * dt;

            // Normalise quaternion
            recipNorm = invSqrt(q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z);
            q.W *= recipNorm;
            q.X *= recipNorm;
            q.Y *= recipNorm;
            q.Z *= recipNorm;
        }

        static public void updateIMU(Vector3 g, Vector3 a, ref Quaternion q, float dt)
        {
            float recipNorm;
            float s0, s1, s2, s3;
            float qDot1, qDot2, qDot3, qDot4;
            float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

            // Rate of change of quaternion from gyroscope
            qDot1 = 0.5f * (-q.X * g.X - q.Y * g.Y - q.Z * g.Z);
            qDot2 = 0.5f * (q.W * g.X + q.Y * g.Z - q.Z * g.Y);
            qDot3 = 0.5f * (q.W * g.Y - q.X * g.Z + q.Z * g.X);
            qDot4 = 0.5f * (q.W * g.Z + q.X * g.Y - q.Y * g.X);

            // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
            if (!((a.X == 0.0f) && (a.Y == 0.0f) && (a.Z == 0.0f)))
            {
                // Normalise accelerometer measurement
                recipNorm = invSqrt(a.X * a.X + a.Y * a.Y + a.Z * a.Z);
                a.X *= recipNorm;
                a.Y *= recipNorm;
                a.Z *= recipNorm;

                // Auxiliary variables to avoid repeated arithmetic
                _2q0 = 2.0f * q.W;
                _2q1 = 2.0f * q.X;
                _2q2 = 2.0f * q.Y;
                _2q3 = 2.0f * q.Z;
                _4q0 = 4.0f * q.W;
                _4q1 = 4.0f * q.X;
                _4q2 = 4.0f * q.Y;
                _8q1 = 8.0f * q.X;
                _8q2 = 8.0f * q.Y;
                q0q0 = q.W * q.W;
                q1q1 = q.X * q.X;
                q2q2 = q.Y * q.Y;
                q3q3 = q.Z * q.Z;

                // Gradient decent algorithm corrective step
                s0 = _4q0 * q2q2 + _2q2 * a.X + _4q0 * q1q1 - _2q1 * a.Y;
                s1 = _4q1 * q3q3 - _2q3 * a.X + 4.0f * q0q0 * q.X - _2q0 * a.Y - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * a.Z;
                s2 = 4.0f * q0q0 * q.Y + _2q0 * a.X + _4q2 * q3q3 - _2q3 * a.Y - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * a.Z;
                s3 = 4.0f * q1q1 * q.Z - _2q1 * a.X + 4.0f * q2q2 * q.Z - _2q2 * a.Y;
                recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
                s0 *= recipNorm;
                s1 *= recipNorm;
                s2 *= recipNorm;
                s3 *= recipNorm;

                // Apply feedback step
                qDot1 -= beta * s0;
                qDot2 -= beta * s1;
                qDot3 -= beta * s2;
                qDot4 -= beta * s3;
            }

            // Integrate rate of change of quaternion to yield quaternion
            q.W += qDot1 * dt;
            q.X += qDot2 * dt;
            q.Y += qDot3 * dt;
            q.Z += qDot4 * dt;

            // Normalise quaternion
            recipNorm = invSqrt(q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z);
            q.W *= recipNorm;
            q.X *= recipNorm;
            q.Y *= recipNorm;
            q.Z *= recipNorm;
        }

        // inverse square-root
        private static float invSqrt(float x)
        {
            float xhalf = 0.5f * x;
            int i = BitConverter.SingleToInt32Bits(x);
            i = 0x5f3759df - (i >> 1);
            x = BitConverter.Int32BitsToSingle(i);
            x = x * (1.5f - xhalf * x * x);
            return x;
        }
    }
}