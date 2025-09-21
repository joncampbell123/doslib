
int c11yy_char2digit(const char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'z')
		return c + 10 - 'a';
	else if (c >= 'A' && c <= 'Z')
		return c + 10 - 'A';
	else
		return -1;
}

