@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: 8042
cd 8042
call make.bat %WHAT%
cd ..

echo Building: 8237
cd 8237
call make.bat %WHAT%
cd ..

echo Building: 8250
cd 8250
call make.bat %WHAT%
cd ..

echo Building: 8254
cd 8254
call make.bat %WHAT%
cd ..

echo Building: 8259
cd 8259
call make.bat %WHAT%
cd ..

echo Building: acpi
cd acpi
call make.bat %WHAT%
cd ..

echo Building: adlib
cd adlib
call make.bat %WHAT%
cd ..

echo Building: apm
cd apm
call make.bat %WHAT%
cd ..

echo Building: biosdisk
cd biosdisk
call make.bat %WHAT%
cd ..

echo Building: cpu
cd cpu
call make.bat %WHAT%
cd ..

echo Building: dos
cd dos
call make.bat %WHAT%
cd ..

echo Building: flatreal
cd flatreal
call make.bat %WHAT%
cd ..

