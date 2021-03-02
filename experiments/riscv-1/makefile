PROGS=ex1.bin

all: $(PROGS)

ex1.bin: ex1.c ex1b.c
	/usr/riscv/bin/riscv32-unknown-elf-gcc -nostdlib -nostartfiles -S -Os -o ex1b.s ex1b.c
	/usr/riscv/bin/riscv32-unknown-elf-gcc -nostdlib -nostartfiles -S -Os -o ex1.s ex1.c
	/usr/riscv/bin/riscv32-unknown-elf-as -o ex1b.o ex1b.s
	/usr/riscv/bin/riscv32-unknown-elf-as -o ex1.o ex1.s
	/usr/riscv/bin/riscv32-unknown-elf-ld -o ex1.bin -M -Map=ex1.map --oformat binary --entry=startup ex1.o ex1b.o

clean:
	rm -f *.o *.s *.map $(PROGS)

