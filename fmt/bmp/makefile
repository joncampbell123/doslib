
BMPLIB = linux-host/libbmp.a

LIB_OUT = $(BMPLIB)

# GNU makefile, Linux host
all: lib

lib: linux-host $(LIB_OUT)

BMPLIB_DEPS = linux-host/bmpread.o linux-host/maskshft.o linux-host/strid1.o linux-host/mkbf8.o

$(BMPLIB): $(BMPLIB_DEPS)
	rm -f $(BMPLIB)
	ar r $(BMPLIB) $(BMPLIB_DEPS)

linux-host:
	mkdir -p linux-host

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/*.o linux-host/*.a
	(rmdir linux-host 2>/dev/null) || true

