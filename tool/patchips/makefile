
IPSDUMP = linux-host/ipsdump
IPSMAKE = linux-host/ipsmake
IPSPATCH = linux-host/ipspatch

BIN_OUT = $(IPSDUMP) $(IPSMAKE) $(IPSPATCH)

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(IPSDUMP): linux-host/ipsdump.o
	gcc -o $@ linux-host/ipsdump.o

$(IPSMAKE): linux-host/ipsmake.o
	gcc -o $@ linux-host/ipsmake.o

$(IPSPATCH): linux-host/ipspatch.o
	gcc -o $@ linux-host/ipspatch.o

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -I../../ext/zlib -I../../ext/libiconv/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/ipsdump linux-host/*.o linux-host/*.d linux-host/*.a
	rm -Rfv linux-host

-include $(wildcard linux-host/*.d)
