all: linux-host linux-host/expr1

linux-host:
	mkdir -p $@

# -MMD -MP writes a .d file of the headers it uses, so that changing a header rebuilds it
linux-host/expr1: expr1.c
	gcc -DLINUX -Wall -Wextra -pedantic -MMD -MP -o $@ $<

clean:
	rm -Rf linux-host

-include $(wildcard linux-host/*.d)
