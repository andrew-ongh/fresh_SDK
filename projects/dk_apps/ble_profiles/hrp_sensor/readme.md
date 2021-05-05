Heart Rate demo application {#hrp}
===========================

## Overview

This application is a sample implementation of Heart Rate Sensor as defined
by Heart Rate Profile (https://developer.bluetooth.org/TechnologyOverview/Pages/HRP.aspx).

It supports the mandatory Heart Rate Service (all mandatory and optional features) and optional
Device Information Service.

Features:

- Advertising is started automatically once application is started. To start advertising
  if it is turned off push the putton K1 (by default connected to P1_6)

- Heart Rate Measurement notifications are sent automatically as soon as client
  enables them by writing proper value to CCC descriptor

- Energy Expended is reset automatically on write to HRCP

- Measurements are sent every 1 second and contain some randomly generated data
  in a way that different possible combinations of fields can be tested:

 * BPM value is in range 80-90 by default (8-bit values) but can be changed to 280-290
   in order to send also 16-bit values
 * Contact Detected bit is toggled every 4 measurements sent
 * Energy Expended value is sent (and then increased) every 10 measurements
 * RR-Interval is includes and has 1 or 2 values

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/hrp_sensor` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested Configurable parameters

- Device name:
  In hrp_sensor_task.c, put the device name in line 61 (GAP_DATA_TYPE_LOCAL_NAME).

- Connect P1_2 to GND to trigger 16-bit HRP measurement value send. To change default pin port
  P1_2, in hrp_sensor_config.h define the proper port and pin value of
  CFG_SEND_16_BIT_VALUE_TRIGGER_GPIO_PIN and CFG_SEND_16_BIT_VALUE_TRIGGER_GPIO_PORT.

- To set the HRP Sensor to send Security Request, change CFG_PAIR_AFTER_CONNECT
  value to '1' in an `hrp_sensor_config.h ` file. By default this flag is set to
  '0' - HRP Sensor will not try to pair first with HRP Collector.

## PTS testing

Application can be readily used for executing PTS test cases against LLS, IAS and TPS.
It does not require any user intervention during testing.

## Manual testing

- Build the demo for execution from flash
- Download to flash and execute
- Scan for the device by name "Dialog HR Sensor" and connect
- Register for notifications from Heart Rate Measurement characteristic in Heart Rate service

 * About every second should be received random measurement:

   + By default 8-bit values of BPM are sent. This will generate values in range of 80-90.
   + To send 16-bit values of BPM, trigger port pin P1_2 state should be set to low (GND).
     This will generate values in range of 280-290
   + Contact Detected bit is toggled every 4 measurements sent
   + Energy Expended value is sent (and then increased) every 10 measurements
   + RR-Interval is includes and has 1 or 2 values

- read value of Body Sensor Location characteristic in Heart Rate service:

 * at each reading should be given the same value ('Chest' - hardcoded in application)

- write value to Heart Rate Control Point characteristic in Heart Rate service:

 * 01 value resets the value of the Energy Expended field in the Heart Rate Measurement characteristic
