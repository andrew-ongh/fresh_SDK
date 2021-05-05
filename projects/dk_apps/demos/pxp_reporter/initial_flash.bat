@echo OFF
SETLOCAL
SETLOCAL ENABLEEXTENSIONS
SETLOCAL ENABLEDELAYEDEXPANSION

..\..\..\..\utilities\scripts\suota\v11\initial_flash.bat -v %1\pxp_reporter.bin %2
