@echo off

deltree /Y 
del /s /q win302l\*.*
deltree /Y 
del /s /q win313l\*.*
deltree /Y 
del /s /q win32s3\*.*
deltree /Y 
del /s /q winnt\*.*
deltree /Y 
del /s /q win32\*.*
deltree /Y 
del /s /q win386\*.*
del *.obj
del *.exe
del *.lib
del *.com
del foo.gz
