
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

linux-host/%.o : %.c
	gcc -I../.. -I../../ext/libmspack/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/expand linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

