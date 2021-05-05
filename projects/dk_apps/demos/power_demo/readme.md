Power Demo application {#power_demo}
============================================

## Overview

This application is a simple advertising demo. It allows to configure several parameters:

- Advertising interval

- Advertising channel map

- Connection parameters (if any device connected)

- Recharge period

There are two ways of configuration: using serial console or GPIO configuration.

## Installation procedure

The project is located in the \b `projects/dk_apps/demos/power_demo` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Usage

### UART configuration

The application can be controlled using the serial console exposed over UART2.
To enable this function, POWER_DEMO_CLI_CONFIGURATION macro in file 'config/power_demo_config.h' must be set to 1.

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

Once the application has started, 'Advertising started...' message will be printed out.

### GPIO configuration

The application can be controlled using GPIO settings and button presses.
To enable this function, POWER_DEMO_GPIO_CONFIGURATION macro in file 'config/power_demo_config.h' must be set to 1.

Button must be connected to P1.0. To trigger action, connect pins of configuration type and
configuration index from table below. I.e. to set second advertising channel map configuration, connect P1.2 and P1.5
to VCC, P1.4 and P1.7 to GND, then press button.

Configuration type      | P1.2 | P1.4
------------------------|------|------
Advertising interval    | GND  | GND
Advertising channel map | VCC  | GND
Recharge period         | GND  | VCC
Connection param update | VCC  | VCC


Configuration idx | P1.5 | P1.7
------------------|------|------
0                 | GND  | GND
1                 | VCC  | GND
2                 | GND  | VCC
3                 | VCC  | VCC

### Triggering actions

#### Set advertising interval

This option allows to set advertising interval.

Configuration idx | Interval min [ms] | Interval max [ms]
------------------|-------------------|-------------------
0                 | 400               | 600
1                 | 30                | 60
2                 | 1000              | 1200

Using serial console, type <b>`set_adv_interval <cfg_idx>`</b>.
I.e., after typing 'set_adv_interval 1':

    Set advertising interval
            Interval min: 0x001E ms
            Interval max: 0x003C ms
    Advertising stopped
    Advertising started...

Using GPIO, connect P1.2 and P1.4 to GND and select configuration index using table from GPIO configuration.

#### Set channel map

This option allows to set channel map configuration.

Configuration idx | Channel map
------------------|------------------
0                 | 37 and 38 and 39
1                 | 38 and 39

Using serial console, type <b>`set_adv_channel_map <cfg_idx>`</b>.
I.e., after typing 'set_adv_channel_map 1':

    Set advertising channel map to: 0x06
    Advertising stopped
    Advertising started...

Using GPIO, connect P1.2 to VCC and P1.4 to GND and select configuration index using table from GPIO configuration.

#### Set recharge period

This option allows to set recharge priod.

Configuration idx | Recharge period
------------------|-----------------
0                 | 3000
1                 | 100
2                 | 900

Using serial console, type <b>`set_recharge_period <cfg_idx>`</b>.
I.e., after typing 'set_recharge_period 1':

    Set recharge period to: 0x0064

Using GPIO, connect P1.2 to GND and P1.4 to VCC and select configuration index using table from GPIO configuration.

#### Set connection parameters

This option allows to update connection parameters (any device must be connected to platform).

Configuration idx | Interval min [ms] | Interval max [ms] | Slave latency | Sup. timeout [ms]
------------------|-------------------|-------------------|---------------|-------------------
0                 | 400               | 600               | 0             | 1500
1                 | 10                | 15                | 0             | 100
2                 | 1000              | 1200              | 0             | 3000

Using serial console, type <b>`conn_param_update <cfg_idx>`</b>.
I.e., after typing 'conn_param_update 1':

    Connection param update
            Interval min: 0x000A ms
            Interval max: 0x000F ms
            Slave latency: 0x0000 ms
            Supervision timeout: 0x0064 ms

Using GPIO, connect P1.2 and P1.4 to VCC and select configuration index using table from GPIO configuration.

