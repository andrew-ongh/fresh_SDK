Cycling Speed and Cadence collector demo demo application {#cscp_collector}
===========================================================================

## Overview

This application is a sample implementation of Cycling Speed And Cadence collector as defined by
CSCP specification 1.0. All features are supported (i.e. both mandatory and optional features).

The application is controlled using command line interface exposed over UART2.

The application supports up to 8 simultaneous connections.

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/cscp_collector` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested Configurable parameters

## Manual testing

The application is controlled using command line interface (CLI) which can be accessed using serial
port available when platform is connected to PC using USB2 connector (e.g. `/dev/ttyUSBx` or `COMx`).

### Quick start

To quickly scan and connect to sensor device:

1. Type <b>`scan start`</b> to start scanning for CSCP Sensor devices.
2. When device is found, type <b>`scan stop`</b> to stop scanning.
~~~
Scanning...
[01] Device found: public D6:D6:CB:0B:83:DA (Wahoo BlueSC v1.4)
Scan stopped
~~~
3. Now you can connect to a device by using one of following methods:
 - type <b>`connect 1`</b> to connect to 1st device found
 - type <b>`connect D6:D6:CB:0B:83:DA`</b> to connect to device with given address
4. The connection is established and device is automatically discovered:

    \note
    The collector stores services of bonded sensors. Any subsequent connection to already bonded
    sensor will not generate additional GATT traffic. The sensor information is to be read from storage.
    Since the profile requires bonding first connection will start bonding automatically. Application
    verifies bond status on reconnection, thus any lose of encryption keys information will trigger pairing
    procedure and re-discovery of all services

~~~
Connecting to D6:D6:CB:0B:83:DA ...
Device connected
        Address: public D6:D6:CB:0B:83:DA
        Connection index: 0
Security level changed
        Connection index: 0
        Security level: 2
Browsing...
Pairing completed
        Connection index: 0
        Status: 0x00
        Bond: 1
        MITM: 0
Browse completed
        Cycling Speed and Cadence service found
        Device Information Service found
Querying device...
        Manufacturer: Wahoo Fitness
        Firmware version: 1.4
        Features: 0x03
                wheel revolution data supported
                crank revolution data supported
                multiple sensor locations NOT SUPPORTED
        Sensor Location: front wheel
Control Point enabled
Measurement notifications enabled
Ready.
~~~
   As can be seen above, discovery includes following actions:
    - search for Device Information Service and read available characteristics,
    - search for Cycling Speed and Cadence Service and read available characteristics, e.g.
      features and sensor location,
    - enable indications for Control Point (if supported by sensor)
    - enable notifications for measurement characteristic.

5. Initialization is finished when <b>`Ready.`</b> message is received, as above. You should see
   incoming measurement notifications as below.
~~~
CSC measurement notification received
        Last wheel event time: 41.517 [s] (raw 42514)
        Cumulative wheel revolutions: 111
        Last crank event time: 40.955 [s] (raw 41938)
        Cumulative crank revolutions: 141
        Calculated speed: 22.725 [km/h]
        Calculated cadence: 35 [rpm]
~~~
6. To use full functionality of the application, check the complete list of commands below.

### Available commands

#### `scan <start|stop> [any]`

Start and stop scanning procedure.

By default, the application scans for devices which include Cycling Speed and Cadence service UUID
(0x1816) in advertising data. To scan for any device in range use optional <b>`any`</b> parameter,
i.e. <b>`scan start any`</b>.

The returned list of devices includes an index, a device address and a device name (if available).

\note
The application can cache up to 25 devices. When this limit is reached, application stops filtering
duplicated devices and any new devices found are reported with index `00`.

#### `connect <address [public|private] | index>`

Connect to peripheral device.

The device can be specified by either providing an address or an index of device found during last
scan session.

The optional address type applies only when connecting to a device using an address. If not specified,
the address type is searched on results from previous scan session or <b>`public`</b> is used when not
available.

#### `connect cancel`

Cancel ongoing connection attempt.

~~~
Connection completed
        Status: 0x0c
~~~

#### `notifications <conn_idx> <on|off>`

Enable and disable mesaurement notifications on device with specified `<conn_idx>`.

This command is provided for debugging purposes. Notifications are always enabled after sensor is
connected and there is no need to enable them explicitly.

#### `disconnect <conn_idx>`

Disconnect sensor with specified `<conn_idx>`.

~~~
Disconnected from D6:D6:CB:0B:83:DA
Device disconnected
        Connection index: 0
        Reason: 0x16
~~~

#### `get <conn_idx> <parameter>`

Read parameter from sensor with specified `<conn_idx>`.

Valid parameters are:

* <b>`location`</b> - sensor location
* <b>`locations`</b> - supported sensor locations

\note
Not all parameters may be available to read from the sensor device since support for them is
optional as per CSCP specification.

#### `set <conn_idx> <parameter> <value>`

Write parameter to sensor with specified `<conn_idx>`.

Valid parameters are:

* <b>`location`</b> - sensor location, shall be one of values suppored by sensor (see <b>`get locations`</b>)
* <b>`wheelrev`</b> - cumulative wheel revolutions data

\note
Not all parameters may be available to write to the sensor device since support for them is
optional as per CSCP specification.

#### `show [connected|bonded]`

Show list of connected/bonded sensors

#### `unbond [[public|private] <address> | all]`

Unbond sensor with specified address or unbond all bonded sensors

\note
Write `show bonded` to see list of bonded sensors to unbond
