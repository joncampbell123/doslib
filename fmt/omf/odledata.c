
#include <fmt/omf/omf.h>

static void print_b(FILE *fp,unsigned char c) {
    if (c >= 32 && c < 127)
        fprintf(fp,"%c",(char)c);
    else
        fprintf(fp,".");
}

void dump_LEDATA(FILE *fp,const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info) {
    unsigned int col,colstart;
    unsigned long i,pos,ics;
    unsigned int ci;

    fprintf(fp,"LEDATA segment=\"%s\"(%u) data_offset=0x%lX(%lu) length=0x%lX(%lu)\n",
        omf_context_get_segdef_name_safe(ctx,info->segment_index),
        info->segment_index,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->data_length,
        (unsigned long)info->data_length);

    i = 0;
    col = 0;
    pos = info->enum_data_offset;
    colstart = (unsigned int)(pos & 0xFUL);
    while (i < info->data_length) {
        if (col == 0) {
            fprintf(fp,"    0x%08lX: ",pos & (~0xFUL));
            ics = i;
        }

        while (col < (unsigned int)(pos & 0xFUL)) {
            fprintf(fp,"  %c",col==7?'-':' ');
            col++;
        }

        fprintf(fp,"%02X%c",info->data[i],col==7?'-':' ');
        col++;
        pos++;
        i++;

        if (col == 0x10) {
            ci=0;
            while (ci < colstart) {
                fprintf(fp," ");
                ci++;
            }

            assert((ics+ci) >= colstart);
            while (ci < 16) {
                print_b(fp,info->data[ics+ci-colstart]);
                ci++;
            }

            colstart = 0;
            fprintf(fp,"\n");
            col = 0;
        }
    }

    if (col != 0) {
        unsigned int colend = col;

        while (col < 16) {
            fprintf(fp,"  %c",col==7?'-':' ');
            col++;
        }

        ci=0;
        while (ci < colstart) {
            fprintf(fp," ");
            ci++;
        }

        assert((ics+ci) >= colstart);
        while (ci < colend) {
            print_b(fp,info->data[ics+ci-colstart]);
            ci++;
        }

        fprintf(fp,"\n");
    }
}

