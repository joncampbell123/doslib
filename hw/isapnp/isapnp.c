
#if !defined(TARGET_PC98)

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <dos/dos.h>
#include <isapnp/isapnp.h>

#include <hw/8254/8254.h>		/* 8254 timer */

uint16_t			isapnp_read_data = 0;
uint8_t				isapnp_probe_next_csn = 0;

uint16_t			isa_pnp_bios_offset = 0;
struct isa_pnp_bios_struct	isa_pnp_info;
unsigned int			(__cdecl far *isa_pnp_rm_call)() = NULL;

#if TARGET_MSDOS == 32
uint16_t			isa_pnp_pm_code_seg = 0;	/* code segment to call protected mode BIOS if */
uint16_t			isa_pnp_pm_data_seg = 0;	/* data segment to call protected mode BIOS if */
uint8_t				isa_pnp_pm_use = 0;		/* 1=call protected mode interface  0=call real mode interface */
uint8_t				isa_pnp_pm_win95_mode = 0;	/* 1=call protected mode interface with 32-bit stack (Windows 95 style) */
uint16_t			isa_pnp_rm_call_area_code_sel = 0;
uint16_t			isa_pnp_rm_call_area_sel = 0;
void*				isa_pnp_rm_call_area = NULL;
#endif

int init_isa_pnp_bios() {
#if TARGET_MSDOS == 32
	isa_pnp_rm_call_area_code_sel = 0;
	isa_pnp_rm_call_area_sel = 0;
	isa_pnp_pm_code_seg = 0;
	isa_pnp_pm_data_seg = 0;
	isa_pnp_pm_use = 0;
#endif
	return 1;
}

void free_isa_pnp_bios() {
#if TARGET_MSDOS == 32
	if (isa_pnp_pm_code_seg != 0)
		dpmi_free_descriptor(isa_pnp_pm_code_seg);
	if (isa_pnp_pm_data_seg != 0)
		dpmi_free_descriptor(isa_pnp_pm_data_seg);
	if (isa_pnp_rm_call_area_code_sel != 0)
		dpmi_free_descriptor(isa_pnp_rm_call_area_code_sel);
	if (isa_pnp_rm_call_area_sel != 0)
		dpmi_free_dos(isa_pnp_rm_call_area_sel);
	isa_pnp_pm_code_seg = 0;
	isa_pnp_pm_data_seg = 0;
	isa_pnp_rm_call_area_sel = 0;
	isa_pnp_rm_call_area_code_sel = 0;
	isa_pnp_rm_call_area = NULL;
	isa_pnp_pm_use = 0;
#endif
}

int find_isa_pnp_bios() {
#if TARGET_MSDOS == 32
	uint8_t *scan = (uint8_t*)0xF0000;
#else
	uint8_t far *scan = (uint8_t far*)MK_FP(0xF000U,0x0000U);
#endif
	unsigned int offset,i;

	free_isa_pnp_bios();
	isa_pnp_bios_offset = (uint16_t)(~0U);
	memset(&isa_pnp_info,0,sizeof(isa_pnp_info));

	/* NTS: Stop at 0xFFE0 because 0xFFE0+0x21 >= end of 64K segment */
	for (offset=0U;offset != 0xFFE0U;offset += 0x10U,scan += 0x10) {
		if (scan[0] == '$' && scan[1] == 'P' && scan[2] == 'n' &&
			scan[3] == 'P' && scan[4] >= 0x10 && scan[5] >= 0x21) {
			uint8_t chk=0;
			for (i=0;i < scan[5];i++)
				chk += scan[i];

			if (chk == 0) {
				isa_pnp_bios_offset = (uint16_t)offset;
				_fmemcpy(&isa_pnp_info,scan,sizeof(isa_pnp_info));
				isa_pnp_rm_call = (void far*)MK_FP(isa_pnp_info.rm_ent_segment,
					isa_pnp_info.rm_ent_offset);

#if TARGET_MSDOS == 32
				isa_pnp_rm_call_area = dpmi_alloc_dos(ISA_PNP_RM_DOS_AREA_SIZE,&isa_pnp_rm_call_area_sel);
				if (isa_pnp_rm_call_area == NULL) {
					fprintf(stderr,"WARNING: Failed to allocate common area for DOS realmode calling\n");
					goto fail;
				}

				/* allocate descriptors to make calling the BIOS from pm mode */
				if ((isa_pnp_pm_code_seg = dpmi_alloc_descriptor()) != 0) {
					if (!dpmi_set_segment_base(isa_pnp_pm_code_seg,isa_pnp_info.pm_ent_segment_base)) {
						fprintf(stderr,"WARNING: Failed to set segment base\n"); goto fail; }
					if (!dpmi_set_segment_limit(isa_pnp_pm_code_seg,0xFFFFUL)) {
						fprintf(stderr,"WARNING: Failed to set segment limit\n"); goto fail; }
					if (!dpmi_set_segment_access(isa_pnp_pm_code_seg,0x9A)) {
						fprintf(stderr,"WARNING: Failed to set segment access rights\n"); goto fail; }
				}

				if ((isa_pnp_pm_data_seg = dpmi_alloc_descriptor()) != 0) {
					if (!dpmi_set_segment_base(isa_pnp_pm_data_seg,isa_pnp_info.pm_ent_data_segment_base)) {
						fprintf(stderr,"WARNING: Failed to set segment base\n"); goto fail; }
					if (!dpmi_set_segment_limit(isa_pnp_pm_data_seg,0xFFFFUL)) {
						fprintf(stderr,"WARNING: Failed to set segment limit\n"); goto fail; }
					if (!dpmi_set_segment_access(isa_pnp_pm_data_seg,0x92)) {
						fprintf(stderr,"WARNING: Failed to set segment access rights\n"); goto fail; }
				}

				/* allocate code selector for realmode area */
				if ((isa_pnp_rm_call_area_code_sel = dpmi_alloc_descriptor()) != 0) {
					if (!dpmi_set_segment_base(isa_pnp_rm_call_area_code_sel,(uint32_t)isa_pnp_rm_call_area)) {
						fprintf(stderr,"WARNING: Failed to set segment base\n"); goto fail; }
					if (!dpmi_set_segment_limit(isa_pnp_rm_call_area_code_sel,0xFFFFUL)) {
						fprintf(stderr,"WARNING: Failed to set segment limit\n"); goto fail; }
					if (!dpmi_set_segment_access(isa_pnp_rm_call_area_code_sel,0x9A)) {
						fprintf(stderr,"WARNING: Failed to set segment access rights\n"); goto fail; }
				}

				isa_pnp_pm_use = 1;
#endif

				return 1;
#if TARGET_MSDOS == 32
fail:				free_isa_pnp_bios();
				return 0;
#endif
			}
		}
	}

	return 0;
}

#endif //!defined(TARGET_PC98)

