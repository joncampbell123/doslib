
#include <stdint.h>
#include <stdio.h>
#include <math.h>

static void TESTd(const double x) {
	printf("0x%016llx     f=%f a=%a A=%A\n",(unsigned long long)*((uint64_t*)(&x)),x,x,x);
}

static void TESTl(const long double x) {
	printf("0x%04x%016llx Lf=%Lf La=%La LA=%LA\n",((uint16_t*)(&x))[4],(unsigned long long)*((uint64_t*)(&x)),x,x,x);
}

#define TEST(x) {\
	const double dv = x;\
	const long double lv = x ## l;\
	TESTd(dv); \
	TESTl(lv); }

int main(void) {
	TEST(1.0);
	TEST(1.5);
	TEST(1.75);
	TEST(2.0);
	TEST(3.0);
	TEST(4.0);
	TEST(0.0);
	TEST(-1.0);
	TEST(-1.5);
	TEST(-1.75);
	TEST(-2.0);
	TEST(-3.0);
	TEST(-4.0);
	return 0;
}

