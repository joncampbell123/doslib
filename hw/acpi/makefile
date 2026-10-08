
TEST = linux-host/test
ACPIDUMP = linux-host/acpidump

BIN_OUT = $(TEST) $(ACPIDUMP)
ACPILIB = linux-host/acpi.a

LIB_OUT = $(ACPILIB)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

ACPILIB_DEPS = linux-host/acpi.o

linux-host:
	mkdir -p linux-host

$(ACPILIB): $(ACPILIB_DEPS)
	rm -f $(ACPILIB)
	ar r $(ACPILIB) $(ACPILIB_DEPS)

$(TEST): linux-host/test.o $(ACPILIB)
	gcc -o $@ $^

$(ACPIDUMP): linux-host/acpidump.o $(ACPILIB)
	gcc -o $@ $^

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -MMD -MP -c -o $@ $<

clean:
	rm -f linux-host/test linux-host/acpidump linux-host/*.o linux-host/*.d linux-host/*.a
	rmdir linux-host

-include $(wildcard linux-host/*.d)
