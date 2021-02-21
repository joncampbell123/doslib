PROGS=ex1

all: $(PROGS)

.c.o:
	/usr/openrisc/bin/or1k-elf-gcc -nostdlib -nostartfiles -c -o $@ $<

ex1: ex1.o
	/usr/openrisc/bin/or1k-elf-ld -o $@ -M -Map=$@.map --oformat binary --entry=startup $<

clean:
	rm -f *.o *.map $(PROGS)

