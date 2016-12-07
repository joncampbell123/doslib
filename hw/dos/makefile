
EXEHDMP = linux-host/exehdmp

BIN_OUT = $(EXEHDMP)

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(EXEHDMP): linux-host/exehdmp.o
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/exehdmp linux-host/*.o linux-host/*.a

