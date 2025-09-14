
all: yyparser
	
yyparser: c11.l.h c11.l.c c11.y.h c11.y.c

clean:
	rm -fv c11.?.h c11.?.c

c11.l.h c11.l.c: c11.l
	flex --header-file=c11.l.h -o c11.l.c c11.l

c11.y.h c11.y.c: c11.y
	bison -d -v --defines=c11.y.h -o c11.y.c c11.y

