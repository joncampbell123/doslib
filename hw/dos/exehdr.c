
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <hw/dos/exehdr.h>

int exe_dos_header_to_layout(struct exe_dos_layout * const lay,const struct exe_dos_header * const hdr) {
    unsigned long t;

    lay->min_mem_footprint = 0;
    lay->max_mem_footprint = 0;
    lay->code_entry_point_res = 0;
    lay->code_entry_point_file = 0;
    lay->stack_entry_point_res = 0;
    lay->stack_entry_point_file = 0;
    exe_dos_layout_range_deinit(&lay->header);
    exe_dos_layout_range_deinit(&lay->run_resident);
    exe_dos_layout_range_deinit(&lay->load_resident);
    exe_dos_layout_range_deinit(&lay->relocation_table);
    if (hdr->magic != 0x5A4DU/*MZ*/)
        return -1;

    t = exe_dos_header_file_header_size(hdr);
    if (t == 0UL) return -1;
    exe_dos_layout_range_set(&lay->header,0,t - 1UL);

    /* exit now if EXE header denies it's own size */
    if (t < 0x1C) return 0;

    /* resident section follows EXE header.
     * EXE header expresses resident section as EXE header + resident section.
     * I'm guessing this made the DOS kernel's job easier since it helps dictate
     * how much to load into memory before beginning to parse the EXE header and
     * relocation table. */
    t = exe_dos_header_file_resident_size(hdr);
    if (t == 0UL) return 0;
    exe_dos_layout_range_set(&lay->load_resident,0,t - 1UL);

    /* once loaded, the resident portion no longer includes EXE header.
     * if the resident area is smaller than EXE header then bail now. */
    if (t <= exe_dos_header_file_header_size(hdr)) return 0;
    exe_dos_layout_range_set(&lay->run_resident,exe_dos_header_file_header_size(hdr),lay->load_resident.end);

    /* finally, if there is a relocation table, note it.
     * each relocation is 4 bytes long (32 bits), and stores a (relative SEG):OFF address of a 16-bit WORD that DOS adds the segment base to.
     * if the EXE header somehow puts the relocation table out of the resident range, well, that's the EXE's stupidity. */
    if (hdr->number_of_relocations != 0U) {
        exe_dos_layout_range_set(&lay->relocation_table,
            hdr->relocation_table_offset,
            hdr->relocation_table_offset + (hdr->number_of_relocations * 4UL) - 1UL);
    }

    /* we can determine runtime memory footprint too */
    lay->min_mem_footprint =
        exe_dos_layout_range_get_length(&lay->run_resident) +
        ((uint32_t)(hdr->min_additional_paragraphs) << 4UL);
    lay->max_mem_footprint =
        exe_dos_layout_range_get_length(&lay->run_resident) +
        ((uint32_t)(hdr->max_additional_paragraphs) << 4UL);

    /* we can also pinpoint CS:IP and SS:IP */
    /* NTS: we mask by 0xFFFFFUL for 16-bit real mode 1MB aliasing.
     *      some early EXE files are in fact .COM converted to .EXE
     *      and then the instruction pointer in the header set to
     *      0xFFEF:0x0100 to set CS:IP up like a .COM executable.
     *      that only works because of 1MB wraparound and because
     *      of 16-bit segment registers. */
    lay->code_entry_point_res =
        ((uint32_t)((hdr->init_code_segment << 4UL) +
         (uint32_t)(hdr->init_instruction_pointer))) & 0xFFFFFUL;
    if (lay->code_entry_point_res <= lay->run_resident.end) {
        lay->code_entry_point_file =
            lay->code_entry_point_res + lay->run_resident.start;
    }

    /* NTS: it doesn't happen often, but the EXE header can define
     *      SS:SP to point within the resident image at startup. */
    lay->stack_entry_point_res =
        ((uint32_t)((hdr->init_stack_segment << 4UL) +
         (uint32_t)(hdr->init_stack_pointer))) & 0xFFFFFUL;
    if (lay->stack_entry_point_res <= lay->run_resident.end) {
        lay->stack_entry_point_file =
            lay->stack_entry_point_res + lay->run_resident.start;
    }

    return 0;
}

