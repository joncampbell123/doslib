
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <algorithm>
#include <string>
#include <vector>
#include <map>

using namespace std;

struct filesrcpos {
	unsigned int	line,col,col_count,token_count;
	bool		init;

	filesrcpos() : line(1), col(1), col_count(1), token_count(0), init(false) { }

	void clear(void) {
		token_count = 0;
		col_count = 1;
		init = false;
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
		srcpos.init = false;
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

		srcpos.init = true;
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
	int getc_filter(void) {
		int r = getc_internal();
		if (r == '\r') r = getc_internal(); /* MS-DOS style \r \n -> \n */
		return r;
	}
	int getc(void) {
		int r = getc_filter();
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
		if (next < 0) next = getc_filter();
		return next;
	}
};

#define MAX_FILE_DEPTH		64

static filesource		fsrc[MAX_FILE_DEPTH];
static int			fsrc_cur = -1;

static vector<string>		fsrc_to_process;
static string			cpudef_file;
static string			cpu_name;

static bool			dbg_tok = false;

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
	const char *rc = sc;

	*v = (int64_t)strtoll(sc,(char**)(&rc),0);
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
			else if (!strcmp(a,"dbg-tok")) {
				dbg_tok = true;
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
	TK_COLON,
	TK_M,
	TK_UNKNOWNCHAR,
	TK_INCLUDE,

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
	"COLON",
	"M",
	"UNKNOWNCHAR",
	"INCLUDE"

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
	{TK_INCLUDE,				"include"},
	{TK_IP,					"ip"},
	{TK_OPLOW3,				"oplow3"},
	{TK_M,					"m"},
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
	bool iserr(void) const {
		return	(type == TK_ERR) ||
			(type == TK_UNKNOWN_IDENTIFIER) ||
			(type == TK_UNKNOWNCHAR);
	}
};

struct token_t {
	filesrcpos			srcpos;
	token_type			type;
	union {
		int64_t			si;
		uint64_t		ui;
	} vali;
	long double			valf;
	string				str;
	vector< vector<token_t>	>	subtok;

	void clear(void) {
		srcpos.clear();
		subtok.clear();
		type.clear();
		vali.ui = 0;
		valf = 0;
	}
	bool iserr(void) const {
		return type.iserr();
	}
	token_t() : valf(0.0) {
		vali.ui = 0;
	}
	~token_t() {
	}

	string errmsg(void) const {
		switch (type.type) {
			case TK_ERR:
				return "General or I/O error";
			case TK_UNKNOWN_IDENTIFIER:
				return string("Unknown identifier \"")+str+"\"";
			case TK_UNKNOWNCHAR:
				return string("Unknown character '")+(char)vali.ui+"\'";
			default:
				break;
		};

		return "No error message available";
	}

	void dumpvector(FILE *fp,vector<token_t> &tokens) {
		size_t si;

		for (si=0;si < tokens.size();si++) {
			tokens[si].dump(fp);
			if ((si+1u) < tokens.size()) fprintf(fp," ");
		}
	}

	void dumpvvector(FILE *fp,vector< vector<token_t> > &tokens) {
		size_t si;

		for (si=0;si < tokens.size();si++) {
			dumpvector(fp,tokens[si]);
			if ((si+1u) < tokens.size()) fprintf(fp,", ");
		}
	}

	void dump(FILE *fp) {
		assert(type.type < TK__MAX);

		switch (type.type) {
			case TK_STRING:
				fprintf(fp,"STR(\"%s\")",str.c_str());
				break;
			case TK_UNKNOWN_IDENTIFIER:
				fprintf(fp,"UNKNOWNID(\"%s\")",str.c_str());
				break;
			case TK_CHAR:
				fprintf(fp,"CHAR(0x%llx)",(unsigned long long)vali.ui);
				break;
			case TK_INT:
				fprintf(fp,"INT(0x%llx)",(unsigned long long)vali.ui);
				break;
			case TK_FLOAT:
				fprintf(fp,"FLOAT(%.20Lf)",valf);
				break;
			case TK_ARRAYOP:
				fprintf(fp,"[");
				dumpvvector(fp,subtok);
				fprintf(fp,"]");
				break;
			case TK_PARENOP:
				fprintf(fp,"(");
				dumpvvector(fp,subtok);
				fprintf(fp,")");
				break;
			default:
				fprintf(fp,"%s",token_type_t_to_str[type.type]);
				break;
		}
	}

	bool operator==(const enum token_type_t &v) { return type == v; }
};

bool is_whitespace(int c) {
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

bool fsrc_skip_whitespace(token_t &tok,filesource *fsrc) {
	int c;

	do {
		if ((c=fsrc->peekc()) < 0) {
			tok.srcpos = fsrc->srcpos;
			tok.type = fsrc->eof() ? TK_EOF : TK_ERR;
			return false;
		}

		if (is_whitespace(c))
			fsrc->getc();
		else
			break;
	} while (1);

	return true;
}

bool fsrc_skip_until_newline(token_t &tok,filesource *fsrc) {
	int c;

	do {
		if ((c=fsrc->peekc()) < 0) {
			tok.srcpos = fsrc->srcpos;
			tok.type = fsrc->eof() ? TK_EOF : TK_ERR;
			return false;
		}

		if (c == '\n')
			break;
		else
			fsrc->getc();
	} while (1);

	return true;
}

void fsrctok(token_t &tok,filesource *fsrc) {
	int c;

	tok.clear();

	if (!fsrc_skip_whitespace(tok,fsrc))
		return;

	c = fsrc->getc();
	assert(c >= 0);
	fsrc->srcpos.token_count++;
	tok.srcpos = fsrc->srcpos;

	if (c == ';') {
		tok.type = TK_SEMICOLON;

		/* if this is the first token of the line, then it is a comment.
		 * NTS: Newline resets token count to zero, token count is incremented then assigned to this token, therefore first token is token_count == 1 */
		if (tok.srcpos.token_count <= 1) {
			tok.type = TK_COMMENT;
			fsrc_skip_until_newline(tok,fsrc);
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
	else if (c == ':') {
		tok.type = TK_COLON;
	}
	else if (c == '\"') {
		tok.type = TK_STRING;
		tok.str.clear();

		do {
			if ((c=fsrc->getc()) < 0) goto eof;
			if (c == '\"') break;
			tok.str += (char)c;
		} while (1);
	}
	else if (c == '\'') {
		tok.type = TK_CHAR;
		tok.vali.ui = 0;

		do {
			if ((c=fsrc->getc()) < 0) goto eof;
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
			if ((c=fsrc->peekc()) < 0) goto tokeof;

			if (isalpha(c) || c == '_' || isdigit(c)) {
				tok.str += (char)c;
				fsrc->getc();
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
			if ((c=fsrc->peekc()) < 0) goto inteof;

			if (c == 'x' || isdigit(c))
				tok.str += (char)c;
			else
				goto inteof;

			if (c == 'x') {
				fsrc->getc();//discard 'x'
				if ((c=fsrc->peekc()) < 0) goto inteof;
				maxdec = '9';
				hex = true;
			}
		}
		else {
			if ((c=fsrc->peekc()) < 0) goto inteof;
		}

		while ((c >= '0' && c <= maxdec) || (hex && isxdigit(c))) {
			tok.str += (char)c;
			fsrc->getc();
			if ((c=fsrc->peekc()) < 0) goto inteof;
		}

		if (maxdec == '9' && !hex && c == '.') {
			tok.str += (char)c;
			tok.type = TK_FLOAT;
			fsrc->getc();
			if ((c=fsrc->peekc()) < 0) goto inteof;

			while (isdigit(c)) {
				tok.str += (char)c;
				fsrc->getc();
				if ((c=fsrc->peekc()) < 0) goto inteof;
			}
		}

inteof:		if (tok.type == TK_INT)
			tok.vali.ui = strtoul(tok.str.c_str(),NULL,0);
		else
			tok.valf = strtof(tok.str.c_str(),NULL);
	}
	else {
		tok.type = TK_UNKNOWNCHAR;
		tok.vali.si = c;
	}

	return;

tokeof:	return;

eof:	tok.type = fsrc->eof() ? TK_EOF : TK_ERR;
	return;
}

struct token_substatement_t {
	vector<token_t>			tokens;
	filesrcpos			srcpos;
	bool				eom;

	void dump(FILE *fp) {
		size_t si;

		for (si=0;si < tokens.size();si++) {
			tokens[si].dump(fp);
			if ((si+1u) < tokens.size()) fprintf(fp," ");
		}
	}

	void clear(void) {
		tokens.clear();
		srcpos.clear();
		eom = false;
	}

	token_substatement_t() : eom(false) { }
	~token_substatement_t() { }
};

struct token_statement_t {
	vector<token_substatement_t> 	subst;
	filesrcpos			srcpos;
	token_t				errtok;
	bool				eof;
	bool				err;

	void clear(void) {
		eof = err = false;
		srcpos.clear();
		errtok.clear();
		subst.clear();
	}

	void dump(FILE *fp) {
		size_t si;

		for (si=0;si < subst.size();si++) {
			subst[si].dump(fp);
			if ((si+1u) < subst.size()) fprintf(fp,", ");
		}
		if (eof) fprintf(fp," <eof>");
		if (err) fprintf(fp," <err>");
	}

	token_statement_t() : eof(false), err(false) { }
	~token_statement_t() { }
};

bool process_source_statement_sub(token_statement_t &statement,token_substatement_t &tss,token_t &ptok,enum token_type_t end_token,filesource *fsrc) { /* always apppends */
	token_t tok;

	ptok.subtok.clear();
	ptok.subtok.resize(1);

	do {
		fsrctok(tok,fsrc);
		if (tok == TK_EOF) {
			statement.eof = true;
			break;
		}
		if (tok.iserr()) {
			statement.errtok = tok;
			statement.err = true;
			return false;
		}

		if (tok == TK_COMMENT) continue; /* ignore */

		if (tok == end_token) break; /* do not include end token */

		if (tok == TK_COMMA) {
			ptok.subtok.push_back(vector<token_t>());
			continue;
		}
		else if (tok == TK_SQRBRKT_OPEN) {
			tok.type = TK_ARRAYOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_SQRBRKT_CLOSE,fsrc)) return false;
		}
		else if (tok == TK_PAREN_OPEN) {
			tok.type = TK_PARENOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_PAREN_CLOSE,fsrc)) return false;
		}

		ptok.subtok[ptok.subtok.size()-1u].push_back(tok);
	} while (1);

	/* if only one element, no comma was encountered.
	 * if that one element is empty, then it should be removed.
	 * without this "[]" will have one element of nothing.
	 * [token] should result in subtok.size() == 1 and subtok[0].size() == 1.
	 * [,] should result in subtok.size() == 2 and subtok[0].size() == 0, subtok[1].size() == 0 */
	if (ptok.subtok.size() == 1 && ptok.subtok[0].empty())
		ptok.subtok.pop_back();

	return true;
}

bool process_source_substatement(token_substatement_t &tss,token_statement_t &statement,filesource *fsrc) { /* always apppends */
	int count = 0;
	token_t tok;

	tss.srcpos = fsrc->srcpos;

	do {
		fsrctok(tok,fsrc);
		if (tok == TK_EOF) {
			statement.eof = true;
			break;
		}
		if (tok.iserr()) {
			statement.errtok = tok;
			statement.err = true;
			return false;
		}

		if (tok == TK_COMMENT) continue; /* ignore */

		if ((count++) == 0)
			tss.srcpos = tok.srcpos;

		if (tok == TK_COMMA) break; /* time to stop, end of substatement */

		if (tok == TK_SEMICOLON) {
			tss.eom = true;
			break; /* time to stop, end of statement */
		}
		else if (tok == TK_SQRBRKT_OPEN) {
			tok.type = TK_ARRAYOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_SQRBRKT_CLOSE,fsrc)) return false;
		}
		else if (tok == TK_PAREN_OPEN) {
			tok.type = TK_PARENOP;
			if (!process_source_statement_sub(statement,tss,tok,TK_PAREN_CLOSE,fsrc)) return false;
		}

		tss.tokens.push_back(tok);
	} while (1);

	return true;
}

bool process_source_statement(token_statement_t &statement,filesource *fsrc) { /* always apppends */
	token_substatement_t tss;
	int count = 0;

	statement.srcpos = fsrc->srcpos;
	while (!fsrc->eof()) {
		tss.clear();
		if (!process_source_substatement(tss,statement,fsrc)) return false;
		statement.subst.push_back(tss);

		if ((count++) == 0)
			statement.srcpos = tss.srcpos;

		if (tss.eom) break;
	}

	if (statement.subst.size() == 1 && statement.subst[0].tokens.empty())
		statement.subst.pop_back();

	return true;
}

void emit_dump_statement(token_statement_t &statement) {
	fprintf(stderr,"  -> ");
	statement.dump(stderr);
	fprintf(stderr,"\n");
}

void emit_error(token_statement_t &statement,filesource *fsrc,const char *fmt,...) {
	fprintf(stderr,"Error in statement in %s, line %u, col %u: ",
		fsrc->path.c_str(),statement.errtok.srcpos.line,statement.errtok.srcpos.col);

	{
		va_list va;

		va_start(va,fmt);
		vfprintf(stderr,fmt,va);
		va_end(va);
	}

	fprintf(stderr,"\n");

	emit_dump_statement(statement);
}

bool process_source_file(filesource *fsrc);

struct opcode_byte {
	vector<uint8_t>		val;		/* to support ranges */

	opcode_byte() {
	}

	~opcode_byte() {
	}

	void sort(void) {
		std::sort(val.begin(),val.end());
	}

	bool has_duplicates(void) { /* you must sort first */
		vector<uint8_t>::iterator i = val.begin();
		uint8_t pv;

		if (i != val.end()) {
			pv = *i;
			i++;
			while (i != val.end()) {
				if (pv == *i) return true;
				pv = *i;
				i++;
			}
		}

		return false;
	}
};

struct opcode_sequence {
	vector<opcode_byte>	seq;

	opcode_sequence() {
	}

	~opcode_sequence() {
	}

	void add(const uint8_t v) {
		const size_t idx = seq.size();
		seq.resize(idx+1u);
		assert(seq[idx].val.empty());
		seq[idx].val.push_back(v);
	}

	opcode_byte &add_seq(void) {
		const size_t idx = seq.size();
		seq.resize(idx+1u);
		assert(seq[idx].val.empty());
		return seq[idx];
	}
};

enum {
	PF_NONE=0,
	PF_REPNE,
	PF_REPE,
	PF_LOCK,
	PF_SEG_OVERRIDE
};

struct opcode_st {
	string			name;
	opcode_sequence		opcode_seq;
	bool			mod_reg_rm;
	int8_t			match_reg; /* mod/reg/rm match reg i.e. opcode 0xFE /2 */
	uint8_t			prefix;
	int8_t			seg_override;

	opcode_st() : mod_reg_rm(false), match_reg(-1), prefix(PF_NONE), seg_override(-1) {
	}
	~opcode_st() {
	}
};

bool validate_range_regfield(uint64_t i) {
	return i < (uint64_t)8u;
}

bool validate_range_uint8(uint64_t i) {
	return i < (uint64_t)256u;
}

bool process_statement_opcode_OPCODE_singleval(opcode_st &opcode,vector<token_t>::iterator toki,vector<token_t>::iterator toki_end,token_statement_t &statement,filesource *fsrc) {
	assert(toki != toki_end);

	if (!validate_range_uint8(toki->vali.ui)) {
		emit_error(statement,fsrc,"Invalid opcode byte value");
		return false;
	}

	opcode.opcode_seq.add((uint8_t)toki->vali.ui);
	return true;
}

bool process_statement_opcode_OPCODE_multivalarray_statement_oneval(opcode_byte &b,opcode_st &opcode,vector<token_t>::iterator stli,vector<token_t>::iterator stli_end,token_statement_t &statement,filesource *fsrc) {
	assert(stli != stli_end);

	(void)opcode;

	if (!validate_range_uint8(stli->vali.ui)) {
		emit_error(statement,fsrc,"Invalid opcode byte value");
		return false;
	}

	b.val.push_back((uint8_t)stli->vali.ui);
	return true;
}

bool process_statement_opcode_OPCODE_multivalarray_statement_rangeval(opcode_byte &b,opcode_st &opcode,vector<token_t>::iterator stli,vector<token_t>::iterator stli_end,token_statement_t &statement,filesource *fsrc) {
	assert((stli+2) < stli_end);

	(void)opcode;

	if (!validate_range_uint8(stli[0].vali.ui) || !validate_range_uint8(stli[2].vali.ui) || stli[0].vali.ui > stli[2].vali.ui) {
		emit_error(statement,fsrc,"Invalid opcode byte values");
		return false;
	}

	unsigned int i;

	for (i=(unsigned int)stli[0].vali.ui;i <= (unsigned int)stli[2].vali.ui;i++)
		b.val.push_back((uint8_t)i);

	return true;
}

bool process_statement_opcode_OPCODE_multivalarray_statement(opcode_byte &b,opcode_st &opcode,vector< vector<token_t> >::iterator tli,vector< vector<token_t> >::iterator tli_end,token_statement_t &statement,filesource *fsrc) {
	assert(tli != tli_end);

	(void)opcode;

	vector<token_t>::iterator stli = (*tli).begin();
	while (stli != (*tli).end()) {
		if ((stli+2) < (*tli).end() && (*stli) == TK_INT && stli[1] == TK_NEG && stli[2] == TK_INT) { /* INT-INT range inclusive */
			if (!process_statement_opcode_OPCODE_multivalarray_statement_rangeval(b,opcode,stli,(*tli).end(),statement,fsrc))
				return false;

			stli += 3;
		}
		else if ((*stli) == TK_INT) { /* INT single value */
			if (!process_statement_opcode_OPCODE_multivalarray_statement_oneval(b,opcode,stli,(*tli).end(),statement,fsrc))
				return false;

			stli++;
		}
		else {
			emit_error(statement,fsrc,"Unexpected token in opcode byte sequence");
			return false;
		}
	}

	return true;
}

bool process_statement_opcode_OPCODE_multivalarray(opcode_st &opcode,vector<token_t>::iterator toki,vector<token_t>::iterator toki_end,token_statement_t &statement,filesource *fsrc) {
	assert(toki != toki_end);

	opcode_byte &b = opcode.opcode_seq.add_seq();
	vector< vector<token_t> >::iterator tli = toki->subtok.begin();

	while (tli != toki->subtok.end()) {
		if (!process_statement_opcode_OPCODE_multivalarray_statement(b,opcode,tli,toki->subtok.end(),statement,fsrc))
			return false;

		tli++;
	}

	b.sort();
	if (b.has_duplicates()) {
		emit_error(statement,fsrc,"Duplicate value in opcode range or multiple values");
		return false;
	}

	return true;
}

bool process_statement_opcode_OPCODE_slashreg(opcode_st &opcode,vector<token_t>::iterator toki,vector<token_t>::iterator toki_end,token_statement_t &statement,filesource *fsrc) {
	assert((toki+1) < toki_end);

	if (!validate_range_regfield(toki[1].vali.ui)) {
		emit_error(statement,fsrc,"Invalid reg field in /N syntax");
		return false;
	}

	/* this means there is a mod/reg/rm field */
	opcode.match_reg = (int8_t)(toki[1].vali.ui);
	opcode.mod_reg_rm = true;
	return true;
}

bool process_statement_opcode_OPCODE(opcode_st &opcode,vector<token_t>::iterator &toki,vector<token_t>::iterator toki_end,token_statement_t &statement,filesource *fsrc) {
	/* toki points just after OPCODE */
	while (toki != toki_end) {
		if ((*toki) == TK_INT) { // 1 token match
			if (!process_statement_opcode_OPCODE_singleval(opcode,toki,toki_end,statement,fsrc))
				return false;

			toki++;
		}
		else if ((*toki) == TK_ARRAYOP) { // 1 token match
			if (!process_statement_opcode_OPCODE_multivalarray(opcode,toki,toki_end,statement,fsrc))
				return false;

			toki++;
		}
		else if ((toki+1) < toki_end && (*toki) == TK_FWSLASH && toki[1] == TK_INT) { /* /0, /2, etc. syntax meaning opcode defined by mod/reg/rm */
			if (!process_statement_opcode_OPCODE_slashreg(opcode,toki,toki_end,statement,fsrc))
				return false;

			toki += 2;
		}
		else {
			emit_error(statement,fsrc,"Unexpected token in opcode");
			return false;
		}
	}

	return true;
}

bool process_statement_opcode_NAME(opcode_st &opcode,vector<token_t>::iterator &toki,vector<token_t>::iterator toki_end,token_statement_t &statement,filesource *fsrc) {
	/* toki already points past TK_NAME */
	if (toki == toki_end) {
		emit_error(statement,fsrc,"Opcode name without any other tokens");
		return false;
	}

	if (*toki == TK_STRING) {
		if (opcode.name.empty() || opcode.name == (*toki).str) {
			opcode.name = (*toki).str;
		}
		else {
			emit_error(statement,fsrc,"Opcode name already defined");
			return false;
		}
	}
	else {
		emit_error(statement,fsrc,"Unexpected token after NAME");
		return false;
	}

	return true;
}

void debug_dump_opcode_st_seq_vals(vector<uint8_t> &val) {
	vector<uint8_t>::iterator obi = val.begin();
	while (obi != val.end()) {
		fprintf(stderr,"%02x",*obi);

		obi++;
		if (obi != val.end()) fprintf(stderr,",");
	}
}

void debug_dump_opcode_st_seq(vector<opcode_byte> &seq) {
	vector<opcode_byte>::iterator bi = seq.begin();
	while (bi != seq.end()) {
		fprintf(stderr,"[");

		debug_dump_opcode_st_seq_vals((*bi).val);
		bi++;

		fprintf(stderr,"]");
		if (bi != seq.end()) fprintf(stderr,",");
	}
}

void debug_dump_opcode_st(opcode_st &opcode) {
	fprintf(stderr,"Opcode '%s':\n",opcode.name.c_str());

	fprintf(stderr,"  Sequence:    ");
	debug_dump_opcode_st_seq(opcode.opcode_seq.seq);

	if (opcode.mod_reg_rm) {
		fprintf(stderr," m/r/m");

		if (opcode.match_reg >= 0)
			fprintf(stderr," /%d ",opcode.match_reg);
	}

	fprintf(stderr,"\n");
}

bool process_statement_opcode(token_statement_t &statement,filesource *fsrc) {
	assert(statement.subst.size() > 0);
	vector<token_substatement_t>::iterator subsi = statement.subst.begin();
	token_substatement_t &first = *subsi;
	subsi++;

	vector<token_t>::iterator toki = first.tokens.begin();
	assert(toki != first.tokens.end());
	assert((*toki) == TK_OPCODE);
	toki++;

	opcode_st opcode;

	/* OPCODE <pattern>, name <string>, display [ ... ], ... */
	if (!process_statement_opcode_OPCODE(opcode,toki,first.tokens.end(),statement,fsrc))
		return false;

	/* anything else to parse...? */
	while (subsi != statement.subst.end()) {
		token_substatement_t &current = *subsi;
		subsi++;

		toki = current.tokens.begin();
		if (toki == current.tokens.end()) continue;

		if ((*toki) == TK_NAME) {
			toki++;
			if (!process_statement_opcode_NAME(opcode,toki,current.tokens.end(),statement,fsrc))
				return false;
		}
		else if ((*toki) == TK_DISPLAY) {
		}
		else if ((*toki) == TK_DEST) {
		}
		else if ((*toki) == TK_SRC) {
		}
		else if ((*toki) == TK_RI) {
		}
		else if ((*toki) == TK_ST) {
		}
		else if ((*toki) == TK_STIDX) {
		}
		else {
			emit_error(statement,fsrc,"Unexpected substatement in opcode");
			return false;
		}
	}

	if (true) debug_dump_opcode_st(opcode);

	return true;
}

struct include_st {
	vector<string>		paths;
};

bool process_statement_include(token_statement_t &statement,filesource *fsrc) {
	assert(statement.subst.size() > 0);
	vector<token_substatement_t>::iterator subsi = statement.subst.begin();
	token_substatement_t &first = *subsi;
	subsi++;

	vector<token_t>::iterator toki = first.tokens.begin();
	assert(toki != first.tokens.end());
	assert((*toki) == TK_INCLUDE);
	toki++;

	include_st incl_st;

	do {
		if (toki == first.tokens.end()) {
			emit_error(statement,fsrc,"Include statement without argument");
			return false;
		}

		if ((*toki) == TK_STRING) {
			incl_st.paths.push_back(first.tokens[1].str);
		}
		else {
			emit_error(statement,fsrc,"Include statement without string");
			return false;
		}
		toki++;

		if (toki != first.tokens.end()) {
			emit_error(statement,fsrc,"Excess tokens in include statement");
			return false;
		}

		if (subsi == statement.subst.end()) break;
		first = *subsi; subsi++;
		toki = first.tokens.begin();
	} while (1);

	{
		vector<string>::iterator pi = incl_st.paths.begin();
		while (pi != incl_st.paths.end()) {
			string &path = *pi; pi++;

			if (!fsrc_push(path.c_str())) {
				emit_error(statement,fsrc,"Unable to open include file %s",path.c_str());
				return false;
			}

			if (!process_source_file(fsrccur()))
				return false;

			fsrc_pop();
		}
	}

	return true;
}

bool process_statement(token_statement_t &statement,filesource *fsrc) {
	if (statement.subst.empty()) return true; /* not an error */

	assert(statement.subst.size() > 0);
	token_substatement_t &first = statement.subst[0];

	/* do not tolerate a line like: " , something" */
	if (first.tokens.empty()) {
		emit_error(statement,fsrc,"No primary substatement");
		return false;
	}

	assert(first.tokens.size() > 0);
	if (first.tokens[0] == TK_INCLUDE)
		return process_statement_include(statement,fsrc);
	else if (first.tokens[0] == TK_OPCODE)
		return process_statement_opcode(statement,fsrc);
	else {
		emit_error(statement,fsrc,"Unknown primary substatment");
		return false;
	}

	return true;
}

bool process_source_file(filesource *fsrc) {
	token_statement_t statement;

	while (!fsrc->eof()) {
		statement.clear();
		if (!process_source_statement(statement,fsrc)) {
			emit_error(statement,fsrc,"%s",statement.errtok.errmsg().c_str());
			return false;
		}

		if (dbg_tok) {
			if (statement.srcpos.init)
				fprintf(stderr,"Statement in %s line %u col %u: ",fsrc->path.c_str(),statement.srcpos.line,statement.srcpos.col);
			else
				fprintf(stderr,"Statement in %s: ",fsrc->path.c_str());

			statement.dump(stderr);
			fprintf(stderr,"\n");
		}

		if (!process_statement(statement,fsrc))
			return false; /* will have already printed error */
	}

	return true;
}

bool process_source_stack(void) {
	while (!fsrcend()) {
		if (!process_source_file(fsrccur()))
			return false;

		fsrc_pop();
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

