
LINKPLUS = linux-host/linkplus

BIN_OUT = $(LINKPLUS)

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

linux-host:
	mkdir -p linux-host

$(LINKPLUS): linux-host/linkplus.o
	$(CXX) -o $@ $^

linux-host/%.o : %.cpp
	$(CXX) -I../.. -DLINUX -Wall -Wextra -pedantic -std=c++11 -c -o $@ $^

clean:
	rm -f linux-host/linkplus linux-host/*.o linux-host/*.a
	rm -Rf linux-host

