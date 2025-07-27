
OCTT1 = linux-host/octt1

BIN_OUT = $(OCTT1)

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(OCTT1): linux-host/octt1.o linux-host/libbmp.o
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/octt1 linux-host/*.o linux-host/*.a

