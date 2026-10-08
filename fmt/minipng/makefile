
TEST = linux-host/test
MINIPNGLIB = linux-host/minipng.a

BIN_OUT = $(TEST)

LIB_OUT = $(MINIPNGLIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

linux-host:
	mkdir -p linux-host

MINIPNGLIB_DEPS = linux-host/minipn48.o linux-host/minipng.o linux-host/minipnid.o linux-host/minipnph.o linux-host/minipnrb.o linux-host/minipnrw.o linux-host/minipnx8.o linux-host/miniprid.o

$(TEST): linux-host/test.o $(MINIPNGLIB)
	gcc -o $@ $^ -lz

$(MINIPNGLIB): $(MINIPNGLIB_DEPS)
	rm -f $(MINIPNGLIB)
	ar r $(MINIPNGLIB) $(MINIPNGLIB_DEPS)

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/test linux-host/*.o linux-host/*.d linux-host/*.a

-include $(wildcard linux-host/*.d)
