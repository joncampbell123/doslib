
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
	TK_NONE=0,				// 0
	TK_EOF,
	TK_ERR,
	TK_STRING,
	TK_UNKNOWN_IDENTIFIER,

	TK_CHAR,				// 5
	TK_INT,
	TK_FLOAT,
	TK_NEG,
	TK_POS,

	TK_COMMA,				// 10
	TK_SEMICOLON,
	TK_COMMENT,
	TK_EQUAL,
	TK_STAR,

	TK_FWSLASH,				// 15
	TK_PAREN_OPEN,
	TK_PAREN_CLOSE,
	TK_SQRBRKT_OPEN,
	TK_SQRBRKT_CLOSE,

	TK_OPCODE,				// 20
	TK_NAME,
	TK_DISPLAY,
	TK_DEST,
	TK_SRC,

	TK_DS,					// 25
	TK_I,
	TK_R,
	TK_W,
	TK_RW,

	TK_STACK16,				// 30
	TK_R8,
	TK_R16,
	TK_M8,
	TK_M16,

	TK_M32,					// 35
	TK_M32FP,
	TK_M64FP,
	TK_M80FP,
	TK_RI,

	TK_ST,					// 40
	TK_AX,
	TK_AL,
	TK_CL,
	TK_OPLOW3,

	TK_STIDX,				// 45
	TK_BX,
	TK_CX,
	TK_DX,
	TK_SI,

	TK_DI,					// 50
	TK_SP,
	TK_BP,
	TK_IP,
	TK_FLAGS,

	TK_CS,					// 55
	TK_ES,
	TK_SS,
	TK_BL,
	TK_DL,

	TK_AH,					// 60
	TK_BH,
	TK_CH,
	TK_DH,
	TK_ARRAYOP,/*statement parser*/

	TK_PARENOP,/*statement parser*/		// 65

	TK__MAX
};

const char *token_type_t_to_str[TK__MAX] = {
	"NONE",					// 0
	"EOF",
	"ERR",
	"STRING",
	"UNKNOWN_IDENTIFIER",

	"CHAR",					// 5
	"INT",
	"FLOAT",
	"NEG",
	"POS",

	"COMMA",				// 10
	"SEMICOLON",
	"COMMENT",
	"EQUAL",
	"STAR",

	"FWSLASH",				// 15
	"PAREN_OPEN",
	"PAREN_CLOSE",
	"SQRBRKT_OPEN",
	"SQRBRKT_CLOSE",

	"OPCODE",				// 20
	"NAME",
	"DISPLAY",
	"DEST",
	"SRC",

	"DS",					// 25
	"I",
	"R",
	"W",
	"RW",

	"STACK16",				// 30
	"R8",
	"R16",
	"M8",
	"M16",

	"M32",					// 35
	"M32FP",
	"M64FP",
	"M80FP",
	"RI",

	"ST",					// 40
	"AX",
	"AL",
	"CL",
	"OPLOW3",

	"STIDX",				// 45
	"BX",
	"CX",
	"DX",
	"SI",

	"DI",					// 50
	"SP",
	"BP",
	"IP",
	"FLAGS",

	"CS",					// 55
	"ES",
	"SS",
	"BL",
	"DL",

	"AH",					// 60
	"BH",
	"CH",
	"DH",
	"ARRAYOP",

	"PARENOP",				// 65

};

struct token_identifier_t {
	enum token_type_t	type;
	const char*		str;
};

struct token_identifier_t token_identifiers[] = {
	{TK_AH,					"ah"},
	{TK_AL,					"al"},
	{TK_AX,					"ax"},
	{TK_BH,					"bh"},
	{TK_BL,					"bl"},
	{TK_BP,					"bp"},
	{TK_BX,					"bx"},
	{TK_CH,					"ch"},
	{TK_CL,					"cl"},
	{TK_CS,					"cs"},
	{TK_CX,					"cx"},
	{TK_DH,					"dh"},
	{TK_DI,					"di"},
	{TK_DL,					"dl"},
	{TK_DX,					"dx"},
	{TK_DEST,				"dest"},
	{TK_DISPLAY,				"display"},
	{TK_DS,					"ds"},
	{TK_ES,					"es"},
	{TK_FLAGS,				"flags"},
	{TK_I,					"i"},
	{TK_IP,					"ip"},
	{TK_OPLOW3,				"oplow3"},
	{TK_M8,					"m8"},
	{TK_M16,				"m16"},
	{TK_M32,				"m32"},
	{TK_M32FP,				"m32fp"},
	{TK_M64FP,				"m64fp"},
	{TK_M80FP,				"m80fp"},
	{TK_NAME,				"name"},
	{TK_OPCODE,				"opcode"},
	{TK_R,					"r"},
	{TK_R8,					"r8"},
	{TK_R16,				"r16"},
	{TK_RI,					"ri"},
	{TK_RW,					"rw"},
	{TK_SI,					"si"},
	{TK_SP,					"sp"},
	{TK_SRC,				"src"},
	{TK_SS,					"ss"},
	{TK_ST,					"st"},
	{TK_STACK16,				"stack16"},
	{TK_STIDX,				"stidx"},
	{TK_W,					"w"},

	{TK_NONE,				NULL}
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
	vector<token_t>		subtok;

	void clear(void) {
		type.clear();
		subtok.clear();
	}
	token_t() : valf(0.0) {
		vali.ui = 0;
	}
	~token_t() {
	}

	void dumpvector(FILE *fp,vector<token_t> &tokens) {
		size_t si;

		for (si=0;si < tokens.size();si++) {
			tokens[si].dump(fp);
			if ((si+1u) < tokens.size()) fprintf(fp," ");
		}
	}

	void dump(FILE *fp) {
		assert(type.type < TK__MAX);
		fprintf(fp,"%s",token_type_t_to_str[type.type]);
		switch (type.type) {
			case TK_STRING:
			case TK_UNKNOWN_IDENTIFIER:
				fprintf(fp," \"%s\"",str.c_str());
				break;
			case TK_CHAR:
			case TK_INT:
				fprintf(fp," 0x%llx",(unsigned long long)vali.ui);
				break;
			case TK_FLOAT:
				fprintf(fp," %.20Lf",valf);
				break;
			case TK_ARRAYOP:
				fprintf(fp," [");
				dumpvector(fp,subtok);
				fprintf(fp,"]");
				break;
			case TK_PARENOP:
				fprintf(fp," (");
				dumpvector(fp,subtok);
				fprintf(fp,")");
				break;
			default:
				break;
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

	tok.clear();

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

		c = 0;
		while (token_identifiers[c].str != NULL) {
			if (tok.str == token_identifiers[c].str) {
				tok.type = token_identifiers[c].type;
				break;
			}

			c++;
		}
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

struct token_substatement_t {
	vector<token_t>			tokens;

	void dump(FILE *fp) {
		size_t si;

		for (si=0;si < tokens.size();si++) {
			tokens[si].dump(fp);
			if ((si+1u) < tokens.size()) fprintf(fp," ");
		}
	}

	void clear(void) {
		tokens.clear();
	}

	token_substatement_t() { }
	~token_substatement_t() { }
};

struct token_statement_t {
	vector<token_substatement_t> 	subst;
	bool				eof;
	bool				err;

	void clear(void) {
		eof = err = false;
		subst.clear();
	}

	void dump(FILE *fp) {
		size_t si;

		for (si=0;si < subst.size();si++) {
			subst[si].dump(fp);
			if ((si+1u) < subst.size()) fprintf(fp,",\n");
		}
		if (eof) fprintf(fp," <eof>");
		if (err) fprintf(fp," <err>");
	}

	token_statement_t() : eof(false), err(false) { }
	~token_statement_t() { }
};

bool process_source_statement_sub(token_statement_t &statement,token_substatement_t &tss,token_t &ptok,enum token_type_t end_token=TK_SEMICOLON) { /* always apppends */
	token_t tok;

	do {
		fsrctok(tok);
		if (tok == TK_EOF || tok == TK_NONE) {
			statement.eof = true;
			break;
		}
		if (tok == TK_ERR) {
			statement.err = true;
			return false;
		}

		if (tok == end_token) break; /* do not include end token */

		if (tok == TK_SQRBRKT_OPEN) {
			tok.type = TK_ARRAYOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_SQRBRKT_CLOSE)) return false;
		}
		else if (tok == TK_PAREN_OPEN) {
			tok.type = TK_PARENOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_PAREN_CLOSE)) return false;
		}

		ptok.subtok.push_back(tok);
	} while (1);

	return true;
}

bool process_source_substatement(token_substatement_t &tss,token_statement_t &statement) { /* always apppends */
	token_t tok;

	tss.clear();

	do {
		fsrctok(tok);
		if (tok == TK_EOF || tok == TK_NONE) {
			statement.eof = true;
			break;
		}
		if (tok == TK_ERR) {
			statement.err = true;
			return false;
		}

		if (tok == TK_COMMENT) continue; /* ignore */
		if (tok == TK_COMMA) break;
		if (tok == TK_SEMICOLON) return false; /* time to stop */

		if (tok == TK_SQRBRKT_OPEN) {
			tok.type = TK_ARRAYOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_SQRBRKT_CLOSE)) return false;
		}
		else if (tok == TK_PAREN_OPEN) {
			tok.type = TK_PARENOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_PAREN_CLOSE)) return false;
		}

		tss.tokens.push_back(tok);
	} while (1);

	return true;
}

bool process_source_statement(token_statement_t &statement) { /* always apppends */
	token_substatement_t tss;

	do {
		if (fsrceof()) break;

		if (!process_source_substatement(tss,statement)) {
			if (statement.err) return false;
			else break;
		}

		statement.subst.push_back(tss);
	} while (1);

	return true;
}

bool process_source_file(void) {
	token_statement_t statement;

	while (!fsrceof()) {
		statement.clear();
		if (!process_source_statement(statement)) return false;

		fprintf(stderr,"Statement: ");
		statement.dump(stderr);
		fprintf(stderr,"\n");
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

