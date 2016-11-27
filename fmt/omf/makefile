
OMFSEGDG = linux-host/omfsegdg
OMFDUMP = linux-host/omfdump
OMFLIB = linux-host/omf.a

BIN_OUT = $(OMFDUMP) $(OMFSEGDG)

LIB_OUT = $(OMFLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

linux-host:
	mkdir -p linux-host

OMFLIB_DEPS = linux-host/omfcstr.o linux-host/omfctx.o linux-host/omfrec.o linux-host/omfrecs.o linux-host/olnames.o linux-host/osegdefs.o linux-host/osegdeft.o linux-host/ogrpdefs.o linux-host/oextdefs.o linux-host/oextdeft.o linux-host/opubdefs.o linux-host/opubdeft.o linux-host/omledata.o linux-host/ofixupps.o linux-host/ofixuppt.o linux-host/opledata.o linux-host/omfctxnm.o linux-host/omfctxrf.o linux-host/omfctxlf.o linux-host/optheadr.o linux-host/opextdef.o linux-host/opfixupp.o linux-host/opgrpdef.o linux-host/oppubdef.o linux-host/opsegdef.o linux-host/oplnames.o linux-host/odlnames.o linux-host/odextdef.o linux-host/odfixupp.o linux-host/odgrpdef.o linux-host/odledata.o linux-host/odlidata.o linux-host/odpubdef.o linux-host/odsegdef.o linux-host/odtheadr.o linux-host/omfctxwf.o linux-host/omfrecw.o

$(OMFSEGDG): linux-host/omfsegdg.o $(OMFLIB)
	gcc -o $@ $^

$(OMFDUMP): linux-host/omfdump.o $(OMFLIB)
	gcc -o $@ $^

$(OMFLIB): $(OMFLIB_DEPS)
	rm -f $(OMFLIB)
	ar r $(OMFLIB) $(OMFLIB_DEPS)

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/omfdump linux-host/*.o linux-host/*.a

