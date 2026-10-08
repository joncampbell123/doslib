
LNKDOS16 = linux-host/lnkdos16
OMFLIB = ../../fmt/omf/linux-host/omf.a

BIN_OUT = $(LNKDOS16)

LIB_OUT = $(OMFLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

# always ask the library's makefile, which knows when the library is out of date
$(OMFLIB): FORCE
	$(MAKE) -C ../../fmt/omf

FORCE:

linux-host:
	mkdir -p linux-host

$(LNKDOS16): linux-host/lnkdos16.o $(OMFLIB)
	gcc -o $@ $^

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/lnkdos16 linux-host/*.o linux-host/*.a
	rm -Rf linux-host

-include $(wildcard linux-host/*.d)

