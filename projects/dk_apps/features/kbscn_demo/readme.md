Keyboard scanner demo {#kbscn_demo}
======================================================================
## Overview
This application is an example app about how to use keyboard scanner adapter.

### Setup
The application assumes a 4x4 keyboard connected to pins p4_0 to p4_7. 

### Demo functionality
- Connect over UART @ 115200. Key pressed is printed in UART terminal
- Kbd adapter will not let application go to sleep for an "inactivity period" after every kbd press.
In all other cases system will try to go to sleep
- When '#' button is pressed, kbd adapter will reply to CPM that system cannot go to sleep whenever CPM decides that system could go to sleep. 
This way we demonstrate kbd adapter's operation in the "cancel sleep" use case (which may as well be generated by other application/adapter). 
The behavior is toggled on every '#' button press (an informative msg is printed to UART notifying the user about the current state)
- When '*' button is pressed, kbd adapter will toggle CPM configuration from "stay_alive" (never sleep) to "resume sleep" (go to sleep when possible). 
This way we demonstrate kbd adapter's operation in active and sleep modes (an informative msg is printed to UART notifying the user about the current state)
- White LED is ON when system is active and OFF when it sleeps. System should wake up on every kbd press
- Inactivity period can be configured using AD_KBSCN_CONFIG_WITH_INACTIVE_TIME macro

