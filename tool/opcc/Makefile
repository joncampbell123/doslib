all: opcc

# -MMD -MP writes a .d file of the headers it uses, so that changing a header rebuilds it
opcc: opcc.cpp
	g++ -Wall -Wextra -std=c++03 -MMD -MP -o $@ $<

clean:
	rm -f opcc opcc.d

-include $(wildcard *.d)
