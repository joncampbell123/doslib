TOOLS=pngmatchpal

all: $(TOOLS)

pngmatchpal: pngmatchpal.c
	gcc -lpng -o $@ $<

clean:
	rm -f $(TOOLS)
