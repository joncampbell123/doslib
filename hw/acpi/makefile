
TEST = linux-host/test

BIN_OUT = $(TEST)
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

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 -g3 -c -o $@ $^

clean:
	rm -f linux-host/test linux-host/*.o linux-host/*.a

