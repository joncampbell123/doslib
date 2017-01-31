
MINX86DEP = ../../minx86dec/decodeall

HW_DOS_LIB = ../../hw/dos/linux-host/dos.a

DOSDASM = linux-host/dosdasm
WNEDASM = linux-host/wnedasm

BIN_OUT = $(DOSDASM) $(WNEDASM)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT) $(MINX86DEP)

lib: linux-host

linux-host:
	mkdir -p linux-host

$(MINX86DEP):
	make -C ../../minx86dec decodeall

$(HW_DOS_LIB):
	make -C ../../hw/dos

$(DOSDASM): linux-host/dosdasm.o $(MINX86DEP) $(HW_DOS_LIB)
	gcc -o $@ linux-host/dosdasm.o ../../minx86dec/string.o ../../minx86dec/coreall.o $(HW_DOS_LIB)

$(WNEDASM): linux-host/wnedasm.o $(MINX86DEP) $(HW_DOS_LIB)
	gcc -o $@ linux-host/wnedasm.o ../../minx86dec/string.o ../../minx86dec/coreall.o $(HW_DOS_LIB)

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/dosdasm linux-host/*.o linux-host/*.a
	rm -Rfv linux-host

