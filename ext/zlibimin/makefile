
ZLIB = linux-host/libz.a

LIB_OUT = $(ZLIB)

# GNU makefile, Linux host
all: bin lib

# the minimal zlib has no programs, just the library
bin: linux-host

lib: linux-host $(LIB_OUT)

linux-host:
	mkdir -p linux-host

# the same objects as common.mak
ZLIB_DEPS = linux-host/adler32.o linux-host/crc32.o linux-host/inffast.o linux-host/inflate.o linux-host/inftrees.o linux-host/trees.o linux-host/zutil.o

$(ZLIB): $(ZLIB_DEPS)
	rm -f $(ZLIB)
	ar r $(ZLIB) $(ZLIB_DEPS)

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/*.o linux-host/*.d linux-host/*.a

-include $(wildcard linux-host/*.d)
