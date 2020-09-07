APPS=fontproc

all: $(APPS)

clean:
	rm -f $(APPS) arialmed_final.png

fontproc: fontproc.c
	gcc -lpng -o $@ $<

test: fontproc
	./fontproc -i arialmed_0.png -o arialmed_final.png -f arialmed.fnt

