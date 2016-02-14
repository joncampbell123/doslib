@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: build-logs
cd build-logs
call buildall.bat %WHAT%
cd ..

echo Building: dosdrv
cd dosdrv
call buildall.bat %WHAT%
cd ..

echo Building: ext
cd ext
call buildall.bat %WHAT%
cd ..

