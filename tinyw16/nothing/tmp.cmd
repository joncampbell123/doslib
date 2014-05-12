option quiet system windows  file win313l/hello.obj
option map=win313l/hello.map
option stub=../stub/dos86s/stub.exe
segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
segment TYPE DATA PRELOAD MOVEABLE
name win313l/hello.exe
