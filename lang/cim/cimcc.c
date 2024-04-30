
#include <stdio.h>
#include <stdlib.h>

int cimcc_yyrap(void) {
	return 1; /* no more */
}

void cimcc_yyerror(const char *s) {
	fprintf(stderr,"yyerror: %s\n",s);
	exit(1);
}

int main(int argc,char **argv) {
	return 0;
}

