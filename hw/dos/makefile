
EXEHDMP = linux-host/exehdmp
EXENEDMP = linux-host/exenedmp
EXENERDM = linux-host/exenerdm
EXENEEXP = linux-host/exeneexp
EXELEDMP = linux-host/exeledmp
EXEPEDMP = linux-host/exepedmp

BIN_OUT = $(EXEHDMP) $(EXENEDMP) $(EXENERDM) $(EXENEEXP) $(EXELEDMP) $(EXEPEDMP)
DOSLIB = linux-host/dos.a

LIB_OUT = $(DOSLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

DOSLIB_DEPS = linux-host/exehdr.o linux-host/exeneres.o linux-host/exenertp.o linux-host/exeneint.o linux-host/exenesrl.o linux-host/exenestb.o linux-host/exenenet.o linux-host/exenents.o linux-host/exeneent.o linux-host/exenew2x.o linux-host/exenebmp.o linux-host/exelest1.o linux-host/exeletio.o linux-host/exeleent.o linux-host/exeleobt.o linux-host/exeleopm.o linux-host/exelefpt.o linux-host/exelepar.o linux-host/exelefrt.o linux-host/exelevxd.o linux-host/exelefxp.o linux-host/exelehsz.o

linux-host:
	mkdir -p linux-host

$(DOSLIB): $(DOSLIB_DEPS)
	rm -f $(DOSLIB)
	ar r $(DOSLIB) $(DOSLIB_DEPS)

$(EXEHDMP): linux-host/exehdmp.o $(DOSLIB)
	gcc -o $@ $^

$(EXELEDMP): linux-host/exeledmp.o $(DOSLIB)
	gcc -o $@ $^

$(EXENEDMP): linux-host/exenedmp.o $(DOSLIB)
	gcc -o $@ $^

$(EXENERDM): linux-host/exenerdm.o $(DOSLIB)
	gcc -o $@ $^

$(EXENEEXP): linux-host/exeneexp.o $(DOSLIB)
	gcc -o $@ $^

$(EXEPEDMP): linux-host/exepedmp.o $(DOSLIB)
	gcc -o $@ $^

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/exehdmp linux-host/*.o linux-host/*.a

