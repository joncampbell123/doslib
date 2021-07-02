TOOLS=pngmatchpal pngquantpal

all: $(TOOLS)

pngquantpal: pngquantpal.c
	gcc -lpng -o $@ $<

pngmatchpal: pngmatchpal.c
	gcc -lpng -o $@ $<

test: pngmatchpal
	./pngmatchpal -i test1.png -o test1.out.png -p palette.png
	./pngmatchpal -i test2.png -o test2.out.png -p palette.png

clean:
	rm -f $(TOOLS) test1.out.png test2.out.png
