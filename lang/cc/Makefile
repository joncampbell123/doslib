
all: ccminic

ccminic: ccminic.o cctoken.o ccsfo.o ccsfo_fd.o ccsft.o ccddipmergeable.o ccrbufcslit.o cccslit.o ccsourcefiles.o
	g++ -std=c++11 -o $@ $+

%.o: %.cpp
	g++ -g3 -O0 -std=c++11 -Wall -pedantic -Wextra -c -o $@ $+

clean:
	rm -f ccminic *.o

