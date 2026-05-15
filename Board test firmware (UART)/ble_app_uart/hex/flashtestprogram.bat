#echo off
REM "C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin\nrfjprog.exe" -e
"C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin\nrfjprog.exe" -f NRF52 --program testprogram.hex --chiperase
"C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin\nrfjprog.exe" -r
