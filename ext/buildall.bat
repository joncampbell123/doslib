@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

echo Building: bzip2
cd bzip2
call make.bat %WHAT%
cd ..

echo Building: faad
cd faad
call make.bat %WHAT%
cd ..

echo Building: flac
cd flac
call make.bat %WHAT%
cd ..

echo Building: jpeg
cd jpeg
call make.bat %WHAT%
cd ..

echo Building: lame
cd lame
call make.bat %WHAT%
cd ..

echo Building: libmad
cd libmad
call make.bat %WHAT%
cd ..

echo Building: libogg
cd libogg
call make.bat %WHAT%
cd ..

echo Building: speex
cd speex
call make.bat %WHAT%
cd ..

echo Building: vorbis
cd vorbis
call make.bat %WHAT%
cd ..

echo Building: vorbtool
cd vorbtool
call make.bat %WHAT%
cd ..

echo Building: zlib
cd zlib
call make.bat %WHAT%
cd ..

echo All done
