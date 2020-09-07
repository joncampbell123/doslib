APPS=fontproc

all: $(APPS)

clean:
	rm -f $(APPS) arialsmall_final.png arialmed_final.png ariallarge_final.png

fontproc: fontproc.c
	gcc -lpng -o $@ $<

fonts: fontproc
	./fontproc -i arialsmall_0.png -o arialsmall_final.png -f arialsmall.fnt
	./fontproc -i arialmed_0.png   -o arialmed_final.png   -f arialmed.fnt
	./fontproc -i ariallarge_0.png -o ariallarge_final.png -f ariallarge.fnt

