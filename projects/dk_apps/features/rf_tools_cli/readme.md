RF Tools Command Line Interface Application {#rf_tools_cli}
===========================================

## Overview

This application provides a serial-based command line interface, running on the target device, 
that allows the user to perform various RF tests.

## Installation and setup

To install the application, the user must burn the image on the board's flash using the
standard SDK tools. The board must be connected to the host using a standard USB-to-serial
cable.

A standard terminal application must be used on the host, like PuTTY on Windows or 
picocom/minicom on Linux.

The host serial port must be configured to 115200 8N1. To use the backspace for deleting 
characters, the backspace character must be set to Ctrl-H (In PuTTY 
settings->Terminal->Keyboard->The Backspace key->Control-H)

## Basic usage instructions

When the device boots, and if the serial port is already setup and connected, you will get
a prompt on your terminal application. Otherwise, just press \<ENTER\> to get one.

- type **help** to get a list of the available commands
- type **verbose enable** to enable verbose return values
- type **verbose disable** to disable verbose return values
- If you enter a command without or with incorrect number or type of arguments, it will 
  output its argument list.
- If you enter arguments with invalid or out-of-range values, it will output the proper
  argument range.

## Command syntax

The following commands are defined:

### Reset the system

This command resets the chip

> da_reset

### Get the chip and CLI software version

This command returns the chip version (AA, AC, etc) and the CLI application version

> da_version

### Set the FEM mode

This command configures the Front-End Module mode of operation

> da_femmode \<TX_MODE\> \<RX_MODE\>

where:

- **TX_MODE**: One of **txhp**, **txlp**, **txbp**
- **RX_MODE**: One of **lna**, **rxbp**

The following table summarizes the modes and the respective FEM pin values:

Mode  | Description      | CSD   | CPS   | CTX   | CRX   | CHL 
----- | ---------------- | ----- | ----- | ----- | ----- | -----
txhp  | TX High Power    |  1    |  0    |  1    |  0    |  1 
txhp  | TX Low Power     |  1    |  0    |  1    |  0    |  0
txbp  | TX Bypass        |  1    |  1    |  1    |  0    |  0
lna   | RX normal (lna)  |  1    |  0    |  0    |  1    |  0
rxbp  | RX bypass        |  1    |  1    |  0    |  1    |  0

### Set the FEM bias

This command controls the FEM bias voltage (either V18 or V18P depending on the board configuration).

> da_fembias \<VOLTAGE_MV\>

where:

- **VOLTAGE_MV**: The FEM bias voltage in mV. Range: [1200, 1975]

### Set the 2nd FEM bias

This command controls the 2nd FEM bias voltage (either V18P or V18 depending on the board configuration).

> da_fembias2 \<VOLTAGE_MV\>

where:

- **VOLTAGE_MV**: The FEM bias voltage in mV. Range: [1200, 1975]

### Configure and set a GPIO

This command configures and sets a GPIO.

> da_gpio_set \<PORT\> \<PIN\> \<DIRECTION\> \<OUTPUT_MODE\> \<VALUE\>

where:

- **PORT**: The GPIO Port. 0 to 4.
- **PIN**: The GPIO Pin. 0 to 7 (0 to 4 for Port 2).
- **DIRECTION**: The GPIO Direction: 0: Input, no resistors, 1: Input w/pull-up, 2: Input w/pull-down, 3: Output.
- **OUTPUT_MODE**: (Only valid when DIRECTION=3) 0: Push-pull, 1: Open drain
- **VALUE**: (Only valid when DIRECTION=3) The GPIO value to set. 0 or 1.


Please note that some GPIOs cannot be configured. The command will return an error in this case.

### Read a GPIO value

This command reads the value of a GPIO.

> da_gpio_get \<PORT\> \<PIN\>

where:

- **PORT**: The GPIO Port. 0 to 4.
- **PIN**: The GPIO Pin. 0 to 7 (0 to 4 for Port 2).

### Set all GPIOs to a specific value.

This command will set all GPIOs (that are free to be configured) to a specific value.

> da_gpio_set_all \<VALUE\>

where:

- **VALUE**: The GPIO value to set. 0 or 1.

Please note that this command implicitly sets the GPIO to DIRECTION=3 (Output) and 
OUTPUT_MODE=0 (Push-pull).

### Set the voltage rail to use for GPIOs

This command selects the Voltage rail to use for GPIOs (V33 or V18P).

> da_gpio_rail \<RAIL\>

where:

- **RAIL**: One of V33 or V18P.

### XTAL16 Trimming command

This command can be used to read/write or autodetect the XTAL16 trimming value.

> da_xtal16 \<CMD\> \<VALUE\>

The **VALUE** depends on the **CMD**. The following commands are supported:

- read 1: read the current XTAL16M trimming value
- write **VALUE**: write a 16-bit HEX trim **VALUE** e.g. 0460
- output **EnPoPi**: drive the XTAL16M clock to a specific PIN.
		En=0|1, Po=0|1|2|3|4, Pi=0|1|2|3|4|5|6|7.
		e.g. da_xtal16 output 130 means export XTAL16M on P3_0,
		da_xtal16 output 030 means stop XTAL16M clock export on P3_0
- \+delta **VALUE**: VALUE=16-bit HEX to add to current XTAL Trimming value
- \-delta **VALUE**: VALUE=16-bit HEX to subtract from current XTAL Trimming value
- autotrim **PoPi**: Po=port and Pi=pin where the 500ms ref. pulse is applied
		e.g. 42 means port 4 pin 2

Please note, that for the autotrim function to work, the GPIO at **PoPi** must be connected 
to a function generator that produces a pulse train, with a 500ms high period.

### Transmit a 15.4 continuous unmodulated carrier (tone)

This command begins the transmission of a 15.4 continuous unmodulated carrier (tone) in the
specified frequency and power.

> wpan_txtone \<FREQUENCY_MHz\> \<POWER\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2405, 2480]
- **POWER**: The power level attenuation, in dbm. Range: [0, -4]

This command must be stopped using the **wpan_txstop** command.

### Transmit a 15.4 continuous stream

This command begins the transmission of a 15.4 continuous (unpacketized) stream of symbols,
from symbol 0x0 to symbol 0xF, in specified frequency and power.

> wpan_txstream \<FREQUENCY_MHz\> \<POWER\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2405, 2480]
- **POWER**: The power level attenuation, in dbm. Range: [0, -4]

This command must be stopped using the **wpan_txstop** command.

### Transmit 15.4 packets

This command begins the transmission of multiple 15.4 packets, with the option to add an
interval between them. These packets have a normal
15.4 header, so that they can be parsed by a 15.4 sniffer, and a payload, that contains bytes
counting from 0 to LENGTH_OCTETS-1. The first 11 octets contain the header, while the rest
contain the payload.

>  wpan_tx \<FREQUENCY_MHz\> \<POWER\> \<LENGTH_OCTETS\> \<NUM_PACKETS\> \<INTERVAL_US\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2405, 2480]
- **POWER**: The power level attenuation, in dbm. Range: [0, -4]
- **LENGTH_OCTETS**: The number of *payload* octets to use. Range: [1, 116]
- **NUM_PACKETS**: The number of packets to transmit. Range [0, 65535]. A value of 0 means 
  transmit continuously.
- **INTERVAL_US**: The inter-packet interval, in us. Range [0, 1048575]. A value of 0 will
  trigger a back-to-back transmission of packets.

This command must be stopped using the **wpan_txstop** command, if *NUM_PACKETS* is set to
zero. Otherwise, it will stop when all packets have been transmitted.

### Stop transmitting 15.4 packets

This command stops a 15.4 transmission started with the *wpan_txtone*, *wpan_txstream* and 
*wpan_tx* (NUM_PACKETS = 0) commands

> wpan_txstop

### Receive 15.4 packets

This command starts reception of 15.4 packets at the specified frequency.

> wpan_rx \<FREQUENCY_MHZ\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2405, 2480]

This command must be stopped using the *wpan_rxstop* command.

### Stop 15.4 packet reception

This command stops a started 15.4 packet reception, started using the *wpan_rx* command and prints
statistics about the received packets.

Please note that since RSSI is also sampled towards the end of the transmission, where the RF may 
have already started ramping down, the moving average RSSI result for WPAN may be inconsistent and 
inaccurate.

> wpan_rxstop

### Transmit a BLE continuous unmodulated carrier (tone)

This command begins the transmission of a BLE continuous unmodulated carrier (tone) in the
specified frequency and power.

> ble_txtone \<FREQUENCY_MHz\> \<POWER\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2402, 2480]
- **POWER**: The power level attenuation, in dbm. Range: [0, -4]

This command must be stopped using the **ble_txstop** command.

### Transmit a BLE continuous stream

This command begins the transmission of a BLE continuous (unpacketized) stream of symbols, 
using the specific payload.

> ble_txstream \<FREQUENCY_MHz\> \<POWER\> \<PAYLOAD_TYPE\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2402, 2480]
- **POWER**: The power level attenuation, in dbm. Range: [0, -4]
- **PAYLOAD_TYPE**: The payload type. Must be one of 0 (PRBS9), 1 (11110000), 2 (10101010)

This command must be stopped using the **ble_txstop** command.

### Transmit BLE packets

This command begins the transmission of multiple BLE packets, with the option to add an
interval between them. The payload of the packets is configurable.

>  ble_tx \<FREQUENCY_MHz\> \<POWER\> \<LENGTH_OCTETS\> \<NUM_PACKETS\> \<INTERVAL_US\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2402, 2480]
- **POWER**: The power level attenuation, in dbm. Range: [0, -4]
- **LENGTH_OCTETS**: The number of *payload* octets to use. Range: [1, 116]
- **PAYLOAD_TYPE**: The payload type. Must be one of 0 (PRBS9), 1 (11110000), 2 (10101010)
- **NUM_PACKETS**: The number of packets to transmit. Range [0, 65535]. A value of 0 means 
  transmit continuously.
- **INTERVAL_US**: The inter-packet interval, in us. Range [0, 1048575]. Note: A value of 0
  will actually trigger a periodic transmission of packets with an interval of 625us. A value
  greater than 0 will actually have an interval no less than 3ms.
  

This command must be stopped using the **ble_txstop** command, if *NUM_PACKETS* is set to
zero. Otherwise, it will stop when all packets have been transmitted.

### Stop transmitting BLE packets

This command stops a BLE transmission started with the *ble_txtone*, *ble_txstream* and 
*ble_tx* (NUM_PACKETS = 0) commands

> ble_txstop

### Receive BLE packets

This command starts reception of BLE packets at the specified frequency.

> ble_rx \<FREQUENCY_MHZ\>

where:

- **FREQUENCY_MHz**: The frequency in MHz. Range: [2402, 2480]

This command must be stopped using the *ble_rxstop* command.

### Stop BLE packet reception

This command stops a started BLE packet reception, started using the *ble_rx* command and prints
statistics about the received packets.

> ble_rxstop








