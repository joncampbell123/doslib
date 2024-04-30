
extern int yydebug;

void (*cimcc_read_input)(char *buffer,int *size,int max_size);

int cimcc_yyparse(void);

