
PATCH = linux-host/patch

BIN_OUT = $(PATCH)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host

linux-host:
	mkdir -p linux-host

$(PATCH): linux-host/patch.o
	gcc -o $@ linux-host/patch.o

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/patch linux-host/*.o linux-host/*.d linux-host/*.a
	rm -Rfv linux-host

-include $(wildcard linux-host/*.d)
