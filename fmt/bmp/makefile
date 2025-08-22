
BMPLIB = linux-host/libbmp.a

T_CPY0 = linux-host/t_cpy0
T_CPY1 = linux-host/t_cpy1

BIN_OUT = $(T_CPY0) $(T_CPY1)

LIB_OUT = $(BMPLIB)

# GNU makefile, Linux host
all: lib bin

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

BMPLIB_DEPS = linux-host/bmpread.o linux-host/bmpwrite.o linux-host/maskshft.o linux-host/strid1.o linux-host/mkbf8.o linux-host/cpy.o linux-host/c32t24.o linux-host/bmpfimg.o

$(T_CPY0): linux-host/t_cpy0.o $(BMPLIB)
	gcc -o $@ $^

$(T_CPY1): linux-host/t_cpy1.o $(BMPLIB)
	gcc -o $@ $^

$(BMPLIB): $(BMPLIB_DEPS)
	rm -f $(BMPLIB)
	ar r $(BMPLIB) $(BMPLIB_DEPS)

linux-host:
	mkdir -p linux-host

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -O0 -c -o $@ $^

clean:
	rm -f linux-host/*.o linux-host/*.a linux-host/t_cpy1
	(rmdir linux-host 2>/dev/null) || true

