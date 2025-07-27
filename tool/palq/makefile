
CFLAGS = -I../../fmt/bmp

OCTT1 = linux-host/octt1

BIN_OUT = $(OCTT1)

BMPLIB = ../../fmt/bmp/linux-host/libbmp.a

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

$(BMPLIB):
	make -C ../../fmt/bmp

linux-host:
	mkdir -p linux-host

$(OCTT1): linux-host/octt1.o $(BMPLIB)
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. $(CFLAGS) -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/octt1 linux-host/*.o linux-host/*.a
	(rmdir linux-host 2>/dev/null) || true

