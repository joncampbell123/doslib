PROGS=ex1.bin

all: $(PROGS)

ex1.bin: ex1.c ex1b.c
	/usr/riscv/bin/riscv32-unknown-elf-gcc -march=rv32g -mabi=ilp32 -nostdlib -nostartfiles -S -Os -o ex1b.s ex1b.c
	/usr/riscv/bin/riscv32-unknown-elf-gcc -march=rv32g -mabi=ilp32 -nostdlib -nostartfiles -S -Os -o ex1.s ex1.c
	/usr/riscv/bin/riscv32-unknown-elf-as -march=rv32g -mabi=ilp32 -o ex1b.o ex1b.s
	/usr/riscv/bin/riscv32-unknown-elf-as -march=rv32g -mabi=ilp32 -o ex1.o ex1.s
	/usr/riscv/bin/riscv32-unknown-elf-ld -o ex1.bin.elf --entry=startup ex1.o ex1b.o
	/usr/riscv/bin/riscv32-unknown-elf-objcopy -O binary ex1.bin.elf ex1.bin

clean:
	rm -f *.o *.s *.map *.bin.elf $(PROGS)

