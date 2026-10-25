MCM = linux-host/multi-capture-match

BIN_OUT = $(MCM)

# FFmpeg libraries and headers, from pkg-config, and zlib for the gzip of a .prproj
FFMPEG_PKGS = libavformat libavcodec libswscale libswresample libavutil zlib
FFMPEG_CFLAGS := $(shell pkg-config --cflags $(FFMPEG_PKGS))
FFMPEG_LIBS := $(shell pkg-config --libs $(FFMPEG_PKGS))

# GNU makefile, Linux host
all: bin

bin: linux-host $(BIN_OUT)

linux-host:
	mkdir -p linux-host

$(MCM): linux-host/multi-capture-match.o linux-host/capture.o linux-host/timeline.o linux-host/project.o linux-host/match.o
	g++ -pthread -o $@ $^ $(FFMPEG_LIBS) -lm

# -MMD -MP writes a .d file of the headers each object uses, so that changing a header rebuilds them.
linux-host/%.o : %.cpp
	g++ -I../../.. -DLINUX -D__STDC_CONSTANT_MACROS $(FFMPEG_CFLAGS) -Wall -Wextra -pedantic -std=gnu++11 -O3 -g -pthread -MMD -MP -c -o $@ $<

clean:
	rm -Rf linux-host

-include $(wildcard linux-host/*.d)
