Simple example for the popular but now obsolete MPU9250 9 DoF motion sensor using I2C to read the data on data ready interrupt and the Madgwick sensor fusion filter to generate quaternions and Euler angles fromt he scaled data.
Using the MPU9250 with this sketch and properly calibrating the sensors should allow ~4 degree rms heading accuracy or better depending on the quality of your breakout board and care with which you calibrate.

The sketch treats the embedded AK8963C magnetometer as a slave to the MPU6500 accel/gyro so that multiple MPU9250 sensors can be used on the same I2C bus without using an I2C multiplexer and without ambiguity as to which magnetometer is being addressed.
