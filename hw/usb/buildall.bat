@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: ohci
cd ohci
call make.bat %WHAT%
cd ..

echo All done
