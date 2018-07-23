
LNKDOS16 = linux-host/lnkdos16
OMFLIB = ../../fmt/omf/linux-host/omf.a

BIN_OUT = $(LNKDOS16)

LIB_OUT = $(OMFLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

$(OMFLIB):
	make -C ../../fmt/omf

linux-host:
	mkdir -p linux-host

$(LNKDOS16): linux-host/lnkdos16.o $(OMFLIB)
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/lnkdos16 linux-host/*.o linux-host/*.a
	rm -Rf linux-host

