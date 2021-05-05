COEX Test Application {#coex_test}
==================================

## Overview

This application tests the COEX/Arbiter subsystem. It periodically transmits FTDF packets
(just like the ftdf_test/ftdf_test_phy_api applications), and listens for a ping response. At
the same time, it advertises using BLE, and waits for a BLE connection.

Its (static) configuration for FTDF is similar to the ftdf_test_phy_api configuration (see
respective documentation).

Apart from the usual Debug and Release build configurations, this application also features 
a FEM-Release configuration, that drives the external FEM module.

## Configuration

The basic application configuration can be found in config/custom_config_qspi.h.

The most important configuration parameters in this file are:

 - FTDF_DBG_BUS_ENABLE: Enable/disable FTDF diagnostic bus (0: Disable, 1: Enable). The FTDF diagnostic bus can be probed at pins P4_0 to P4_7, since FTDF_DBG_BUS_USE_PORT_4 is set in this file. See sdk/ftdf/include/ad_ftdf_phy_api.h (ad_ftdf_dbg_bus_gpio_config()) for more information. The default configuration set (in main.c) is FTDF_DBG_LMAC_PHY_SIGNALS (see macros FTDF_DBG_* in sdk/ftdf/include/ftdf.h). The most important ones are FTDF RX EN (pin 5 / P4_5) and FTDF TX EN (pin 6 / P4_6).

 - dg_configCOEX_ENABLE_DIAGS: Enable/disable COEX diagnostic bus. The actual mode is set by dg_configCOEX_DIAGS_MODE (set to HW_COEX_DIAG_MODE_3). See enum HW_COEX_DIAG_MODE in sdk/peripherals/include/hw_coex.h for more information and pinout. The PTR field (bits [6:3]) corresponds to the decision taken by the arbiter, depicted as the index of the arbiter PTI table.

 - dg_configBLE_DIAGN_CONFIG: Set to 5 to enable pins [0:1] of the BLE diagnostic bus (the rest of the BLE diag pins are overridden by the COEX diagnostic bus). The pins correspond to: P3_0: BLE TX EN, P3_1: BLE RX EN

 - FTDF_RX_HIGHER_THAN_BLE: This is an application-specific parameter that defines the relative arbiter/COEX priority of an FTDF RX operation towards the BLE operations. If it is set to 1, FTDF RX has a greater priority than BLE (except from BLE advertising packets). Otherwise, BLE has greater priority.

The Arbiter priority table is defined and set inside main.c (in function system_init()). 

The priority table is defined as:

~~~~~~~~~~~~~~~~~~~~~~~~~~~{.c}
        coex_config.pri[2].mac = HW_COEX_MAC_TYPE_FTDF;
        coex_config.pri[2].pti = COEX_TEST_FTDF_PTI_HI;
        coex_config.pri[4].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[4].pti = 5;
        coex_config.pri[FTDF_RX_PRIO].mac = HW_COEX_MAC_TYPE_FTDF;
        coex_config.pri[FTDF_RX_PRIO].pti = 0; // Rx PTI
        coex_config.pri[7].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[7].pti = 0;
        coex_config.pri[8].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[8].pti = 1;
        coex_config.pri[9].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[9].pti = 2;
        coex_config.pri[10].mac = HW_COEX_MAC_TYPE_FTDF;
        coex_config.pri[10].pti = COEX_TEST_FTDF_PTI_LO;
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This table can be changed accordingly to reorder arbiter priorities.

Using this table, FTDF transmissions where the packet SN is odd, have the highest priority, while FTDF transmissions where the packet SN is even have the lowest priority.
FTDF RX operations have a default PTI of 0. This can be changed using the FTDF_PIB_PTI_CONFIG FTDF PIB configuration parameter.

BLE operations have fixed PTIs, as indicated below. These are configured in the priority table, seen above, to define the arbiter priority of each BLE operation.

- Connect Request Response: 0
- LLCP Packets: 1
- Data Channel transmission: 2
- Initiating (Scan): 3
- Active Scanning Mode: 4
- Connectable Advertising Mode: 5
- Non-connectable Advertising Mode: 6
- Passive Scanning Mode: 7

According to the priority table defined by default in the application, BLE Connectable Advertising (PTI: 5) will always have a lower priority than FTDF High Priority transmission (PTI: COEX_TEST_FTDF_PTI_HI), but a higher priority than FTDF RX.

The rest of the BLE operations will either have a higher or lower priority than FTDF RX, depending on the value of macro FTDF_RX_HIGHER_THAN_BLE.

### Timing (default values, if not changed by the user)

 - FTDF TX/RX (ping-pong) period: 370ms (automatic retransmissions in case ACK is not received happen immediately, within this time).
 - FTDF TX duration: ~3.8ms
 - FTDF RX duration (on succesful reception of ACK + pong packet): ~6ms
 - BLE advertisement period: ~689ms
 - BLE connection interval: ~19.81ms

## Setup

The test setup includes two boards with DA1510x-xx Chips. The first board will be programmed with this firmware (coex_test), making sure macro NODE_ROLE in main.c is set to 1 (FTDF pinger). The second board will be programmed with a firmware produced by the ftdf_test_phy_api project, after having set the respective NODE_ROLE macro (in that project's main.c) to 0 (FTDF ponger). 

The boards must be reset after been programmed.

## Operation

During operation, the first board (coex_test firmware), hereafter called pinger, starts periodically sending FTDF packets to the second board (ftdf_test_phy_api firmware), hereafter called ponger. The pinger expects first an ACK by the ponger, followed by a "pong" (or "echo") packet. Finally, the pinger sends an ACK to the ponger for the echo packet.

At the same time, the pinger also advertises using BLE. The advertising device name is "Dialog COEX Test".

Please note that the actual operation of the arbiter can be monitored by enabling and probing the diagnostic signals, as described in a previous section.

There are four possible tests to be performed, depending on the value of the macro FTDF_RX_HIGHER_THAN_BLE and whether the ponger device is active. 

1. FTDF_RX_HIGHER_THAN_BLE is set and the ponger device is active

   In this test, the pinger periodically sends FTDF packets to the ponger, and then turns on its receiver to first receive the ACK and, then, the pong/echo packet. 

   BLE advertisements have a higher priority than FTDF RX, so they manage to reach the air (they have lower priority than FTDF TX, but the latter are very short, so the  probability of collision is small). 

   When a remote BLE central device (e.g. a smartphone) connects to the pinger, it starts exchanging packets more frequently to maintain the BLE connection. Since both FTDF TX and RX on the pinger are very short, BLE packets are able to pass through the arbiter in most cases, so the connection persists.

2. FTDF_RX_HIGHER_THAN_BLE is set and the ponger device is NOT active

   In this case, after performing an FTDF transmission the pinger device will keep its receiver on, waiting for the pong packet (that will never come), for the entire time until the next scheduled FTDF transmission.

   The BLE advertisement packets will pass through the arbiter, since they have a higher priority than FTDF RX. However, it will NOT be possible to create and then maintain a BLE connection, since the connection-related BLE operations will be superseded on the arbiter by the extended, long running FTDF RX operation.

3. FTDF_RX_HIGHER_THAN_BLE is NOT set and the ponger device is active

   This case is expected to behave the same or better than case 1 (FTDF_RX_HIGHER_THAN_BLE set, ponger active), since BLE operations will have higher priority than FTDF RX.

4. FTDF_RX_HIGHER_THAN_BLE is NOT set and the ponger device is NOT active

   In this case, even though FTDF RX will be active for the entire amount of time between two consecutive FTDF TX operations, the BLE connection will be successfully done and maintained, since the BLE operations related to this connection will have higher priority than FTDF RX, and therefore will be selected to pass through by the arbiter.

The following table summarizes the four aforementioned cases:

| FTDF_RX_HIGHER_THAN_BLE | ponger device status | Behaviour                                                                                         |
|:-----------------------:|:--------------------:|---------------------------------------------------------------------------------------------------|
|            1            |        active        | BLE connection works (should expect small packet loss that doesn't however affect the connection) |
|            1            |       inactive       | BLE connection cannot be established                                                              |
|            0            |        active        | BLE Connection established and maintained                                                         |
|            0            |       inactive       | BLE Connection established and maintained                   

