SUOTA 1.2 client application {#suota_client}
============================================

## Overview

This application is a SUOTA 1.2 client implementation and allows to update SUOTA-enabled devices
over the air, using simple serial console interface.

Both SUOTA 1.1 (image transferred over GATT) and SUOTA 1.2 (image transferred over L2CAP CoC) are
supported.

## Installation procedure

The project is located in the \b `projects/dk_apps/features/ble_suota_client` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Usage

### UART configuration

The application can be controlled using the serial console exposed over UART2.

GPIO pins configuration is as follows:

GPIO pin | Function
---------|----------
P1.3     | UART2 TX
P2.3     | UART2 RX
P1.6     | UART2 CTS

UART settings are as follows:

Setting      | Value
-------------|--------
Baudrate     | 115200
Data bits    | 8
Stop bits    | 1
Parity       | None
Flow control | RTS/CTS

### Firmware image to upload

New firmware image (the one which will be uploaded to remote device) shall be written to flash on
`NVMS_BIN_PART` partition at offset 0.

The SUOTA Client application checks image header during boot and will print data extracted from
image header if proper image is found.

    FW Image size: 89368 bytes
    FW Image information:
            Code Size: 89332 bytes
            Version: 1.0.0.2
            Timestamp: 2016-06-14 08:00:00 UTC
            CRC: 0x92112c09
            Flags: 0xffff

### Connecting to remote device

#### Connecting to device with unknown address

Type <b>`scan start`</b> to start scanning for devices with the SUOTA service. Only devices which
include SUOTA Service UUID (0xFEF5) in their advertising data will be listed. To list any device
found type <b>`scan start any`</b> instead.

    Scanning...
    [01] Device found: 80:EA:CA:00:00:06
    [01] Device found: 80:EA:CA:00:00:06 (DIALOG-OTA_NEW)
    [02] Device found: 80:EA:CA:C0:DE:C2
    [02] Device found: 80:EA:CA:C0:DE:C2 (Dialog PX Reporter)

Once the desired device is found, type <b>`scan stop`</b> to stop scanning.

    Scan stopped

A connection can be initiated either by typing <b>`connect <idx>`</b> or <b>`connect <address>`</b>
where <b>`<idx>`</b> shall be replaced with found device index and <b>`<address>`</b> shall be
replaced by found device address.

See below for further details about connecting to remote device.

#### Connecting to a device with a known address

If the device address is known in advance or has been found by the procedure described above,
connection can be initiated by typing <b>`connect <address>`</b> where <b>`<address>`</b> shall be
replaced with device address.

    Connecting to 80:EA:CA:C0:DE:C2...

The address type is searched on the found devices list (if present) or assumed to be public if not
found. It can be however specified manually by typing <b>`connect <address> public`</b> or
<b>`connect <address> random`</b> for public and random address type respectively.

Once a connection is established, the application automatically queries remote device for
information about available services and device information. The output is printed as follows:

    Device connected
            Address: 80:EA:CA:C0:DE:C2
            Connection index: 0
    Browsing...
    Browse completed
            SUOTA service found
            Device Information Service found
    Querying device...
            Manufacturer: Dialog Semiconductor
            Model: Dialog BLE
            Firmware version: 1.0
            Software version: 1.0.0.1
            L2CAP PSM: 0x0081
    Ready.

The presence of <b>`L2CAP PSM`</b> line indicates that remote device supports SUOTA 1.2 and
therefore L2CAP CoC can be used to transfer the image.

#### Updating

The update process can be started by typing <b>`update`</b>. If SUOTA 1.2 is detected on remote
device, the SUOTA Client will automatically use L2CAP CoC to transfer the new image. Otherwise,
GATT will be used.

    Updating...
    SUOTA status: 0x10
    Starting update via L2CAP CoC...
    Data channel connected
    Sent 21 bytes
    Sent 42 bytes
    Sent 63 bytes
    Sent 84 bytes
    Sent 105 bytes
    Sent 126 bytes
    Sent 147 bytes
    Sent 168 bytes

After the image transfer has been completed, the remote device will disconnect and reboot.

    Sent 89313 bytes
    Sent 89334 bytes
    Sent 89355 bytes
    Sent 89368 bytes
    Transfer completed
    SUOTA status: 0x02
    Rebooting remote...
    Device disconnected
            Connection index: 0
            Reason: 0x13

\note
It is possible to force image transfer over GATT by typing <b>`update gatt`</b>.

\note
    Updating...
    SUOTA status: 0x10
    Starting update via GATT...
    Sent 20 bytes
    Sent 40 bytes
    Sent 60 bytes
    Sent 80 bytes
    Sent 100 bytes
    Sent 120 bytes
    Sent 140 bytes
