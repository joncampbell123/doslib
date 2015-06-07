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

echo Building: hw
cd hw
call buildall.bat %WHAT%
cd ..

echo Building: mak
cd mak
call buildall.bat %WHAT%
cd ..

echo Building: media
cd media
call buildall.bat %WHAT%
cd ..

echo Building: misc
cd misc
call buildall.bat %WHAT%
cd ..

echo Building: os2
cd os2
call buildall.bat %WHAT%
cd ..

echo Building: tiny
cd tiny
call buildall.bat %WHAT%
cd ..

echo Building: tinyw16
cd tinyw16
call buildall.bat %WHAT%
cd ..

echo Building: tinyw32
cd tinyw32
call buildall.bat %WHAT%
cd ..

echo Building: tool
cd tool
call buildall.bat %WHAT%
cd ..

echo Building: vir
cd vir
call buildall.bat %WHAT%
cd ..

echo Building: windows
cd windows
call buildall.bat %WHAT%
cd ..

echo All done
