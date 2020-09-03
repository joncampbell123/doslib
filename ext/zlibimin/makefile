
EXAMPLE = linux-host/example
MINIGZIP = linux-host/minigzip
ZLIB = linux-host/libz.a

BIN_OUT = $(MINIGZIP) $(EXAMPLE)

LIB_OUT = $(ZLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

linux-host:
	mkdir -p linux-host

ZLIB_DEPS = linux-host/adler32.o linux-host/compress.o linux-host/crc32.o linux-host/deflate.o linux-host/gzclose.o linux-host/gzlib.o linux-host/gzread.o linux-host/gzwrite.o linux-host/infback.o linux-host/inffast.o linux-host/inflate.o linux-host/inftrees.o linux-host/trees.o linux-host/uncompr.o linux-host/zutil.o

$(EXAMPLE): linux-host/example.o $(ZLIB)
	gcc -o $@ $^

$(MINIGZIP): linux-host/minigzip.o $(ZLIB)
	gcc -o $@ $^

$(ZLIB): $(ZLIB_DEPS)
	rm -f $(ZLIB)
	ar r $(ZLIB) $(ZLIB_DEPS)

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/minigzip linux-host/example linux-host/*.o linux-host/*.a

