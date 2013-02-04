@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: a
cd a
call make.bat %WHAT%
cd ..

echo Building: c
cd c
call make.bat %WHAT%
cd ..

echo Building: t
cd t
call make.bat %WHAT%
cd ..

echo All done
