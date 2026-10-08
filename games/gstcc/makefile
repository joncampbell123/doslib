
GSTCC = linux-host/gstcc
GSTDMP = linux-host/gstdmp

ICONV = ../../ext/libiconv/linux-host/lib/libiconv.a
ZLIB = ../../ext/zlib/linux-host/libz.a

BIN_OUT = $(GSTCC) $(GSTDMP)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host

linux-host:
	mkdir -p linux-host

# always ask the library's makefile, which knows when the library is out of date
$(ZLIB): FORCE
	$(MAKE) -C ../../ext/zlib

FORCE:

$(ICONV):
	cd ../../ext/libiconv && ./make.sh

$(GSTCC): linux-host/gstcc.o $(ZLIB) $(ICONV)
	gcc -o $@ linux-host/gstcc.o $(ZLIB) $(ICONV)

$(GSTDMP): linux-host/gstdmp.o $(ZLIB) $(ICONV)
	gcc -o $@ linux-host/gstdmp.o $(ZLIB) $(ICONV)

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -I../../ext/zlib -I../../ext/libiconv/linux-host/include -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/gstcc linux-host/*.o linux-host/*.d linux-host/*.a
	rm -Rfv linux-host

-include $(wildcard linux-host/*.d)
