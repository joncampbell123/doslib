@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: intel
cd intel
call buildall.bat %WHAT%
cd ..

echo All done
