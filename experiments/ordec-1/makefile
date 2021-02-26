PROGS=ordec

all: $(PROGS)

ordec: ordec.o
	g++ -o $@ $<

ordec.o: ordec.cpp
	g++ -Wall -Wextra -pedantic -std=c++03 -c -o $@ $<

clean:
	rm -f *.o $(PROGS)

