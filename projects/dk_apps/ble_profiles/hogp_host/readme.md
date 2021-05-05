HOGP Host {#hogp_host}
===================================

## Overview

The application is a sample implementation of the HOGP Host as defined
in the HID Over GATT Profile. The application supports both the Boot Host and
the Report Host modes. (https://developer.bluetooth.org/TechnologyOverview/Pages/HOGP.aspx).

It supports clients for:
- the GATT Service
- the HID Service
- the Scan Parameters Service
- the Device Information Service
- the Battery Service

Features:

- Connection and disconnection are triggered by button presses (a remote device address is configurable in
  hogp_host_config.h)
- Browsing for supported services (the services are listed above)
- Both Boot Host and Report Host roles are supported (configurable in build time in hogp_host_config.h)
- The Report Host role supports reading and writing to Input, Output and Feature reports and notifications
  for Input reports
- The Report Host role reads Report Map, HID Info and External Report References
- The Boot Host role supports notifications for the Boot Mouse Input, the Boot Keyboard Input reports and
  reading/writing operations for all Boot Report characteristics
- Most user interactions such as reports writing are exposed via console interface
- Incoming report's values are printed out to a terminal

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/hogp_host` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested configurable parameters

- the Host role:
  Set the CFG_REPORT_MODE macro in the hogp_host_config.h

- the Sensor address:
  Set the CFG_PERIPH_ADDR macro in the hogp_host_config.h

- the Connection parameters:
  Set the CFG_CONN_PARAMS macro in the hogp_host_config.h

- the Button settings:
  Set the CFG_TRIGGER_CONNECT_GPIO_PORT and the CFG_TRIGGER_CONNECT_GPIO_PIN macros in the hogp_host_config.h

## PTS testing

The application can be easily used for executing HOGP and SCPP PTS test cases.
User interaction might be triggered by using serial terminal.

## Manual testing

- Build the demo for an execution from the flash.
- Download the hogp_host.bin binary to flash and execute.
- Connect with the serial port terminal.
- Press the button to connect/disconnect HID Device
- Debug logs are printed out to the console
- In order to trigger commands, connect serial terminal

- Get Protocol Mode command:

 * write "hogp get_protocol <hid_client_id>" in serial terminal

- Control Point Command

 * write "hogp cp_command <hid_client_id> <command>" in serial terminal

- Read Boot Report

 * write "hogp boot_read <hid_client_id> <boot_report_type>" in serial terminal

- Write Boot Report

 * write "hogp boot_write <hid_client_id> <boot_report_type> <data>" in serial terminal

- Write Boot Report CCC descriptor

 * write "hogp boot_notif <hid_client_id> <boot_report_type> <enable flag>" in serial terminal

- Read Boot Report CCC descriptor

 * write "hogp boot_read_ccc <hid_client_id> <boot_report_type>" in serial terminal

- Read Report

 * write "hogp report_read <hid_client_id> <report_type> <report_id>" in serial terminal

- Write Report

 * write "hogp report_write <hid_client_id> <report_type> <report_id> <confirm_flag> <data>"
   in serial terminal

- Write Input Report CCC descriptor

 * write "hogp report_notif <hid_client_id> <report_id> <enable>" in serial terminal

- Read Input Report CCC descriptor

 * write "hogp report_read_ccc <hid_client_id> <report_id>" in serial terminal
