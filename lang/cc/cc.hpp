
#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <memory>
#include <vector>
#include <string>
#include <limits>
#include <stack>
#include <deque>
#include <new>

#ifndef O_BINARY
# define O_BINARY 0
#endif

////////////////////////////////////////////////////////////////////

typedef int32_t unicode_char_t;
static constexpr unicode_char_t unicode_eof = unicode_char_t(-1l);
static constexpr unicode_char_t unicode_invalid = unicode_char_t(-2l);
static constexpr unicode_char_t unicode_nothing = unicode_char_t(-3l);
static constexpr unicode_char_t unicode_bad_escape = unicode_char_t(-4l);

////////////////////////////////////////////////////////////////////

typedef unsigned int ast_node_id_t;
static constexpr ast_node_id_t ast_node_none = ~ast_node_id_t(0u);

////////////////////////////////////////////////////////////////////

static constexpr size_t no_source_file = ~size_t(0);
static_assert( ~no_source_file == size_t(0), "oops" );

static constexpr int errno_return(int e) {
	return (e >= 0) ? (-e) : (e); /* whether errno values are positive or negative, return as negative */
}

///////////////////////////////////////////////////////////////////

typedef uint64_t data_size_t;
static constexpr data_size_t data_size_none = ~data_size_t(0u);
static_assert( ~data_size_none == data_size_t(0u), "oops" );

typedef uint64_t data_offset_t;
static constexpr data_offset_t data_offset_none = ~data_offset_t(0u);
static_assert( ~data_offset_none == data_offset_t(0u), "oops" );

typedef uint64_t addrmask_t;
static constexpr addrmask_t addrmask_none(0u);

typedef unsigned int type_specifier_t;
typedef unsigned int type_qualifier_t;
typedef unsigned int storage_class_t;

typedef size_t csliteral_id_t;
static constexpr csliteral_id_t csliteral_none = ~csliteral_id_t(0u);

typedef size_t identifier_id_t;
static constexpr identifier_id_t identifier_none = ~size_t(0u);

typedef unsigned int scope_id_t;
static constexpr unsigned int scope_none = ~((unsigned int)0u);
static constexpr unsigned int scope_global = 0;

typedef unsigned int segment_id_t;
static constexpr unsigned int segment_none = ~((unsigned int)0u);

typedef size_t symbol_id_t;
static constexpr size_t symbol_none = ~size_t(0);

typedef unsigned char bitfield_pos_t;
static constexpr bitfield_pos_t bitfield_pos_none = bitfield_pos_t(0xFFu);

static constexpr size_t ptr_deref_sizeof_addressof = ~size_t(0);
static_assert( (~ptr_deref_sizeof_addressof) == size_t(0), "oops" );

static constexpr addrmask_t addrmask_make(const addrmask_t sz/*must be power of 2*/) {
	return ~(sz - addrmask_t(1u));
}

struct pack_state_t {
	addrmask_t				align_limit = addrmask_none;
	std::string				identifier;
};

struct declaration_t;
struct parameter_t;

/////////////////////////////////////////////////////////////////////

static constexpr unsigned int			CBIS_USER_HEADER = (1u << 0u);
static constexpr unsigned int			CBIS_SYS_HEADER = (1u << 1u);
static constexpr unsigned int			CBIS_NEXT = (1u << 2u);

/////////////////////////////////////////////////////////////////////

/* declspec flags */
static constexpr unsigned int			DCS_FL_DEPRECATED = 1u << 0u;
static constexpr unsigned int			DCS_FL_DLLIMPORT = 1u << 1u;
static constexpr unsigned int			DCS_FL_DLLEXPORT = 1u << 2u;
static constexpr unsigned int			DCS_FL_NAKED = 1u << 3u;

//////////////////////////////////////////////////////////////////////////////

static constexpr unsigned int			DECLSPEC_OPTIONAL = 1u << 0u;
static constexpr unsigned int			DECLSPEC_ALLOW_DEF = 1u << 1u; /* allow definitions i.e. struct { ... } union { .... } enum { .... } */

static constexpr unsigned int			DIRDECL_ALLOW_ABSTRACT = 1u << 0u;
static constexpr unsigned int			DIRDECL_NO_IDENTIFIER = 1u << 1u;

/////////////////////////////////////////////////////////////////////

enum cpp11attr_namespace_t {
	CPP11ATTR_NS_NONE=0,
	CPP11ATTR_NS_GNU,
	CPP11ATTR_NS_GSL,
	CPP11ATTR_NS_MSVC,
	CPP11ATTR_NS_UNKNOWN
};

/////////////////////////////////////////////////////////////////////

#define CCERR_RET(code,pos,...) \
	do { \
		CCerr(pos,__VA_ARGS__); \
		return errno_return(code); \
	} while(0)

///////////////////////////////////////////////////////////////////

struct data_type_t {
	data_size_t			size;
	addrmask_t			align;
};

struct data_var_type_t {
	data_type_t			t;
	type_specifier_t		ts;
};

struct data_ptr_type_t {
	data_type_t			t;
	type_qualifier_t		tq;
};

struct data_type_set_ptr_t {
	data_ptr_type_t			dt_ptr;
	data_ptr_type_t			dt_ptr_near;
	data_ptr_type_t			dt_ptr_far;
	data_ptr_type_t			dt_ptr_huge;
	data_var_type_t			dt_size_t;
	data_var_type_t			dt_intptr_t;
};

struct data_type_set_t {
	/* base integer/float types */
	data_var_type_t			dt_bool;
	data_var_type_t			dt_char;
	data_var_type_t			dt_short;
	data_var_type_t			dt_int;
	data_var_type_t			dt_long;
	data_var_type_t			dt_longlong;
	data_var_type_t			dt_float;
	data_var_type_t			dt_double;
	data_var_type_t			dt_longdouble;
};

struct lgtok_state_t {
	unsigned int		flags = FL_NEWLINE;
	unsigned int		curlies = 0;

	static constexpr unsigned int FL_MSASM = (1u << 0u); /* __asm ... */
	static constexpr unsigned int FL_NEWLINE = (1u << 1u);
	static constexpr unsigned int FL_ARROWSTR = (1u << 2u); /* <string> in #include <string> */
};

//////////////////////////////////////////////////////////////////////////////

struct declspec_t { // parsing results of __declspec()
	addrmask_t				align = addrmask_none;
	unsigned int				dcs_flags = 0;
};

//////////////////////////////////////////////////////////////////////////////

extern const data_type_set_t data_types_default;
extern const data_type_set_ptr_t data_ptr_types_default;
extern const data_type_set_t data_types_intel16;
extern const data_type_set_ptr_t data_ptr_types_intel16_small;
extern const data_type_set_ptr_t data_ptr_types_intel16_big;
extern const data_type_set_ptr_t data_ptr_types_intel16_huge;
extern const data_type_set_t data_types_intel32;
extern const data_type_set_ptr_t data_ptr_types_intel32_segmented;
extern const data_type_set_ptr_t data_ptr_types_intel32_flat;
extern const data_type_set_t data_types_intel64;
extern const data_type_set_t data_types_intel64_windows;
extern const data_type_set_ptr_t data_ptr_types_intel64_flat;

//////////////////////////////////////////////////////////////////////////////

template <typename T> constexpr bool only_one_bit_set(const T &t) {
	return (t & (t - T(1u))) == T(0u);
}

////////////////////////////////////////////////////////////////////

template <class T> void typ_delete(T *p) {
	if (p) delete p;
	p = NULL;
}

///////////////////////////////////////////////////////

template <class obj_t,typename id_t,const id_t none> class obj_pool {
	public:
		obj_pool() { }
		~obj_pool() { }
	public:
		obj_t &operator()(const id_t id) {
			return _lookup(id);
		}
	public:
		id_t alloc(void) {
			obj_t &a = __internal_alloc();
			a.clear().addref();
			return next++;
		}

		void assign(id_t &d,const id_t s) {
			if (d != none) _lookup(d).release();
			d = s;
			if (d != none) _lookup(d).addref();
		}

		void assignmove(id_t &d,id_t &s) {
			if (d != none) _lookup(d).release();
			d = s;
			s = none;
		}

		id_t returnmove(id_t &s) {
			const id_t r = s;
			s = none;
			return r;
		}

		void release(id_t &d) {
			if (d != none) _lookup(d).release();
			d = none;
		}

		void refcount_check(void) {
			for (size_t i=0;i < pool.size();i++) {
				if (pool[i].ref != 0) {
					fprintf(stderr,"Leftover refcount=%u for object '%s'\n",
							pool[i].ref,
							pool[i].to_str().c_str());
				}
			}
		}

		void update_next(obj_t *p) {
			if (p >= &pool[0]) {
				const size_t i = p - &pool[0];
				if (i < pool.size()) next = i;
			}
		}

		obj_t& __internal_alloc() {
#if 1//SET TO ZERO TO MAKE SURE DEALLOCATED NODES STAY DEALLOCATED
			while (next < pool.size() && pool[next].ref != 0)
				next++;
#endif
			if (next == pool.size())
				pool.resize(pool.size()+(pool.size()/2u)+16u);

			assert(next < pool.size());
			assert(pool[next].ref == 0);
			return pool[next];
		}
	public:
		id_t next = id_t(0);
	private:
		inline obj_t &_lookup(const id_t id) {
#if 1//DEBUG
			if (id < pool.size()) {
				if (pool[id].ref == 0)
					throw std::out_of_range("object not initialized");

				return pool[id];
			}

			throw std::out_of_range("object out of range");
#else
			return pool[id];
#endif
		}
	private:
		std::vector<obj_t> pool;
};

////////////////////////////////////////////////////////////////////

#include "cctokentype.hpp"

////////////////////////////////////////////////////////////////////

const char *token_type_t_str(const token_type_t t);

//////////////////////////////////////////////////////////////////

#define CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(sfclass) \
	/* no copy */ \
	sfclass(const sfclass &) = delete; \
	sfclass &operator=(const sfclass &) = delete; \
	/* move allowed */ \
	sfclass(sfclass &&x) { common_move(x); } \
	sfclass &operator=(sfclass &&x) { common_move(x); return *this; }

//////////////////////////////////////////////////////////////

struct position_t {
	uint16_t			col;
	uint16_t			row;
	uint32_t			ofs;

	position_t() : col(0),row(0),ofs(0) { }
	position_t(const unsigned int row) : col(1),row(row),ofs(0) { }
};

////////////////////////////////////////////////////////////////////

struct ident2token_t {
	const char*		str;
	uint16_t		len; /* more than enough */
	uint16_t		token; /* make larger in case more than 65535 tokens defined */
};

//////////////////////////////////////////////////////////////////

extern const ident2token_t ident2tok_pp[];
extern const size_t ident2tok_pp_length;

extern const ident2token_t ident2tok_cc[];
extern const size_t ident2tok_cc_length;

extern const ident2token_t ppident2tok[];
extern const size_t ppident2tok_length;

//////////////////////////////////////////////////////////////////

struct source_file_object {
	enum {
		IF_BASE=0,
		IF_FD=1
	};

	unsigned int			iface;

	virtual const char*		getname(void);
	virtual ssize_t			read(void *,size_t);
	virtual void			close(void);

	source_file_object(const unsigned int iface=IF_BASE);
	virtual				~source_file_object();

	CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_file_object);
	virtual void			common_move(source_file_object &);
};

//////////////////////////////////////////////////////////////

struct source_fd : public source_file_object {
	virtual const char*		getname(void);
	virtual ssize_t			read(void *buffer,size_t count);
	virtual void			close(void);

	source_fd();
	source_fd(const int new_fd/*takes ownership*/,const std::string &new_name);
	virtual				~source_fd();

	CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_fd);
	virtual void			common_move(source_fd &x);

	std::string			name;
	int				fd = -1;
};

//////////////////////////////////////////////////////////////

struct source_null_file : public source_file_object {
	virtual const char*		getname(void) { return "null"; }
	virtual ssize_t			read(void *,size_t) { return 0; }
	virtual void			close(void) { }

	source_null_file() { }
	virtual				~source_null_file() { }

	CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_null_file);
	virtual void			common_move(source_null_file &) { }
};

////////////////////////////////////////////////////////////////////

struct rbuf {
	unsigned char*			base = NULL;
	unsigned char*			data = NULL;
	unsigned char*			end = NULL;
	unsigned char*			fence = NULL;
	int				err = 0;
	unsigned int			flags = 0;
	position_t			pos;

	static constexpr unsigned int	PFL_EOF = 1u << 0u;

	size_t				source_file = no_source_file;

	rbuf();
	~rbuf();

	rbuf(const rbuf &x) = delete;
	rbuf &operator=(const rbuf &x) = delete;

	rbuf(rbuf &&x);
	rbuf &operator=(rbuf &&x);

	void set_source_file(const size_t i);
	void common_move(rbuf &x);

	size_t buffer_size(void) const;
	size_t data_offset(void) const;
	size_t data_avail(void) const;
	size_t can_write(void) const;
	bool sanity_check(void) const;
	void free(void);
	unsigned char peekb(const size_t ofs=0);
	void discardb(size_t count=1);
	unsigned char getb(void);
	bool allocate(const size_t sz=4096);
	void flush(void);
	void lazy_flush(void);
	void pos_track(const unsigned char c);
	void pos_track(const unsigned char *from,const unsigned char *to);
};

////////////////////////////////////////////////////////////////////

struct source_file_t {
	std::string		path;
	uint16_t		refcount = 0;

	bool			empty(void) const;
	void			clear(void);
};

////////////////////////////////////////////////////////////////////

struct floating_value_t {
	enum class type_t:unsigned char {
		NONE=0,			// 0
		FLOAT,
		DOUBLE,
		LONGDOUBLE,

		__MAX__
	};

	uint64_t				mantissa;
	int32_t					exponent; /* if special, this is a code for how special i.e. NaN, inf, etc. */
	unsigned char				flags;
	type_t					type;

	static constexpr unsigned int		FL_SPECIAL    = (1u << 0u); /* NaN, inf, etc */
	static constexpr unsigned int           FL_OVERFLOW   = (1u << 1u);
	static constexpr unsigned int           FL_NEGATIVE   = (1u << 2u);

	static constexpr uint64_t		mant_msb = uint64_t(1ull) << uint64_t(63ull);

	std::string to_str(void) const;
	bool to_bool(void) const;
	void init(void);
	void setsn(const uint64_t m,const int32_t e);
	void normalize(void);
};

////////////////////////////////////////////////////////////////////

struct integer_value_t {
	enum class type_t:unsigned char {
		NONE=0,			// 0
		BOOL,
		CHAR,
		SHORT,
		INT,
		LONG,			// 5
		LONGLONG,

		__MAX__
	};

	union {
		uint64_t			u;
		int64_t				v;//DEPRECATE
		int64_t				s;
	} v;
	unsigned char				flags;
	type_t					type;

	static constexpr unsigned int		FL_SIGNED     = (1u << 0u);
	static constexpr unsigned int		FL_OVERFLOW   = (1u << 1u);

	std::string to_str(void) const;
	bool to_bool(void) const;
	void init(void);
};

///////////////////////////////////////////////////////

struct csliteral_t {
	enum type_t {
		CHAR=0,
		UTF8, /* multibyte */
		UNICODE16,
		UNICODE32,

		__MAX__
	};

	static const unsigned char unit_size[type_t::__MAX__];

	type_t			type = CHAR;
	void*			data = NULL;
	size_t			length = 0; /* in bytes */
	size_t			allocated = 0;
	unsigned int		ref = 0;

	inline bool operator!=(const csliteral_t &rhs) const { return !(*this == rhs); }
	inline bool operator!=(const std::string &rhs) const { return !(*this == rhs); }

	csliteral_t &clear(void);
	csliteral_t &addref(void);
	csliteral_t &release(void);
	void free(void);
	bool copy_from(const csliteral_t &x);
	const char *as_char(void) const;
	const unsigned char *as_binary(void) const;
	const unsigned char *as_utf8(void) const;
	const uint16_t *as_u16(void) const;
	const uint32_t *as_u32(void) const;
	bool operator==(const csliteral_t &rhs) const;
	bool operator==(const std::string &rhs) const;
	size_t unitsize(void) const;
	size_t units(void) const;
	bool alloc(const size_t sz);
	static std::string to_escape(const uint32_t c);
	bool shrinkfit(void);
	std::string makestring(void) const;
	std::string to_str(void) const;
};

/////////////////////////////////////////////////////////////////////

struct identifier_t {
	unsigned char*		data = NULL;
	size_t			length = 0;
	unsigned int		ref = 0;

	identifier_t &clear(void);
	identifier_t &addref(void);
	identifier_t &release(void);
	identifier_t();
	identifier_t(const identifier_t &x) = delete;
	identifier_t &operator=(const identifier_t &x) = delete;
	identifier_t(identifier_t &&x);
	identifier_t &operator=(identifier_t &&x);
	~identifier_t();

	void free_data(void);
	void free(void);
	void common_move(identifier_t &other);
	bool copy_from(const unsigned char *in_data,const size_t in_length);
	bool copy_from(const std::string &s);
	std::string to_str(void) const;

	bool operator==(const identifier_t &rhs) const;
	inline bool operator!=(const identifier_t &rhs) const { return !(*this == rhs); }

	bool operator==(const csliteral_t &rhs) const;
	inline bool operator!=(const csliteral_t &rhs) const { return !(*this == rhs); }

	bool operator==(const std::string &rhs) const;
	inline bool operator!=(const std::string &rhs) const { return !(*this == rhs); }
};

/////////////////////////////////////////////////////////////////////

struct token_t {
	token_type_t		type = token_type_t::none;
	position_t		pos;
	size_t			source_file = no_source_file; /* index into source file array to indicate token source or -1 */

	token_t();
	~token_t();
	token_t(const token_t &t);
	token_t(token_t &&t);
	token_t(const token_type_t t);
	token_t(const token_type_t t,const position_t &n_pos,const size_t n_source_file);
	token_t &operator=(const token_t &t);
	token_t &operator=(token_t &&t);

	void set_source_file(const size_t i);
	bool operator==(const token_t &t) const;

	inline bool operator!=(const token_t &t) const { return !(*this == t); }

	union v_t {
		integer_value_t		integer; /* token_type_t::integer */
		floating_value_t	floating; /* token_type_t::floating */
		csliteral_id_t		csliteral; /* token_type_t::charliteral/strliteral/identifier/asm */
		size_t			paramref; /* token_type_t::r_macro_paramref */
		declaration_t*		declaration; /* token_type_t::op_declaration */
		identifier_id_t		identifier;
		symbol_id_t		symbol;/* token_type_t::op_symbol */
		scope_id_t		scope; /* token_type_t::op_compound_statement */
		void*			general_ptr; /* for use in clearing general ppinter */
	} v;

	std::string to_str(void) const;
	void clear_v(void);

	private:
	void common_delete(void);
	void common_init(void);
	void common_copy(const token_t &x);
	void common_move(token_t &x);
};

////////////////////////////////////////////////////////////////////

struct pptok_macro_t {
	std::vector<token_t>		tokens;
	std::vector<identifier_id_t>	parameters;
	unsigned int			flags = 0;

	static constexpr unsigned int	FL_PARENTHESIS = 1u << 0u;
	static constexpr unsigned int	FL_VARIADIC = 1u << 1u;
	static constexpr unsigned int	FL_NO_VA_ARGS = 1u << 2u; /* set if GNU args... used instead */
	static constexpr unsigned int	FL_STRINGIFY = 1u << 3u; /* uses #parameter to stringify */

	pptok_macro_t();
	pptok_macro_t(pptok_macro_t &&x);
	pptok_macro_t &operator=(pptok_macro_t &&x);
	~pptok_macro_t();

	void common_move(pptok_macro_t &o);
};

////////////////////////////////////////////////////////////////////

struct pptok_state_t {
	static constexpr size_t macro_bucket_count = 4096u / sizeof(char*); /* power of 2 */
	static_assert((macro_bucket_count & (macro_bucket_count - 1)) == 0, "must be power of 2"); /* is power of 2 */

	struct pptok_macro_ent_t {
		pptok_macro_t		ment;
		identifier_id_t		name = identifier_none;
		pptok_macro_ent_t*	next;

		pptok_macro_ent_t();
		pptok_macro_ent_t(const pptok_macro_ent_t &) = delete;
		pptok_macro_ent_t &operator=(const pptok_macro_ent_t &) = delete;
		pptok_macro_ent_t(pptok_macro_ent_t &&) = delete;
		pptok_macro_ent_t &operator=(pptok_macro_ent_t &&) = delete;
		~pptok_macro_ent_t();
	};

	struct cond_block_t {
		enum {
			WAITING=0,	/* false, waiting for elseif/else */
			IS_TRUE,	/* true, next elseif/else switches to done. #else && WAITING == IS_TRUE */
			DONE		/* state was IS_TRUE, skip anything else */
		};
		unsigned char		state = WAITING;
		unsigned char		flags = 0;
		static constexpr unsigned char FL_ELSE = 1u << 0u;
	};

	/* recursion stack, for #include */
	struct include_t {
		source_file_object*	sfo = NULL;
		rbuf			rb;
	};

	std::stack<include_t>		include_stk;
	std::stack<cond_block_t>	cond_block;

	unsigned int macro_expansion_counter = 0; /* to prevent runaway expansion */
	std::deque<token_t> macro_expansion;

	pptok_macro_ent_t* macro_buckets[macro_bucket_count] = { NULL };

	void include_push(source_file_object *sfo);
	void include_pop(void);
	void include_popall(void);
	bool condb_true(void) const;
	const pptok_macro_ent_t* lookup_macro(const identifier_id_t &i) const;
	bool create_macro(const identifier_id_t i,pptok_macro_t &m);
	bool delete_macro(const identifier_id_t i);
	void free_macro_bucket(const unsigned int bucket);
	void free_macros(void);

	static uint8_t macro_hash_id(const unsigned char *data,const size_t len);

	inline uint8_t macro_hash_id(const identifier_t &i) const {
		return macro_hash_id(i.data,i.length);
	}

	uint8_t macro_hash_id(const identifier_id_t &i) const;

	static uint8_t macro_hash_id(const csliteral_t &i) {
		return macro_hash_id((const unsigned char*)i.data,i.length);
	}

	pptok_state_t();
	pptok_state_t(const pptok_state_t &) = delete;
	pptok_state_t &operator=(const pptok_state_t &) = delete;
	pptok_state_t(pptok_state_t &&) = delete;
	pptok_state_t &operator=(pptok_state_t &&) = delete;
	~pptok_state_t();
};

//////////////////////////////////////////////////////////////////////////////

struct ast_node_t {
	token_t			t;
	ast_node_id_t		next = ast_node_none;
	ast_node_id_t		child = ast_node_none;
	unsigned int		ref = 0;

	ast_node_t&		set_child(const ast_node_id_t n);
	ast_node_t&		set_next(const ast_node_id_t n);
	ast_node_t&		addref(void);
	ast_node_t&		release(void);
	ast_node_t&		clear_and_move_assign(token_t &tt);
	ast_node_t&		clear(const token_type_t t);

	ast_node_t() { }
	ast_node_t(const ast_node_t &) = delete;
	ast_node_t &operator=(const ast_node_t &) = delete;
	ast_node_t(ast_node_t &&x) { common_move(x); }
	ast_node_t &operator=(ast_node_t &&x) { common_move(x); return *this; }

	static void arraycopy(std::vector<ast_node_id_t> &d,const std::vector<ast_node_id_t> &s);
	static void arrayrelease(std::vector<ast_node_id_t> &d);

	void common_move(ast_node_t &o);
	std::string to_str(void) const;
};

////////////////////////////////////////////////////////////////////

using csliteral_pool_t = obj_pool<csliteral_t,csliteral_id_t,csliteral_none>;
using ast_node_pool_t = obj_pool<ast_node_t,ast_node_id_t,ast_node_none>;

//////////////////////////////////////////////////////////////////////////////

class ast_node_pool : public ast_node_pool_t {
	public:
		using pool_t = ast_node_pool_t;
	public:
		ast_node_pool();
		~ast_node_pool();
	public:
		ast_node_id_t alloc(token_type_t t=token_type_t::none);
		ast_node_id_t alloc(token_t &t);
};

////////////////////////////////////////////////////////////////////

extern ast_node_pool ast_node;

//////////////////////////////////////////////////////////////////////////////

struct declaration_specifiers_t {
	storage_class_t				storage_class = 0;
	type_specifier_t			type_specifier = 0;
	type_qualifier_t			type_qualifier = 0;
	symbol_id_t				type_identifier_symbol = symbol_none;
	data_size_t				size = data_size_none;
	addrmask_t				align = addrmask_none;
	unsigned int				dcs_flags = 0;
	std::vector<symbol_id_t>		enum_list;
	unsigned int				count = 0;

	bool empty(void) const;

	declaration_specifiers_t();
	declaration_specifiers_t(const declaration_specifiers_t &x);
	declaration_specifiers_t &operator=(const declaration_specifiers_t &x);
	declaration_specifiers_t(declaration_specifiers_t &&x);
	declaration_specifiers_t &operator=(declaration_specifiers_t &&x);
	~declaration_specifiers_t();

	void common_move(declaration_specifiers_t &o);
	void common_copy(const declaration_specifiers_t &o);
};

//////////////////////////////////////////////////////////////////////////////

struct pointer_t {
	type_qualifier_t			tq = 0;

	bool operator==(const pointer_t &o) const;
	bool operator!=(const pointer_t &o) const;
};

//////////////////////////////////////////////////////////////////////////////

struct ddip_t {
	std::vector<pointer_t>		ptr;
	std::vector<ast_node_id_t>	arraydef;
	std::vector<parameter_t>	parameters;
	unsigned int			dd_flags = 0;

	ddip_t();
	ddip_t(const ddip_t &x);
	ddip_t &operator=(const ddip_t &x);
	ddip_t(ddip_t &&x);
	ddip_t &operator=(ddip_t &&x);

	~ddip_t();

	void common_copy(const ddip_t &o);
	void common_move(ddip_t &o);
	bool empty(void) const;
};

//////////////////////////////////////////////////////////////////////////////

class ddip_list_t : public std::vector<ddip_t> {
	public:
		using BT = std::vector<ddip_t>;
	public:
		ddip_list_t();
		~ddip_list_t();
	public:
		void addcombine(ddip_t &&x);
		void addcombine(const ddip_t &x);
		ddip_t *funcparampair(void);
};

//////////////////////////////////////////////////////////////////////////////

struct declarator_t {
	static constexpr unsigned int FL_FUNCTION = 1u << 0u; /* it saw () */
	static constexpr unsigned int FL_FUNCTION_POINTER = 1u << 1u; /* it saw () but it's a function pointer, not a function, hence actually a variable */
	static constexpr unsigned int FL_ELLIPSIS = 1u << 2u; /* we saw ellipsis ... in the parameter list */

	ddip_list_t ddip;
	identifier_id_t name = identifier_none;
	symbol_id_t symbol = symbol_none;
	unsigned int flags = 0;

	ast_node_id_t expr = ast_node_none; /* function body if FL_FUNCTION, else initval */

	declarator_t();
	declarator_t(const declarator_t &x);
	declarator_t &operator=(const declarator_t &x);
	declarator_t(declarator_t &&x);
	declarator_t &operator=(declarator_t &&x);

	void common_copy(const declarator_t &o);

	void common_move(declarator_t &o);

	~declarator_t();
};

//////////////////////////////////////////////////////////////////////////////

struct declaration_t {
	declaration_specifiers_t	spec;
	std::vector<declarator_t>	declor;

	declarator_t &new_declarator(void);

	declaration_t();
	declaration_t(declaration_t &&x);
	declaration_t &operator=(declaration_t &&x);

	void common_move(declaration_t &o);

	~declaration_t();
};

//////////////////////////////////////////////////////////////////////////////

struct parameter_t {
	declaration_specifiers_t		spec;
	declarator_t				decl;

	parameter_t();
	parameter_t(const parameter_t &x);
	parameter_t &operator=(const parameter_t &x);
	parameter_t(parameter_t &&x);
	parameter_t &operator=(parameter_t &&x);
	~parameter_t();

	void common_copy(const parameter_t &o);
	void common_move(parameter_t &o);
};

//////////////////////////////////////////////////////////////////////////////

struct enumerator_t {
	identifier_id_t				name = identifier_none;
	ast_node_id_t				expr = ast_node_none;
	position_t				pos;

	enumerator_t();
	enumerator_t(const enumerator_t &x);
	enumerator_t &operator=(const enumerator_t &x);
	enumerator_t(enumerator_t &&x);
	enumerator_t &operator=(enumerator_t &&x);

	void common_copy(const enumerator_t &o);
	void common_move(enumerator_t &o);
	~enumerator_t();
};

//////////////////////////////////////////////////////////////////////////////

struct structfield_t {
	declaration_specifiers_t		spec;
	ddip_list_t				ddip;
	identifier_id_t				name = identifier_none;
	data_offset_t				offset = data_offset_none;
	bitfield_pos_t				bf_start = bitfield_pos_none,bf_length = bitfield_pos_none;

	structfield_t();
	structfield_t(structfield_t &&x);
	structfield_t &operator=(structfield_t &&x);
	~structfield_t();
	void common_move(structfield_t &x);
};

//////////////////////////////////////////////////////////////////////////////

struct segment_t {
	enum class type_t {
		NONE=0,
		CODE,
		CONST,
		DATA,
		BSS,
		STACK
	};

	enum class use_t {
		NONE=0,

		X86_16=1,
		X86_32,
		X86_64
	};

	static constexpr unsigned int FL_NOTINEXE = 1u << 0u; /* not stored in EXE i.e. stack */
	static constexpr unsigned int FL_READABLE = 1u << 1u;
	static constexpr unsigned int FL_WRITEABLE = 1u << 2u;
	static constexpr unsigned int FL_EXECUTABLE = 1u << 3u;
	static constexpr unsigned int FL_PRIVATE = 1u << 4u;
	static constexpr unsigned int FL_FLAT = 1u << 5u;

	addrmask_t				align = addrmask_none;
	identifier_id_t				name = identifier_none;
	type_t					type = type_t::NONE;
	use_t					use = use_t::NONE;
	data_size_t				limit = data_size_none;
	unsigned int				flags = 0;

	segment_t();
	segment_t(const segment_t &) = delete;
	segment_t &operator=(const segment_t &) = delete;
	segment_t(segment_t &&x);
	segment_t &operator=(segment_t &&x);
	~segment_t();

	void common_move(segment_t &x);
};

//////////////////////////////////////////////////////////////////////////////

struct symbol_t {
	enum type_t {
		NONE=0,
		VARIABLE,
		FUNCTION,
		TYPEDEF,
		STRUCT,
		UNION,
		CONST,
		ENUM,
		STR
	};

	static constexpr unsigned int FL_DEFINED = 1u << 0u; /* function body, variable without extern */
	static constexpr unsigned int FL_DECLARED = 1u << 1u; /* function without body, variable with extern */
	static constexpr unsigned int FL_PARAMETER = 1u << 2u; /* within scope of function, parameter value */
	static constexpr unsigned int FL_STACK = 1u << 3u; /* exists on the stack */
	static constexpr unsigned int FL_ELLIPSIS = 1u << 4u; /* function has ellipsis param */
	static constexpr unsigned int FL_FUNCTION_POINTER = 1u << 5u; /* it's a variable you can can call like a function, a function pointer */

	declaration_specifiers_t		spec;
	ddip_list_t				ddip;
	std::vector<structfield_t>		fields;
	identifier_id_t				name = identifier_none;
	ast_node_id_t				expr = ast_node_none; /* variable init, function body, etc */
	scope_id_t				scope = scope_none;
	scope_id_t				parent_of_scope = scope_none;
	segment_id_t				part_of_segment = segment_none;
	enum type_t				sym_type = NONE;
	unsigned int				flags = 0;

	symbol_t();
	symbol_t(const symbol_t &) = delete;
	symbol_t &operator=(const symbol_t &) = delete;
	symbol_t(symbol_t &&x);
	symbol_t &operator=(symbol_t &&x);
	~symbol_t();

	void common_move(symbol_t &x);
	bool identifier_exists(const identifier_id_t &id);
};

//////////////////////////////////////////////////////////////////////////////

struct scope_t {
	struct decl_t {
		declaration_specifiers_t	spec;
		declarator_t			declor;
	};

	ast_node_id_t				root = ast_node_none;
	std::vector<decl_t>			localdecl;
	std::vector<symbol_id_t>		symbols;

	scope_t();
	scope_t(const scope_t &) = delete;
	scope_t &operator=(const scope_t &) = delete;
	scope_t(scope_t &&x);
	scope_t &operator=(scope_t &&x);
	~scope_t();

	decl_t &new_localdecl(void);
	void common_move(scope_t &x);
};

//////////////////////////////////////////////////////////////////////////////

enum target_cpu_t {
	CPU_NONE=0,			// 0
	CPU_INTEL_X86,

	CPU__MAX
};

enum target_cpu_sub_t {
	CPU_SUB_NONE=0,			// 0

	// CPU_INTEL_X86
	CPU_SUB_X86_16,
	CPU_SUB_X86_32,
	CPU_SUB_X86_64,

	CPU_SUB__MAX
};

enum target_cpu_rev_t {
	CPU_REV_NONE=0,			// 0

	// CPU_INTEL_X86
	CPU_REV_X86_8086,
	CPU_REV_X86_80186,
	CPU_REV_X86_80286,
	CPU_REV_X86_80386,
	CPU_REV_X86_80486,		// 5
	CPU_REV_X86_80586,
	CPU_REV_X86_80686,

	CPU_REV__MAX
};

//////////////////////////////////////////////////////////////////////////////

typedef bool (*cb_include_accept_path_t)(const std::string &p);
typedef source_file_object* (*cb_include_search_t)(pptok_state_t &pst,lgtok_state_t &lst,const token_t &t,unsigned int fl);

//////////////////////////////////////////////////////////////////////////////

bool cb_include_accept_path_default(const std::string &/*p*/);
source_file_object* cb_include_search_default(pptok_state_t &/*pst*/,lgtok_state_t &/*lst*/,const token_t &t,unsigned int fl);

//////////////////////////////////////////////////////////////////////////////

bool ptrmergeable(const ddip_t &to,const ddip_t &from);
bool arraymergeable(const ddip_t &to,const ddip_t &from);
int rbuf_copy_csliteral(rbuf &dbuf,csliteral_id_t &csid);

//////////////////////////////////////////////////////////////////////////////

extern csliteral_pool_t csliteral;
extern std::vector<source_file_t> source_files;

//////////////////////////////////////////////////////////////////////////////

int rbuf_sfd_refill(rbuf &buf,source_file_object &sfo);

//////////////////////////////////////////////////////////////////////////////

size_t alloc_source_file(const std::string &path);
void clear_source_file(const size_t i);
void release_source_file(const size_t i);
void addref_source_file(const size_t i);
void source_file_refcount_check(void);

//////////////////////////////////////////////////////////////////////////////

void path_slash_translate(std::string &path);
std::string path_combine(const std::string &base,const std::string &rel);

//////////////////////////////////////////////////////////////////////////////

void utf8_to_str(unsigned char* &w,unsigned char *f,unicode_char_t c);
std::string utf8_to_str(const unicode_char_t c);
unicode_char_t p_utf8_decode(const unsigned char* &p,const unsigned char* const f);

//////////////////////////////////////////////////////////////////////////////

void utf16_to_str(uint16_t* &w,uint16_t *f,unicode_char_t c);
unicode_char_t p_utf16_decode(const uint16_t* &p,const uint16_t* const f);

//////////////////////////////////////////////////////////////////////////////

unicode_char_t getcnu(rbuf &buf,source_file_object &sfo); /* non-unicode */
unicode_char_t getc(rbuf &buf,source_file_object &sfo);

//////////////////////////////////////////////////////////////////////////////

enum storage_class_idx_t {
	SCI_TYPEDEF,		// 0
	SCI_EXTERN,
	SCI_STATIC,
	SCI_AUTO,
	SCI_REGISTER,
	SCI_CONSTEXPR,		// 5
	SCI_INLINE,
	SCI_CONSTEVAL,
	SCI_CONSTINIT,

	SCI__MAX
};

//////////////////////////////////////////////////////////////////////////////

#define X(c) static constexpr storage_class_t SC_##c = 1u << SCI_##c
X(TYPEDEF);			// 0
X(EXTERN);
X(STATIC);
X(AUTO);
X(REGISTER);
X(CONSTEXPR);			// 5
X(INLINE);
X(CONSTEVAL);
X(CONSTINIT);
#undef X

//////////////////////////////////////////////////////////////////////////////

/* this is a bitmask because you can specify more than one, these are indexes */
enum type_specifier_idx_t {
	TSI_VOID=0,		// 0
	TSI_CHAR,
	TSI_SHORT,
	TSI_INT,
	TSI_LONG,
	TSI_FLOAT,		// 5
	TSI_DOUBLE,
	TSI_SIGNED,
	TSI_UNSIGNED,
	TSI_LONGLONG,
	TSI_ENUM,		// 10
	TSI_STRUCT,
	TSI_UNION,
	TSI_MATCH_TYPEDEF,
	TSI_MATCH_BUILTIN,
	TSI_SZ8,		// 15
	TSI_SZ16,
	TSI_SZ32,
	TSI_SZ64,

	TSI__MAX
};

//////////////////////////////////////////////////////////////////////////////

#define X(c) static constexpr type_specifier_t TS_##c = 1u << TSI_##c
X(VOID);			// 0
X(CHAR);
X(SHORT);
X(INT);
X(LONG);
X(FLOAT);			// 5
X(DOUBLE);
X(SIGNED);
X(UNSIGNED);
X(LONGLONG);
X(ENUM);			// 10
X(STRUCT);
X(UNION);
X(MATCH_TYPEDEF);
X(MATCH_BUILTIN);
X(SZ8);				// 15
X(SZ16);
X(SZ32);
X(SZ64);
#undef X

//////////////////////////////////////////////////////////////////////////////

/* this is a bitmask because you can specify more than one, these are indexes */
enum type_qualifier_idx_t {
	TQI_CONST=0,		// 0
	TQI_VOLATILE,
	TQI_NEAR,
	TQI_FAR,
	TQI_HUGE,
	TQI_RESTRICT,		// 5

	TQI__MAX
};

//////////////////////////////////////////////////////////////////////////////

#define X(c) static constexpr type_qualifier_t TQ_##c = 1u << TQI_##c
X(CONST);
X(VOLATILE);
X(NEAR);			// 5
X(FAR);
X(HUGE);
X(RESTRICT);
#undef X

//////////////////////////////////////////////////////////////////////////////

extern const char *storage_class_idx_t_str[SCI__MAX];
extern const char *type_specifier_idx_t_str[TSI__MAX];
extern const char *type_qualifier_idx_t_str[TSI__MAX];

//////////////////////////////////////////////////////////////////////////////

extern const char *target_cpu_str_t[CPU__MAX];
extern const char *target_cpu_sub_str_t[CPU_SUB__MAX];
extern const char *target_cpu_rev_str_t[CPU_REV__MAX];

//////////////////////////////////////////////////////////////////////////////

using identifier_pool_t = obj_pool<identifier_t,identifier_id_t,identifier_none>;
extern identifier_pool_t identifier;

//////////////////////////////////////////////////////////////////////////////

void CCerr(const position_t &pos,const char *fmt,...);

//////////////////////////////////////////////////////////////////////////////

bool is_newline(const unsigned char b);
bool is_whitespace(const unsigned char b);
int cc_parsedigit(unsigned char c,const unsigned char base=10);
bool is_identifier_first_char(unsigned char c);
bool is_identifier_char(unsigned char c);
bool is_asm_non_ident_text_char(unsigned char c);
bool is_asm_ident_text_char(unsigned char c);
bool is_hchar(unsigned char c);

//////////////////////////////////////////////////////////////////////////////

int32_t lgtok_cslitget(rbuf &buf,source_file_object &sfo,const bool unicode=false);

//////////////////////////////////////////////////////////////////////////////

bool eat_whitespace(rbuf &buf,source_file_object &sfo);
void eat_newline(rbuf &buf,source_file_object &sfo);

//////////////////////////////////////////////////////////////////////////////

int eat_c_comment(unsigned int level,rbuf &buf,source_file_object &sfo);
int eat_cpp_comment(rbuf &buf,source_file_object &sfo);

//////////////////////////////////////////////////////////////////////////////

bool looks_like_arrowstr(rbuf &buf,source_file_object &sfo);

//////////////////////////////////////////////////////////////////////////////

int lgtok_charstrlit(rbuf &buf,source_file_object &sfo,token_t &t,const csliteral_t::type_t cslt=csliteral_t::type_t::CHAR);

//////////////////////////////////////////////////////////////////////////////

int lgtok_asm_text(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);
int lgtok_identifier(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);
int lgtok_number(rbuf &buf,source_file_object &sfo,token_t &t);
int lgtok(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);

//////////////////////////////////////////////////////////////////////////////

bool pptok_define_asm_allowed_token(const token_t &t);
bool pptok_define_allowed_token(const token_t &t);

//////////////////////////////////////////////////////////////////////////////

int pptok_lgtok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);
void pptok_lgtok_ungetch(pptok_state_t &pst,token_t &t);

//////////////////////////////////////////////////////////////////////////////

int pptok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);

//////////////////////////////////////////////////////////////////////////////

extern cb_include_search_t cb_include_search;

//////////////////////////////////////////////////////////////////////////////

int lctok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);

//////////////////////////////////////////////////////////////////////////////

struct symbol_lookup_t {
	symbol_t::type_t st = symbol_t::NONE;
	scope_id_t cursco = scope_none;
	symbol_id_t sid = symbol_none;
	unsigned int flags = 0;
	position_t pos;
};

//////////////////////////////////////////////////////////////////////////////

/* target settings */
extern target_cpu_t					target_cpu;
extern target_cpu_sub_t					target_cpusub;
extern target_cpu_rev_t					target_cpurev;

/* data types */
extern data_type_set_t					data_types;
extern data_type_set_ptr_t				data_types_ptr_code;
extern data_type_set_ptr_t				data_types_ptr_data;
extern data_type_set_ptr_t				data_types_ptr_stack;

extern addrmask_t					default_packing;
extern addrmask_t					current_packing;
extern std::vector<pack_state_t>			packing_stack; /* #pragma pack */

//////////////////////////////////////////////////////////////////////////////

extern std::vector<segment_t>				segments;

//////////////////////////////////////////////////////////////////////////////

segment_t &segref(segment_id_t id);
segment_id_t new_segment(void);
segment_id_t find_segment(identifier_id_t name);
void default_segment_setup(segment_t &so);

//////////////////////////////////////////////////////////////////////////////

extern std::vector<scope_t>				scopes;
extern std::vector<scope_id_t>				scope_stack;

//////////////////////////////////////////////////////////////////////////////

scope_t &scope(scope_id_t id);
scope_id_t new_scope(void);
scope_id_t push_new_scope(void);
void pop_scope(void);
scope_id_t current_scope(void);

//////////////////////////////////////////////////////////////////////////////

extern std::vector<symbol_t>				symbols;

//////////////////////////////////////////////////////////////////////////////

symbol_id_t match_str_symbol(const csliteral_t &csl);
symbol_id_t new_symbol(const identifier_id_t name);
symbol_t &symbol(symbol_id_t id);
bool sym_match_type(const symbol_t::type_t at,const symbol_t::type_t bt);
symbol_id_t lookup_symbol(scope_t &sco,const identifier_id_t name,const symbol_t::type_t st);
symbol_id_t lookup_symbol(const identifier_id_t name,const symbol_t::type_t st);
symbol_t::type_t classify_symbol(const declaration_specifiers_t &spec,const declarator_t &declor);
int prep_symbol_lookup(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor);
bool do_local_symbol_lookup(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor);
int check_symbol_param_match(symbol_lookup_t &sl,const ddip_list_t &p1,const ddip_list_t &p2);
int check_symbol_param_match(symbol_lookup_t &sl,const ddip_list_t &p);
int check_symbol_lookup_match(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor);
int add_symbol(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor);
segment_id_t decide_sym_segment(symbol_t &sym);
std::string ddip_list_to_str(const ddip_list_t &dl);

//////////////////////////////////////////////////////////////////////////////

extern segment_id_t					code_segment;
extern segment_id_t					const_segment;
extern segment_id_t					conststr_segment;
extern segment_id_t					data_segment;
extern segment_id_t					stack_segment;
extern segment_id_t					bss_segment;
extern segment_id_t					fardata_segment;

//////////////////////////////////////////////////////////////////////////////

addrmask_t calc_alignofmask(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0);
addrmask_t calc_alignof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0);
data_size_t calc_sizeof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0);
bool ast_constexpr_sizeof(token_t &r,token_t &op,size_t ptr_deref=0);

bool ast_constexpr_alignof(token_t &r,token_t &op,size_t ptr_deref=0);

bool ast_constexpr_to_bool(const token_t &t);
bool ast_constexpr_to_bool(const integer_value_t &iv);
bool ast_constexpr_to_bool(const floating_value_t &iv);

bool is_ast_strconstexpr(token_t &t);
bool is_ast_constexpr(token_t &t);

bool ast_constexpr_leftshift(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_rightshift(token_t &r,const token_t &op1,const token_t &op2);

bool ast_constexpr_lessthan_equals(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_greaterthan_equals(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_lessthan(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_greaterthan(token_t &r,const token_t &op1,const token_t &op2);

bool ast_constexpr_equals(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_not_equals(token_t &r,const token_t &op1,const token_t &op2);

bool ast_constexpr_binary_not(token_t &r,const token_t &op);
bool ast_constexpr_binary_or(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_binary_and(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_binary_xor(token_t &r,const token_t &op1,const token_t &op2);

bool ast_constexpr_logical_not(token_t &r,const token_t &op);
bool ast_constexpr_logical_or(token_t &r,token_t &op1,token_t &op2);
bool ast_constexpr_logical_and(token_t &r,token_t &op1,token_t &op2);

bool ast_constexpr_negate(token_t &r,const token_t &op);

bool ast_constexpr_add(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_subtract(token_t &r,const token_t &op1,const token_t &op2);

bool ast_constexpr_multiply(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_divide(token_t &r,const token_t &op1,const token_t &op2);
bool ast_constexpr_modulus(token_t &r,token_t &op1,token_t &op2);

//////////////////////////////////////////////////////////////////////////////

void debug_dump_ast(const std::string prefix,ast_node_id_t r);
void debug_dump_general(const std::string prefix,const std::string &name=std::string());
void debug_dump_declaration_specifiers(const std::string prefix,declaration_specifiers_t &ds);
void debug_dump_data_type_set(const std::string prefix,const data_type_set_t &dts,const std::string &name=std::string());
void debug_dump_data_type_set_ptr(const std::string prefix,const data_type_set_ptr_t &dtsp,const std::string &name=std::string());
void debug_dump_var_type(const std::string prefix,const data_var_type_t &dt,const std::string &name=std::string());
void debug_dump_ptr_type(const std::string prefix,const data_ptr_type_t &dt,const std::string &name=std::string());
void debug_dump_declarator(const std::string prefix,declarator_t &declr,const std::string &name=std::string());
void debug_dump_declaration(const std::string prefix,declaration_t &decl,const std::string &name=std::string());
void debug_dump_declaration_specifier_flags(const std::string prefix,const unsigned int flags,const std::string &name=std::string());
void debug_dump_pointer(const std::string prefix,std::vector<pointer_t> &ptr,const std::string &name=std::string());
void debug_dump_direct_declarator(const std::string prefix,declarator_t &ddecl,const std::string &name=std::string());
void debug_dump_arraydef(const std::string prefix,std::vector<ast_node_id_t> &arraydef,const std::string &name=std::string());
void debug_dump_parameter(const std::string prefix,parameter_t &p,const std::string &name=std::string());
void debug_dump_structfield(const std::string prefix,structfield_t &f,const std::string &name=std::string());
void debug_dump_ddip(const std::string prefix,ddip_t &ddip,const std::string &name=std::string());
void debug_dump_ddip(const std::string prefix,ddip_list_t &ddip,const std::string &name=std::string());
void debug_dump_symbol(const std::string prefix,symbol_t &sym,const std::string &name=std::string());
void debug_dump_symbol_table(const std::string prefix,const std::string &name=std::string());
void debug_dump_scope(const std::string prefix,scope_t &sco,const std::string &name=std::string());
void debug_dump_scope_table(const std::string prefix,const std::string &name=std::string());
void debug_dump_segment(const std::string prefix,segment_t &s,const std::string &name=std::string());
void debug_dump_segment_table(const std::string prefix,const std::string &name=std::string());
void debug_dump_enumerator(const std::string prefix,enumerator_t &en);

//////////////////////////////////////////////////////////////////////////////

bool arrange_symbols(void);
bool cc_init(void);

//////////////////////////////////////////////////////////////////////////////

void ast_node_reduce(ast_node_id_t &eroot,const std::string &prefix=std::string());

//////////////////////////////////////////////////////////////////////////////

struct cc_state_t {
	lgtok_state_t		lst;
	pptok_state_t		pst;
	rbuf*			buf = NULL;
	source_file_object*	sfo = NULL;

	std::vector<token_t>	tq;
	size_t			tq_tail = 0;
	int			err = 0;

	bool			ignore_whitespace = true;

	void tq_ft(void);
	void tq_refill(const size_t i=1);
	const token_t &tq_peek(const size_t i=0);
	void tq_discard(const size_t i=1);
	token_t &tq_get(void);
	int cc_chkerr(void);
	int do_pragma(void);
	int check_for_pragma(void);
	int do_pragma_comma_or_closeparens(void);
	int parse_declspec_align(addrmask_t &align);
	int typeid_or_expr_parse(ast_node_id_t &aroot);
	int ms_declspec_parse(declspec_t &dsc,const position_t &pos);
	int gnu_attribute_parse(declspec_t &dsc,const position_t &pos);
	int cpp11_attribute_parse(declspec_t &dsc,const position_t &pos);
	int declspec_alignas(addrmask_t &ds_align,const position_t &pos);
	int cpp11_attribute_namespace_parse(bool &nsv_using,std::vector<identifier_id_t> &nsv);
	cpp11attr_namespace_t cpp11_attribute_identify_namespace(std::vector<identifier_id_t> &nsv);
	int direct_declarator_inner_parse(ddip_list_t &dp,declarator_t &dd,position_t &pos,unsigned int flags=0);
	int direct_declarator_parse(declaration_specifiers_t &ds,declarator_t &dd,unsigned int flags=0);
	int declaration_inner_parse(declaration_specifiers_t &spec,declarator_t &declor);
	int declarator_parse(declaration_specifiers_t &ds,declarator_t &declor);
	int declaration_specifiers_parse(declaration_specifiers_t &ds,const unsigned int declspec = 0);
	int enumerator_list_parse(declaration_specifiers_t &ds,std::vector<symbol_id_t> &enum_list);
	int struct_declarator_parse(const symbol_id_t sid,declaration_specifiers_t &ds,declarator_t &declor);
	int asm_statement(ast_node_id_t &aroot);
	int struct_bitfield_validate(token_t &t);
	int struct_field_layout(symbol_id_t sid);
	int msasm_statement(ast_node_id_t &aroot);
	bool declaration_specifiers_check(const unsigned int token_offset=0);
	int compound_statement(ast_node_id_t &aroot,ast_node_id_t &nroot);
	int struct_declaration_parse(const symbol_id_t sid,const token_type_t &tt);
	int asm_statement_gcc_colon_section(std::vector<token_t> &tokens,int &parens);
	int multiplicative_expression(ast_node_id_t &aroot);
	int exclusive_or_expression(ast_node_id_t &aroot);
	int inclusive_or_expression(ast_node_id_t &aroot);
	int conditional_expression(ast_node_id_t &aroot);
	int logical_and_expression(ast_node_id_t &aroot);
	int logical_or_expression(ast_node_id_t &aroot);
	int assignment_expression(ast_node_id_t &aroot);
	int relational_expression(ast_node_id_t &aroot);
	int pointer_parse(std::vector<pointer_t> &ptr);
	int expression_statement(ast_node_id_t &aroot);
	int declaration_parse(declaration_t &declion);
	int additive_expression(ast_node_id_t &aroot);
	int equality_expression(ast_node_id_t &aroot);
	int postfix_expression(ast_node_id_t &aroot);
	int primary_expression(ast_node_id_t &aroot);
	int shift_expression(ast_node_id_t &aroot);
	int unary_expression(ast_node_id_t &aroot);
	int cast_expression(ast_node_id_t &aroot);
	int compound_statement_declarators(void);
	int and_expression(ast_node_id_t &aroot);
	int xor_expression(ast_node_id_t &aroot);
	int or_expression(ast_node_id_t &aroot);
	int initializer(ast_node_id_t &aroot);
	int expression(ast_node_id_t &aroot);
	int statement(ast_node_id_t &aroot);
	int external_declaration(void);
	int translation_unit(void);
};

int cc_step(cc_state_t &cc,rbuf &_buf,source_file_object &_sfo);

