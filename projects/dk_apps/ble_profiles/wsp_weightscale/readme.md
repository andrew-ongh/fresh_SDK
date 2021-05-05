Weightscale profile demo application {#wsp_weightscale}
==================================

## Overview

This application is a sample implementation of Weight Scale role as defined by Weight Scale Profile
specification located at: https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=293525

It supports the mandatory Weight Scale Service and Device Information Service and optional services
like User Data Service, Body Composition Service, Battery Service and Current Time Service.

Features:
- The application can be controlled using the 'User Control Point' or the command line interface exposed
  over UART2. Check 'Manual Testing' section for more details.

- For any read/write characteristics actions, UDS 'consent' procedure is required.

- WSP is supporting multiple users (CFG_MULTIPLE_CLIENTS).

- WSP can be set into 'test mode'. In this mode WSS and BCS services can be accessible without
  sending the 'consent' procedure (measurements are generated to each user connected to WSP).
  This mode can be used while testing WSS and BCS against PTS. Please see 'Manual Testing' section
  for more details (attach_user).

- There is an ability to remove the selected user or all users from the UDS database by using the
  CLI command interface.

- Measurements for each registered user are stored in the WSP application when it is not possible
  send it to the user (Client Characteristic Configuration is not set, user is disconnected).
  Stored measurement are retransmitted when a user is connected again. Currently platform is able
  to store 25 measurements per user but realistic value depends on the platform memory limit.

- One user ID with multiple connections is supported. One user can be authenticated (using the
  consent procedure) with more than one device.

## Installation procedure

The project is located in the \b `projects/dk_apps/ble_profiles/wsp_weightscale` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Suggested Configurable parameters

- The maximum number of supported users can be modified by the CFG_UDS_MAX_USERS macro
  (config/wsp_weightscale_config.h). CFG_UDS_MAX_USERS can have the maximum value of 4.

- Multiple users support can be enabled by seting the CFG_MULTIPLE_CLIENTS macro to 1.

- Maximum stored measurements threshold can be set by using the CFG_MAX_MEAS_TO_STORE macro.

## PTS testing

- By using the wsp_weightscale application the user is able to perform PTS tests for services listed
  below:
  * Weightscale Service (WSS),
  * Body Composition Service (BCS),
  * User Data Service (UDS),
  * Device Information Service (DIS),
  * Current Time Service (CTS).
- During testing the WSP please activate multiple users support by setting the CFG_MULTIPLE_CLIENTS
  macro to 1.

- PTS actions that require user intervention are described below:
  * Measurement generation:
    - to be able to trigger WSS measurements press the K1 button,
    - BCS measurement samples are emitted every 5 seconds using system timer.
  * Current time manual change notification: to be able to trigger CTS 'Manual Change' a write of
    valid data to CTS service needs to be performed.

## Manual testing

- The application can be controlled by using 'User Control Point' or command line (CLI).
- To be able trigger wakeup routine, K1 button must be connected to P1_0 port/pin.

### Using 'User Control Point'

- By using 'User Control Point' with UUID 0x2A9F located in UDS the user is able to perform
  the actions listed below:
  * (0x01) 'Register New User' - user can be registered in a service (generating measurement is
    enabled)
  * (0x02) 'Consent' - using this command the user is able to authenticate identity in a service,
    (receiving measurement is enabled),
  * (0x03) 'Delete User Data' - the user profile is removed from a service.
  * User data stored in UDS service are non-volatile. To remove this data from the service the user
    must use 'Delete User Data'.

- The procedure to issue the above commands is described below:
  * build the demo for execution from qspi
  * download the demo to qspi and execute
  * scan for the device by name "Dialog Weight Scale" and connect to it
  * perform pairing procedure with "Dialog Weight Scale"
  * write value to 'User Control Point' in User Data Service:
    + 0x01 - 'Register New User', the consent value shall be set from range (0-9999) and then
      press send. In a response, 'Response Value' shall be set to 0x01 (success) and a new user
      index value shall be returned.
    + 0x02 - 'Consent'. To be authorized as a user with id set to 0 set 'User index' value to 0
      and 'consent value' corresponding to this user (set before in 'Register New Use' command),
      and then press send. In a response, 'Response Value' shall be set to 0x01 (success).
    + 0x03 - 'Delete User Data' and press send. In a response, 'Response Value' shall be set to
      0x01 (success). Note that to be able to perform this command the user must first perform
      'Consent' command (0x02).
  * if UDS is configured and populated then using CLI 'select' command switching between users
    can be done.

- The procedure to generate WSS and BCS measurements is described below:
  * build the demo for execution from qspi
  * download the demo to qspi and execute
  * scan for the device by name "Dialog Weight Scale" and connect
  * perform pairing procedure with "Dialog Weight Scale"
  * write value to 'User Control Point' in User Data Service:
    + 0x01 - 'Register New User', the consent value shall be set from range (0-9999) and then
      press send. In a response, 'Response Value' shall be set to 0x01 (success) and a new user
      index value shall be returned
    + 0x02 - 'Consent'. To be autorized as a user with id set to 0 set 'User index' value to 0
      and 'consent value' corresponding to this user (set before using 'Register New User' command),
      and then press send. In a response, 'Response Value' shall be set to 0x01 (success)
    + in WSS, enable the Weight Measurement Indication
    + in BCS, enable the Body Composition Measurement Indication
    + pressing the K1 button shall generate a WSS service indication, BCS service indications are
      emitted periodically by system timer.

- The procedures to generate CTS 'Manual Change' notification are described below:
  * build the demo for execution from qspi
  * download the demo to qspi and execute
  * scan for the device by name "Dialog Weight Scale" and connect
  * select 'Current Time Service'
  * write valid value to 'Current Time'

### Using command line

#### `user register <consent> [first_name]`

Register user in UDS database.

A user can be registered by providing consent and optionally a first name.
If the maximum number of registered users has been reached (this number is defined by `CFG_UDS_MAX_USERS`,
see section: `Suggested Configurable parameters`), the following message message will be printed on console:

~~~
All users IDs are occupied
~~~

#### `user select <index>`

Select current user available in UDS dataset. Selected user becomes invalid every time during collector's
connection and disconnection.

#### `user remove <index | all>`

Remove a user or all users from the UDS database.

If the user is correctly removed, the following message will be printed on console:

~~~
User with id 0 removed
~~~

If all users are properly removed, the following message will be printed on console:

~~~
All users removed
~~~

If one tries to remove a user which is not currently registred, the following message will be printed
on console:

~~~
User with id 0 doesn't exist
~~~

#### `user list`

List all registered users.

After issuing a `user list` command, the following message will be printed on console as follows:

~~~
Idx    Cons    First name
-------------------------------
  0    1234
  1    5678
  2    5555
~~~

If no users are registered, the following message will be printed on console:

~~~
List of users is empty
~~~

#### `user info <index>`

Display information about a user.

After issuing a `user info` command, the following message will be printed on console:

~~~
User id: 0
Consent: 123
First name: John
Age: 24
Height: 180
Gender: male
Date of birth: 01-01-1993
~~~

#### `time get`

Display current time.

After issuing a `time get` command, the following message will be printed on console:

~~~
Thursday, 01-01-1970 00:15:46
~~~

#### `time set <date> <time>`

Set current time.

This command sets the current time. Input data must be in `dd-mm-rrrr hh:mm:ss` format.

