
MINX86DEP = ../../minx86dec/decodeall

HW_DOS_LIB = ../../hw/dos/linux-host/dos.a

DOSDASM = linux-host/dosdasm
WNEDASM = linux-host/wnedasm
WLEDASM = linux-host/wledasm

BIN_OUT = $(DOSDASM) $(WNEDASM) $(WLEDASM)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT) $(MINX86DEP)

lib: linux-host

linux-host:
	mkdir -p linux-host

# always ask minx86dec's makefile, which knows when it is out of date
$(MINX86DEP): FORCE
	$(MAKE) -C ../../minx86dec decodeall

# always ask the library's makefile, which knows when the library is out of date
$(HW_DOS_LIB): FORCE
	$(MAKE) -C ../../hw/dos

FORCE:

$(DOSDASM): linux-host/dosdasm.o $(MINX86DEP) $(HW_DOS_LIB)
	gcc -o $@ linux-host/dosdasm.o ../../minx86dec/string.o ../../minx86dec/coreall.o $(HW_DOS_LIB)

$(WNEDASM): linux-host/wnedasm.o $(MINX86DEP) $(HW_DOS_LIB)
	gcc -o $@ linux-host/wnedasm.o ../../minx86dec/string.o ../../minx86dec/coreall.o $(HW_DOS_LIB)

$(WLEDASM): linux-host/wledasm.o $(MINX86DEP) $(HW_DOS_LIB)
	gcc -o $@ linux-host/wledasm.o ../../minx86dec/string.o ../../minx86dec/coreall.o $(HW_DOS_LIB)

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/dosdasm linux-host/*.o linux-host/*.d linux-host/*.a
	rm -Rfv linux-host

-include $(wildcard linux-host/*.d)
