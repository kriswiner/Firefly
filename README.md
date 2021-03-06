# Firefly

Firefly is a development board for ST's dual core STM32WB55CG BLE-enabled microcontroller. The Cortex M0+ manages the BLE stack while the Cortex M4F running at 64 MHz manages the user's application program. The WB55CG has 1 MByte of program flash and 256 kB of SRAM and supports the usual Serial peripherals including I2C, UART, SPI, PDM and I2S. The Firefly exposes 21 GPIOs to the user at the board edge. The pcb antenna provides excellent sensitivity allowing BLE signal range up to 50 meters line of sight. 

The simplest sketch is the blink program which uses the RTC alarm to blink the blue led once per second with the STM32WB55 otherwise in STOP mode. Average current usage at 64 MHz clock speed for this sketch is ~300 uA, peak current (when the led blinks) is about 10 mA and current in STOP mode is 4 uA.

The Firefly is intended to be programmed via USB connector using the Arduino IDE. Although traditionalists can make use of the SWD port via tag connect and their favorite adult tool chan instead. 

This repository contains several Arduino sketches for the Firefly a bit more elaborate than those available in the example folder of the Arduino IDE.

Firefly is available for purchase on Tindie.
![Firefly](https://user-images.githubusercontent.com/6698410/109915306-6d77ac80-7c66-11eb-9eba-2d169f79dbd4.jpg)

![pinmap](https://user-images.githubusercontent.com/6698410/110013388-6c7e6380-7cd6-11eb-804a-39de0d4bfd9e.jpg)
