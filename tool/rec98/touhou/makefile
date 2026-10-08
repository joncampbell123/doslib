all: linux-host linux-host/bmp2arr

linux-host:
	mkdir -p $@

linux-host/bmp2arr: linux-host/bmp2arr.o linux-host/bmp2arrl.o
	gcc -o $@ $^

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them
linux-host/%.o : %.c
	gcc -DLINUX -Wall -g3 -O0 -Wextra -pedantic -MMD -MP -c -o $@ $<

clean:
	rm -Rf linux-host

-include $(wildcard linux-host/*.d)
