Simple example for the popular but now obsolete MPU9250 9 DoF motion sensor using I2C to read the data on data ready interrupt and the Madgwick sensor fusion filter to generate quaternions and Euler angles from the scaled data.

The Madgwick filter is called iteratively after each new gyro data set is read. The iteration time is set to 10x in the sketch but can be increased. At 10X the fusion rate is 2 kHz on the Firefly, which should be adequate at 200 Hz gyro data rate. Higher data rates provide better resolution for fast motion, higher sensor fusion rates provide better tracking of orienttion at the cost of more power, etc.

Using the MPU9250 with this sketch and properly calibrating the sensors should allow ~4 degree rms heading accuracy or better depending on the quality of your breakout board and care with which you calibrate.

The sketch treats the embedded AK8963C magnetometer as a slave to the MPU6500 accel/gyro so that multiple MPU9250 sensors can be used on the same I2C bus without using an I2C multiplexer and without ambiguity as to which magnetometer is being addressed.
