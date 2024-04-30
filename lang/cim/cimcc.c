
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "cimcc.lex.h"

void cimcc_parse_int_const(char *buffer,unsigned long long *val,unsigned int *flags,int base) {
	*flags = 0;
	if (*buffer == '-') {
		*flags |= CIMCC_YYFL_NEGATE | CIMCC_YYFL_SIGNED;
		buffer++;
	}

	*val = strtoull(buffer,&buffer,base);

	if (*buffer == 'l' || *buffer == 'L') {
		*flags |= CIMCC_YYFL_LONG;
		buffer++;
		if (*buffer == 'l' || *buffer == 'L') {
			*flags |= CIMCC_YYFL_LONGLONG;
			buffer++;
		}
	}
	else if (*buffer == 'u' || *buffer == 'U') {
		*flags &= ~CIMCC_YYFL_SIGNED;
	}
}

void cimcc_parse_float_const(char *buffer,long double *val,unsigned int *flags) {
	unsigned int neg = 0;

	*flags = CIMCC_YYFL_LONG; /* default double type */
	if (*buffer == '-') {
		/* do not set NEGATE, the floating point data type has a sign bit */
		buffer++;
		neg = 1;
	}

	*val = strtold(buffer,&buffer);
	if (*buffer == 'd' || *buffer == 'D') {
		/* nothing */
		buffer++;
	}
	else if (*buffer == 'l' || *buffer == 'L') {
		*flags |= CIMCC_YYFL_LONGLONG;
		buffer++;
	}
	else if (*buffer == 'f' || *buffer == 'F') {
		*flags &= ~(CIMCC_YYFL_LONGLONG|CIMCC_YYFL_LONG);
		buffer++;
	}
}

struct my_cimcc_buf_t {
	char*		buf;
	char*		read;
	char*		fence;
	size_t		alloc;
};

static struct my_cimcc_buf_t my_cimcc_buf = {NULL,NULL,NULL,0};

void my_cimcc_buf_free(void) {
	if (my_cimcc_buf.buf) {
		free(my_cimcc_buf.buf);
		my_cimcc_buf.buf = NULL;
	}
	my_cimcc_buf.read = NULL;
	my_cimcc_buf.fence = NULL;
}

void my_cimcc_buf_rewind(void) {
	my_cimcc_buf.read = my_cimcc_buf.buf;
}

size_t my_cimcc_buf_remain(void) {
	return my_cimcc_buf.fence - my_cimcc_buf.read;
}

int my_cimcc_buf_alloc(size_t size) {
	if (my_cimcc_buf.buf != NULL) {
		if (my_cimcc_buf.alloc == size) {
			/* nothing to do */
		}
		else {
			char *p = (char*)realloc(my_cimcc_buf.buf,size+1);
			if (p == NULL) return -1;
			my_cimcc_buf.alloc = size;
			my_cimcc_buf.buf = p;
		}
	}
	else {
		char *p = (char*)malloc(size+1);
		if (p == NULL) return -1;
		my_cimcc_buf.alloc = size;
		my_cimcc_buf.buf = p;
	}

	my_cimcc_buf.fence = my_cimcc_buf.buf + my_cimcc_buf.alloc;
	*(my_cimcc_buf.fence) = 0;
	my_cimcc_buf_rewind();
	return 0;
}

void my_cimcc_read_input(char *buffer,int *size,int max_size) {
#if 0//DEBUG
	fprintf(stderr,"my_cimcc read buf=%p size=%d max_size=%d at %u/%u\n",buffer,*size,max_size,(unsigned int)(my_cimcc_buf.read-my_cimcc_buf.buf),(unsigned int)(my_cimcc_buf.fence-my_cimcc_buf.buf));
#endif
	size_t r = my_cimcc_buf_remain();
	if (max_size < 0) max_size = 0;
	if (r > (size_t)(max_size)) r = (size_t)(max_size);
	if (r != 0) {
		memcpy(buffer,my_cimcc_buf.read,r);
		my_cimcc_buf.read += r;
		assert(my_cimcc_buf.read <= my_cimcc_buf.fence);
	}
	*size = (int)r;
#if 0//DEBUG
	fprintf(stderr,"my_cimcc actually read buf=%p size=%d max_size=%d\n",buffer,*size,max_size);
#endif
}

int cimcc_yywrap(void) {
	return 1; /* no more */
}

void cimcc_yyerror(void *scanner,const char *s) {
	fprintf(stderr,"yyerror: %s\n",s);
//	exit(1);
}

int main(int argc,char **argv) {
	char *src_file = NULL;

	if (argc > 1) {
		src_file = argv[1];
	}
	else {
		fprintf(stderr,"File?\n");
		return 1;
	}

	{
		off_t fsz;
		int fd;

		fd = open(src_file,O_RDONLY);
		if (fd < 0) {
			fprintf(stderr,"Cannot open file\n");
			return 1;
		}

		fsz = lseek(fd,0,SEEK_END);
		if (fsz < 0) fsz = -1ll;
		if (lseek(fd,0,SEEK_SET) != 0) fsz = -1ll;

		if (fsz > (off_t)(64ull*1024ull*1024ull)) fsz = -1ll;

		if (fsz > (off_t)0ll) {
			if (my_cimcc_buf_alloc((size_t)fsz)) {
				fprintf(stderr,"Failed to allocate memory\n");
				fsz = 0;
			}
			if ((size_t)read(fd,my_cimcc_buf.buf,my_cimcc_buf.alloc) != my_cimcc_buf.alloc) {
				fprintf(stderr,"Failed to read file\n");
				fsz = 0;
			}
		}

		close(fd);

		if (fsz < (off_t)0) {
			fprintf(stderr,"Error\n");
			return -1;
		}
	}

	cimcc_yydebug = 1;

	cimcc_read_input = my_cimcc_read_input;
	my_cimcc_buf_rewind();

	{
		void *lex = NULL;
		cimcc_yylex_init(&lex);
		cimcc_yyparse(lex);
		cimcc_yylex_destroy(lex);
	}
	fprintf(stderr,"Read %u/%u\n",(unsigned int)(my_cimcc_buf.read-my_cimcc_buf.buf),(unsigned int)(my_cimcc_buf.fence-my_cimcc_buf.buf));

	my_cimcc_buf_free();
	return 0;
}

