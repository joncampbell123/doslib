
# GNU makefile, Linux host
all: bin lib

linux-host:
	mkdir -p linux-host

linux-host/%.o : %.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu11 -g3 -c -o $@ $^

linux-host/%.o : %.cpp
	g++ -DLINUX -Wall -Wextra -pedantic -std=gnu++11 -g3 -c -o $@ $^

linux-host/% : linux-host/%.o
	g++ -o $@ $^

clean:
	rm -f linux-host/gstcc linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

GSTCC = linux-host/pmtcc

BIN_OUT = $(GSTCC)

bin: linux-host $(BIN_OUT)

lib: linux-host

