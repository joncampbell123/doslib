@echo off

rem shut up DOS4G/W
set DOS4G=quiet

if "%1" == "clean" call clean.bat
if "%1" == "clean" goto end
wmake -f watcom_c.mak
:end
