@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: dispdib
cd dispdib
call make.bat %WHAT%
cd ..

echo Building: exmenus
cd exmenus
call make.bat %WHAT%
cd ..

echo Building: hello
cd hello
call make.bat %WHAT%
cd ..

echo Building: hexmem
cd hexmem
call make.bat %WHAT%
cd ..

echo Building: ntvdm
cd ntvdm
call make.bat %WHAT%
cd ..

echo Building: w9xvmm
cd w9xvmm
call make.bat %WHAT%
cd ..

echo Building: win16eb
cd win16eb
call make.bat %WHAT%
cd ..

echo All done
