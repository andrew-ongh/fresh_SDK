Blood Pressure Profile Sensor demo application {#blp_sensor}
============================================================

## Overview

The application is a sample implementation of the Blood Pressure Profile (BLP) as defined in
the BLP specification (https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=243125).
The demo includes an implementation of the Blood Pressure Sensor role.

### Features:

- The application starts advertising after reboot.
- This demo supports only one connection at a time.
- Some basic information is printed over UART.
- Push K1 button to generate a measurement and send it to the connected device immediately or store it in storage
  if there is no connection.
- The application stores up to 20 measurements.
- When the application has reached the maximum number of stored measurements and a new measurement is generated,
  the oldest one is removed.
- During the measurement the application sends an Intermediate Cuff Pressure measurement to the device.

## Installation procedure

The project is located in the `/projects/dk_apps/ble_profiles/blp_sensor` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## PTS testing

The application can be easily used for executing PTS. In some PTS test cases sending general or intermediate
measurements is required, which can be done by pushing K1 button.

## Suggested configuration parameters

The following configuration parameters can be set in `config/blp_sensor_config.h`:
- A port and a pin to trigger the generation of a measurement (by default this is the K1 button - P1_6).
- Interval between two Intermediate Cuff Pressure measurements in [ms].
- Maximum number of measurements stored in queue.

## Manual testing

- Download the code on the board using QSPI.
- Run the serial communication terminal (could be as an administrator).
  + Connection settings: 115200 8n1.
- Run `blp_sensor_qspi`. The message `Blood Pressure Sensor application started` should be displayed.
- Scan (e.g. using a mobile phone) for the device by name `"Dialog BLP Sensor"` and connect to it.
  + Accept a pair request.
- Push K1 button to generate the intermediate cuff pressure measurements and a single
  blood pressure measurement.
  * The information about the measurements should be displayed, but the Collector
    shouldn't get any data.

\note When the device (Collector) is connected, bonded and has enabled the indications,
\note the application sends all stored measurements.

- Turn on indications and notifications and then push K1 button again.
  + The information about sending the measurement is displayed in the serial communication terminal.
  + The measurements should be delivered to the Collector (e.g. mentioned mobile phone).
    * The intermediate cuff pressure and every stored blood pressure measuremets should be delivered.
