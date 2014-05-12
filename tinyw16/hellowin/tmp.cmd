option quiet system windows  file win313l/hello.obj
option map=win313l/hello.map
option stub=../stub/dos86s/stub.exe
option stack=4096 option heapsize=1024
EXPORT myWndProc.1
segment TYPE CODE MOVEABLE DISCARDABLE LOADONCALL
segment TYPE DATA MOVEABLE LOADONCALL
name win313l/hello.exe
