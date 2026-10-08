
PALQUANT = linux-host/palquant

BIN_OUT = $(PALQUANT)

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(PALQUANT): linux-host/palquant.o linux-host/bmpio.o linux-host/quantize.o linux-host/remap.o
	g++ -o $@ $^

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.cpp
	g++ -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu++11 -O2 -g -MMD -MP -c -o $@ $<

clean:
	rm -Rf linux-host

-include $(wildcard linux-host/*.d)
