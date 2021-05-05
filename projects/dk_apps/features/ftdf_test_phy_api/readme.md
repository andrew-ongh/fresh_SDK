FTDF Test Application (ping-pong) using PHY API {#ftdf_test_phy_api}
====================================================================

## Overview

This application demonstrates usage of the FTDF APIs using two devices sending FTDF packets 
to each other. One of the devices is the 'pinger', that is, initiates the packet transmission
('pings' the other device) and the other device is the 'ponger', that is, it waits a packet
from the first device, then sends back a reply ('pongs' the other device)

This application uses the FTDF driver PHY API. The application configured (statically, during
compile time) as 'pinger' starts pinging the device configured (also statically) as 'ponger' with
a fixed ping interval.

## Installation and setup

To install the application, the user must burn the image on the board's flash using the
standard SDK tools. The board must be connected to the host using a standard USB-to-serial
cable.

## Configuration

The application is configured using a set of macros defined near the beginning of file
main.c. The most important are:

- NODE_ROLE: Configures whether the device will act as a pinger (1) or a ponger (0). Default: 1 (pinger)
- CONTINUOUS_TX: If set to 1, the device will not use any interval, but will ping continuously, 
  initiating the next packet transmission as soon as it receives confirmation from the FTDF driver that
  the previous packet has been sent. Default: 0
- ALLOW_BLOCK_SLEEP: If set to 1, the FTDF block will be put to sleep after a ping packet is sent, 
  and the corresponding pong packet is received successfully. It will wakeup in time to send the
  next ping packet. Not available on CONTINUOUS_TX mode. Default: 1
- TX_DELAY: The ping interval, in ms. Default: 2000 ms.
- TX_POWER: The value of the 3-bit bus that drives the RF_ANT_TRIM pins (BA only). Default: 5
- PINGER_ADDRESS: The address to use for the pinger. Default: 0x10
- PONGER_ADDRESS: The address to use for the ponger. Default: 0xd1a1
- PANID: The value to use for the PAN ID. Default: 0xcafe
- RADIO_CHANNEL: The radio channel to use. Valid values: 11-26. Default: 11
- ACK_ENABLE: Enables the ACK functionality. Default: 1
- PING_PACKET_SIZE: The size of the ping packet payload to use (excl. headers). Valid values:
  1-116. Default: 5

