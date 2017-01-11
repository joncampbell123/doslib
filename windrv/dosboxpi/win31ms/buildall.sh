#!/bin/bash
# 
# Unlike the DOS batch files, this script is much more powerful.
# In fact, this script BUILDs the DOS batch files.
# 
# NTS: Make sure your editor uses unix LF endings in the file,
#      but keeps the CR LF DOS endings for the DOS batch files.

cat >buildall.bat <<_EOF
@echo off

set WHAT=make
if "%1" == "clean" set WHAT=clean

_EOF
cat >clean.bat <<_EOF
@echo off
if exist *.map del *.map
if exist *.obj del *.obj
if exist *.sym del *.sym
if exist *.exe del *.exe
_EOF
for i in *; do if [ -d "$i" ]; then
    (cd $i && (
        cp -vu ../buildall.sh buildall.sh || exit 1

        if [ -x buildall.sh ]; then
            echo Building: $i
            ./buildall.sh $* || exit 1

            cat >>../buildall.bat <<_EOF
echo Building: $i
cd $i
call buildall.bat %WHAT%
cd ..

_EOF
        fi

        if [ -x make.sh ]; then
            ./make.sh $* || exit 1

            echo Building: $i
            cat >>../buildall.bat <<_EOF
echo Building: $i
cd $i
call make.bat %WHAT%
cd ..

_EOF
        fi)
    ) || exit 1
fi; done

cat >>buildall.bat <<_EOF
echo All done
_EOF

