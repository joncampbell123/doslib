
CXXFLAGS=-std=c++11 -Wall -Wextra

linux-host/linkascxx: linkascxx.cpp
	mkdir -p linux-host
	g++ -o linux-host/linkascxx $(CXXFLAGS) linkascxx.cpp

all: linux-host/linkascxx

clean:
	rm -f linux-host/linkascxx
	rmdir linux-host

