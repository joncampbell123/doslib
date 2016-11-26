# GNU makefile, Linux host
all: linux-host linux-host/omfdump

linux-host:
	mkdir -p linux-host

linux-host/omfdump: linux-host/omfcstr.o linux-host/omfctx.o linux-host/omfrec.o linux-host/omfrecs.o linux-host/olnames.o linux-host/osegdefs.o linux-host/osegdeft.o linux-host/ogrpdefs.o linux-host/oextdefs.o linux-host/oextdeft.o linux-host/opubdefs.o linux-host/opubdeft.o linux-host/omledata.o linux-host/ofixupps.o linux-host/ofixuppt.o linux-host/omfdump.o
	gcc -o $@ $^

linux-host/omfctx.o: omfctx.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omfrec.o: omfrec.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omfrecs.o: omfrecs.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omfcstr.o: omfcstr.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omfdump.o: omfdump.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/olnames.o: olnames.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/osegdefs.o: osegdefs.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/osegdeft.o: osegdeft.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/oextdefs.o: oextdefs.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/oextdeft.o: oextdeft.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/ogrpdefs.o: ogrpdefs.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/opubdefs.o: opubdefs.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/opubdeft.o: opubdeft.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/ofixupps.o: ofixupps.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/ofixuppt.o: ofixuppt.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omledata.o: omledata.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

clean:
	rm -f linux-host/omfdump linux-host/*.o

