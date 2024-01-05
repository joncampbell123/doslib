
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

#ifdef __WATCOMC__
# define FLD __declspec(fld)
#else
# define FLD
#endif

static const long double			lv = 1.0l;
static const FLD long double			lv2 = 1.0l;
static const double				dv = 1.0;

int main(void) {
	printf("long double:                  "); TEST((unsigned char*)(&lv), sizeof(lv));
	printf("long double __declspec(fld):  "); TEST((unsigned char*)(&lv2),sizeof(lv2));
	printf("double:                       "); TEST((unsigned char*)(&dv), sizeof(dv));
	return 0;
}

