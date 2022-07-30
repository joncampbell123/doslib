TOOLS=pngmatchpal pngquantpal pnggenpal

all: $(TOOLS)

pnggenpal: pnggenpal.c
	gcc -lpng -o $@ $<

pngquantpal: pngquantpal.c
	gcc -lpng -o $@ $<

pngmatchpal: pngmatchpal.c
	gcc -lpng -o $@ $<

test: pngmatchpal
	./pngmatchpal -i test1.png -o test1.out.png -p palette.png
	./pngmatchpal -i test2.png -o test2.out.png -p palette.png
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.png
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.vga.png -vga
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.png
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.vga.png -vga

clean:
	rm -f $(TOOLS) test1.out.png test2.out.png *.palgen.png *.palgen.vga.png
