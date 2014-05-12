option quiet system windows  file win313l/hello.obj
option map=win313l/hello.map
option stack=4096 option heapsize=512
option stub=../stub/dos86s/stub.exe
segment TYPE CODE MOVEABLE DISCARDABLE LOADONCALL
segment TYPE DATA MOVEABLE LOADONCALL
name win313l/hello.exe
