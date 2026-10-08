
EXPAND = linux-host/expand

MSPACK = ../../ext/libmspack/linux-host/lib/libmspack.a

BIN_OUT = $(EXPAND)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host

linux-host:
	mkdir -p linux-host

$(MSPACK):
	cd ../../ext/libmspack && ./make.sh

$(EXPAND): linux-host/expand.o $(MSPACK)
	gcc -o $@ linux-host/expand.o $(MSPACK)

# expand.c includes mspack.h, which building libmspack installs, so build that first
linux-host/expand.o: | $(MSPACK)

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -I../../ext/libmspack/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/expand linux-host/*.o linux-host/*.d linux-host/*.a
	rm -Rfv linux-host

-include $(wildcard linux-host/*.d)
