
enum class token_type_t:unsigned int {
	none=0,					// 0
	eof,
	plus,
	plusplus,
	minus,
	minusminus,				// 5
	semicolon,
	equal,
	equalequal,
	tilde,
	ampersand,				// 10
	ampersandampersand,
	pipe,
	pipepipe,
	caret,
	integer,				// 15
	floating,
	charliteral,
	strliteral,
	identifier,
	comma,					// 20
	pipeequals,
	caretequals,
	ampersandequals,
	plusequals,
	minusequals,				// 25
	star,
	forwardslash,
	starequals,
	forwardslashequals,
	percent,				// 30
	percentequals,
	exclamationequals,
	exclamation,
	question,
	colon,					// 35
	lessthan,
	lessthanlessthan,
	lessthanlessthanequals,
	lessthanequals,
	lessthanequalsgreaterthan,		// 40
	greaterthan,
	greaterthangreaterthan,
	greaterthangreaterthanequals,
	greaterthanequals,
	minusrightanglebracket,			// 45
	minusrightanglebracketstar,
	period,
	periodstar,
	opensquarebracket,
	closesquarebracket,			// 50
	opencurlybracket,
	closecurlybracket,
	openparenthesis,
	closeparenthesis,
	coloncolon,				// 55
	ppidentifier,
	r_alignas,
	r_alignof,
	r_auto,
	r_bool,					// 60
	r_break,
	r_case,
	r_char,
	r_const,
	r_constexpr,				// 65
	r_continue,
	r_default,
	r_do,
	r_double,
	r_else,					// 70
	r_enum,
	r_extern,
	r_false,
	r_float,
	r_for,					// 75
	r_goto,
	r_if,
	r_inline,
	r_int,
	r_long,					// 80
	r_nullptr,
	r_register,
	r_restrict,
	r_return,
	r_short,				// 85
	r_signed,
	r_sizeof,
	r_static,
	r_static_assert,
	r_struct,				// 90
	r_switch,
	r_thread_local,
	r_true,
	r_typedef,
	r_typeof,				// 95
	r_typeof_unqual,
	r_union,
	r_unsigned,
	r_void,
	r_volatile,				// 100
	r_while,
	r__Alignas,
	r__Alignof,
	r__Atomic,
	r__BitInt,				// 105
	r__Bool,
	r__Complex,
	r__Decimal128,
	r__Decimal32,
	r__Decimal64,				// 110
	r__Generic,
	r__Imaginary,
	r__Noreturn,
	r__Static_assert,
	r__Thread_local,			// 115
	r_char8_t,
	r_char16_t,
	r_char32_t,
	r_consteval,
	r_constinit,				// 120
	r_namespace,
	r_template,
	r_typeid,
	r_typename,
	r_using,				// 125
	r_wchar_t,
	r_ppif,
	r_ppifdef,
	r_ppdefine,
	r_ppundef,				// 130
	r_ppelse,
	backslash,
	r_ppelif,
	r_ppelifdef,
	r_ppifndef,				// 135
	r_ppinclude,
	r_pperror,
	r_ppwarning,
	r_ppline,
	r_pppragma,				// 140
	ellipsis,
	r___LINE__,
	r___FILE__,
	r___VA_OPT__,
	r___VA_ARGS__,				// 145
	opensquarebracketopensquarebracket,
	closesquarebracketclosesquarebracket,
	r_intmax_t,
	r_uintmax_t,
	r_int8_t,				// 150
	r_uint8_t,
	r_int16_t,
	r_uint16_t,
	r_int32_t,
	r_uint32_t,				// 155
	r_int64_t,
	r_uint64_t,
	r_int_least8_t,
	r_uint_least8_t,
	r_int_least16_t,			// 160
	r_uint_least16_t,
	r_int_least32_t,
	r_uint_least32_t,
	r_int_least64_t,
	r_uint_least64_t,			// 165
	r_int_fast8_t,
	r_uint_fast8_t,
	r_int_fast16_t,
	r_uint_fast16_t,
	r_int_fast32_t,				// 170
	r_uint_fast32_t,
	r_int_fast64_t,
	r_uint_fast64_t,
	r_intptr_t,
	r_uintptr_t,				// 175
	r_size_t,
	r_ssize_t,
	r_near,
	r_far,
	r_huge,					// 180
	r___pascal,
	r___watcall,
	r___stdcall,
	r___cdecl,
	r___syscall,				// 185
	r___fastcall,
	r___safecall,
	r___thiscall,
	r___vectorcall,
	r___fortran,				// 190
	r___attribute__,
	r___declspec,
	r_asm, /* GNU asm/__asm__ */
	r___asm, /* MSVC/OpenWatcom _asm __asm */
	r___asm_text,				// 195
	newline,
	pound,
	poundpound,
	backslashnewline,
	r_macro_paramref,			// 200
	r_ppendif,
	r_ppelifndef,
	r_ppembed,
	r_ppinclude_next,
	anglestrliteral,			// 205
	r___func__,
	r___FUNCTION__,
	op_ternary,
	op_comma,
	op_logical_or,				// 210
	op_logical_and,
	op_binary_or,
	op_binary_xor,
	op_binary_and,
	op_equals,				// 215
	op_not_equals,
	op_lessthan,
	op_greaterthan,
	op_lessthan_equals,
	op_greaterthan_equals,			// 220
	op_leftshift,
	op_rightshift,
	op_add,
	op_subtract,
	op_multiply,				// 225
	op_divide,
	op_modulus,
	op_pre_increment,
	op_pre_decrement,
	op_address_of,				// 230
	op_pointer_deref,
	op_negate,
	op_binary_not,
	op_logical_not,
	op_sizeof,				// 235
	op_member_ref,
	op_ptr_ref,
	op_post_increment,
	op_post_decrement,
	op_array_ref,				// 240
	op_assign,
	op_multiply_assign,
	op_divide_assign,
	op_modulus_assign,
	op_add_assign,				// 245
	op_subtract_assign,
	op_leftshift_assign,
	op_rightshift_assign,
	op_and_assign,
	op_xor_assign,				// 250
	op_or_assign,
	op_declaration,
	op_compound_statement,
	op_label,
	op_default_label,			// 255
	op_case_statement,
	op_if_statement,
	op_else_statement,
	op_switch_statement,
	op_break,				// 260
	op_continue,
	op_goto,
	op_return,
	op_while_statement,
	op_do_while_statement,			// 265
	op_for_statement,
	op_none,
	op_function_call,
	op_array_define,
	op_typecast,				// 270
	op_dinit_array,
	op_dinit_field,
	op_gcc_range,
	op_bitfield_range,
	op_symbol,				// 275
	r___int8,
	r___int16,
	r___int32,
	r___int64,
	op_alignof,				// 280
	r__declspec,
	r___pragma,
	op_pragma,
	r__Pragma,
	op_end_asm,				// 285
	whitespace,

	__MAX__
};

