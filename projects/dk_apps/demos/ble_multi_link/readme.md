Multi Link demo application {#multi_link}
===========================

## Overview

This application allow to connect to many devices by writing theirs addresses to characteristic.

Features:

- The application runs in both Central and Peripheral role.
- At the beginning it starts advertising and starts again when the main device (first connected)
  will be disconnected. Then the next device will become the main device.
- Some basic information are written to UART.
- Connected central can use Multilink service (see below) to use device Central role
  and connect to different peripherals.
- Client can write peer address to Peripheral Address characteristic and device
  initiates connection procedure (i.e. start scanning and then connects).
- It is possible to connect more than one device this way. This application connects to up to eight
  devices.

## Installation procedure

The project is located in the \b `projects/dk_apps/demos/ble_multi_link` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

### Eclipse method

Using 'SmartSnippets Studio' import the project and build it.
Then press the Run program_qspi_win button
(it is under Run > External Tools > External Tool Configurations) to download the demo
to the QSPI flash.

## PTS testing

Application can be readily used for executing PTS. It does not require any user
intervention during testing.

## Manual testing

Multilink service UUID:
{3292546e-0a42-4348-aa38-33aab6f9af93}

Peripheral Address characteristic UUID:
{3292546e-0a42-4348-aa38-33aab6f9af94} (properties=Write Without Response)

- Run the serial communication terminal to see all needed informations.
  Connection settings: 15200 8n1.
- Run the application.
- Connect to the application. Name of device shall be "Dialog Multi-link".
  After succesful connection your device will become main the device (main device decided which device
  will be connected)
- Find Peripheral characteristic and write down there addresses of devices (slaves), with which
  you want to be connected (one or more addresses of devices).
  After every succesful connection you will see an information on the communication terminal.
- Disconnect one of your slave devices (for example shut down the slave device because you can not
  do this by sending any data to application).
- Check if you have been informed of the device disconnection if you done above point.
- Disconnect your device and check if you are able to connect another device (main device)
  to application. If so, you can write down another addresses of devices (slaves) to current
  main device, with which you want to be connected.
