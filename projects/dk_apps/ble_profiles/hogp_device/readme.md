HOGP Device demo application {#hogp_device}
============================

## Overview

This application is a sample implementation of a HOGP Device. It can emulate
HID Mouse and HID Keyboard (both roles can be supported at the same time, but
can be also configured to be only mouse or only keyboard). See config.h for
details.

Features:
- application emulates mouse and keyboard roles
- advertising is started automatically once application is started
- application supports both report and boot host
- more than one bonded devices are supported
- when in HID Mouse role, once connected, a cursor will move on a square track
- when in HID Keyboard role, press button to generate keyboard report (it will
  send random key-code combination with pressed keys in range 'a' - 'z' (both capital
  and small letters) and '0' - '9' numbers)

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/hogp_device` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested configurable parameters

- set CFG_DEVICE_MOUSE to support MOUSE role
- set CFG_DEVICE_KEYBOARD to support KEYBOARD role
- to change mouse reports frequency set MOUSE_TIMEOUT in hogp_device_task.c

## Manual testing:

- use an Android tablet with OS version 4.1 or higher
- scan for the device by name "Dialog HoGP Device" and connect
- a cursor should appear and move on the screen
- after pressing the button, random keyboard input event should be generated

## PTS testing:

This application can be used for PTS testing after selecting configuration and supported features
in PICS.
