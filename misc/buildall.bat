@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: hexmem1
cd hexmem1
call make.bat %WHAT%
cd ..

echo Building: td2192r
cd td2192r
call make.bat %WHAT%
cd ..

echo Building: zerofill
cd zerofill
call make.bat %WHAT%
cd ..

echo All done
