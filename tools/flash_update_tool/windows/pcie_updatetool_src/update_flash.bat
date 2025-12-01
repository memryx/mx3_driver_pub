@echo off
setlocal
set LOGFILE=%~dp0..\..\flash_update.log

echo [Firmware Update Start] > "%LOGFILE%"
echo Time: %DATE% %TIME% >> "%LOGFILE%"
echo. >> "%LOGFILE%"

"%~dp0pcieupdateflash_win.exe" -f "%SystemRoot%\System32\drivers\cascade_4chips_flash.bin" >> "%LOGFILE%" 2>&1

set ERR=%ERRORLEVEL%
echo. >> "%LOGFILE%"
echo [Firmware Update End] >> "%LOGFILE%"
echo Exit Code: %ERR% >> "%LOGFILE%"

exit /b %ERR%