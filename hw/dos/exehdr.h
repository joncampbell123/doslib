
#ifndef __HW_DOS_EXEHDR_H
#define __HW_DOS_EXEHDR_H

#include <stdint.h>

#pragma pack(push,1)
struct exe_dos_header {
    uint16_t            magic;                      // +0x00 0x5A4D 'MZ'
    uint16_t            last_block_bytes;           // +0x02 number of bytes in the last block if nonzero, or zero for whole block
    uint16_t            exe_file_blocks;            // +0x04 number of 512-byte blocks that make up the EXE (resident part + header)
    uint16_t            number_of_relocations;      // +0x06 number of relocation entries
    uint16_t            header_size_paragraphs;     // +0x08 header size in paragraphs (16-byte units)
    uint16_t            min_additional_paragraphs;  // +0x0A minimum additional memory needed in paragraphs
    uint16_t            max_additional_paragraphs;  // +0x0C maximum additional memory in paragraphs
    uint16_t            init_stack_segment;         // +0x0E initial stack segment, relative to loaded EXE base
    uint16_t            init_stack_pointer;         // +0x10 initial stack pointer
    uint16_t            checksum;                   // +0x12 if nonzero, 16-bit checksum of all words in the file
    uint16_t            init_instruction_pointer;   // +0x14 initial value of instruction pointer
    uint16_t            init_code_segment;          // +0x16 initial code segment, relative to loaded EXE base
    uint16_t            relocation_table_offset;    // +0x18 offset of first relocation table entry
    uint16_t            overlay_number;             // +0x1A overlay number, 0 if main EXE
                                                    // =0x1C total header size
};

struct exe_dos_header_ne {
    struct exe_dos_header   main;                   // +0x00 main EXE header
    uint32_t                ne_offset;              // +0x1C offset of NE/PE/LE/LX/etc header
                                                    // =0x20 total header size
};
#pragma pack(pop)

static inline unsigned long exe_dos_header_file_header_size(const struct exe_dos_header * const header) {
    return ((unsigned long)(header->header_size_paragraphs)) << 4UL; /* *16 */
}

static inline unsigned long exe_dos_header_bss_size(const struct exe_dos_header * const header) {
    return ((unsigned long)(header->min_additional_paragraphs)) << 4UL; /* *16 */
}

static inline unsigned long exe_dos_header_file_resident_size(const struct exe_dos_header * const header) {
    unsigned long ret;

    if (header->exe_file_blocks == 0)
        return 0;

    ret = ((unsigned long)(header->exe_file_blocks)) << 9UL; /* *512 */
    if (header->last_block_bytes != 0U)
        ret += (unsigned long)header->last_block_bytes - 512UL;

    return ret;
}

#endif //__HW_DOS_EXEHDR_H

