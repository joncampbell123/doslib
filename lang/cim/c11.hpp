/* C++ only header */
#ifdef __cplusplus

void c11yyskip(const char* &s,unsigned int c);
int c11yy_iconst_readc(const unsigned int base,const char* &s);
void c11yy_iconst_readsuffix(struct c11yy_struct_integer &val,const char* &s);
void c11yy_iconst_read(const unsigned int base,struct c11yy_struct_integer &val,const char* &s);
void c11yy_iconst_readchar(const enum c11yystringtype st,struct c11yy_struct_integer &val,const char* &s);
void c11yy_check_stringtype_prefix(enum c11yystringtype &st,const char* &s);
void c11yy_read_utf8(struct c11yy_struct_integer &val,const char* &s);

#endif

