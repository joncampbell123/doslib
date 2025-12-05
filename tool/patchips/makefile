
IPSDUMP = linux-host/ipsdump
IPSPATCH = linux-host/ipspatch

BIN_OUT = $(IPSDUMP) $(IPSPATCH)

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(IPSDUMP): linux-host/ipsdump.o
	gcc -o $@ linux-host/ipsdump.o

$(IPSPATCH): linux-host/ipspatch.o
	gcc -o $@ linux-host/ipspatch.o

linux-host/%.o : %.c
	gcc -I../.. -I../../ext/zlib -I../../ext/libiconv/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/ipsdump linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

