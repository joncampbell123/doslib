@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: beep1
cd beep1
call make.bat %WHAT%
cd ..

echo Building: beep2
cd beep2
call make.bat %WHAT%
cd ..

echo Building: cat
cd cat
call make.bat %WHAT%
cd ..

echo Building: covoxwav
cd covoxwav
call make.bat %WHAT%
cd ..

echo Building: echo
cd echo
call make.bat %WHAT%
cd ..

echo Building: hello
cd hello
call make.bat %WHAT%
cd ..

echo Building: hexdump
cd hexdump
call make.bat %WHAT%
cd ..

echo Building: more
cd more
call make.bat %WHAT%
cd ..

echo Building: pcspkwav
cd pcspkwav
call make.bat %WHAT%
cd ..

echo All done
