/* C++ only header */
#ifdef __cplusplus

void c11yyskip(const char* &s,unsigned int c);
int c11yy_iconst_readc(const unsigned int base,const char* &s);
void c11yy_iconst_readsuffix(struct c11yy_struct_integer &val,const char* &s);
void c11yy_iconst_read(const unsigned int base,struct c11yy_struct_integer &val,const char* &s);
void c11yy_iconst_readchar(const enum c11yystringtype st,struct c11yy_struct_integer &val,const char* &s);
void c11yy_check_stringtype_prefix(enum c11yystringtype &st,const char* &s);
void c11yy_write_utf16(uint16_t* &d,uint32_t c);
void c11yy_write_utf8(uint8_t* &d,uint32_t c);
uint32_t c11yy_read_utf8(const char* &s);
void c11yy_scopes_table_clear(void);
void c11yy_string_table_clear(void);

#include <vector>

extern std::vector<struct c11yy_string_obj> c11yy_string_table;
extern std::vector<c11yy_identifier_obj> c11yy_ident_table;

struct c11yy_scope_obj {
						c11yy_scope_obj();
						~c11yy_scope_obj();
};

extern std::vector<struct c11yy_scope_obj> c11yy_scope_table;

void c11yy_string_obj_free(struct c11yy_string_obj &o);
void c11yy_string_table_clear(void);

/* this is a typedef. we're going to allow private trees in addition to the global tree.
 * of course the node IDs for ASTNODE refer then to the private tree. */
typedef std::vector<union c11yy_struct> c11yy_astnode_array;

extern c11yy_astnode_array c11yy_astnode;

struct c11yy_struct_astnode &c11yy_astnode_ref(c11yy_astnode_array &anr,const c11yy_astnode_id id);
const struct c11yy_struct_astnode &c11yy_astnode_ref(const c11yy_astnode_array &anr,const c11yy_astnode_id id);

void multiply64x64to128(uint64_t &f_rh,uint64_t &f_rl,const uint64_t a,const uint64_t b);
int c11yy_mul_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b);
int c11yy_div_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b);
int c11yy_add_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_sub_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_mul_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_div_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_mod_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_addsub_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b,unsigned int aflags);
int c11yy_fconst_match_mantissa_prep(int &exp,uint64_t &ama,uint64_t &bma,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b);
int c11yy_shl_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_shr_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b);
int c11yy_cmp_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b,const enum c11yy_cmpop op);
int c11yy_cmp_fconst(struct c11yy_struct_integer &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b,const enum c11yy_cmpop op);
int c11yy_binop_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b,const enum c11yy_binop op);
int c11yy_logop_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b,const enum c11yy_logop op);

#endif

