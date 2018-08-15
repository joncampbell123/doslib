
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

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/patch linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

