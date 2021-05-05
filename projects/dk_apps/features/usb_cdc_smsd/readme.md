USB CDC and Smart MSDapplication
======================================================================
#Overview
This application is an example app about how to implement USB CDC and SmartMSD at the same time.

## USB feature
- USB feature can be enabled with below defines in custom\_config\_qspi.h.
	- #define dg_configUSE\_USB                        1
	- #define dg_configUSE\_USB\_ENUMERATION            1
	- #define dg_configUSE\_HW\_USB                     1
- USB framework is based on Segger emUSB-Device. Refer to below link for more information.
	- https://www.segger.com/emusb.html

## Configurable parameters
- The default values for usb PID/VID can be used during development period. It should be changed to your company's values.
	- USB PID/VID and com port name in usb_cdc_smsd.c.
	- Windows driver(dialog_usb.inf).
- SmartMSD is an emulated FAT file system. The data can be stored to RAM or NVMS which can be switch below define in usb\_cdc\_smsd.c.
	- #define SMSD\_USE\_NVMS
- File size of a file is limited in this application.
	- #define SMSD\_DATA\_SIZE 2*1024
- Sector size of FAT can be adjusted.
	- #define SMARTMSD\_NUM\_SECTORS (32+6*2)
- Maximum number of files is limited in this application. See macro below:
	- #define MAX\_FILE\_NUM\_SMSD 3

## Operation of SmartMSD
- Connect the debug/power USB ( USB2(DBG) on a proDK ).
- Run the example app for execution.
- Connect USB ( USB1(CHG) on a ProDK ) to Host(Window).
- No special driver is required.
- Confirm a portable stroage disk is attached to Host.
- 3 files are shown at the portable storage in Host.
- Any file can be read from Host.
- 1 file(APP.DAT) can be written to DA1468x. The first header of the file must be 'FWBIN'. This magic value will decide whether it is proper file for writing in device.  
- If the header is different, usb is disconnect/reconnected automatically.
- After writing the file, eject/disconnect the USB device properly, using the O/S infrastructure (click to: "Safely Remove Hardware and Eject Media")

## Operation of USB CDC
-  Refer to readme.md in usb\_cdc project.
