Health Thermometer Profile Thermometer role demo application {#htp_thermometer}
==============================================================================

## Overview

The application is a sample implementation of the Health Thermometer Profile (HTP) as defined in
the HTP specification (https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=238687).
The demo includes an implementation of a Thermometer device.

It supports two mandatory services: Health Thermometer Service (HTS) and Device Information Service
(DIS).

The features:

- Advertising is started automatically when application is started. If no connection is established
  after a period of time, then advertising uses a reduced power interval and is finally stopped
  until a new measurement is done.

- Advertising is started automatically (if it hadn't been started before) following every new 
  measurement.

- If the connection is in idle state for 30 seconds then Thermometer disconnects from the client and
  stops advertising until a new measurement is done.

- Temperature measurements can be sent periodically or on-demand (by pressing the K1 button).

- When notifications of the Intermediate Temperature characteristic value are enabled, then every
  new value is sent to the client to show the measurement progress.

- The measurements are stored in the following two cases:
  * A user is disconnected but had previously enabled indications of the Temperature Measurement
    characteristic value,
  * A user is connected but indications of the Temperature Measurement characteristic value are
    disabled.

- Data are sent only to the connected user who enabled the indications of the Temperature
  Measurement characteristic value.

- A user can set a measurement interval ranging from 1 to 65535 seconds. For value equal to 0,
  only on-demand measurement data are sent.

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/htp_thermometer` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested Configurable parameters

- Device name:

  In the `htp_thermometer_task.c` file, put the device name in a `scan_rsp` table
  (GAP_DATA_TYPE_LOCAL_NAME).

- Default UART configuration:

  The default configuration is: 115200 baudrate, 8 data bits, 1 stop bit, parity - none.

- Default input pin of sending measurement button:

  \b CFG_MAKE_MEASUREMENTS_BUTTON_GPIO_PORT and \b CFG_MAKE_MEASUREMENTS_BUTTON_GPIO_PIN macros set
  a pin which is used for generating sample measurement data.

- Initial interval value:

  \b CFG_HTS_MEAS_INTERVAL macro is used only for an initialization purpose when sending periodic data
  manner is used.

- Interval range:

  By setting \b CFG_HTS_MEAS_INTERVAL_LOW_BOUND and \b CFG_HTS_MEAS_INTERVAL_HIGH_BOUND macros
  the application specifies interval bounds. It defines possible interval values which can be set
  by a user by writing to Measurement Interval characteristic.

- Maximum number of measurements to be stored:

  \b CFG_HTS_MAX_MEAS_TO_STORE specifies how many measurements are stored when a storing is required.

- Timeout to trigger disconnection when connection is in idle state

  \b CFG_HTS_CONN_IDLE_TIME specifies how many seconds application waits since it decides that it is
  in idle state (nothing was happend on the link while this time) and disconnects from the connected
  user.

## PTS testing

The htp_thermometer demo allows to perform PTS test cases for Health Thermometer Profile (HTP)
(defined as a Thermometer device (TSPC_HTP_1_1)) and for Health Thermometer Service (HTS).

## Manual testing

- Build the demo
- Download the generated binary to the FLASH and execute it (e.g. by clicking the K2 button
  (reset the board))
- Scan (e.g. using a mobile phone) for the device by name `"Black Orca HT Thermometer"` and
  connect to it
- Open a serial terminal and connnect the device to the proper serial port (e.g. /dev/ttyUSB0)

- Register for an indication of Temperature Measurement characteristic:

  * If \b CFG_HTS_MEAS_INTERVAL equals 0 then pushing the K1 button causes sending a new sample
    measurement to a client (Collector) device

  * If \b CFG_HTS_MEAS_INTERVAL is different from 0 then measurements are sent periodically with
    the defined interval (in seconds)

- Unregister from an indication of Temperature Measurement characteristic in Health Thermometer
  Service:

  * Unregistering from the indication starts a storing action. Data is stored in the following two
    cases:
     + For a periodic mode, a frequency of sending data depends on an interval settings, e.g. for
       an interval = 5 sec, data is sent every 5 second,
     + For a non-periodic mode, data is sent each time the button is pressed.
    After registering for the indication the stored data is immediately sent again to a user.

- Register for a notification of Intermediate Temperature characteristic:

  * After registering for the notification the intermediate values are sent to the device since
    temperature value is stable (3 measurements in a row must be the same). After that a new
    temperature measurement is sent to a user.

- Read from a Measurement Interval characteristic:

  * The current measurement interval value is read.

- Write to a Measurement Interval characteristic:

  * A measurement interval value in the range of 1 to 65535 seconds may be set. For 0 value
    non-periodic mode is used.

- Read a Valid Range descriptor from the Measurement Interval characteristic:

  * The current interval range is read (low [\b CFG_HTS_MEAS_INTERVAL_LOW_BOUND] and high
    [\b CFG_HTS_MEAS_INTERVAL_HIGH_BOUND] bounds).

The application can be managed by writing the following commands in the CLI:

<table>
        <tr>
                <th> Command </th>
                <th> Description </th>
                <th> Parameters </th>
                <th> Parameters description </th>
                <th> Example </th>
        </tr>

        <tr>
                <td> interval \<interval_val\> </td>
                <td> send temperature measurements periodically with set interval </td>
                <td> interval_val </td>
                <td> Mesurement Interval value </td>
                <td> interval 10 </td>
        </tr>
</table>

## Known limitations

- The HTP demo works with only one device at the same time.

- If a value of \b CFG_HTS_MAX_MEAS_TO_STORE is too high then a problem with a memory allocation may
  occur due to a lack of free space for storing data.
