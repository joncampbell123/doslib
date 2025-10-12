
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

void cleanup() {
	segments.clear();
	symbols.clear();
	scopes.clear();
	scope_stack.clear();
}

enum test_mode_t {
	TEST_NONE=0,
	TEST_SFO,      /* source file object */
	TEST_RBF,      /* rbuf read buffer, manual fill */
	TEST_RBFGC,    /* rbuf read buffer, getc() */
	TEST_RBFGCNU,  /* rbuf read buffer, getcnu() (unicode getc) */
	TEST_LGTOK,    /* lgtok lowest level general tokenizer */
	TEST_PPTOK,    /* pptok preprocessor token processing and macro expansion */
	TEST_LCTOK     /* lctok low level C token processing below compiler */
};

static std::string			input_file;
static enum test_mode_t			test_mode = TEST_NONE;
static bool				debug_dump = false;
bool					debug_astreduce = false;

static void help(void) {
	fprintf(stderr,"cimcc [options] [input file]\n");
	fprintf(stderr,"  --test <none|sfo|rbf|rbfgc|rbfgcnu|lgtok|pptok|lctok>         Test mode\n");
	fprintf(stderr,"  --dd                                                          debug dump\n");
	fprintf(stderr,"  --ddastreduce                                                 debug ast reduce\n");
}

static int parse_argv(int argc,char **argv) {
	unsigned int nonsw = 0;
	int i = 1; /* argv[0] is command */
	char *a;

	while (i < argc) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return -1;
			}
			else if (!strcmp(a,"dd")) {
				debug_dump = true;
			}
			else if (!strcmp(a,"ddastreduce")) {
				debug_astreduce = true;
			}
			else if (!strcmp(a,"test")) {
				a = argv[i++];
				if (a == NULL) return -1;

				if (!strcmp(a,"sfo"))
					test_mode = TEST_SFO;
				else if (!strcmp(a,"rbf"))
					test_mode = TEST_RBF;
				else if (!strcmp(a,"rbfgc"))
					test_mode = TEST_RBFGC;
				else if (!strcmp(a,"rbfgcnu"))
					test_mode = TEST_RBFGCNU;
				else if (!strcmp(a,"lgtok"))
					test_mode = TEST_LGTOK;
				else if (!strcmp(a,"pptok"))
					test_mode = TEST_PPTOK;
				else if (!strcmp(a,"lctok"))
					test_mode = TEST_LCTOK;
				else if (!strcmp(a,"none"))
					test_mode = TEST_NONE;
				else
					return -1;
			}
			else {
				fprintf(stderr,"Unknown option '%s'\n",a);
				return -1;
			}
		}
		else {
			switch (nonsw) {
				case 0:
					input_file = a;
					break;
				default:
					fprintf(stderr,"Extra parameter %s\n",a);
					return -1;
			}
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	if (parse_argv(argc,argv) < 0)
		return 1;

	if (input_file.empty()) {
		fprintf(stderr,"No input file\n");
		return 1;
	}

	{
		std::unique_ptr<source_file_object> sfo;

		if (input_file == "-") {
			struct stat st;

			if (fstat(0/*STDIN*/,&st)) {
				fprintf(stderr,"Cannot stat STDIN\n");
				return 1;
			}
			if (!(S_ISREG(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))) {
				fprintf(stderr,"Cannot use STDIN. Must be a file, socket, or pipe\n");
				return 1;
			}
			sfo.reset(new source_fd(0/*STDIN, takes ownership*/,"<stdin>"));
			assert(sfo->iface == source_file_object::IF_FD);
		}
		else {
			int fd = open(input_file.c_str(),O_RDONLY|O_BINARY);
			if (fd < 0) {
				fprintf(stderr,"Cannot open '%s', '%s'\n",input_file.c_str(),strerror(errno));
				return 1;
			}
			sfo.reset(new source_fd(fd/*takes ownership*/,input_file));
			assert(sfo->iface == source_file_object::IF_FD);
		}

		if (test_mode == TEST_SFO) {
			char tmp[512];
			ssize_t sz;

			while ((sz=sfo->read(tmp,sizeof(tmp))) > 0) {
				if (write(1/*STDOUT*/,tmp,size_t(sz)) != ssize_t(sz))
					return -1;
			}

			if (sz < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)sz);
				return -1;
			}
		}
		else if (test_mode == TEST_RBF) {
			rbuf rb;
			assert(rb.allocate(128));

			do {
				int r = rbuf_sfd_refill(rb,*sfo);
				if (r < 0) {
					fprintf(stderr,"RBUF read err from %s, %d\n",sfo->getname(),(int)r);
					return -1;
				}
				else if (r == 0/*EOF*/) {
					break;
				}

				const size_t av = rb.data_avail();
				if (av == 0) {
					fprintf(stderr,"Unexpected data end?\n");
					break;
				}

				if (write(1/*STDOUT*/,rb.data,av) != ssize_t(av)) return -1;
				rb.data += av;
				assert(rb.sanity_check());
			} while (1);
		}
		else if (test_mode == TEST_RBFGC) {
			rbuf rb;
			assert(rb.allocate());

			do {
				unicode_char_t c = getc(rb,*sfo);
				if (c == unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == unicode_invalid) {
					if (write(1/*STDOUT*/,"<INVALID>",9) != 9) return -1;
				}
				else {
					const std::string s = utf8_to_str(c);
					if (write(1/*STDOUT*/,s.data(),s.size()) != ssize_t(s.size())) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_RBFGCNU) {
			rbuf rb;
			assert(rb.allocate());

			do {
				unicode_char_t c = getcnu(rb,*sfo);
				if (c == unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == unicode_invalid) {
					abort(); /* should not happen! */
				}
				else {
					assert(c >= unicode_char_t(0l) && c <= unicode_char_t(0xFFul));
					unsigned char cc = (unsigned char)c;
					if (write(1/*STDOUT*/,&cc,1) != ssize_t(1)) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_LGTOK) {
			lgtok_state_t lst;
			token_t tok;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));
			while ((r=lgtok(lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < source_files.size()) printf(" src='%s'",source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_PPTOK) {
			lgtok_state_t lst;
			pptok_state_t pst;
			token_t tok;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));
			while ((r=pptok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < source_files.size()) printf(" src='%s'",source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_LCTOK) {
			lgtok_state_t lst;
			pptok_state_t pst;
			token_t tok;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));
			while ((r=lctok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < source_files.size()) printf(" src='%s'",source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else {
			/* normal compilation */
			cc_state_t ccst;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));

			if (!cc_init()) {
				fprintf(stderr,"Failed to init compiler\n");
				return -1;
			}

			while ((r=cc_step(ccst,rb,*sfo)) > 0);

			if (!arrange_symbols())
				fprintf(stderr,"Failed to arrange symbols\n");

			if (debug_dump) {
				debug_dump_general("");
				debug_dump_segment_table("");
				debug_dump_scope_table("");
				debug_dump_symbol_table("");
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}

		assert(sfo.get() != NULL);
	}

	cleanup();
	source_file_refcount_check();
	identifier.refcount_check();
	csliteral.refcount_check();
	ast_node.refcount_check();

	if (test_mode == TEST_SFO || test_mode == TEST_LGTOK || test_mode == TEST_RBF ||
		test_mode == TEST_RBFGC || test_mode == TEST_RBFGCNU || test_mode == TEST_LGTOK ||
		test_mode == TEST_PPTOK || test_mode == TEST_LGTOK) return 0;

	return 0;
}

