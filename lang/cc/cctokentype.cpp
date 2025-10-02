
#include "cc.hpp"

// str_name[] = "token" str_name_len = strlen("token")
// str_ppname[] = "#token" str_ppname_len = strlen("#token") str_name = str_ppname + 1 = "token" str_name_len = strlen("token")

// identifiers only
#define DEFX(name) static const char str_##name[] = #name; static constexpr size_t str_##name##_len = sizeof(str_##name) - 1
// __identifiers__ only
#define DEFXUU(name) static const char str___##name##__[] = "__" #name "__"; static constexpr size_t str___##name##___len = sizeof(str___##name##__) - 1
// identifiers and/or #identifiers
#define DEFB(name) static const char strpp_##name[] = "#"#name; static constexpr size_t strpp_##name##_len = sizeof(strpp_##name) - 1; static const char * const str_##name = (strpp_##name)+1; static constexpr size_t str_##name##_len = strpp_##name##_len - 1
DEFX(alignas);
DEFX(alignof);
DEFX(auto);
DEFX(bool);
DEFX(break);
DEFX(case);
DEFX(char);
DEFX(const);
DEFX(constexpr);
DEFX(continue);
DEFX(default);
DEFX(do);
DEFX(double);
DEFB(else);
DEFX(enum);
DEFX(extern);
DEFX(false); 
DEFX(float);
DEFX(for);
DEFX(goto);
DEFB(if);
DEFX(int);
DEFX(long);
DEFX(nullptr);
DEFX(register);
DEFX(restrict);
DEFX(return);
DEFX(short);
DEFX(signed);
DEFX(sizeof);
DEFX(static);
DEFX(static_assert);
DEFX(struct);
DEFX(switch);
DEFX(thread_local);
DEFX(true);
DEFX(typedef);
DEFX(typeof);
DEFX(typeof_unqual);
DEFX(union);
DEFX(unsigned);
DEFX(void);
DEFX(while);
DEFX(_Alignas);
DEFX(_Alignof);
DEFX(_Atomic);
DEFX(_BitInt);
DEFX(_Bool);
DEFX(_Complex);
DEFX(_Decimal128);
DEFX(_Decimal32);
DEFX(_Decimal64);
DEFX(_Generic);
DEFX(_Imaginary);
DEFX(_Noreturn);
DEFX(_Static_assert);
DEFX(_Thread_local);
DEFX(char8_t);
DEFX(char16_t);
DEFX(char32_t);
DEFX(consteval);
DEFX(constinit);
DEFX(namespace);
DEFX(template);
DEFX(typeid);
DEFX(typename);
DEFX(using);
DEFX(wchar_t);
DEFB(ifdef);
DEFB(define);
DEFB(undef);
DEFB(elif);
DEFB(elifdef);
DEFB(ifndef);
DEFB(include);
DEFB(error);
DEFB(warning);
DEFB(line);
DEFB(pragma);
DEFB(embed);
DEFB(include_next);
DEFXUU(LINE);
DEFXUU(FILE);
DEFXUU(VA_OPT);
DEFXUU(VA_ARGS);
DEFX(intmax_t);
DEFX(uintmax_t);
DEFX(int8_t);
DEFX(uint8_t);
DEFX(int16_t);
DEFX(uint16_t);
DEFX(int32_t);
DEFX(uint32_t);
DEFX(int64_t);
DEFX(uint64_t);
DEFX(int_least8_t);
DEFX(uint_least8_t);
DEFX(int_least16_t);
DEFX(uint_least16_t);
DEFX(int_least32_t);
DEFX(uint_least32_t);
DEFX(int_least64_t);
DEFX(uint_least64_t);
DEFX(int_fast8_t);
DEFX(uint_fast8_t);
DEFX(int_fast16_t);
DEFX(uint_fast16_t);
DEFX(int_fast32_t);
DEFX(uint_fast32_t);
DEFX(int_fast64_t);
DEFX(uint_fast64_t);
DEFX(intptr_t);
DEFX(uintptr_t);
DEFX(size_t);
DEFX(ssize_t);
DEFX(near);
DEFX(far);
DEFX(huge);
DEFX(__pascal);
DEFX(__watcall);
DEFX(__stdcall);
DEFX(__cdecl);
DEFX(__syscall);
DEFX(__fastcall);
DEFX(__safecall);
DEFX(__thiscall);
DEFX(__vectorcall);
DEFX(__fortran);
DEFX(__attribute__);
DEFX(__declspec);
DEFB(endif);
DEFB(elifndef);
DEFXUU(func);
DEFXUU(FUNCTION);
DEFX(__int8);
DEFX(__int16);
DEFX(__int32);
DEFX(__int64);
DEFX(_declspec);
DEFX(__pragma);
DEFX(_Pragma);
// asm, _asm, __asm, __asm__
static const char         str___asm__[] = "__asm__";   static constexpr size_t str___asm___len = sizeof(str___asm__) - 1;
static const char * const str___asm = str___asm__;     static constexpr size_t str___asm_len = sizeof(str___asm__) - 1 - 2;
static const char * const str__asm = str___asm__+1;    static constexpr size_t str__asm_len = sizeof(str___asm__) - 1 - 2 - 1;
static const char * const str_asm = str___asm__+2;     static constexpr size_t str_asm_len = sizeof(str___asm__) - 1 - 2 - 2;
// volatile, __volatile__
static const char         str___volatile__[] = "__volatile__";   static constexpr size_t str___volatile___len = sizeof(str___volatile__) - 1;
static const char * const str_volatile = str___volatile__+2;     static constexpr size_t str_volatile_len = sizeof(str___volatile__) - 1 - 2 - 2;
// inline, _inline, __inline, __inline__
static const char         str___inline__[] = "__inline__";   static constexpr size_t str___inline___len = sizeof(str___inline__) - 1;
static const char * const str___inline = str___inline__;     static constexpr size_t str___inline_len = sizeof(str___inline__) - 1 - 2;
static const char * const str__inline = str___inline__+1;    static constexpr size_t str__inline_len = sizeof(str___inline__) - 1 - 2 - 1;
static const char * const str_inline = str___inline__+2;     static constexpr size_t str_inline_len = sizeof(str___inline__) - 1 - 2 - 2;
#undef DEFX

////////////////////////////////////////////////////////////////////

#define X(name) { str_##name, str_##name##_len, uint16_t(token_type_t::r_##name) }
#define XAS(name,tok) { str_##name, str_##name##_len, uint16_t(token_type_t::r_##tok) }
#define XUU(name) { str___##name##__, str___##name##___len, uint16_t(token_type_t::r___##name##__) }
#define XPP(name) { str_##name, str_##name##_len, uint16_t(token_type_t::r_pp##name) }
const ident2token_t ident2tok_pp[] = { // normal tokens, preprocessor time (er, well, actually lgtok time to be used by preprocessor)
	XUU(LINE),
	XUU(FILE),
	XAS(asm,      asm),
	XAS(__asm__,  asm),
	XAS(_asm,     __asm),
	XAS(__asm,    __asm),
	X(__pragma),
	X(_Pragma)
};
const size_t ident2tok_pp_length = sizeof(ident2tok_pp) / sizeof(ident2tok_pp[0]);

const ident2token_t ident2tok_cc[] = { // normal tokens, compile time
	X(alignas),
	X(alignof),
	X(auto),
	X(bool),
	X(break),
	X(case),
	X(char),
	X(const),
	X(constexpr),
	X(continue),
	X(default),
	X(do),
	X(double),
	X(else),
	X(enum),
	X(extern),
	X(false),
	X(float),
	X(for),
	X(goto),
	X(if),
	X(inline),
	X(int),
	X(long),
	X(nullptr),
	X(register),
	X(restrict),
	X(return),
	X(short),
	X(signed),
	X(sizeof),
	X(static),
	X(static_assert),
	X(struct),
	X(switch),
	X(thread_local),
	X(true),
	X(typedef),
	X(typeof),
	X(typeof_unqual),
	X(union),
	X(unsigned),
	X(void),
	X(while),
	X(_Alignas),
	X(_Alignof),
	X(_Atomic),
	X(_BitInt),
	X(_Bool),
	X(_Complex),
	X(_Decimal128),
	X(_Decimal32),
	X(_Decimal64),
	X(_Generic),
	X(_Imaginary),
	X(_Noreturn),
	X(_Static_assert),
	X(_Thread_local),
	X(char8_t),
	X(char16_t),
	X(char32_t),
	X(consteval),
	X(constinit),
	X(namespace),
	X(template),
	X(typeid),
	X(typename),
	X(using),
	X(wchar_t),
	X(intmax_t),
	X(uintmax_t),
	X(int8_t),
	X(uint8_t),
	X(int16_t),
	X(uint16_t),
	X(int32_t),
	X(uint32_t),
	X(int64_t),
	X(uint64_t),
	X(int_least8_t),
	X(uint_least8_t),
	X(int_least16_t),
	X(uint_least16_t),
	X(int_least32_t),
	X(uint_least32_t),
	X(int_least64_t),
	X(uint_least64_t),
	X(int_fast8_t),
	X(uint_fast8_t),
	X(int_fast16_t),
	X(uint_fast16_t),
	X(int_fast32_t),
	X(uint_fast32_t),
	X(int_fast64_t),
	X(uint_fast64_t),
	X(intptr_t),
	X(uintptr_t),
	X(size_t),
	X(ssize_t),
	X(near),
	X(far),
	X(huge),
	X(__pascal),
	X(__watcall),
	X(__stdcall),
	X(__cdecl),
	X(__syscall),
	X(__fastcall),
	X(__safecall),
	X(__thiscall),
	X(__vectorcall),
	X(__fortran),
	X(__attribute__),
	X(__declspec),
	X(_declspec),
	XAS(inline,       inline),
	XAS(_inline,      inline),
	XAS(__inline,     inline),
	XAS(__inline__,   inline),
	XAS(volatile,     volatile),
	XAS(__volatile__, volatile),
	XUU(func),
	XUU(FUNCTION),
	X(__int8),
	X(__int16),
	X(__int32),
	X(__int64)
};
const size_t ident2tok_cc_length = sizeof(ident2tok_cc) / sizeof(ident2tok_cc[0]);

const ident2token_t ppident2tok[] = { // preprocessor tokens
	XPP(if),
	XPP(ifdef),
	XPP(define),
	XPP(undef),
	XPP(else),
	XPP(elif),
	XPP(elifdef),
	XPP(ifndef),
	XPP(include),
	XPP(error),
	XPP(warning),
	XPP(line),
	XPP(pragma),
	XPP(endif),
	XPP(elifndef),
	XPP(embed),
	XPP(include_next)
};
const size_t ppident2tok_length = sizeof(ppident2tok) / sizeof(ppident2tok[0]);
#undef XPP
#undef XUU
#undef XAS
#undef X

///////////////////////////////////////////////////////////////////////////////

static const char *token_type_t_strlist[size_t(token_type_t::__MAX__)] = {
	"none",					// 0
	"eof",
	"plus",
	"plusplus",
	"minus",
	"minusminus",				// 5
	"semicolon",
	"equal",
	"equalequal",
	"tilde",
	"ampersand",				// 10
	"ampersandampersand",
	"pipe",
	"pipepipe",
	"caret",
	"integer",				// 15
	"floating",
	"charliteral",
	"strliteral",
	"identifier",
	"comma",				// 20
	"pipeequals",
	"caretequals",
	"ampersandequals",
	"plusequals",
	"minusequals",				// 25
	"star",
	"forwardslash",
	"starequals",
	"forwardslashequals",
	"percent",				// 30
	"percentequals",
	"exclamationequals",
	"exclamation",
	"question",
	"colon",				// 35
	"lessthan",
	"lessthanlessthan",
	"lessthanlessthanequals",
	"lessthanequals",
	"lessthanequalsgreaterthan",		// 40
	"greaterthan",
	"greaterthangreaterthan",
	"greaterthangreaterthanequals",
	"greaterthanequals",
	"minusrightanglebracket",		// 45
	"minusrightanglebracketstar",
	"period",
	"periodstar",
	"opensquarebracket",
	"closesquarebracket",			// 50
	"opencurlybracket",
	"closecurlybracket",
	"openparenthesis",
	"closeparenthesis",
	"coloncolon",				// 55
	"ppidentifier",
	str_alignas,
	str_alignof,
	str_auto,
	str_bool,				// 60
	str_break,
	str_case,
	str_char,
	str_const,
	str_constexpr,				// 65
	str_continue,
	str_default,
	str_do,
	str_double,
	str_else,				// 70
	str_enum,
	str_extern,
	str_false,
	str_float,
	str_for,				// 75
	str_goto,
	str_if,
	str___inline__,
	str_int,
	str_long,				// 80
	str_nullptr,
	str_register,
	str_restrict,
	str_return,
	str_short,				// 85
	str_signed,
	str_sizeof,
	str_static,
	str_static_assert,
	str_struct,				// 90
	str_switch,
	str_thread_local,
	str_true,
	str_typedef,
	str_typeof,				// 95
	str_typeof_unqual,
	str_union,
	str_unsigned,
	str_void,
	str___volatile__,			// 100
	str_while,
	str__Alignas,
	str__Alignof,
	str__Atomic,
	str__BitInt,				// 105
	str__Bool,
	str__Complex,
	str__Decimal128,
	str__Decimal32,
	str__Decimal64,				// 110
	str__Generic,
	str__Imaginary,
	str__Noreturn,
	str__Static_assert,
	str__Thread_local,			// 115
	str_char8_t,
	str_char16_t,
	str_char32_t,
	str_consteval,
	str_constinit,				// 120
	str_namespace,
	str_template,
	str_typeid,
	str_typename,
	str_using,				// 125
	str_wchar_t,
	strpp_if,
	strpp_ifdef,
	strpp_define,
	strpp_undef,				// 130
	strpp_else,
	"backslash",
	strpp_elif,
	strpp_elifdef,
	strpp_ifndef,				// 135
	strpp_include,
	strpp_error,
	strpp_warning,
	strpp_line,
	strpp_pragma,				// 140
	"ellipsis",
	str___LINE__,
	str___FILE__,
	str___VA_OPT__,
	str___VA_ARGS__,			// 145
	"opensquarebracketopensquarebracket",
	"closesquarebracketclosesquarebracket",
	str_intmax_t,
	str_uintmax_t,
	str_int8_t,				// 150
	str_uint8_t,
	str_int16_t,
	str_uint16_t,
	str_int32_t,
	str_uint32_t,				// 155
	str_int64_t,
	str_uint64_t,
	str_int_least8_t,
	str_uint_least8_t,
	str_int_least16_t,			// 160
	str_uint_least16_t,
	str_int_least32_t,
	str_uint_least32_t,
	str_int_least64_t,
	str_uint_least64_t,			// 165
	str_int_fast8_t,
	str_uint_fast8_t,
	str_int_fast16_t,
	str_uint_fast16_t,
	str_int_fast32_t,			// 170
	str_uint_fast32_t,
	str_int_fast64_t,
	str_uint_fast64_t,
	str_intptr_t,
	str_uintptr_t,				// 175
	str_size_t,
	str_ssize_t,
	str_near,
	str_far,
	str_huge,				// 180
	str___pascal,
	str___watcall,
	str___stdcall,
	str___cdecl,
	str___syscall,				// 185
	str___fastcall,
	str___safecall,
	str___thiscall,
	str___vectorcall,
	str___fortran,				// 190
	str___attribute__,
	str___declspec,
	"asm",
	"__asm",
	"asm_text",				// 195
	"newline",
	"pound",
	"poundpound",
	"backslashnewline",
	"macro_paramref",			// 200
	strpp_endif,
	strpp_elifndef,
	strpp_embed,
	strpp_include_next,
	"anglestrliteral",			// 205
	"__func__",
	"__FUNCTION__",
	"op:ternary",
	"op:comma",
	"op:log-or",				// 210
	"op:log-and",
	"op:bin-or",
	"op:bin-xor",
	"op:bin-and",
	"op:equals",				// 215
	"op:notequals",
	"op:lessthan",
	"op:greaterthan",
	"op:lessthan_equals",
	"op:greaterthan_equals",		// 220
	"op:leftshift",
	"op:rightshift",
	"op:add",
	"op:subtract",
	"op:multiply",				// 225
	"op:divide",
	"op:modulus",
	"op:++inc",
	"op:--dec",
	"op:addrof",				// 230
	"op:ptrderef",
	"op:negate",
	"op:bin-not",
	"op:log-not",
	"op:sizeof",				// 235
	"op:member_ref",
	"op:ptr_ref",
	"op:inc++",
	"op:dec--",
	"op:arrayref",				// 240
	"op:assign",
	"op:multiply_assign",
	"op:divide_assign",
	"op:modulus_assign",
	"op:add_assign"	,			// 245
	"op:subtract_assign",
	"op:leftshift_assign",
	"op:rightshift_assign",
	"op:and_assign",
	"op:xor_assign",			// 250
	"op:or_assign",
	"op:declaration",
	"op:compound_statement",
	"op:label",
	"op:default_label",			// 255
	"op:case_statement",
	"op:if_statement",
	"op:else_statement",
	"op:switch_statement",
	"op:break",				// 260
	"op:continue",
	"op:goto",
	"op:return",
	"op:while_statement",
	"op:do_while_statement",		// 265
	"op:for_statement",
	"op:none",
	"op:function_call",
	"op:array_define",
	"op:typecast",				// 270
	"op:dinit_array",
	"op:dinit_field",
	"op:gcc-range",
	"op:bitfield-range",
	"op:symbol",				// 275
	"__int8",
	"__int16",
	"__int32",
	"__int64",
	"op:alignof",				// 280
	str__declspec,
	str___pragma,
	"op:pragma",
	str__Pragma,
	"op:end-asm",				// 285
	"whitespace"
};

const char *token_type_t_str(const token_type_t t) {
	return (t < token_type_t::__MAX__) ? token_type_t_strlist[size_t(t)] : "";
}

