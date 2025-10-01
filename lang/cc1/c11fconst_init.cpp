
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

extern "C" void c11yy_init_fconst(struct c11yy_struct_float *val,const char *yytext,const char lexmatch) {
	const char *str = yytext;
	int exponent = 0;

	*val = c11yy_struct_float_F_CONSTANT_INIT;

	/* NTS: The lex syntax file does not consider a leading negative sign part of this number,
	 *      therefore this code does not need to worry about negative numbers. */

	if (lexmatch == 'D') {
		const long double rv = strtold(yytext,(char**)(&yytext));
		const long double nv = frexpl(rv,&exponent);
		if (nv != 0.0l) {
			/* convert 0.5 <= x < 1.0 * (2^exponent) to IEEE representation */
			val->exponent = exponent - 1;
			val->mant = (uint64_t)floor((nv * 18446744073709551616.0l/*2^64*/) + 0.5l);
		}
	}
	else if (lexmatch == 'H') {
		unsigned char ldigit = 0;
		unsigned char shf = 64;
		int v;

		/* skip the "0x" */
		c11yyskip(yytext,2);

		/* read hex digits, fill the mantissa from the top bits down 4 bits at a time.
		 * shf must be a multiple of 4 at this point of the code.
		 * while filling the whole part, track the exponent.
		 * do not track the exponent for the fractional part. */

		/* whole part */
		/* skip leading zeros */
		while (*yytext == '0') yytext++;
		if (*yytext != '0') {
			exponent--; /* whole part is nonzero, adjust so the first digit has exponent=3 */
			while ((v=c11yy_iconst_readc(16,yytext)) >= 0) {
				if (shf >= 4) { /* assume shf a multiple of 4 */
					shf -= 4; exponent += 4; val->mant |= (uint64_t)v << (uint64_t)shf;
				}
				else {
					ldigit = (unsigned char)v;
					while ((v=c11yy_iconst_readc(16,yytext)) >= 0) exponent += 4;
					break;
				}
			}

			/* normalize mantissa */
			if (val->mant != 0ull) {
				while (!(val->mant & 0x8000000000000000ull)) {
					val->mant <<= 1ull;
					exponent--;
					shf++;
				}
			}
		}
		/* fractional part */
		if (*yytext == '.') {
			yytext++;
			while ((v=c11yy_iconst_readc(16,yytext)) >= 0) {
				if (shf >= 4) { /* assume shf a multiple of 4 */
					shf -= 4; val->mant |= (uint64_t)v << (uint64_t)shf;
				}
				else {
					ldigit = (unsigned char)v;
					while ((v=c11yy_iconst_readc(16,yytext)) >= 0);
					break;
				}
			}

			/* normalize mantissa */
			if (val->mant != 0ull) {
				while (!(val->mant & 0x8000000000000000ull)) {
					val->mant <<= 1ull;
					shf++;
				}
			}
		}
		/* was there a leftover digit that didn't fit, and normalization made some room for it? */
		if (shf && ldigit) {
			/* assume shf < 4 then (leftover bits to fill), ldigit is not set otherwise */
			val->mant |= (uint64_t)ldigit >> (uint64_t)(4u - shf);
			shf = 0;
		}
		/* parse the 'p' power of 2 exponent */
		if (*yytext == 'p') {
			struct c11yy_struct_integer val = c11yy_struct_integer_I_CONSTANT_INIT;
			bool negative = false;

			yytext++;
			if (*yytext == '-') {
				negative = true;
				yytext++;
			}

			c11yy_iconst_read(/*base*/10,/*&*/val,yytext);
			if (negative) exponent -= (int)val.v.s;
			else exponent += (int)val.v.s;
		}

		val->exponent = exponent;
	}

	if (*yytext == 'f' || *yytext == 'F') {
		val->sz = 4; /* float */
	}
	else if (*yytext == 'l' || *yytext == 'L') {
		val->sz = 10; /* long double */
	}
#if 0
	const long double rv = ldexpl((long double)val->mant,val->exponent - 63);
	fprintf(stderr,"flt %.6f flags=%lx sz=%u exp=%d mant=0x%016llx '%s'\n",
		(double)rv,
		(unsigned long)val->flags,
		(unsigned int)val->sz,
		(unsigned int)val->exponent,
		(unsigned long long)val->mant,
		str);
#endif
}

