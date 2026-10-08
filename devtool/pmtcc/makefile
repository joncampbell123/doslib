
# GNU makefile, Linux host
all: bin lib

linux-host:
	mkdir -p linux-host

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu11 -g3 -MMD -MP -c -o $@ $<

linux-host/%.o : %.cpp
	g++ -DLINUX -Wall -Wextra -pedantic -std=gnu++11 -g3 -MMD -MP -c -o $@ $<

# keep the objects. Built through this rule, make would delete them as intermediate files, and the .d files
# would make it build them again the next time.
.PRECIOUS: linux-host/%.o

linux-host/% : linux-host/%.o
	g++ -lm -o $@ $^

clean:
	rm -f linux-host/gstcc linux-host/*.o linux-host/*.d linux-host/*.a
	rm -Rfv linux-host

GSTCC = linux-host/pmtcc

BIN_OUT = $(GSTCC)

bin: linux-host $(BIN_OUT)

lib: linux-host

-include $(wildcard linux-host/*.d)
