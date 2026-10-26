
DENOISE = linux-host/cassette-de-noise

BIN_OUT = $(DENOISE)

# FFmpeg libraries and headers, from pkg-config
FFMPEG_PKGS = libavformat libavcodec libswresample libavutil
FFMPEG_CFLAGS := $(shell pkg-config --cflags $(FFMPEG_PKGS))
FFMPEG_LIBS := $(shell pkg-config --libs $(FFMPEG_PKGS))

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(DENOISE): linux-host/cassette-de-noise.o linux-host/denoise.o
	g++ -fopenmp -o $@ $^ $(FFMPEG_LIBS) -lm

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them.
# OpenMP works on the channels at once.
linux-host/%.o : %.cpp
	g++ -I../../.. -DLINUX -D__STDC_CONSTANT_MACROS $(FFMPEG_CFLAGS) -Wall -Wextra -pedantic -std=gnu++11 -O3 -g -fopenmp -MMD -MP -c -o $@ $<

clean:
	rm -Rf linux-host

-include $(wildcard linux-host/*.d)
