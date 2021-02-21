PROGS=ex1

all: $(PROGS)

ex1: ex1.c
	/usr/openrisc/bin/or1k-elf-gcc -nostdlib -nostartfiles -S -o ex1.s ex1.c
	/usr/openrisc/bin/or1k-elf-as -o ex1.o ex1.s
	/usr/openrisc/bin/or1k-elf-ld -o ex1 -M -Map=ex1.map --oformat binary --entry=startup ex1.o

clean:
	rm -f *.o *.s *.map $(PROGS)

