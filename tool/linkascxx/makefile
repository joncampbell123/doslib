
CXXFLAGS=-std=c++11 -Wall -Wextra

# -MMD -MP writes a .d file of the headers it uses, so that changing a header rebuilds it
linux-host/linkascxx: linkascxx.cpp
	mkdir -p linux-host
	g++ -o linux-host/linkascxx $(CXXFLAGS) -MMD -MP linkascxx.cpp

all: linux-host/linkascxx

clean:
	rm -f linux-host/linkascxx linux-host/linkascxx.d
	rmdir linux-host

-include $(wildcard linux-host/*.d)
