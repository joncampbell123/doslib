
void c11yyskip(const char* &s,unsigned int c) {
	while (*s && c) {
		s++;
		c--;
	}
}

