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
void c11yy_string_table_clear(void);

#include <vector>

extern std::vector<struct c11yy_string_obj> c11yy_string_table;

void c11yy_string_obj_free(struct c11yy_string_obj &o);
void c11yy_string_table_clear(void);

#endif

