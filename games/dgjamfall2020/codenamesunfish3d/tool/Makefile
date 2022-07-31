TOOLS=pngmatchpal pngquantpal pnggenpal

all: $(TOOLS)

pnggenpal: pnggenpal.c
	gcc -g3 -lpng -o $@ $<

pngquantpal: pngquantpal.c
	gcc -lpng -o $@ $<

pngmatchpal: pngmatchpal.c
	gcc -lpng -o $@ $<

test: pngmatchpal
	./pngmatchpal -i test1.png -o test1.out.png -p palette.png
	./pngmatchpal -i test2.png -o test2.out.png -p palette.png
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.png
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.vga.png -vga -nc 256
	./pnggenpal -i 1843513.png -o 1843513.png.palgen.vga16.png -vga -nc 16
	./pngquantpal -i 1843513.png -o 1843513.png.quant.png -p 1843513.png.palgen.png
	./pngquantpal -i 1843513.png -o 1843513.png.quant.vga.png -p 1843513.png.palgen.vga.png
	./pngquantpal -i 1843513.png -o 1843513.png.quant.vga16.png -p 1843513.png.palgen.vga16.png
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.png
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.vga.png -vga -nc 256
	./pnggenpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.palgen.vga16.png -vga -nc 16
	./pngquantpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.quant.png -p rainbow-clipart-background3.png.palgen.png
	./pngquantpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.quant.vga.png -p rainbow-clipart-background3.png.palgen.vga.png
	./pngquantpal -i rainbow-clipart-background3.png -o rainbow-clipart-background3.png.quant.vga16.png -p rainbow-clipart-background3.png.palgen.vga16.png
	./pnggenpal -i rgba.png -o rgba.png.palgen.png
	./pnggenpal -i rgba.png -o rgba.png.palgen.vga.png -vga -nc 256
	./pnggenpal -i rgba.png -o rgba.png.palgen.vga16.png -vga -nc 16
	./pngquantpal -i rgba.png -o rgba.png.quant.png -p rgba.png.palgen.png
	./pngquantpal -i rgba.png -o rgba.png.quant.vga.png -p rgba.png.palgen.vga.png
	./pngquantpal -i rgba.png -o rgba.png.quant.vga16.png -p rgba.png.palgen.vga16.png

clean:
	rm -f $(TOOLS) test1.out.png test2.out.png *.palgen.png *.palgen.vga.png *.png.quant.png *.png.quant.vga.png *.png.quant.vga16.png
