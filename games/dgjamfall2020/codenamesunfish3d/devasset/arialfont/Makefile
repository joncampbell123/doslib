APPS=fontproc

all: $(APPS)

clean:
	rm -f $(APPS)

fontproc: fontproc.c
	gcc -lpng -o $@ $<

