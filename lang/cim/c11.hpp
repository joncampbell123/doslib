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

struct c11yy_scope_obj {
	std::vector<c11yy_identifier_obj>	idents;

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

#endif

