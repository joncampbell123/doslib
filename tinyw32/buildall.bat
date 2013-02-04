@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: hello
cd hello
call make.bat %WHAT%
cd ..

echo Building: nothing
cd nothing
call make.bat %WHAT%
cd ..

echo Building: stub
cd stub
call make.bat %WHAT%
cd ..

echo All done
