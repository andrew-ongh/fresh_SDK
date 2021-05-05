Extended sleep application {#extended_sleep}
============================================

## Overview

This application demonstrates the use of the extended sleep mode by employing a ProDK board.

## Installation procedure

The project is located in the \b `projects/dk_apps/features/extended_sleep` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## Usage

The application will go to sleep using the extended sleep mode in approximately 10 seconds,
once being started. The user can monitor the power consumption using SmartSnippets toolbox.

Additionally, the user can interact with the application by pressing the K1 button, waking it up every
one second for a period of 10 seconds. Every time the application wakes up, the D2 LED blinks.

\note
It is possible for the application to also demonstrate hibernation mode by setting the `INITIAL_SLEEP_MODE`
to `pm_mode_hibernation`.

        /* Initial sleep mode */
        #ifndef INITIAL_SLEEP_MODE
        #define INITIAL_SLEEP_MODE      pm_mode_extended_sleep
        #endif
