
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <stdint.h>

#if TARGET_MSDOS == 16

unsigned char flatrealmode_readb(uint32_t addr);
#pragma aux flatrealmode_readb = \
	".386p" \
	"push	es" \
	"xor	cx,cx" \
	"mov	es,cx" \
	"shl	edx,16" \
	"and	eax,0xFFFF" \
	"mov	al,es:[eax+edx]" \
	"pop	es" \
	modify [cx dx] \
	parm [dx ax] \
	value [al]

uint16_t flatrealmode_readw(uint32_t addr);
#pragma aux flatrealmode_readw = \
	".386p" \
	"push	es" \
	"xor	cx,cx" \
	"mov	es,cx" \
	"shl	edx,16" \
	"and	eax,0xFFFF" \
	"mov	ax,es:[eax+edx]" \
	"pop	es" \
	modify [cx dx] \
	parm [dx ax] \
	value [ax]

uint32_t flatrealmode_readd(uint32_t addr);
#pragma aux flatrealmode_readd = \
	".386p" \
	"push	es" \
	"xor	cx,cx" \
	"mov	es,cx" \
	"shl	edx,16" \
	"and	eax,0xFFFF" \
	"mov	eax,es:[eax+edx]" \
	"mov	edx,eax" \
	"shr	edx,16" \
	"pop	es" \
	modify [cx] \
	parm [dx ax] \
	value [dx ax]

void flatrealmode_writeb(uint32_t addr,uint8_t c);
#pragma aux flatrealmode_writeb = \
	".386p" \
	"push	es" \
	"xor	cx,cx" \
	"mov	es,cx" \
	"shl	edx,16" \
	"and	eax,0xFFFF" \
	"mov	es:[eax+edx],bl" \
	"pop	es" \
	modify [cx dx] \
	parm [dx ax] [bl]

void flatrealmode_writew(uint32_t addr,uint16_t c);
#pragma aux flatrealmode_writew = \
	".386p" \
	"push	es" \
	"xor	cx,cx" \
	"mov	es,cx" \
	"shl	edx,16" \
	"and	eax,0xFFFF" \
	"mov	es:[eax+edx],bx" \
	"pop	es" \
	modify [cx dx] \
	parm [dx ax] [bx]

void flatrealmode_writed(uint32_t addr,uint32_t c);
#pragma aux flatrealmode_writed = \
	".386p" \
	"push	es" \
	"push	cx" \
	"xor	cx,cx" \
	"mov	es,cx" \
	"pop	cx" \
	"shl	edx,16" \
	"and	eax,0xFFFF" \
	"shl	ecx,16" \
	"and	ebx,0xFFFF" \
	"add	ebx,ecx" \
	"mov	es:[eax+edx],ebx" \
	"pop	es" \
	modify [bx cx dx] \
	parm [dx ax] [cx bx]

int flatrealmode_setup(uint32_t limit);
int flatrealmode_test(); /* ASM */ /* 1=flat real mode not active   0=flat real mode active */ /* FIXME: Give this a more descriptive name. It's confusing in it's current form */

/* you can't do flat real mode when Windows is running. Nor can you use it when running in virtual 8086 mode (such as when EMM386.EXE is resident) */
#define flatrealmode_allowed() (cpu_v86_active == 0 && windows_mode == WINDOWS_NONE)
#define flatrealmode_ok() (flatrealmode_test() == 0)

/* feed this constant into the setup function to enable the full 4GB range */
#define FLATREALMODE_4GB			0xFFFFFFFFUL
/* feed this constant into the setup function to re-impose the 64KB real-mode limit */
#define FLATREALMODE_64KB			0x0000FFFFUL

#endif /* TARGET_MSDOS == 16 */

#if TARGET_MSDOS == 32
# define flatrealmode_allowed() (0)
# define flatrealmode_ok() (0)
#endif

