
all: cimcc

cimcc.tab.c cimcc.tab.h: cimcc.lex.y
	bison -o cimcc.tab.c --name-prefix=cimcc_yy --graph=cimcc.tab.graph --defines=cimcc.tab.h -v -t cimcc.lex.y

cimcc.lex.yy.c: cimcc.lex.l cimcc.tab.h
	flex -o cimcc.lex.yy.c --prefix=cimcc_yy -d cimcc.lex.l

cimcc.lex.yy.o: cimcc.lex.yy.c cimcc.tab.h
	gcc -c -o cimcc.lex.yy.o cimcc.lex.yy.c

cimcc.tab.o: cimcc.tab.c cimcc.tab.h
	gcc -c -o cimcc.tab.o cimcc.tab.c

cimcc.o: cimcc.c
	gcc -c -o cimcc.o cimcc.c

cimcc: cimcc.lex.yy.o cimcc.tab.o cimcc.o 
	gcc -o cimcc cimcc.o cimcc.lex.yy.o cimcc.tab.o

clean:
	rm -fv cimcc cimcc.lex.yy.c cimcc.tab.c cimcc.tab.h cimcc.tab.graph *.o cimcc.output

