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

echo Building: floppy
cd floppy
call make.bat %WHAT%
cd ..

echo Building: ide
cd ide
call make.bat %WHAT%
cd ..

echo Building: isapnp
cd isapnp
call make.bat %WHAT%
cd ..

echo Building: llmem
cd llmem
call make.bat %WHAT%
cd ..

echo Building: mb
cd mb
call buildall.bat %WHAT%
cd ..

echo Building: parport
cd parport
call make.bat %WHAT%
cd ..

echo Building: pci
cd pci
call make.bat %WHAT%
cd ..

echo Building: pcie
cd pcie
call make.bat %WHAT%
cd ..

echo Building: rtc
cd rtc
call make.bat %WHAT%
cd ..

echo Building: smbios
cd smbios
call make.bat %WHAT%
cd ..

echo Building: sndsb
cd sndsb
call make.bat %WHAT%
cd ..

echo Building: ultrasnd
cd ultrasnd
call make.bat %WHAT%
cd ..

echo Building: usb
cd usb
call buildall.bat %WHAT%
cd ..

echo Building: vesa
cd vesa
call make.bat %WHAT%
cd ..

echo Building: vga
cd vga
call make.bat %WHAT%
cd ..

echo All done
