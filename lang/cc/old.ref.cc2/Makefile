
all: ccminic

# -MMD -MP writes a .d file of the headers it uses, so that changing a header rebuilds it
ccminic: ccminic.cpp
	g++ -g3 -O0 -std=c++11 -Wall -pedantic -Wextra -MMD -MP -o $@ $<

clean:
	rm -f ccminic ccminic.d

-include $(wildcard *.d)
