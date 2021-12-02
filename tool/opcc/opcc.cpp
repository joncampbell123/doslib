
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <string>
#include <vector>
#include <map>

using namespace std;

struct filesrcpos {
	unsigned int	line,col,col_count,token_count;

	filesrcpos() : line(1), col(1), col_count(1), token_count(0) { }

	void clear(void) {
		token_count = 0;
		col_count = 1;
		line = 1;
		col = 1;
	}
};

struct filesource {
	FILE*		fp;
	string		path;
	bool		cpudef;
	filesrcpos	srcpos;
	int		next;

	filesource() : fp(NULL), cpudef(false) { }

	~filesource() {
		close();
	}

	void close(void) {
		next = -1;
		if (fp != NULL) {
			fclose(fp);
			fp = NULL;
		}
	}

	bool open(const char *fpath) {
		if (fp != NULL)
			return false;

		if ((fp=fopen(fpath,"rb")) == NULL)
			return false;

		srcpos.clear();
		cpudef = false;
		path = fpath;
		next = -1;

		return true;
	}
	bool eof(void) {
		if (fp != NULL)
			return next < 0 && feof(fp);
		else
			return false;
	}
	int getc_next(void) {
		const int c = next;
		next = -1;
		return c;
	}
	int getc_internal(void) {
		if (next >= 0)
			return getc_next();
		else if (fp != NULL)
			return fgetc(fp);
		else
			return -1;
	}
	int getc(void) {
		int r = getc_internal();
		if (r == '\r') r = getc_internal(); /* MS-DOS style \r \n -> \n */
		if (r == '\n') {
			srcpos.line++;
			srcpos.token_count = 0;
			srcpos.col = srcpos.col_count = 1;
		}
		else if (r >= 32 || r == 9) {
			srcpos.col = srcpos.col_count++;
		}

		return r;
	}
	int peekc(void) {
		if (next < 0) next = getc();
		return next;
	}
};

#define MAX_FILE_DEPTH		64

static filesource		fsrc[MAX_FILE_DEPTH];
static int			fsrc_cur = -1;

static vector<string>		fsrc_to_process;
static string			cpudef_file;
static string			cpu_name;

enum defvar_type_t {
	DVT_NONE=0,
	DVT_STRING,
	DVT_INT
};

struct defvar {
	enum defvar_type_t	type;
	string			strval;
	int64_t			intval;

	defvar() : type(DVT_NONE), intval(0) { }
	~defvar() { }

	defvar &operator=(const std::string &v) {
		type = DVT_STRING;
		strval = v;
		return *this;
	}
	defvar &operator=(const int64_t v) {
		type = DVT_INT;
		intval = v;
		return *this;
	}
};

static map<string,defvar>       defines;

static filesource *fsrccur(void) {
	if (fsrc_cur >= 0)
		return &fsrc[fsrc_cur];

	return NULL;
}

static bool fsrc_push(const char *path) {
	if (fsrc_cur >= (MAX_FILE_DEPTH-1))
		return false;
	if (fsrc_cur < -1)
		return false;

	if (!fsrc[fsrc_cur+1].open(path))
		return false;

	fsrc_cur++;
	return true;
}

static void fsrc_pop(void) {
	if (fsrc_cur >= 0) {
		fsrc[fsrc_cur].close();
		fsrc_cur--;
	}
}

static int fsrcpeekc(void) {
	if (fsrc_cur >= 0)
		return fsrc[fsrc_cur].peekc();

	return -1;
}

static int fsrcgetc(void) {
	if (fsrc_cur >= 0)
		return fsrc[fsrc_cur].getc();

	return -1;
}

static bool fsrceof(void) {
	if (fsrc_cur >= 0)
		return fsrc[fsrc_cur].eof();

	return true;
}

static bool fsrcend(void) {
	return fsrc_cur < 0;
}

static void help(void) {
	fprintf(stderr,"opcc [options] <source file> [source file ... ]\n");
	fprintf(stderr,"x86 opcode compiler\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    -h --help                       Show this help\n");
	fprintf(stderr,"    -DNAME[=VALUE]                  Define var NAME\n");
	fprintf(stderr,"    -cpudef <path>                  CPU definition file\n");
	fprintf(stderr,"    -cpu <name>                     CPU to generate for\n");
}

bool is_string_a_number(const std::string &s,int64_t *v) {
	if (s.empty())
		return false;

	const char *sc = s.c_str();
	char *rc;

	*v = (int64_t)strtoll(sc,&rc,0);
	if (*rc == 0) return true;

	return false;
}

static bool parse_argv(int argc,char **argv) {
	char *a;
	int i=1;

	while (i < argc) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return false;
			}
			else if (!strcmp(a,"cpudef")) {
				if (i >= argc) return false;
				cpudef_file = argv[i++];
			}
			else if (!strcmp(a,"cpu")) {
				if (i >= argc) return false;
				cpu_name = argv[i++];
			}
			else if (*a == 'D') {
				/* -DNAME or -DNAME=VALUE */
				a++;

				char *namestart = a;
				while (*a != 0 && *a != '=') a++;
				char *nameend = a;

				char *valuestart,*valueend;

				if (*a == '=') {
					a++;
					valuestart = a;
					while (*a != 0) a++;
					valueend = a;
				}
				else if (*a == 0) {
					valuestart = valueend = a;
				}
				else {
					fprintf(stderr,"Unexpected char in definition string\n");
					return false;
				}

				string name = string(namestart,(size_t)(nameend - namestart));
				string value = string(valuestart,(size_t)(valueend - valuestart));
				int64_t intv = 0;

				if (is_string_a_number(value,&intv))
					defines[name] = intv;
				else
					defines[name] = value;
			}
			else {
				fprintf(stderr,"Unknown switch %s, see --help for options\n",a);
				return false;
			}
		}
		else {
			fsrc_to_process.push_back(a); /* conversion from C-string to C++ too */
		}
	}

	return true;
}

enum token_type_t {
	TK_NONE=0,
	TK_EOF,
	TK_ERR,
	TK_STRING,
	TK_UNKNOWN_IDENTIFIER,
	TK_CHAR,
	TK_INT,
	TK_FLOAT,
	TK_NEG,
	TK_POS,
	TK_COMMA,
	TK_SEMICOLON,
	TK_COMMENT,
	TK_EQUAL,
	TK_STAR,
	TK_FWSLASH,
	TK_PAREN_OPEN,
	TK_PAREN_CLOSE,
	TK_SQRBRKT_OPEN,
	TK_SQRBRKT_CLOSE,

	TK_OPCODE,
	TK_NAME,
	TK_DISPLAY,
	TK_DEST,
	TK_SRC,

	TK_DS,
	TK_R,
	TK_W,
	TK_RW,
	TK_STACK_N
};

struct token_type {
	enum token_type_t	type;

	token_type() : type(TK_NONE) { }
	token_type(const enum token_type_t &t) : type(t) { }

	token_type &operator=(const enum token_type_t &v) { type = v; return *this; }
	bool operator==(const enum token_type_t &v) { return type == v; }

	void clear(void) {
		type = TK_NONE;
	}
};

struct token_t {
	filesrcpos		srcpos;
	token_type		type;
	union {
		int64_t		si;
		uint64_t	ui;
	} vali;
	long double		valf;
	string			str;

	void clear(void) {
		type.clear();
	}
	token_t() : valf(0.0) {
		vali.ui = 0;
	}
	~token_t() {
	}

	void dump(FILE *fp) {
		switch (type.type) {
			case TK_NONE:				fprintf(fp,"TK_NONE\n"); break;
			case TK_EOF:				fprintf(fp,"TK_EOF\n"); break;
			case TK_ERR:				fprintf(fp,"TK_ERR\n"); break;
			case TK_STRING:				fprintf(fp,"TK_STRING \"%s\"\n",str.c_str()); break;
			case TK_UNKNOWN_IDENTIFIER:		fprintf(fp,"TK_UNKNOWN_IDENTIFIER \"%s\"\n",str.c_str()); break;
			case TK_CHAR:				fprintf(fp,"TK_CHAR 0x%llx\n",(unsigned long long)vali.ui); break;
			case TK_INT:				fprintf(fp,"TK_INT 0x%llx\n",(unsigned long long)vali.ui); break;
			case TK_FLOAT:				fprintf(fp,"TK_FLOAT %.20Lf\n",valf); break;
			case TK_NEG:				fprintf(fp,"TK_NEG\n"); break;
			case TK_POS:				fprintf(fp,"TK_POS\n"); break;
			case TK_COMMA:				fprintf(fp,"TK_COMMA\n"); break;
			case TK_SEMICOLON:			fprintf(fp,"TK_SEMICOLON\n"); break;
			case TK_COMMENT:			fprintf(fp,"TK_COMMENT\n"); break;
			case TK_EQUAL:				fprintf(fp,"TK_EQUAL\n"); break;
			case TK_STAR:				fprintf(fp,"TK_STAR\n"); break;
			case TK_FWSLASH:			fprintf(fp,"TK_FWSLASH\n"); break;
			case TK_PAREN_OPEN:			fprintf(fp,"TK_PAREN_OPEN\n"); break;
			case TK_PAREN_CLOSE:			fprintf(fp,"TK_PAREN_CLOSE\n"); break;
			case TK_SQRBRKT_OPEN:			fprintf(fp,"TK_SQRBRKT_OPEN\n"); break;
			case TK_SQRBRKT_CLOSE:			fprintf(fp,"TK_SQRBRKT_CLOSE\n"); break;

			case TK_OPCODE:				fprintf(fp,"TK_OPCODE\n"); break;
			case TK_NAME:				fprintf(fp,"TK_NAME\n"); break;
			case TK_DISPLAY:			fprintf(fp,"TK_DISPLAY\n"); break;
			case TK_DEST:				fprintf(fp,"TK_DEST\n"); break;
			case TK_SRC:				fprintf(fp,"TK_SRC\n"); break;

			case TK_DS:				fprintf(fp,"TK_DS\n"); break;
			case TK_R:				fprintf(fp,"TK_R\n"); break;
			case TK_W:				fprintf(fp,"TK_W\n"); break;
			case TK_RW:				fprintf(fp,"TK_RW\n"); break;
			case TK_STACK_N:			fprintf(fp,"TK_STACK%llu\n",(unsigned long long)vali.ui); break;
		}
	}

	bool operator==(const enum token_type_t &v) { return type == v; }
};

bool is_whitespace(int c) {
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

bool fsrc_skip_whitespace(token_t &tok) {
	int c;

	do {
		if ((c=fsrcpeekc()) < 0) {
			tok.srcpos = fsrccur()->srcpos;
			tok.type = fsrceof() ? TK_EOF : TK_ERR;
			return false;
		}

		if (is_whitespace(c))
			fsrcgetc();
		else
			break;
	} while (1);

	return true;
}

bool fsrc_skip_until_newline(token_t &tok) {
	int c;

	do {
		if ((c=fsrcpeekc()) < 0) {
			tok.srcpos = fsrccur()->srcpos;
			tok.type = fsrceof() ? TK_EOF : TK_ERR;
			return false;
		}

		if (c == '\n')
			break;
		else
			fsrcgetc();
	} while (1);

	return true;
}

void fsrctok(token_t &tok) {
	int c;

	if (!fsrc_skip_whitespace(tok))
		return;

	c = fsrcgetc();
	assert(c >= 0);
	fsrccur()->srcpos.token_count++;
	tok.srcpos = fsrccur()->srcpos;

	if (c == ';') {
		tok.type = TK_SEMICOLON;

		/* if this is the first token of the line, then it is a comment.
		 * NTS: Newline resets token count to zero, token count is incremented then assigned to this token, therefore first token is token_count == 1 */
		if (tok.srcpos.token_count <= 1) {
			tok.type = TK_COMMENT;
			fsrc_skip_until_newline(tok);
			return;
		}
	}
	else if (c == '-') {
		tok.type = TK_NEG;
	}
	else if (c == '+') {
		tok.type = TK_POS;
	}
	else if (c == ',') {
		tok.type = TK_COMMA;
	}
	else if (c == '=') {
		tok.type = TK_EQUAL;
	}
	else if (c == '(') {
		tok.type = TK_PAREN_OPEN;
	}
	else if (c == ')') {
		tok.type = TK_PAREN_CLOSE;
	}
	else if (c == '[') {
		tok.type = TK_SQRBRKT_OPEN;
	}
	else if (c == ']') {
		tok.type = TK_SQRBRKT_CLOSE;
	}
	else if (c == '*') {
		tok.type = TK_STAR;
	}
	else if (c == '/') {
		tok.type = TK_FWSLASH;
	}
	else if (c == '\"') {
		tok.type = TK_STRING;
		tok.str.clear();

		do {
			if ((c=fsrcgetc()) < 0) goto eof;
			if (c == '\"') break;
			tok.str += (char)c;
		} while (1);
	}
	else if (c == '\'') {
		tok.type = TK_CHAR;
		tok.vali.ui = 0;

		do {
			if ((c=fsrcgetc()) < 0) goto eof;
			if (c == '\'') break;
			tok.vali.ui <<= 8ull;
			tok.vali.ui += (unsigned int)c & 0xFFu;
		} while (1);
	}
	else if (isalpha(c) || c == '_') {
		tok.type = TK_UNKNOWN_IDENTIFIER;
		tok.str.clear();
		tok.str += (char)c;

		do {
			if ((c=fsrcpeekc()) < 0) goto tokeof;

			if (isalpha(c) || c == '_' || isdigit(c)) {
				tok.str += (char)c;
				fsrcgetc();
			}
			else {
				break;
			}
		} while (1);

		if (tok.str == "opcode")
			tok.type = TK_OPCODE;
		else if (tok.str == "name")
			tok.type = TK_NAME;
		else if (tok.str == "display")
			tok.type = TK_DISPLAY;
		else if (tok.str == "dest")
			tok.type = TK_DEST;
		else if (tok.str == "src")
			tok.type = TK_SRC;
		else if (tok.str == "ds")
			tok.type = TK_DS;
		else if (tok.str == "r")
			tok.type = TK_R;
		else if (tok.str == "w")
			tok.type = TK_W;
		else if (tok.str == "rw")
			tok.type = TK_RW;
		else if (tok.str == "stack16")
			{ tok.type = TK_STACK_N; tok.vali.ui = 16; }
	}
	else if (isdigit(c)) {
		char maxdec = '9';
		bool hex = false;

		tok.type = TK_INT;
		tok.str.clear();
		tok.str += (char)c;

		if (c == '0') {
			maxdec = '6'; // octal
			if ((c=fsrcpeekc()) < 0) goto inteof;

			if (c == 'x' || isdigit(c))
				tok.str += (char)c;
			else
				goto inteof;

			if (c == 'x') {
				fsrcgetc();//discard 'x'
				if ((c=fsrcpeekc()) < 0) goto inteof;
				maxdec = '9';
				hex = true;
			}
		}
		else {
			if ((c=fsrcpeekc()) < 0) goto inteof;
		}

		while ((c >= '0' && c <= maxdec) || (hex && isxdigit(c))) {
			tok.str += (char)c;
			fsrcgetc();
			if ((c=fsrcpeekc()) < 0) goto inteof;
		}

		if (maxdec == '9' && !hex && c == '.') {
			tok.str += (char)c;
			tok.type = TK_FLOAT;
			fsrcgetc();
			if ((c=fsrcpeekc()) < 0) goto inteof;

			while (isdigit(c)) {
				tok.str += (char)c;
				fsrcgetc();
				if ((c=fsrcpeekc()) < 0) goto inteof;
			}
		}

inteof:		if (tok.type == TK_INT)
			tok.vali.ui = strtoul(tok.str.c_str(),NULL,0);
		else
			tok.valf = strtof(tok.str.c_str(),NULL);
	}
	else {
		tok.type = TK_NONE;
	}

	return;

tokeof:	return;

eof:	tok.type = fsrceof() ? TK_EOF : TK_ERR;
	return;
}

bool process_source_file(void) {
	token_t tok;

	while (!fsrceof()) {
		fsrctok(tok);
		tok.dump(stderr);
		if (tok == TK_EOF) break;
		if (tok == TK_ERR) return false;
	}

	fsrc_pop();
	return true;
}

bool process_source_stack(void) {
	while (!fsrcend()) {
		if (!process_source_file())
			return false;
	}

	return true;
}

int main(int argc,char **argv) {
	if (!parse_argv(argc,argv))
		return 1; /* will print error message */

	if (fsrc_to_process.empty()) {
		fprintf(stderr,"No files specified\n");
		return 1;
	}

	if (!cpudef_file.empty()) {
		if (!fsrc_push(cpudef_file.c_str())) {
			fprintf(stderr,"Unable to open cpudef file\n");
			return false;
		}

		fsrccur()->cpudef = true;
		if (!process_source_stack()) {
			fprintf(stderr,"Failure parsing cpudef\n");
			return false;
		}
	}

	{
		 vector<string>::iterator si = fsrc_to_process.begin();
		 while (si != fsrc_to_process.end()) {
			 if (!fsrc_push((*si).c_str())) {
				 fprintf(stderr,"Unable to open file %s\n",(*si).c_str());
				 return false;
			 }

			 if (!process_source_stack()) {
				 fprintf(stderr,"Failure parsing source file %s\n",(*si).c_str());
				 return false;
			 }

			 si++;
		 }
	}

	return 0;
}

