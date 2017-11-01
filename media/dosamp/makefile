
DOSAMP = linux-host/dosamp

BIN_OUT = $(DOSAMP)

LIB_OUT = 

# GNU makefile, Linux host
all: bin lib

bin: linux-host $(BIN_OUT)

lib: linux-host $(LIB_OUT)

linux-host:
	mkdir -p linux-host

$(DOSAMP): linux-host/dosamp.o linux-host/fsref.o linux-host/sndcard.o linux-host/tmpbuf.o linux-host/ts8254.o linux-host/tsrdtsc.o linux-host/tsrdtsc2.o linux-host/trkrbase.o linux-host/snirq.o linux-host/sc_sb.o linux-host/sc_oss.o linux-host/sc_alsa.o linux-host/fsalloc.o linux-host/fssrcfd.o linux-host/resample.o linux-host/cvrdbuf.o linux-host/cvrdbfrf.o linux-host/cvrdbfrs.o linux-host/cvrdbfrb.o linux-host/cvip168.o linux-host/cvipms16.o linux-host/cvipms.o linux-host/cvipsm8.o linux-host/cvip816.o linux-host/cvipms8.o linux-host/cvipsm16.o linux-host/cvipsm.o linux-host/tsclkmon.o linux-host/termios.o
	gcc -o $@ $^ -lrt `pkg-config alsa --libs`

linux-host/%.o : %.c
	gcc -I../.. -DLINUX -Wall -Wextra -pedantic -std=gnu99 `pkg-config alsa --cflags` -c -o $@ $^

clean:
	rm -f $(BIN_OUT) linux-host/*.o linux-host/*.a

