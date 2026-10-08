
CFLAGS = -I../../fmt/bmp

OCTT1 = linux-host/octt1
OCTT2 = linux-host/octt2
OCTT3 = linux-host/octt3
OCTT4 = linux-host/octt4

BIN_OUT = $(OCTT1) $(OCTT2) $(OCTT3) $(OCTT4)

BMPLIB = ../../fmt/bmp/linux-host/libbmp.a

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

# always ask the library's makefile, which knows when the library is out of date
$(BMPLIB): FORCE
	$(MAKE) -C ../../fmt/bmp

FORCE:

linux-host:
	mkdir -p linux-host

$(OCTT1): linux-host/octt1.o $(BMPLIB)
	gcc -o $@ $^

$(OCTT2): linux-host/octt2.o $(BMPLIB)
	gcc -o $@ $^

$(OCTT3): linux-host/octt3.o $(BMPLIB)
	gcc -o $@ $^

$(OCTT4): linux-host/octt4.o $(BMPLIB)
	gcc -o $@ $^

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. $(CFLAGS) -DLINUX -Wall -Wextra -pedantic -std=gnu99 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/octt1 linux-host/octt2 linux-host/octt3 linux-host/octt4 linux-host/*.o linux-host/*.d linux-host/*.a
	(rmdir linux-host 2>/dev/null) || true

-include $(wildcard linux-host/*.d)
