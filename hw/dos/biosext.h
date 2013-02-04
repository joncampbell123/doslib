/* biosext.h
 *
 * DOS library for carrying out extended memory copy using BIOS INT 15h
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdint.h>

#if TARGET_MSDOS == 16
int bios_extcopy(uint32_t dst,uint32_t src,unsigned long copy);
#endif

