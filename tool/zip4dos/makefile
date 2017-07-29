
ZIP4DOS = linux-host/zip4dos

ICONV = ../../ext/libiconv/linux-host/lib/libiconv.a
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

$(ICONV):
	cd ../../ext/libiconv && ./make.sh

$(ZIP4DOS): linux-host/zip4dos.o $(ZLIB) $(ICONV)
	gcc -o $@ linux-host/zip4dos.o $(ZLIB) $(ICONV)

linux-host/%.o : %.c
	gcc -I../.. -I../../ext/zlib -I../../ext/libiconv/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/zip4dos linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

