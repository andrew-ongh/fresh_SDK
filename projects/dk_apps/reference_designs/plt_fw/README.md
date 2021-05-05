Brief command description
==========================

Commands mapped to a function handler
-------------------------------------

Command                    |   Handler
-------------------------- | --------------
`hci_firmware_version_get` | `fw_version_get()`


Command Format      |
:----------------:  | ---------

Byte Description    |   Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x08
Command Opcode MSB  | 0xFE
Parameter Length    | 0x00

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x45
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x08
Command_Opcode MSB                              | 0xFE
BLE_version_length                              | 0xXX (Max value 32).
Application_version_length  (Max value 32).     | 0xXX (Max value 32).
BLE_common firmware_version (32 bytes).         | 32 bytes string  containing the BLE common firmware version.
BLE_application_firmware_version (32 bytes).    | 32 bytes string  containing the BLE application firmware version.


Command                    |   Handler
-------------------------- | --------------
`hci_custom_action`        | `hci_custom_action()`


Command Format      |
:----------------:  | ---------

Byte Description    |  Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x0A
Command Opcode MSB  | 0xFE
Parameter Length    | 0x01
Custom action       | 0xXX

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x04
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x0A
Command_Opcode MSB                              | 0xFE
Return Data                                     | 0xXX (echos back the Custom action byte received)


Command                    |   Handler
-------------------------- | --------------
`hci_read_adc`             | `hci_read_adc()`


Command Format      |
:----------------:  | ---------

Byte Description    | Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x0B
Command Opcode MSB  | 0xFE
Parameter Length    | 0x00

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x05
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x0B
Command_Opcode MSB                              | 0xFE
Result (LSB)                                    | GP_ADC_RESULT_REG (LSB)
Result (MSB)                                    | GP_ADC_RESULT_REG (MSB)


Command                    |   Handler
-------------------------- | --------------
`hci_gpio_set`             | `hci_gpio_set()`


Command Format      |
:----------------:  | ---------

Byte Description    |Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x0D
Command Opcode MSB  | 0xFE
Parameter Length    | 0x04
GPIO                | An enumeration of all available GPIOs. E.g. P0_0 = 0, P0_1 = 1 …
Mode                | 0=Input, 1=Input pullup, 2=Input pulldown, 3=Output, 4=Output Push Pull, 5= Output Open Drain
Voltage level       | 0=3.3V, 1=1.8V
Reset/Set           | 0=Reset, 1=Set


Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
Byte Description                                | Value
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x03
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x0D
Command_Opcode MSB                              | 0xFE
Status                                          | 0=Succeeded, 0xFF=Error


Command                    |   Handler
-------------------------- | ----------------
`hci_gpio_read`            | `hci_gpio_read()`


Command Format      |
:----------------:  | ---------

Byte Description    | Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x0E
Command Opcode MSB  | 0xFE
Parameter Length    | 0x01
GPIO                | An enumeration of all available GPIOs. E.g. P0_0 = 0, P0_1 = 1 …


Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x04
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x0E
Command_Opcode MSB                              | 0xFE
Reset/Set                                       | 0=Low, 1=High


Command                    |   Handler
-------------------------- | -----------------
`hci_uart_loop`            | `hci_uart_loop()`


Command Format      |
:----------------:  | ---------

Byte Description    | Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x0F
Command Opcode MSB  | 0xFE
Parameter Length    | Variable
Data[XX]            | Data to be echoed back in UART. Variable length.


Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x04
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x0F
Command_Opcode MSB                              | 0xFE
Data[XX]                                        | Loop back UART data. Variable length.



Command                    |   Handler
-------------------------- | -------------------
`hci_sensor_action`        | `hci_sensor_test()`

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | ------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x0C
Command Opcode MSB              | 0xFE
Parameter Length                | 17
Iface                           | 0=SPI, 1=I2C
Read/Write                      | 0=Read, 1=Write
spi_clk_port or i2c_scl_port    | P0=0, P1=1, …
spi_clk_pin or i2c_scl_pin      | Px_0=0, Px_1=1, …
spi_di_pin or i2c_sda_pin       | P0=0, P1=1, …
spi_di_pin or i2c_sda_pin       | Px_0=0, Px_1=1, …
spi_do_port                     | P0=0, P1=1, …
spi_do_pin                      | Px_0=0, Px_1=1, …
spi_cs_port                     | P0=0, P1=1, …
spi_cs_pin                      | Px_0=0, Px_1=1, …
Register address                | A sensor register address
Register data to write          | Data to write to the sensor register if Read/Write=1
I2C slave address               | The sensor I2C slave address used if Iface=1
int_gpio_check                  | 0=Do nothing. 1=Set the following GPIO to input pull-down after the interface (SPI or I2C) has been initialized.
int_port                        | P0=0, P1=1, …
int_pin                         | Px_0=0, Px_1=1, …
Pins voltage level              | 0=3.3V, 1=1.8V



Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | ---------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x04
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x0C
Command_Opcode MSB                              | 0xFE
Sensor register data or INT GPIO level          | 0xXX. Byte read from address specified in byte "Register address" shown in the command format above, or the INT GPIO level (high=0x01 or low=0x00) if int_gpio_check=1.


Command                    |   Handler
-------------------------- | -----------------
`xtrim`                    | `xtal_trim()`

Command Format                  |
:-----------------------------: | ---------

Byte Description        | Value                                 | Notes
----------------------- | ------------------------------------- | ------
HCI Command Packet      | 0x01
Command Opcode LSB      | 0x02
Command Opcode MSB      | 0xFE
Parameter Length        | 3
Operation               | 0x00: read trim val <br> 0x01: write trim val <br> 0x02: enable output xtal on P05 <br> 0x03: increase trim value by delta <br> 0x04: decrease trim value by delta <br>0x05: disable XTAL output on P05 <br> 0x06: auto calibration test <br> 0x07: auto calibration
Trim value or delta LSB | 0x00-0xFF                             | trim value LSB when operation=1 <br> delta value LSB when operation=3,4 <br> GPIO when operation = 6,7 <br> 0x00 otherwise.
Trim value or delta MSB | 0x00-0xFF                             | trim value MSB when operation=1 <br> delta value MSB when operation=3,4 <br> 0x00 otherwise.


Return Message                                  |
:--------------------------------------------:  | ---------


Byte Description                                | Value         | Notes
----------------------------------------------- | ------------- | --------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x05
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x02
Command_Opcode MSB                              | 0xFE
Trim value LSB                                  | 0xXX          |   Trim value for operation=0 status code 2 for operation=6,7 0x0000.
Trim value MSB                                  | 0xXX          |   Trim value for operation=0 status code 2 for operation=6,7 0x0000.

> Note 1: GPIO Px_y is encoded as `x * 10 + y`. E.g. P1_5 is encoded as 15 (0x0F in hex).

> Note 2: XTAL trim value calibration returns zero on success. A non zero value indicates failure.


Command                    |   Handler
-------------------------- | -----------------
`hci_uart_baud`            | `hci_uart_baud()`

Command Format                  |
:-----------------------------: | ---------


Byte Description    | Value
------------------- | ---------
HCI Command Packet  | 0x01
Command Opcode LSB  | 0x10
Command Opcode MSB  | 0xFE
Parameter Length    | 1
Data                | Baud Rate <br> 0 ==> 9600 <br> 1 ==> 19200 <br> 2 ==> 57600 <br> 3 ==> 115200  <br> 4 ==> 1000000

Return Message                                  |
:--------------------------------------------:  | ---------


Byte Description                                | Value
----------------------------------------------- | -------------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x04
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x10
Command_Opcode MSB                              | 0xFE
Status                                          | 0=Succeeded, 0x1=Error

Commands NOT mapped to a function handler
-----------------------------------------


Command                         |
------------------------------- | --------------
<center> `cont_pkt_tx`</center> |

Command Format                  |
:-----------------------------: | ---------

Byte Description                                | Value
----------------------------------------------- | -------------
HCI Command Packet                              | 0x01
Command Opcode LSB                              | 0x1E
Command Opcode MSB                              | 0x20
Parameter Length                                | 0x03
Frequency                                       | `= (F – 2402) / 2`, where F ranges from 2402 MHz to 2480 MHz. <br> Range: 0x00 – 0x27.
Data Length                                     | 0x01-0x25: Length in bytes of payload data in each packet
Payload Type                                    | 0x00: Pseudo-Random bit sequence 9 <br> 0x01: Pattern of alternating bits ‘11110000' <br> 0x02: Pattern of alternating bits ‘10101010’ <br> 0x03: Pseudo-Random bit sequence 15 <br> 0x04: Pattern of All ‘1’ bits <br> 0x05: Pattern of All ‘0’ bits <br> 0x06: Pattern of alternating bits ‘00001111' <br> 0x07: Pattern of alternating bits ‘0101’


Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Event Packet                | 0x04
Event Code                      | 0x0E
Parameter Length                | 0x04
Num_HCI_Command_Packets         | 0x01
Command_Opcode LSB              | 0x1E
Command_Opcode MSB              | 0x20
Status                          | 0x00: command succeeded. <br> 0x01 – 0xFF: command failed. <br> See Volume 2, Part D -Error Codes in Bluetooth 4.0 specification for a list of error codes and descriptions.


Command                           |
-------------------------------   | --------------
<center> `start_pkt_rx`</center>  |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x1D
Command Opcode MSB              | 0x20
Parameter Length                | 0x01
Frequency                       | `= (F – 2402) / 2`, where F ranges from 2402 MHz to 2480 MHz. <br> Range: 0x00 – 0x27.

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Event Packet                | 0x04
Event Code                      | 0x0E
Parameter Length                | 0x04
Num_HCI_Command_Packets         | 0x01
Command_Opcode LSB              | 0x1D
Command_Opcode MSB              | 0x20
Status                          | 0x00: Command succeeded. <br> 0x01 – 0xFF: Command failed. <br> See Volume 2, Part D -Error Codes in Bluetooth 4.0 specification for a list of error codes and descriptions.


Command                                 |
--------------------------------------- | --------------
<center> `start_pkt_rx_stats`</center>  |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x81
Command Opcode MSB              | 0xFC
Parameter Length                | 0x01
Frequency                       | `= (F – 2402) / 2`, where F ranges from 2402 MHz to 2480 MHz. <br> Range: 0x00 – 0x27.

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Event Packet                | 0x04
Event Code                      | 0x0E
Parameter Length                | 0x03
Num_HCI_Command_Packets         | 0x01
Command_Opcode LSB              | 0x81
Command_Opcode MSB              | 0xFC


Command                         |
------------------------------- | --------------
<center> `stoptest`</center>    |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x1F
Command Opcode MSB              | 0x20
Parameter Length                | 0x00

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Event Packet                | 0x04
Event Code                      | 0x0E
Parameter Length                | 0x06
Num_HCI_Command_Packets         | 0x01
Command_Opcode LSB              | 0x1F
Command_Opcode MSB              | 0x20
Status  "0x00:                  | Command succeeded. <br> 0x01-0xFF: Command failed. <br> See Volume 2, Part D -Error Codes in Bluetooth 4.0 specification for a list of error codes and descriptions."
Number of packets received LSB  | 0xXX
Number of packets received MSB  | 0xXX

Command                         |
------------------------------- | --------------
<center> `stop_pkt_rx`</center> |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x82
Command Opcode MSB              | 0xFC
Parameter Length                | 0x00

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description                                | Value
----------------------------------------------- | --------------
HCI Event Packet                                | 0x04
Event Code                                      | 0x0E
Parameter Length                                | 0x0B
Num_HCI_Command_Packets                         | 0x01
Command_Opcode LSB                              | 0x82
Command_Opcode MSB                              | 0xFC
Number of received packets LSB                  | 0xXX
Number of received packets MSB                  | 0xXX
Number of received packets with sync errors LSB | 0xXX
Number of received packets with sync errors MSB | 0xXX
Number of received packets with CRC errors LSB  | 0xXX
Number of received packets with CRC errors MSB  | 0xXX
RSSI LSB                                        | 0xXX
RSSI MSB                                        | 0xXX

Command                                         |
----------------------------------------------- | --------------
<center> `unmodulated OFF / TX / RX`</center>   |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x83
Command Opcode MSB              | 0xFC
Parameter Length                | 0x02
Operation                       | 0x4F: OFF <br> 0x54: unmodulated TX <br> 0x52: unmodulated RX
Frequency                       | `= (F – 2402) / 2`, where F ranges from 2402 MHz to 2480 MHz. <br> Range: 0x00 – 0x27.

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description        | Value
----------------------- | --------------
HCI Event Packet        | 0x04
Event Code              | 0x0E
Parameter Length        | 0x03
Num_HCI_Command_Packets | 0x01
Command_Opcode LSB      | 0x83
Command_Opcode MSB      | 0xFC

Command                                 |
--------------------------------------- | --------------
<center> `start_cont_tx`</center>       |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x84
Command Opcode MSB              | 0xFC
Parameter Length                | 0x02
Frequency                       | `= (F – 2402) / 2`, where F ranges from 2402 MHz to 2480 MHz. <br> Range: 0x00 – 0x27.
Payload Type                    | 0x00: Pseudo-Random bit sequence 9 <br> 0x01: Pattern of alternating bits ‘11110000' <br> 0x02: Pattern of alternating bits ‘10101010’ <br> 0x03: Pseudo-Random bit sequence 15 <br> 0x04: Pattern of All ‘1’ bits <br> 0x05: Pattern of All ‘0’ bits <br> 0x06: Pattern of alternating bits ‘00001111' <br> 0x07: Pattern of alternating bits ‘0101’

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description        | Value
----------------------- | --------------
HCI Event Packet        | 0x04
Event Code              | 0x0E
Parameter Length        | 0x03
Num_HCI_Command_Packets | 0x01
Command_Opcode LSB      | 0x84
Command_Opcode MSB      | 0xFC


Command                                 |
--------------------------------------- | --------------
<center> `stop_cont_tx`</center>        |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x85
Command Opcode MSB              | 0xFC
Parameter Length                | 0x00

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description        | Value
----------------------- | --------------
HCI Event Packet        | 0x04
Event Code              | 0x0E
Length                  | 0x03
Num_HCI_Command_Packets | 0x01
Command_Opcode LSB      | 0x85
Command_Opcode MSB      | 0xFC

Command                                 |
--------------------------------------- | --------------
<center> `reset`</center>               |

Command Format                  |
:-----------------------------: | ---------

Byte Description                | Value
------------------------------- | --------------
HCI Command Packet              | 0x01
Command Opcode LSB              | 0x03
Command Opcode MSB              | 0x0C
Parameter Length                | 0x00

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description        | Value
----------------------- | --------------
HCI Event Packet        | 0x04
Event Code              | 0x0E
Parameter Length        | 0x04
Num_HCI_Command_Packets | 0x01
Command_Opcode LSB      | 0x03
Command_Opcode MSB      | 0x0c
Status                  | 0x00: Reset command succeeded, was received and will be executed. <br> 0x01-0xFF: Reset command failed. <br> See Volume 2, Part D -Error Codes in Bluetooth 4.0 specification for a list of error codes and descriptions."

Command                                 |
--------------------------------------- | --------------
<center> `pkt_tx_interval`</center>     |

Command Format                  |
:-----------------------------: | ---------

Byte Description                        | Value
--------------------------------------- | --------------
HCI Command Packet                      | 0x01
Command Opcode LSB                      | 0x90
Command Opcode MSB                      | 0xFC
Parameter Length                        | 0x09
Frequency                               | `= (F – 2402) / 2`, where F ranges from 2402 MHz to 2480 MHz. <br> Range: 0x00 – 0x27.
Data Length                             | 0x01-0x25 Length in bytes of payload data in each packet
Payload Type                            | 0x00: Pseudo-Random bit sequence 9 <br> 0x01: Pattern of alternating bits ‘11110000'  <br> 0x02: Pattern of alternating bits ‘10101010’ <br> 0x03: Pseudo-Random bit sequence 15 <br> 0x04: Pattern of All ‘1’ bits <br> 0x05: Pattern of All ‘0’ bits <br> 0x06: Pattern of alternating bits ‘00001111' <br> 0x07: Pattern of alternating bits ‘0101’ <br>
Number of packets to transmit LSB       | 0xXX
Number of packets to transmit MSB       | 0xXX
Interval in us byte 0 (LSB)             | 0xXX
Interval in us byte 1                   | 0xXX
Interval in us byte 2                   | 0xXX
Interval in us byte 3 (MSB)             | 0xXX

Return Message                                  |
:--------------------------------------------:  | ---------

Byte Description        | Value
----------------------- | --------------
HCI Event Packet        | 0x04
Event Code              | 0x0F
Parameter Length        | 0x03
Num_HCI_Command_Packets | 0x01
Command_Opcode LSB      | 0x90
Command_Opcode MSB      | 0xFC

Message returned when transmission is completed |
:---------------------------------------------: | ---------

Byte Description                        | Value
--------------------------------------- | --------------
HCI Event Packet                        | 0x04
Event Code                              | 0x0E
Parameter Length                        | 0x04
Num_HCI_Command_Packets                 | 0x01
Command_Opcode LSB                      | 0x90
Command_Opcode MSB                      | 0xFC
Status                                  | 0x00: Command successfully completed <br> 0x01: Illegal params
