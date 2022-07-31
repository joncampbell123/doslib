TOOLS=pngmatchpal pngquantpal pnggenpal

all: $(TOOLS)

pnggenpal: pnggenpal.c
	gcc -lpng -lm -o $@ $<

pngquantpal: pngquantpal.c
	gcc -lpng -o $@ $<

pngmatchpal: pngmatchpal.c
	gcc -lpng -o $@ $<

test: pngmatchpal
	./pngmatchpal -i test1.png -o test1.out.png -p palette.png
	./pngmatchpal -i test2.png -o test2.out.png -p palette.png
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.png
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.vga.png -vga
	./pngquantpal -i 1843513.png -o 1843513.png.quant.png -p 1843513.png.palgen.png
	./pngquantpal -i 1843513.png -o 1843513.png.quant.vga.png -p 1843513.png.palgen.vga.png
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.png
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.vga.png -vga
	./pngquantpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.quant.png -p rainbow-clipart-background3.png.palgen.png
	./pngquantpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.quant.vga.png -p rainbow-clipart-background3.png.palgen.vga.png

clean:
	rm -f $(TOOLS) test1.out.png test2.out.png *.palgen.png *.palgen.vga.png *.png.quant.png *.png.quant.vga.png
