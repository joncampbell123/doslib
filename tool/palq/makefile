
CFLAGS = -I../../fmt/bmp

OCTT1 = linux-host/octt1
OCTT2 = linux-host/octt2
OCTT3 = linux-host/octt3

BIN_OUT = $(OCTT1) $(OCTT2) $(OCTT3)

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

$(OCTT2): linux-host/octt2.o $(BMPLIB)
	gcc -o $@ $^

$(OCTT3): linux-host/octt3.o $(BMPLIB)
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. $(CFLAGS) -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/octt1 linux-host/octt2 linux-host/octt3 linux-host/*.o linux-host/*.a
	(rmdir linux-host 2>/dev/null) || true

