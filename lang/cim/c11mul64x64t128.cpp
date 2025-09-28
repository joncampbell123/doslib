
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

#if 1//DEBUG use libgmp to check our work
#include <gmp.h>
#endif

void multiply64x64to128(uint64_t &f_rh,uint64_t &f_rl,const uint64_t a,const uint64_t b) {
	const uint64_t la = a & (uint64_t)0xffffffffull;
	const uint64_t lb = b & (uint64_t)0xffffffffull;
	const uint64_t ha = a >> (uint64_t)32u;
	const uint64_t hb = b >> (uint64_t)32u;

	/*         hb lb
	 *         ha la
	 *       x------
	 *         aa aa           la * lb
	 * +    bb bb ..           la * hb
	 * +    cc cc ..           ha * lb
	 * + dd dd .. ..           ha * hb
	 *   -----------
	 *
	 *         12
	 *       x 34
	 *       ----
	 *          8              4 * 2
	 *         4.              4 * 1
	 *         6.              3 * 2
	 *        3..              3 * 1
	 *       ----
	 *        408
	 *
	 *         67
	 *       x 89
	 *       ----
	 *         63              9 * 7
	 *        54.              9 * 6
	 *        56.              8 * 7
	 *       48..              8 * 6
	 *       ----
	 *       5963
	 */

	const uint64_t aa = la * lb;
	const uint64_t bb = la * hb;
	const uint64_t cc = ha * lb;
	const uint64_t dd = ha * hb;

	uint64_t rl = aa;
	uint64_t rh = dd;
	{
		const uint64_t t = rl + (bb << (uint64_t)32u);
		if (t < rl) rh++;
		rl = t;
	}
	{
		const uint64_t t = rl + (cc << (uint64_t)32u);
		if (t < rl) rh++;
		rl = t;
	}
	f_rh = rh + (bb >> (uint64_t)32u) + (cc >> (uint64_t)32u);
	f_rl = rl;

#if 1//DEBUG use libgmp to check our work
	{
		mpz_t ma,mb,mr;

		mpz_init_set_ui(ma,(uint32_t)(a >> (uint64_t)32u));
		mpz_mul_2exp(ma,ma,32);
		mpz_add_ui(ma,ma,(uint32_t)a);

		mpz_init_set_ui(mb,(uint32_t)(b >> (uint64_t)32u));
		mpz_mul_2exp(mb,mb,32);
		mpz_add_ui(mb,mb,(uint32_t)b);

		mpz_init(mr);
		mpz_mul(mr,ma,mb);

		uint64_t chk_lo  = (uint32_t)mpz_get_ui(mr);
		mpz_div_2exp(mr,mr,32);
		chk_lo += (uint64_t)((uint32_t)mpz_get_ui(mr)) << (uint64_t)32u;
		mpz_div_2exp(mr,mr,32);

		uint64_t chk_hi  = (uint32_t)mpz_get_ui(mr);
		mpz_div_2exp(mr,mr,32);
		chk_hi += (uint64_t)((uint32_t)mpz_get_ui(mr)) << (uint64_t)32u;

		mpz_clear(ma);
		mpz_clear(mb);
		mpz_clear(mr);

		if (f_rh != chk_hi || f_rl != chk_lo) {
			fprintf(stderr,"64x64 multiply fail:    Got 0x16%llx16%llx\n",(unsigned long long)f_rh,(unsigned long long)f_rl);
			fprintf(stderr,"                     Should 0x16%llx16%llx\n",(unsigned long long)chk_hi,(unsigned long long)chk_lo);
		}
	}
#endif
}

