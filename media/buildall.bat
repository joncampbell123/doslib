@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: playmp3
cd playmp3
call make.bat %WHAT%
cd ..

echo All done
