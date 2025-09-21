
all: yyparser c11
	
yyparser: c11.l.h c11.l.c c11.y.h c11.y.c

# disable automatic rule
%.c: %.y

# disable automatic rule
%.c: %.l

# disable automatic rule
%.r: %.l

%.o: %.c
	gcc -std=c11 -Wall -pedantic -D_POSIX_SOURCE -c -o $@ $<

%.o: %.cpp
	g++ -std=c++11 -Wall -pedantic -D_POSIX_SOURCE -c -o $@ $<

c11: c11.o c11.l.o c11.y.o c11iconst_sz.o c11yyskip.o
	g++ -o $@ $+

clean:
	rm -fv c11.?.h c11.?.c *.y.output *.o c11

c11.l.h c11.l.c: c11.l
	flex --batch --yylineno --prefix=c11yy --header-file=c11.l.h -o c11.l.c c11.l

c11.y.h c11.y.c: c11.y
	bison -d -v --defines=c11.y.h -o c11.y.c c11.y

