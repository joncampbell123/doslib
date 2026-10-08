APPS=fontproc

all: $(APPS)

clean:
	rm -f $(APPS) *.d

# -MMD -MP writes a .d file of the headers it uses, so that changing a header rebuilds it
fontproc: fontproc.c
	gcc -MMD -MP -o $@ $< -lpng -lz

fonts: fontproc
	./fontproc -i arialsmall_0.png  -o arialsmall_final.png  -f arialsmall.fnt
	./fontproc -i arialmed_0.png    -o arialmed_final.png    -f arialmed.fnt
	./fontproc -i ariallarge_0.png  -o ariallarge_final.png  -f ariallarge.fnt
	./fontproc -i ariallarge2_0.png -o ariallarge2_final.png -f ariallarge2.fnt
	./fontproc -i ariallargei_0.png -o ariallargei_final.png -f ariallargei.fnt

-include $(wildcard *.d)
