/* himemsys.h
 *
 * Support calls to use HIMEM.SYS
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)

/* HMA memory is present */
#define HIMEM_F_HMA		(1 << 0)
/* HIMEM.SYS supports extended functions to address up to 4GB of RAM (surpassing the older API's 64MB limit) */
#define HIMEM_F_4GB		(1 << 1)

extern unsigned char himem_sys_present;
extern unsigned int himem_sys_version;
extern unsigned long himem_sys_entry;
extern unsigned char himem_sys_flags;
extern unsigned long himem_sys_total_free;
extern unsigned long himem_sys_largest_free;

#pragma pack(push,1)
struct himem_block_info {
	uint32_t		block_length_kb;
	unsigned char		lock_count;
	unsigned char		free_handles;
};
#pragma pack(pop)

int probe_himem_sys();
int himem_sys_query_a20();
int himem_sys_local_a20(int enable);
int himem_sys_global_a20(int enable);
void himem_sys_update_free_memory_status();
int __cdecl himem_sys_alloc(unsigned long size/* in KB---not bytes*/);
int himem_sys_move(unsigned int dst_handle,uint32_t dst_offset,unsigned int src_handle,uint32_t src_offset,uint32_t length);
int __cdecl himem_sys_realloc(unsigned int handle,unsigned long size/* in KB---not bytes*/);
int himem_sys_get_handle_info(unsigned int handle,struct himem_block_info *b);
uint32_t himem_sys_lock(unsigned int handle);
int himem_sys_unlock(unsigned int handle);
int himem_sys_free(int handle);

#endif /* !defined(TARGET_WINDOWS) && !defined(TARGET_OS2) */

