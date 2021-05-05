SUOUSB application {#suousb}
======================

## Overview

This is an application for Software Update over the USB CDC (SUOUSB).

## QSPI based SUOUSB

### Prepare bootloader

To install the project follow the [General Installation and Debugging Procedure](@ref install\_and\_debug\_procedure).

1. Import the project `suousb_loader` from `projects/dk_apps/features/suousb_loader`.
2. Build this project with the `DA14681-01-Release_QSPI` configuration. 

#### Prepare main image
1. Import a project like `pxp_reporter` from `projects/dk_apps/demos/pxp_reporter`.
2. To make the flash partition table the same to the thing of suousb_loader project, below defines must be included.
	- #define dg_configIMAGE\_FLASH\_OFFSET             (0x20000)
	- #define USE\_PARTITION\_TABLE\_1MB\_WITH_SUOTA

	The build configuration for SUOTA like `DA14681-01-Release_QSPI_SUOTA` or `DA14681-01-Debug_QSPI_SUOTA` has the defines already. It can be used without any changes. 

3. Build this project with a configuration like `DA14681-01-Release_QSPI` or `DA14681-01-Debug_QSPI` with the defines or `DA14681-01-Release_QSPI_SUOTA` or `DA14681-01-Debug_QSPI_SUOTA`.
4. Erase flash entirely using `erase_qspi_jtag_win(or linux)`.
5. Download the bootloader and the main image to flash using script `suousb_initial_flash_jtag_win(or linux)`.

#### Prepare SUOUSB image for test
SUOUSB image is a binary file with a proper header that can be sent to a target device from Windows and Linux.
To create an image, open command prompt at the project folder like `projects/dk_apps/demos/pxp_reporter`
and run script to create the image file.

To build an image in Windows run:

> `mkimage.bat <build_configuration>`

Where build_configuration may be `DA14681-01-Release_QSPI`, `DA14681-01-Debug_QSPI`, etc.


To build an image in Linux run:

> `./mkimage.sh <build_configuration>`

Where build_configuration may be `DA14681-01-Release_QSPI`, `DA14681-01-Debug_QSPI`, etc

It prepares a image file like `pxp_reporter.1.0.0.1.img`. The file name contains a version number taken from a `sw_version.h`.

#### Prepare host update tool and test with Linux or Windows OS

##### Linux side test
- Copy `host_usb_updater.c` in `utilities\suousb_host\` of sdk to Linux machine.
- Build with `gcc -o host_usb_updater host_usb_updater.c`.
- Run with `sudo ./host_usb_updater /dev/ttyACM0 ./pxp_reporter.1.0.0.1.img`.
	- The `/dev/ttyACM0` means usb-cdc driver of Linux. It can be changed according to test machine.
- Sometimes a modemmanager in Linux system like Ubuntu might interrupt the usb-cdc communication. So it should be disabled using one of the methods below.
	- Remove the modemmanger : `sudo apt-get remove modemmanager`.
	- Disable the modemmanger in case of usb-cdc communication by adding below rule to `/etc/udev/rules.d/10-local.rules`.
		- `ATTRS{idVendor}=="2dcf", ATTRS{idProduct}=="6001", ENV{ID_MM_DEVICE_IGNORE}="1"`  

##### Windows side test
- Open command prompt at `utilities\suousb_host\` of sdk in Windows.
- Build with `C:\DiaSemi\SmartSnippetsStudio\Tools\mingw64_targeting32\bin\gcc.exe -o host_usb_updater.exe host_usb_updater.c`.
	- Another gcc.exe can be used. (e.g, cygwin or mingw.)
- Run with `host_usb_updater.exe 24 ..\..\..\..\..\projects\dk_apps\demos\pxp_reporter\DA14681-01-Release_QSPI\pxp_reporter.1.0.0.1.img -verbose`.
	- The `24` means com port number of USB-CDC device. You can see the com port number on Windows device manager. Also, it can be changed according to test machine. (If Windows OS requires driver installation, install usb-cdc driver `utilities\windows\cdc\dialog_usb.inf`)
	- Debug message can be enabled by `-verbose` option.

#### How to enter download mode
- Once you flash a working image e.g pxp_reporter using SUOUSB, target will boot with the working image. if you want to enter download mode via USB-CDC, below sequence is required.
- Press `K1` button. While it is pressed, press and release `K2 RESET` button. Then release `K1` button.
- If target enters download mode, below message will be shown on the serial console of host through UART.
	- `Bootloader started.`
	- `Checking status of K1 Button..`
	- `K1 Button is pressed, starting SUOUSB service without booting application.`

## RAM based SUOUSB

### Prepare bootloader
1. Import the project `suousb_loader` from `sdk/bsp/system/loaders/suousb_loader`.
2. Build the project with the `DA14681-01-Release_RAM` configuration.

#### Prepare main image
1. Import a project like `pxp_reporter` from `projects/dk_apps/demos/pxp_reporter`.
2. To make the flash partition table the same to the thing of suousb_loader project, below configuration must be included.
	- #define dg\_configIMAGE\_FLASH\_OFFSET             (0x20000)
	- #define USE\_PARTITION\_TABLE\_1MB\_WITH\_SUOTA

	The build configruation for SUOTA like `DA14681-01-Release_QSPI_SUOTA` or `DA14681-01-Debug_QSPI_SUOTA` has the defines already. It can be used without any changes. 

3. Build this project with a configuration like `DA14681-01-Release_QSPI` or `DA14681-01-Debug_QSPI` with the defines or `DA14681-01-Release_QSPI_SUOTA` or `DA14681-01-Debug_QSPI_SUOTA`.
4. Erase flash entirly using `erase_qspi_jtag_win(or linux)`.
5. Download the main image to flash using script `program_flash_jtag_win(or linux)`.

#### Prepare SUOUSB image for test
- The same as the QSPI based SUOUSB.

#### Prepare host update tool and test with Linux or Windows OS
- The same as the QSPI based SUOUSB.

#### How to enter download mode
- Load `suousb_loader` into RAM using RAM script.
- `suousb_loader` should be paused at break point of main function.
- Press `K1` button of board and select `resume` of debug mode in SmartSnippets.
- All other things are the same as the QSPI based SUOUSB.

## SUOUSB plus SUOTA
- Both of SUOUSB and SUOTA can be applied. SUOUSB is applied to bootloader(`suousb_loader`) and SUOTA is applied to main image like `pxp_reporter`.
 
### Prepare bootloader
- The same as the method for SUOUSB.

#### Prepare main image
1. Import the project `pxp_reporter` from `projects/dk_apps/demos/pxp_reporter`.
2. Confirm the same partition table with `suousb_loader` is used. 
3. Build this project with a configuration like `DA14681-01-Release_QSPI_SUOTA` or `DA14681-01-Debug_QSPI_SUOTA`.
4. Erase flash entirely using `erase_qspi_jtag_win(or linux)`.
5. Download the bootloader and the main image to flash using script `suota_initial_flash_jtag_win(or linux)`.

#### Prepare SUOUSB image for test
- The same as the method for the QSPI or RAM based SUOUSB.

#### Prepare host update tool and test with Linux or Windows OS
- The same as the method of the QSPI or RAM based SUOUSB.

#### How to enter download mode
- To enter the download mode using SUOUSB while on the bootloader, the method is the same as in the other case.
- To enter the SUOTA download mode while running the main image, follow the instructions in `readme_suota.md`.