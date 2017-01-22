
EXEHDMP = linux-host/exehdmp
EXENEDMP = linux-host/exenedmp

BIN_OUT = $(EXEHDMP) $(EXENEDMP)
DOSLIB = linux-host/dos.a

LIB_OUT = $(DOSLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

DOSLIB_DEPS = linux-host/exehdr.o linux-host/exeneres.o linux-host/exenertp.o linux-host/exeneint.o linux-host/exenesrl.o linux-host/exenestb.o linux-host/exenenet.o

linux-host:
	mkdir -p linux-host

$(DOSLIB): $(DOSLIB_DEPS)
	rm -f $(DOSLIB)
	ar r $(DOSLIB) $(DOSLIB_DEPS)

$(EXEHDMP): linux-host/exehdmp.o $(DOSLIB)
	gcc -o $@ $^

$(EXENEDMP): linux-host/exenedmp.o $(DOSLIB)
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/exehdmp linux-host/*.o linux-host/*.a

