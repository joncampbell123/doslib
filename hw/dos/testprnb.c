
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>

static void TEST(const unsigned char *x,size_t sz) {
	printf("0x%p: ",x);
	while (sz != (size_t)0)
		printf("%02x",x[--sz]);
	printf("\n");
}

/* HA! Found the one possible way that Open Watcom -fld allows me to declare a long double AND obtain the address of it! */
static const long double	lv = 1.0l;
static const double		dv = 1.0;

int main(void) {
	TEST((unsigned char*)(&dv),sizeof(dv));
	TEST((unsigned char*)(&lv),sizeof(lv));
	return 0;
}

