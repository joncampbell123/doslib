
W4TOW3 = linux-host/w4tow3

BIN_OUT = $(W4TOW3)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host

linux-host:
	mkdir -p linux-host

$(W4TOW3): linux-host/w4tow3.o
	gcc -o $@ linux-host/w4tow3.o

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/w4tow3 linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

