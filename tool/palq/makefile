
OCTT1 = linux-host/octt1

BIN_OUT = $(OCTT1)

BMPLIB = linux-host/libbmp.a

LIB_OUT = $(BMPLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

BMPLIB_DEPS = linux-host/libbmp.o

$(BMPLIB): $(BMPLIB_DEPS)
	rm -f $(BMPLIB)
	ar r $(BMPLIB) $(BMPLIB_DEPS)

linux-host:
	mkdir -p linux-host

$(OCTT1): linux-host/octt1.o $(BMPLIB)
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/octt1 linux-host/*.o linux-host/*.a
	rmdir linux-host

