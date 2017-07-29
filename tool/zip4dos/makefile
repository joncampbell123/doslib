
ZIP4DOS = linux-host/zip4dos

ZLIB = ../../ext/zlib/linux-host/libz.a

BIN_OUT = $(ZIP4DOS)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host

linux-host:
	mkdir -p linux-host

$(ZLIB):
	cd ../../ext/zlib && make

$(ZIP4DOS): linux-host/zip4dos.o $(ZLIB)
	gcc -o $@ linux-host/zip4dos.o $(ZLIB)

linux-host/%.o : %.c
	gcc -I../.. -I../../ext/libmspack/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/zip4dos linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

