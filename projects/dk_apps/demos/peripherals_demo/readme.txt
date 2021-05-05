Demo application for the SDK.

Application can be controlled using text-based menu via UART1.
Demos output debug messages via UART2.
GPIO assignments can be configured via gpio_setup.h file (see structure below).

Folders structure:
adapters/	adapters implementations
app/		application framework, i.e. menu and tasks management
bsp_include/	common BSP includes
config/		configuration of demo applications
demos/		demo files (see below for detailed description)
ldscripts/	linker scripts
osal/		OS abstraction layer
startup/	startup files

'config' folder contents:
default/gpio_setup.h	Default GPIO pins assignment (see below)
default/userconfig.h	Default application configuration (see below)
config.h		Application configuration (should not be modified directly,
			modify files above instead)

'demos' folder contents:
common.h	function prototypes for demo applications
periph_setup.c	peripherals configuration (i.e. actual GPIO configuration)
demo_*.c	demo for single peripheral (see below for detailed description)

GPIO assignments and configuration:

It is recommended to copy gpio_setup.h file to "config/gpio_setup.h" and make
changes there. Peripheral defines can be set with chosen HW_GPIO_PORT_x and
HW_GPIO_PIN_x e.g.

#define CFG_GPIO_IR_PORT                (HW_GPIO_PORT_3)
#define CFG_GPIO_IR_PIN                 (HW_GPIO_PIN_7)

This configures port 3 with pin 7 as GPIO for IR generator.

Selecting demos:

userconfig.h can be modified to set configuration for peripherals demo application.
It is recommended to copy this file to "config/userconfig.h" and make changes there.
Unfortunately not all demos can be turn on simultaneously because of memory and
GPIO constraints so proper demos should be chosen or GPIO configuration should be
adjusted to build project. As a result it has to be remembered that:

- Timer0 and Timer1 demo can't be run simultaneously (they use the same GPIO)
- GPADC demo uses various GPIO so rest demos running together with GPADC shouldn't
  used those GPIO
- UART, I2C demos require bigger than standard stack size what applies amount
  limitations of running demos (the best way is to run this demos when other demos
  are shut down but there is also possibility to run them with selecting demos which
  don't cause stack overflow)

End of line:

In userconfig.h can set CFG_UART_USE_CRLF flag which defines newline sequence.
When it is set to 1 "\r\n" is used as newline sequence, otherwise "\n". It supports
terminals which does not handle \n alone properly.

Demos provided:

demo_breath.c
	Breath timer demo
	Automated breathing function for external LED, few examples of possible
	configurations to achieve different pulsing schemes.

demo_gpadc.c
	General purpose ADC demo
	Typical usage example of general purpose Analog-to-Digital Converter
	with 10-bit resolution. There is a possibility to choose input source,
	sampling rate or additional features like e.g. chopping, oversampling,
	enabling attenuator or sign changing.

demo_i2c.c
	I2C demo
	For demo application external temperature sensor FM75 and EEPROM
	memory were used to present standard appliance of the bus. It shows
	how to read/write from/to external devices.

demo_irgen.c
	IR generator demo
	A complete example of how IR generator can be used to implement IR
	transmission protocol. NEC protocol is used here.

demo_qspi.c
	Quad SPI demo
	Interface to an external FLASH device. There is a communication with
	W25Q16DV (external flash memory) with using fast (quad mode) or slow
	(single mode) speed data transmission.

demo_quad.c
	Quadrature decoder demo
	Presents automatically decoding the signals for the X, Y and Z axes of
	a HID input device, reporting step count and direction.

demo_spi_os.c
	SPI demo with using OS abstraction layer
	Serial peripheral interface with master/slave capability. Demo based on
	AT45DB011D (FLASH memory) showing basic operations like writing to,
	reading from and erasing memory.

demo_timer0.c
	Timer 0 demo
	General purpose timer with PWM output, few examples of how this can be
	used to make LED blink or dim using precisely calculated PWM settings.

demo_timer1.c
	Timer 1 demo
	General purpose up/down 16-bit timer with PWM capability. Couple settings
	how to configure PWM output.

demo_timer2.c
	Timer 2 demo
	14-bit timer which controls three PWM signals with respect to frequency
	and duty cycle. It can be used with external RGB LED to change its color
	and brightness.

demo_uart_os.c
	UART demo with using OS abstraction layer
	Demo shows concurrent access to UART from two tasks. This uses UART
	locking to properly handle concurrent access from different tasks.

demo_uart_printf.c
	UART demo with concurrent access and queues
	An example of printf-like calls which can be used to output data from
	application over UART2. This uses resource locking to properly handle
	concurrent access from different tasks and uses queues to implement
	asynchronous write requests, especially useful when called from ISR.

demo_wkup.c
	Wakeup timer
	Timer for capturing external events, that can be used as a wake-up
	trigger with programmable number of external events on chosen GPIO
	and debounce time. After reaching the proper amount of events interrupt
	is generated.

Output:

Some demos need uart2 for printing additional informations. To see the results
in uart2 there is a necessity to connect an external serial converter to P1.2 (RX)
and P1.3 (TX). It is used for all demos and it can be assigned to other GPIO which
are not in conflict with other GPIO used in demos.
