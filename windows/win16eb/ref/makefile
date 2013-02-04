all: noptst.bin msgbox16.bin

noptst.bin: noptst.asm
	nasm -f bin -o $@ $<
	ndisasm $@ >$@.lst

msgbox16.bin: msgbox16.asm
	nasm -f bin -o $@ $<
	ndisasm $@ >$@.lst

clean:
	rm -f *.bin *.bin.lst

