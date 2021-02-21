PROGS=ex1

all: $(PROGS)

.c.o:
	/usr/openrisc/bin/or1k-elf-gcc -nostdlib -c -o $@ $<

ex1: ex1.o
	/usr/openrisc/bin/or1k-elf-ld -o $@ --oformat binary $<

clean:
	rm -f *.o $(PROGS)

