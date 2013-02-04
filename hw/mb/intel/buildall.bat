@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: piix3
cd piix3
call make.bat %WHAT%
cd ..

echo All done
