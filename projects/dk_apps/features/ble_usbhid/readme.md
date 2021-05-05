HoGP Device with USB HID dongle {#ble_usbhid}
=============================================

## Overview

The `ble_usbhid_device` (HoGP Device) application implements a HoGP Device compliant with HoGP
specification and can be used with any BLE device supporting HoGP Host (either Boot Host or Report
Host), e.g. PC or a smartphone.

    ,-- DK ---------------.           ,-- PC/smartphone ----.
    |                     |           |                     |
    |  ble_usbhid_device  | <= BLE => |    any HoGP Host    |
    |                     |           |                     |
    `---------------------'           `---------------------'

The `ble_usbhid_dongle` (USB HID Dongle) application implements parts of HoGP Report Host role which
are necessary to work with HoGP Device implementation of `ble_usbhid_device`. Its purpose is to act
as a bridge between HoGP Device and non-BLE HID host supporting USB HID host. It is a plug & play
solution which works only with specific, preconfigured HoGP Device.

    ,-- DK ---------------.           ,-- DK ---------------.           ,-- PC/smartphone ----.
    |                     |           |                     |           |                     |
    |  ble_usbhid_device  | <= BLE => |   ble_usb_dongle    | <= USB => |    non-BLE device   |
    |                     |           |                     |           |                     |
    `---------------------'           `---------------------'           `---------------------'

Once connected to HoGP Host, the HoGP Device application will act as a keyboard and mouse combo.

## Installation procedure

HoGP Device project is located in the \b `projects/dk_apps/features/ble_usbhid/ble_usbhid_device` folder.

USB HID Dongle project is located in the \b `projects/dk_apps/features/ble_usbhid/ble_usbhid_dongle` folder.

To install both projects follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).


## PTS testing

The HoGP Device application can be tested with PTS as a HID Device.

The USB HID Dongle application can be tested with PTS as generic BLE device.


## Limitations

The USB HID Dongle works with only single compatible device which address is pre-programmed in dongle.
This is due to following assumptions:

1. The dongle is non-interactive device thus user cannot select to which device it should connect.
   With pre-programmed device address the connection process is as quick as possible since there is
   no need to scan for compatible device prior to making connection and also there will be no conflict
   if there are more similar compatible devices in neighborhood.

2. The dongle does not have full HID parser thus it cannot parse and understand reports received from
   either device or host. It acts only as a bridge passing reports from one side to another. This
   means it needs to use the same report map over USB as device connected over BLE, which means the
   report map has to be hardcoded to match the device's report map. This is achieved by sharing
   report map configuration between device and dongle applications.


## Manual testing

### HoGP Device only

After startup, device starts directed advertising to bonded host, if any, and then switches to
undirected advertising. The undirected advertising is enabled for 180 seconds. To enable it again,
press K2 button on DK.

The advertising data contains *HID Service* UUID and *Generic HID* appearance. The scan response
contains *Dialog HoGP Device* name.

To remove bonding information from the device, reset device while holding K2 button pressed.

Once connected, device acts as a keyboard and mouse combo with following features:
- mouse cursor movement is simulated in a 45 degree square pattern
- key press is simulated by sending press and release of the Num Lock key (LED state should change on
  other keyboards connected to the same host)
- D2 LED is synchronized with Caps Lock state of the host (LED can be toggled by pressing Caps Lock
  key on other keyboards connected to the same host)

\note The Num Lock and Caps Lock states synchronization depends on the host. The behaviour as described
above can be observed with Windows and Linux hosts, while OSX host does not seem to synchronize key
states.

Testing procedure:
1. Flash `ble_usbhid_device` application as described above.
2. Reset DK
3. The device starts advertising, use compatible PC or smartphone to scan and connect to the device.
4. Due to security requirements (encryption required) the device sends *Security Request* after
   connection is established with the host to initiate pairing and enable encryption.
5. The device actions should be visible on the host, as described above.
   - check that mouse cursor is moving as expected
   - check that Num Lock LED toggles every 1 second on host
   - check that Caps Lock key toggles D2 LED on DK
6. Disconnect the device (e.g. reset DK)
7. The host should reconnect with the device and encryption should be enabled immediately withour
   need to pair both devices again.


### USB HID Dongle with HoGP Device

The procedure to setup HoGP Device is the same as in **HoGP Device only** scenario.

The USB HID Dongle device does not require any user interaction. Once run, it constantly scans for
predefined device and automatically connects once matching device is found. If the connected device
is compatible with the dongle, the security and configuration is handled automatically and the device
transfers all HID traffic from/to the HoGP Device to and form the connected USB HID host.

The address of HoGP Device the dongle will try to connect to needs to be written to flash at address
0x80040 in following format:

     len | name
    -----+----------------------- 
       1 | address type
         |     0x00 = public
         |     0x01 = random
       6 | device address

The above address can be written using `cli_programmer`, e.g. following command will setup the dongle
to work with the device using a public address `80:EA:CA:AA:AA:AA`:

    ./binaries/cli_programmer gdbserver write_qspi_bytes 0x80040 0x00 0xAA 0xAA 0xAA 0xca 0xea 0x80

If address is not found on the flash (or address type is invalid), the default address is used.

Testing procedure:
1. Flash `ble_usbhid_device` and `ble_usbhid_dongle` applications as described above.
   \warning Make sure both applications are build from the same source code.
2. Setup device DK address on dongle DK as described above.
3. Connect dongle DK via USB to compatible host, it should be enumerated as HID device.
4. Reset device DK
5. The dongle DK should connect to device DK which is indicated by D2 LED on dongle DK.
6. The same behaviour should be visible on host as in **HoGP Device only** scenario.
7. Reset either of devices and the connection should be established again automatically.

