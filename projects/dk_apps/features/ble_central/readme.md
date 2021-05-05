BLE Central demo application {#ble_central}
============================

## Overview

This application is an example of BLE central role device implementation. It connects to a remote
device, searches for services, characteristic and descriptors. All details of the discovered
characteristics will be printed over UART. Application uses two methods of searching for attributes:
browse API and discovery API. It can be configured with flag CFG_USE_BROWSE_API in ble_central_config.h.
Application can also write the remote device name characteristic if CFG_UPDATE_NAME in
ble_central_config.h is set.

Features:
- application implements BLE central role device
- once the application is started, it connects to a remote device
- details of the discovered attributes are printed out to console
- application subscribes for notifications and indications
- application writes remote device name if flag CFG_UPDATE_NAME in ble_central_config.h is set

## Installation procedure

The project is located in the \b `projects/dk_apps/features/ble_central` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested configurable parameters

- set CFG_USE_BROWSE_API flag to enable Browse API or discovery API
- set CFG_UPDATE_NAME flag if application should write remote device name characteristic
- set addr variable in ble_central_task.c to peripheral device address

## Manual testing

- start advertising on peripheral device
- start application and wait for connection complete, line "handle_evt_gap_connected: conn_idx=0000"
  will be displayed if connected
- application will display all attributes
- if peripheral sends notifications or indications, they will be displayed on console
