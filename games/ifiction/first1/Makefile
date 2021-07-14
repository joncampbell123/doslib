# compile and runtime dependencies:
# Linux, SDL2
#
# DOS and Windows 3.1/95 builds will use Open Watcom and DOSLIB to compile, and will use a separate
# makefile.
#
# This makefile might need to be duplicated for Mac OS X if necessary.
#
# It might be neat to make another makefile for Emscripten so that this code can also run within
# a web browser :)
#
# For compatibility with Open Watcom, C++ is limited to C++03, as much as I would like to use C++11

all: ifictsdl2

.cpp.o:
	g++ -std=gnu++03 -Wall -Wextra -pedantic -DUSE_SDL2=1 -c -o $@ $< `pkg-config --cflags sdl2`

ifictsdl2: ifict.o utils.o debug.o
	g++ -o $@ $^ `pkg-config --libs sdl2`

clean:
	rm -fv *.o

distclean: clean
	rm -fv ifictsdl2 dos4gw.exe

