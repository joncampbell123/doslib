# GNU makefile, Linux host
all: linux-host linux-host/omfdump

linux-host:
	mkdir -p linux-host

OMFDUMP = linux-host/omfdump

OMFLIB = linux-host/omf.a
OMFLIB_DEPS = linux-host/omfcstr.o linux-host/omfctx.o linux-host/omfrec.o linux-host/omfrecs.o linux-host/olnames.o linux-host/osegdefs.o linux-host/osegdeft.o linux-host/ogrpdefs.o linux-host/oextdefs.o linux-host/oextdeft.o linux-host/opubdefs.o linux-host/opubdeft.o linux-host/omledata.o linux-host/ofixupps.o linux-host/ofixuppt.o

$(OMFDUMP): linux-host/omfdump.o $(OMFLIB)
	gcc -o $@ $^

$(OMFLIB): $(OMFLIB_DEPS)
	rm -f $(OMFLIB)
	ar r $(OMFLIB) $(OMFLIB_DEPS)

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $^

clean:
	rm -f linux-host/omfdump linux-host/*.o linux-host/*.a

