USB CDC application
======================================================================
#Overview
This application is an example app about how to implement USB CDC.

## USB feature

- USB feature can be enabled with below defines in custom\_config\_qspi.h.
	- #define dg_configUSE\_USB                        1
	- #define dg_configUSE\_USB\_ENUMERATION            1
	- #define dg_configUSE\_HW\_USB                     1	
- USB framework is based on Segger emUSB-Device. Refer to below link for more information.
	- https://www.segger.com/emusb.html

## Configurable parameters

- The default values for usb PID/VID can be used during development period. It should be changed to your company's values.
	- USB PID/VID and com port name in usb\_cdc\_smsd.c
	- Windows driver(dialog_usb.inf file)
	 
## Operation of CDC

- Connect the debug/power USB ( USB2(DBG) on a proDK ).
- Run the example app for execution.
- Connect USB ( USB1(CHG) on a ProDK ) to Host(Windows).
- Confirm com port name in Windows device manager.
	* Install the driver (`utilities/windows/cdc/dialog_usb.inf`) if required.
	* `USB CDC serial port emulation (COMxx)` in Ports(COM & LPT) of device manager would be shown.
- Open the com port on terminal program(teraterm or putty, etc).
- Type any text on the terminal program.
- Confirm the typed text is echoed in the terminal.
