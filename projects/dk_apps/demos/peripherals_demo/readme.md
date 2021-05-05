Peripherals demo application {#peripherals}
============================

## Overview

The application is a sample implementation of how various peripherals can be used.

Features:

- The application can be simply controlled using a text-based menu via UART1,
- Some demos (e.g. a UART demo) print debug messages which can be read only via UART2,
- The demo checks the usage of almost all available peripherals.

### Folder structure

| Top level folders   | Description
|-------------------- | -------------------------------------------------------
| sdk/adapters/       | Adapter implementations
| app/                | Application framework, i.e. menu and task management
| sdk/bsp_include/    | Common BSP includes
| config/             | Configuration of demo applications
| demos/              | Demo files (see Demos for detailed description)
| ldscripts/          | Linker scripts
| sdk/osal/           | OS abstraction layer
| startup/            | Startup files



| Config folder contents      | Description
|---------------------------- | ---------------------------------------------------------------------------------------
| config/default/gpio_setup.h | Default GPIO pin assignment (see GPIO assignments)
| config/default/userconfig.h | Default application configuration (see Suggested configurable parameters)
| config.h                    | Application configuration (should not be modified directly, modify files above instead)



| Demos folder contents | Description
|---------------------- | -----------------------------------------------------------
| common.h              | Function prototypes for demo applications
| periph_setup.c        | Peripherals configuration (i.e. actual GPIO configuration)
| demo_*.c              | Demo for single peripheral

###Demos

Using the peripherals demo it is possible to check how peripherals work (e.g Timers, I2C, QSPI
etc.). The features depend on the selected demos which are described below.

#### Breath timer demo

- <b>Demonstrated peripheral</b>: Breath Timer

- <b>Source file</b>: demo_breath.c

- \b Description:<br>
  The demo presents an automated breathing function for an external LED without software interference.<br>
  There are a few examples of possible configurations to achieve various pulsing schemes. The D1 LED<br>
  located on the board is used by default to present how this peripheral can be used in a practical way.

- \b Pin-connections: There is no need to make any additional connections - everything is available
on the board.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_BREATH      (1)
~~~
Following macros have to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_AD_SPI         (0)
#define CFG_DEMO_AD_SPI_I2C     (0)
#define CFG_DEMO_SENSOR_ADXL362 (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Constant dim led
The LED shines dim.

##### Constant bright led
The LED shines brightly.

##### Emergency led
The LED blinks fast.

##### Dim standby breath
The LED blinks slow.

##### Slow standby breath
The LED lights up slowly.

##### Disable breath
Turn off the LED.

The following configurations on breath timer are used in this example:
- PWM duty cycle: the percentage of one period in which signal is active - LED is shining
- PWM duty cycle step
- PWM frequency: system clock division factor
- PWM polarity: positive or negative

#### General purpose ADC demo
- <b>Demonstrated peripheral</b>: GPADC

- <b>Source file</b>: demo_gpadc.c

- \b Description:<br>
  The demo presents the typical usage of a General Purpose Analog-to-Digital Converter (GPADC) with
  10-bit resolution. After calibration there is a possibility to choose an input source, sampling rate.
  As well as additional features e.g. chopping, oversampling, enabling input attenuator or sign change.

> Note: It is advised not to run  <b>General Purpose ADC demo</b> and <b>General Purpose ADC
> adapter demo</b> consecutively. The reason is, that the former uses the low level driver
> API of the GPADC while the latter the corresponding of the adapter.
> As a result the GPADC configuration applied by one demo may not be in sync with the other.

- \b Pin-connections:
  The below picture shows to which GPIOs and how external devices should be connected to the board.

<pre>
   DIALOG_DEV_BOARD__          SOURCE1___         SOURCE2___         SOURCE3___
   |                 |         |      GND|--.     |      GND|--.     |      GND|--.
   |       GPADC_____|         |         |  |     |         |  |     |         |  |
   |       |     P0_7|<--------|V_OUT____|  |  .--|V_OUT____|  |  .--|V_OUT____|  |
   |       |     P1_2|<---------------------|--'               |  |               |
   |       |     P1_4|<---------------------|------------------|--'               |
   |       `---------|                      |                  |                  |
   |              GND|----------------------o------------------o------------------'
   '-----------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_GPADC       (1)
~~~
Following macros have to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_I2C         (0)
#define CFG_DEMO_HW_I2C_ASYNC   (0)
#define CFG_DEMO_AD_SPI_I2C     (0)

#define CFG_DEMO_SENSOR_BH1750  (0)
#define CFG_DEMO_SENSOR_BME280  (0)
#define CFG_DEMO_SENSOR_BMM150  (0)
#define CFG_DEMO_SENSOR_BMG160  (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Configure
- Enabling a digital clock turns on an external clock (16 or 96 MHz). If it is disabled then
  an internal high-speed ADC clock is used.

- Input attenuator allows higher input voltage up to +3.6V in a single-ended mode and
  -3.6V - +3.6V in a differential mode. Disabling it allows voltage up to +1.2V in the single-ended
  mode and -1.2V - +1.2V in a differential mode.

- Chopping function causes the GPADC to take two samples with opposite polarity in each conversion.
  This is used to cancel an internal offset voltage of the GPADC. It is recommended for
  DC-measurements.

- Sign change enables conversion with an opposite sign at GPADC input and output to cancel out
  the internal offset of the ADC.

- Oversampling provides effectively better measurements precision. The more conversion the better
  precision.

- An interval for continuous mode - a period after which the next conversion will be executed.

##### Select input
Set an input for GPADC. The input will be used by GPADC to measure the voltage on it.

##### Measure
Get raw and converted values of measured voltage results.

##### Continuous mode
In continuous mode, GPADC performs conversions at configured intervals automatically.

##### Show state
Print the settings of GPADC.

#### General Purpose ADC adapter demo
- <b>Demonstrated peripheral</b>: GPADC

- <b>Source file</b>: demo_ad_gpadc.c

- \b Description:<br>
  The demo shows how a GPADC adapter can be used to manage devices connected to GPADC.

> Note: It is advised not to run  <b>General Purpose ADC demo</b> and <b>General Purpose ADC
> adapter demo</b> consecutively. The reason is, that the former uses the low level driver
> API of the GPADC while the latter the corresponding of the adapter.
> As a result the GPADC configuration applied by one demo may not be in sync with the other.

- \b Pin-connections:
  The below picture shows to which GPIOs and how external devices should be connected to the board.

<pre>
                                 EXAMPLE                        EXAMPLE
   DIALOG_DEV_BOARD__            LIGHT_SENSOR___                ENCODER_SENSOR___
   |                 |           |           GND|--.            |             GND|--.
   |                 |           |              |  |         .--|V_OUT1          |  |
   |                 |        .--|V_OUT_________|  |         |  |V_OUT2__________|  |
   |       GPADC_____|        |                    |         |     |                |
   |       |     P1_2|<-------o--------------------|---------'     |                |
   |       |     P1_4|<----------------------------|---------------'                |
   |       `---------|                             |                                |
   |              GND|-----------------------------o--------------------------------'
   '-----------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_AD_GPADC       (1)
~~~
Following macros have to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_I2C         (0)
#define CFG_DEMO_HW_I2C_ASYNC   (0)
#define CFG_DEMO_AD_SPI_I2C     (0)

#define CFG_DEMO_SENSOR_BH1750  (0)
#define CFG_DEMO_SENSOR_BME280  (0)
#define CFG_DEMO_SENSOR_BMM150  (0)
#define CFG_DEMO_SENSOR_BMG160  (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Read battery level
Read the battery voltage (VBAT).

##### Read temperature sensor
Measure the ambient temperature with the embeeded temperature sensor. The results are displayed in
degrees Celsius.

##### Read light sensor
Read the voltage on P1.2 pin (by default), where the value of an external light sensor is measured.
The light sensor is only an example here, other different devices can be connected to the same pin
to measure their outputs.
The P1.2 pin was set as a single-ended input where the value is measured with reference to a ground
signal (GND).

##### Read encoder sensor
Encoder sensors are mostly used in counting the motors' rotations. The demo is an example of using
inputs set in differential mode. This can be used e.g. to estimate the direction of a motor
revolution. In this case the voltage value is measured between the P1.2 and P1.4 pins. The results
are printed in mV.

#### Temperature sensor adapter demo
- <b>Demonstrated peripheral</b>: Embedded temperature sensor

- <b>Source file</b>: ad_temp_sens.c

- \b Description:<br>
  The demo reads the temperature value of the temperature sensor which is embedded into the chip.
  The results are printed out in a serial terminal in degrees Celsius.

- \b Pin-connections: There is no need to make any additional connections - everything is available
on the board.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_AD_TEMPSENS      (1)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Read Temperature Sensor
Read the value of Temperature Sensor synchronously.

##### Read Asynchronously Temperature Sensor
Read the value of Temperature Sensor asynchronously.

##### Read Temperature every 5 seconds
Read the value of Temperature Sensor synchronously every 5 seconds.

#### I2C demo
- <b>Demonstrated peripheral</b>: I2C

- <b>Source file</b>: demo_i2c.c

- \b Description:<br>
  The demo presents the usage of I2C interface with the external devices. It shows read/write operations
  on external temperature sensor and EEPROM memory.

> Note: The demo requires an external temperature sensor (FM75) and EEPROM memory (24LC256) devices
> which have to be connected to the I2C interface (P1.2 - SDA, P3.5 - SCL)!!

- \b Pin-connections:
  The below picture shows to which GPIOs and how external devices should be connected to the board.

<pre>
   DIALOG_DEV_BOARD__            TEMP_SENS_FM75_                EEPROM_24LC256___
   |                 |           |           GND|--.            |             GND|--.
   |                 |        .--|SDA           |  |         .--|SDA             |  |
   |                 |        |  |SCL___________|  |         |  |SCL_____________|  |
   |       I2C1______|        |    |               |         |    |                 |
   |       |SDA(P1_2)|<-------o----|---------------|---------'    |                 |
   |       |SCL(P3_5)|<------------o---------------|--------------'                 |
   |       `---------|                             |                                |
   |              GND|-----------------------------o--------------------------------'
   '-----------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_I2C         (1)
~~~
Following macros have to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_GPADC       (0)
#define CFG_DEMO_AD_GPADC       (0)
#define CFG_DEMO_HW_TIMER2      (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Write data to EEPROM
Write random data to the EEPROM memory.

##### Read data from EEPROM
Read data which had been written to the memory.

##### Set FM75 configuration
There is a possibility to set:
- Sensor resolution to 9, 10, 11 or 12 bit,
- An alarm range where it is not activated

##### Enable temperature sensor
Toggle on/off reading from the temperature sensor.

The following configurations on I2C are used in this example:
- I2C speed: standard, it can be changed to fast
- I2C mode: master, which means that the board manages the external devices
- I2C addressing mode: 7-bit addressing, there is also possibility to select 10-bit addressing

#### I2C demo async
- <b>Demonstrated peripheral</b>: I2C

- <b>Source file</b>: demo_i2c_async.c

- \b Description:<br>
  The demo does exactly the same as I2C demo but this one uses asynchronous write and read operations
  on external devices, resulting in faster actions.

- \b Pin-connections:
  The same as in I2C demo

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_I2C_ASYNC      (1)
~~~
Following macros have to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_GPADC          (0)
#define CFG_DEMO_AD_GPADC          (0)
#define CFG_DEMO_HW_TIMER2         (0)
~~~

#### IR generator demo
- <b>Demonstrated peripheral</b>: InfraRed Generator (IR)

- <b>Source file</b>: demo_irgen.c

- \b Description:<br>
  This is a complete example of how the IR generator can be used to implement the IR transmission protocol.
  A NEC protocol is used here.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_IRGEN      (1)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Send NEC protocol command
4 shots - the IR sends NEC command 4 times and stops.
Repeat - the IR is sending NEC command repeatedly.

##### Stop sending command
Turn off the IR generator.

The following configurations on InfraRed Generator are used in this example:
- IR logic bit format: mark is followed by space
- IR FIFO: code and repeat FIFO
- Paint symbol: space and mark
- Output mode: inverted

#### Power mode demo
- <b>Demonstrated peripheral</b>: Clock-Power manager

- <b>Source file</b>: demo_power_mode.c

- \b Description:<br>
  This is a typical example of using the various sleep modes. Demostrating the platform in sleep mode
  if the user had previously chosen one of the sleep modes. The platform can be awakened by pressing
  the K1 button.

- \b Pin-connections: There is no need to make any additional connections - everything is available
on the board.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_POWER_MODE      (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_WKUP         (0)
~~~
and functional conflicts:
~~~{.c}
#define CFG_DEMO_AD_UART         (0)
~~~
After build and run the demo on the board a menu with the following options should appear.

 * Go to Active Mode
 * Go to Extended Sleep Mode
 * Go to Hibernation Mode

Expected result:<br>
If the platform is in an Extended Sleep, a Deep Sleep or a Hibernation Sleep Mode pressing
the button wakes it and sets Active Mode. The proper messages about these operations will be
printed in a serial terminal.
SmartSnippets toolbox power profile or digital current meter can be used to monitor the current
changes across the power mode switch.

#### Quad SPI demo
- <b>Demonstrated peripheral</b>: QSPI

- <b>Source file</b>: demo_qspi.c

- \b Description:<br>
  The demo presents communication with the FLASH memory by using Quad SPI interface. There are two
  speeds of data transmission:
   - fast (quad mode) - 4 signal lines are used to execute read/write operations,
   - slow (single mode) - 1 signal line is used to execute read/write operations.

- \b Pin-connections: There is no need to make any additional connections - everything is available
on the board.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_QSPI        (1)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Set fastest quad mode
Set fastest data transmission - quad mode.

##### Set slowest single mode
Set slowest data transmission - single mode.

##### Test performance
Copy data from RAM to RAM, ROM to RAM, FLASH to RAM memories, execute it and show the time statistics
of the performed operations.

##### QSPI divider X
Divide SPI clock by X divider. It is selected by a user. By default it is set to 1.

The following configurations on QSPI are used in this example:
- QSPI address size: flash memory uses 24 bits address
- Idle clock state: high state on idle state of the SPI clock
- QSPI sampling edge: negative sampling edge
- QSPI bus mode: single and quad modes

#### Quadrature decoder demo
- <b>Demonstrated peripheral</b>: Quadrature decoder (QUAD)

- <b>Source file</b>: demo_quad.c

- \b Description:<br>
  Demonstrates automatic signal decoding for the X, Y and Z axes of an externally
  connected HID device. Step count and direction data are printed in a serial terminal.

- \b Pin-connections:
  The below picture shows to which GPIOs and how external devices should be connected to the board.

<pre>
   DIALOG_DEV_BOARD__          HID_DEVICE___
   |       QUAD______|         |            |
   |       | XB(P4_1)|<--------|X2          |
   |       | ZA(P4_2)|<--------|Z1          |
   |       | ZB(P4_3)|<--------|Z2          |
   |       | XA(P4_4)|<--------|X1          |
   |       | YA(P4_6)|<--------|Y1          |
   |       | YB(P4_7)|<--------|Y2          |
   |       `---------|         |            |
   |              GND|---------|GND         |
   '-----------------'         '------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_QUAD      (1)
~~~
Following macros have to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_AD_SPI       (0)
#define CFG_DEMO_AD_SPI_I2C   (0)
#define CFG_AD_SPI_1          (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Enable/disable channel
Enable/disable channel for X, Y or Z axis.

##### Set threshold
Set threshold to 1, 64 or 127 events. If one of the axes will reach the threshold value an interrupt
is generated and current values of steps for each axis are printed in a serial terminal.

##### Get channels state
Print activity state (active or inactive) and number of steps for each axis.

#### Sensor demos
- <b>Demonstrated peripheral</b>: Accelerometer sensor (ADXL362), Environmental sensor (BME280),
  Geomagnetic sensor (BMM150), Gyroscope sensor (BMG160), Light sensor (BH1750)

- <b>Source file</b>: demo_sensors.c

- \b Description:<br>
  Sensor demos can be activated in userconfig.h. To use sensor demos `DA14680_Sensor_Board` is needed.

- \b Pin-connections:
  Put DA14680_Sensor_Board on dialog development board and needed connections will be created.

##### Accelerometer sensor (ADXL362)
Print raw and converted in 4g range x,y,z axes acceleration using 8 bit and 12 bit resolution.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_SENSOR_ADXL362      (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_TIMER2           (0)
~~~

For following sensors BME280, BMM150, BMG160 and BH1750 the below macros have to be disabled:
~~~{.c}
#define CFG_DEMO_HW_GPADC            (0)
#define CFG_DEMO_AD_GPADC            (0)
#define CFG_DEMO_HW_TIMER2           (0)
~~~

##### Environmental sensor (BME280)
Print raw and converted (according to a specification) temperature, pressure and humidity
measurements.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_SENSOR_BME280       (1)
~~~

##### Geomagnetic sensor (BMM150)
Print raw and converted compensated magnetometer data (for x,y,z axes) in 16 bit resolution.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_SENSOR_BMM150       (1)
~~~

##### Gyroscope sensor (BMG160)
Print raw and converted gyroscope data (for x,y,z axes) in 16 bit resolution.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_SENSOR_BMG160       (1)
~~~

##### Light sensor (BH1750)
Prints raw and calculated illumination data in 16 bit resolution.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_SENSOR_BH1750       (1)
~~~

#### SPI demo with using OS abstraction layer
- <b>Demonstrated peripheral</b>: SPI

- <b>Source file</b>: demo_spi_os.c

- \b Description:<br>
  The demo presents the Serial Peripheral Interface (SPI) with a master/slave capability. The demo
  uses FLASH memory (AT45DB011D) to show basic operations like writing, reading and erasing
  the memory.

> Note: AT45DB011D (FLASH memory) has to be connected externally.
>       It is not placed on the development board!

- \b Pin-connections:
  The below picture shows to which GPIOs and how external devices should be connected to the board.

<pre>
   DIALOG_DEV_BOARD__          AT45DB011D______
   |       SPI_______|         |               |
   |       | DO(P3_7)|-------->|SO             |
   |       | DI(P4_0)|<--------|SI             |
   |       |CLK(P4_1)|-------->|SCK            |
   |       | CS(P2_0)|-------->|!CS            |
   |       `---------|         |               |
   |(MASTER)      GND|---------|GND     (SLAVE)|
   '-----------------'         '---------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b 'config/default/userconfig.h' file and set:
~~~{.c}
#define CFG_DEMO_AD_SPI      (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_BREATH   (0)
#define CFG_DEMO_HW_TIMER2   (0)
#define CFG_DEMO_HW_QUAD     (0)
#define CFG_DEMO_AD_UART     (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Write time to AT45DB011D
Write time (tick time) to the flash.

##### Erase log area in AT45DB011D
Erase page with the results.

##### Print log from AT45DB011D
Read data (time results) written to the flash.

#### SPI and I2C demo with using OS abstraction layer
- <b>Demonstrated peripheral</b>: SPI, I2C

- <b>Source file</b>: demo_i2c_spi.c

- \b Description:<br>
  The demo presents synchronous and asynchronous actions using the SPI and I2C interfaces.

> Note: AT45DB011D (FLASH memory) and 24xx256 (EEPROM memory) has to be connected externally.
>       They are not placed on the development board!

- \b Pin-connections:
  The below picture shows to which GPIOs and how external devices should be connected to the board.

<pre>
   DIALOG_DEV_BOARD__            AT45DB011D______
   |       SPI_______|           |               |
   |       | DO(P3_7)|---------->|SO             |
   |       | DI(P4_0)|<----------|SI             |
   |       |CLK(P4_1)|---------->|SCK            |
   |       | CS(P2_0)|---------->|!CS            |
   |       `---------|           |               |
   |              GND|-----------|GND     (SLAVE)|
   |(MASTER)         |           '---------------'
   |                 |
   |       I2C_______|           24xx256_________
   |       |SDA(P1_2)|<--------->|SDA            |
   |       |SCL(P3_5)|---------->|SCL            |
   |       `---------|           |               |
   |              GND|-----------|VSS     (SLAVE)|
   '-----------------'           '---------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b 'config/default/userconfig.h' file and set:
~~~{.c}
#define CFG_DEMO_AD_SPI_I2C  (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_HW_BREATH   (0)
#define CFG_DEMO_HW_TIMER2   (0)
#define CFG_DEMO_HW_QUAD     (0)
#define CFG_DEMO_AD_UART     (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Copy from AT45DB011D to 24xx256 [async]
Depending on a chosen option it copies data from AT45DB011D (SPI) to 24xx256 (I2C) memory
synchronously or asynchronously.

##### Copy from 24xx256 to AT45DB011D [async]
Depending on chosen option it copies data from 24xx256 (I2C) to AT45DB011D (SPI) memory
synchronously or asynchronously.

#### Timer 0 demo
- <b>Demonstrated peripheral</b>: Timer0

- <b>Source file</b>: demo_timer0.c

- \b Description:<br>
  The demo presents a general purpose timer with PWM output. There are a few examples of how
  the timer can be used to blink or dim the D2 LED by using precisely calculated PWM settings.

- \b Pin-connections: There is no need to make any additional connections - everything is available
on the board.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_TIMER0      (1)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Blink LED 4 times
The LED blinks 4 times and turns off.

##### Blink LED 10 times
The LED blinks 10 times and turns off.

##### Blink LED 7 times lower intensity
The LED blinks 7 times with lower intensity and turns off.

##### Set bright LED with PWM 90%
The LED shines brightly.

##### Set bright LED with PWM 20%
The LED shines dim.

##### Low power indicator (short blinks)
The LED starts generating very short blinks like a power indicator when the battery is low.

##### Turn off LED
Turns off the LED.

The following configurations on timer0 are used in this example:
- Clock resource of timer0 : slow 32 kHz clock
- PWM mode: full brightness PWM mode or half brightness CLOCK mode that will change duty cycle to 50% when LED is on
- Duty cycle: percentage of one period in which a signal or system is active
- Reload ON counter to set up the time when timer0 interrupt happens

#### Timer 1 demo
- <b>Demonstrated peripheral</b>: Timer1

- <b>Source file</b>: demo_timer1.c

- \b Description:<br>
  The demo presents a general purpose timer with PWM capability. The timer may count up or down
  with a 16-bit resolution. There are couple settings how to configure the PWM output. The higher
  value of frequency and/or duty cycle, the brighter the D2 LED shines.

- \b Pin-connections:
  The picture below shows the GPIOs and how the external devices should be connected to the board
  or the way that a user can use LED2 which is located on the board (read Timer1 brief section in
  userconfig.h)

<pre>
   DIALOG_DEV_BOARD__
   |                 |
   |       GPIO______|      _____       _____
   |       |     P1_6|--o--|__R__|--o--|A___K|--o--.
   |       `---------|      330R         LED       |
   |                 |                             |
   |              GND|-----------------------------'
   '-----------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_TIMER1      (1)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### PWM frequency
Set the frequency of the PWM signal:
64 Hz, 128 Hz or 256 Hz.

##### PWM duty cycle
Set the duty cycle of the PWM signal:
25%, 50% or 75%.

> Note: In order to apply the settings, the frequency must be set first before setting duty cycle.

#### Timer 2 demo
- <b>Demonstrated peripheral</b>: Timer2

- <b>Source file</b>: demo_timer2.c

- \b Description:<br>
  The demo presents usage of a 14-bit timer which controls three PWM signals. An external RGB LED
  is used to show how the timer can be used in a typical situation. Depending on the PWM settings,
  the LED changes color and brightness. By default the LED should be connected to
  P3.5, P3.6 and P3.7 pins (inputs can be changed in the `config/default/gpio_setup.h` file).

- \b Pin-connections:
  The picture below shows the GPIOs and how the external devices should be connected to the board.

<pre>
                                         RGB_LED_____
   DIALOG_DEV_BOARD__         _____      |           |
   |                 |  .-o--|__R__|--o--|RED        |
   |       GPIO______|  |     330R       |           |
   |       |     P3_5|--'     _____      |           |
   |       |     P3_6|----o--|__R__|--o--|GREEN      |
   |       |     P3_7|--.     330R       |           |
   |       `---------|  |     _____      |           |
   |                 |  '-o--|__R__|--o--|BLUE       |
   |                 |        330R       |           |
   |                 |                   |           |
   |              GND|-------------------|COMMON     |
   '-----------------'                   '-----------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_TIMER2      (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_AD_SPI         (0)
#define CFG_DEMO_AD_SPI_I2C     (0)

#define CFG_DEMO_HW_I2C         (0)
#define CFG_DEMO_HW_I2C_ASYNC   (0)

#define CFG_DEMO_SENSOR_BH1750  (0)
#define CFG_DEMO_SENSOR_BME280  (0)
#define CFG_DEMO_SENSOR_ADXL362 (0)
#define CFG_DEMO_SENSOR_BMM150  (0)
#define CFG_DEMO_SENSOR_BMG160  (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Set PWMs frequency
Set the timer frequency to 20kHz, 4 kHz or 250 Hz. The higher frequency the brighter the LED shines.

##### Set start/end PWMs duty cycles
PWM signals are generated with the same period but there is the possibility to manually specify
start and end position of high state inside single cycle for each PWM separately. The option shows
the PWM duty cycles (25%) which are shifted beetwen themselves. It causes the LED to change the color
fluently.

##### Light RGB LED
Turn on the LED with chosen color (Grey, Orange, Violet, Yellow, Magenta or Cyan).

##### PWMs state
Print information about the frequency and duty cycles of each PWM signal.

##### Pause timer
Pause/Resume the timer, i.e. toggle on/off the LED. If option is checked then timer is paused.

The following configurations on timer2 are used in this example:
- Division factor: use to divide the main frequency (16 MHz) to smaller values to slow the timing
- 3 PWM's are used with different start/end duty cycles

#### UART demo with using OS abstraction layer
- <b>Demonstrated peripheral</b>: UART

- <b>Source file</b>: demo_uart_os.c

- \b Description:<br>
  The demo presents concurrent access to UART from two tasks. It uses UART locking system to properly
  handle the concurrent access from the different tasks.

- \b Pin-connections:
  The picture below shows the GPIOs and how the external devices should be connected to the board.

<pre>
   DIALOG_DEV_BOARD__
   |                 |            UART<->USB_CONVERTER__              PC___________
   |       UART2_____|           |                      |            |             |
   |       | TX(P4_1)|---------->|RX      FT232      USB|<---------->|USB          |
   |       | RX(P4_2)|<----------|TX                 GND|            |             |
   |       `---------|           '----------------------'            |   TERMINAL  |
   |                 |                                |              |  |        | |
   |              GND|--------------------------------'              |  '--------' |
   '-----------------'                                               '-------------'
</pre>

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_AD_UART        (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_AD_SPI         (0)
#define CFG_DEMO_AD_SPI_I2C     (0)
#define CFG_DEMO_SENSOR_ADXL362 (0)
~~~
and functional conflicts:
~~~{.c}
#define CFG_DEMO_POWER_MODE     (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### UART2 user 1
Enable a user 1 on UART2 - this will be printing a message on the UART2 from the user every second.

##### UART2 user 2
Enable a user 2 on UART2 - this will be printing a message on the UART2 from the user 2 every half
of a second.

##### Lock UART2 for user X
Lock the UART2 for the user 1 or the user 2, then only one user may use UART2, the second user is
blocked.

##### Prompt from user X
Stop printing messages from users and wait for typing message from the selected user (X).

#### UART demo with concurrent access and queues
- <b>Demonstrated peripheral</b>: UART

- <b>Source file</b>: demo_uart_printf.c

- \b Description:<br>
  The demo presents an example of printf-like calls which can be used to output data from the application
  over the UART2. It uses resource locking to properly handle concurrent access from different tasks.
  It uses also queues to implement asynchronous write requests, which is especially useful when called
  from a ISR.

- \b Pin-connections:
  The same as in 'UART demo with using OS abstraction layer' demo.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_AD_UART        (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_AD_SPI         (0)
#define CFG_DEMO_AD_SPI_I2C     (0)
#define CFG_DEMO_SENSOR_ADXL362 (0)
~~~
and functional conflicts:
~~~{.c}
#define CFG_DEMO_POWER_MODE     (0)
~~~
After build and run the demo on the board the same menu as in 'UART demo with using OS abstraction
layer' demo should appear.

#### Wakeup timer
- <b>Demonstrated peripheral</b>: Wake-up timer

- <b>Source file</b>: demo_wkup.c

- \b Description:<br>
  The demo presents a timer used for capturing external events. It can be used as a wake-up trigger
  with a programmable number of external events on chosen GPIO and debounce time. After reaching
  the predetermined amount of events an interrupt is generated.

- \b Pin-connections: There is no need to make any additional connections - everything is available
on the board.

- <b>Run steps</b>:<br>
To enable the demo go to the \b `config/default/userconfig.h` file and set:
~~~{.c}
#define CFG_DEMO_HW_WKUP      (1)
~~~
Following macro has to be disabled due to pin conflicts:
~~~{.c}
#define CFG_DEMO_POWER_MODE   (0)
~~~
After build and run the demo on the board a menu with the following options should appear.
Expected result is described in each menu option below.

##### Input configuration
Enable or disable GPIOs used by the wake-up (P3.0, P3.1, P3.2 by default) and set how they should be
triggered - high or low state. A wakeup interrupt should be generated in two cases:
- Connecting to GND when the GPIO has the low state trigger,
- Disconnecting from GND when the GPIO has the high state trigger.
When the interrupt is generated, then in the terminal a "Wake up interrupt triggered" notification
is shown.

##### Timer configuration
Set a threshold and a debounce time.

The threshold determines how many times the state on the GPIO has to be changed to generate
the interrupt, e.g. for 5 events the state of the pin is to be changed five times to fire
the interrupt (see "Wake up interrupt triggered").

From the moment the trigger is fired for the chosen pin, the wake-up timer counts down every millisecond.
When zero is reached, and the key (the GPIO) is still pressed (i.e. has the same state) the event
counter will be incremented. After the key is released and pressed once again the above operation
will be repeated. The debounce time in this case is the time from which the timer starts counting
down. It allows better stabilizing state of the pin. The longer the debounce time the less often
the state of the pin is checked.

##### Reset timer counter
The event counter is set to 0.

##### Emulate key hit
In this case the debounce time has to be disabled in a timer configuration. This simply emulates
a button pressing action (an additional option instead of using wire and the chosen pin) causing
the event counter to be incremented. When the predetermined amount of events is reached then
the interrupt is fired.

##### Get timer state
Show active pins - they can be changed in gpio_setup.h. This option also shows the threshold, debounce
time and counter values. The counter in this case is not used for counting how many times interrupt was
fired but it is used for counting events before the interrupt is generated e.g. for 5 events threshold
when state of the pin is changed 3 times then counter should be equal to 3. When interrupt is fired
then counter is reset and his value equals 0.

The following configurations on wake-up timer are used in this example:
- Threshold counter: get/set the amount of events after that interrupt is generated
- Debounce time: time after each state of a key press/release is checked
- Trigger: set pins that triggers the wake-up and increments the counter. It can be triggered by low
  or high state of the pin (depends of settings)

## Installation procedure

The project is located in the \b `projects/dk_apps/demos/peripherals_demo` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested Configurable parameters

Suggested configurable parameters are localized in the `config/default/userconfig.h` file. When
changing configuration is needed, it is recommended to copy that file to `config/userconfig.h`
and make changes there - it will be used instead of the previous one.

### Selecting demos
In `userconfig.h` there are a lot of demo \#defines (macros) like e.g
~~~{.c}
#define CFG_DEMO_HW_TIMER2      (0)
#define CFG_DEMO_HW_WKUP        (1)
#define CFG_DEMO_HW_BREATH      (0)
~~~
~~~{.c}
#define CFG_DEMO_SENSOR_BH1750  (0)
#define CFG_DEMO_SENSOR_BME280  (1)
~~~
if macro has (0) value then it is inactive, if (1) it is active but keep in mind that there is
no possibility to run all demos simultaneously!

### End of line
In `userconfig.h` it is also possible to set the \b CFG_UART_USE_CRLF flag which defines
a newline sequence. If it is set to 1, "\r\n" is used as the newline sequence, otherwise "\n". It
supports terminals which do not handle properly a "\n" endline sign.

### GPIO assignments
It is recommended to copy the `gpio_setup.h` file to `config/gpio_setup.h` and make changes
there. Peripheral defines can be set with HW_GPIO_PORT_x and HW_GPIO_PIN_x e.g.

~~~{.c}
 #define CFG_GPIO_IR_PORT                (HW_GPIO_PORT_3)
 #define CFG_GPIO_IR_PIN                 (HW_GPIO_PIN_7)
~~~

This configures port 3 with pin 7 as GPIO for IR generator.

## Manual testing

Select from the text-based menu, in a serial terminal, the demo which is to be tested. Configure
a peripheral (optional) and run the proper action. Check if performed action is consistent with
the description and specification.

## Known limitations

The correct flag in the `userconfig.h` file has to be chosen carefully, keeping in mind that not
all combinations are allowed due to memory and GPIO restrictions. Therefore correct demos should be
chosen or GPIO configuration should be adjusted to build the project. As a result it has to be
remembered that:

- To enable I2C, I2C_async or I2C sensors demo the Timer2, GPADC and GPADC adapter demos have to be
  disabled (they use the same GPIOs).

- To enable demos which use SPI interface, Breath, Timer2, QUAD and UART demos have to be disabled.

- To enable the QUAD demo, the UART demo should be disabled.

- Timer2 can't be run simultaneously with ADXL362 sensor demo.

- UART, I2C demos require bigger than the standard stack size which applies to the amount limitations
  of running demos (the best way is to run the demos when other demos are shut down, but there is also
  a possibility to run them with selecting demos which do not cause stack overflow).

The best way is to use one demo, check how it works, turn it off and turn on another one but it is
not a demand.

Some demos require external devices e.g:

- Sensors' demos need the sensor board (DA14680_Sensor_Board),

- I2C and I2C async demos require FM75 and EEPROM memory devices which must be connected to the I2C
  interface,

- SPI demo, SPI and I2C demo require AT45DB011D (FLASH memory) and 24xx256 (EEPROM memory) which
  must be connected externally - they are not placed on the development board.

- UART demo requires UART<->USB converter e.g. FT232.
