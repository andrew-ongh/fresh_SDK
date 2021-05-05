Bond Management Service demo application {#BMS}
========================================

## Overview

This application is an implementation of the Bond Management Service
(https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=293524)

Features:

- The application starts advertising. If advertising is completed,
  it starts again immediately.
- This demo connects to up to eight devices.
- During bonding password is required.
- Some basic information are written to UART. Among other things, the password
  to bonding is printed.
- Every bonded device, using Bond Management Control Point (BMCP) characteristic, can send values:
  - 03 - delete required bonded device,
  - 06 - delete all bonded devices,
  - 09 - delete all bonded devices except current device.
- If application receives authorization code, it is checked against constant code hardcoded in
  application. Default value is "bms_auth_code" but it can be changed via CFG_AUTH_CODE constant.
- Non-connected devices may be unpaired by 'K1' button press.

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/bms` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested configurable parameters

Default pin configuration may be changed in `config/bms_config.h` file.

## PTS testing

Application can be readily used for executing PTS. It does not require any user
intervention during testing.

## Manual testing

First download (by QSPI) the code on the board. Then run serial communication terminal
(could be as administrator). Connection settings: 115200 8n1. Now you can run
bms_qspi. You should see a message "Start advertising...". During
the tests basic information will be printed, like which
function was called, how many devices are connected, if any device should be
unpaired, which is the pass key to bond two devices, etc. That last one is very
helpful when you have to pair two devices with each other. The pass key should be
entered to the BMS client device in order to connect.

To test the project, you will need to run a suitable application (contact Dialog
support for suggested applications). After running the application, it should
automatically start scanning for devices. If not, press button "SCAN".
Find the device named 'Dialog BMS demo' and press "CONNECT" button next to
this device. If the connection is completed successfully on the communication terminal
screen you should see information about this connection. If the device will want to pair
and ask for pass key, enter the pass key printed in serial communication terminal.

On the application, you should see two characteristics: 'Bond Management
Control Point' and 'Bond Management Feature'. The first is writable, and
the following values can be written to it:
- value 0x03 to delete bonded device,
- value 0x06 to delete all bonded devices,
- value 0x09 to delete all bonded devices except current device,

with or without authorization code. If you write an authorization code it
should be displayed on communication terminal screen. If you write different values
than 0x03, 0x06, 0x09, you should see no information on the screen because the function
is not called.

When all values are writen to previous characteristic you should see the table
of connected devices where the last column should show information about
unpairing device during its disconnecion (0 - not unpair device, 1 - unpair device).

To unpair non-connected devices, press 'K1' button. If any device has been unpaired, you
should see information about bond data on screen.
