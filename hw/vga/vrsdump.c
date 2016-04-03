
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vrl.h"
#include "vrs.h"
#include "pcxfmt.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

unsigned char		*buffer = NULL;
unsigned char		*fence = NULL;

struct vrs_header	*vrshdr = NULL;

int main(int argc,char **argv) {
	unsigned long sz,offs;
	int fd;

	if (argc < 2) {
		fprintf(stderr,"vrsdump <vrs file>\n");
		return 1;
	}

	fd = open(argv[1],O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"cannot open %s\n",argv[1]);
		return 1;
	}

	sz = lseek(fd,0,SEEK_END);
#if TARGET_MSDOS == 16
	if (sz > 0xFFFFUL) {
		fprintf(stderr,"Too large\n");
		return 1;
	}
#endif
	if (sz == 0) {
		fprintf(stderr,"Zero length file\n");
		return 1;
	}

	/* VRS sheets are supposed to be loaded into memory as one binary blob.
	 * All file offsets become offsets relative to the base memory address of the blob. */
	buffer = malloc(sz);
	if (buffer == NULL) {
		fprintf(stderr,"Failed to alloc buffer\n");
		return 1;
	}
	fence = buffer + sz;
	lseek(fd,0,SEEK_SET);
	if (read(fd,buffer,sz) != (int)sz) {
		fprintf(stderr,"Failed to read all data\n");
		return 1;
	}

	vrshdr = (struct vrs_header*)buffer;
	if (memcmp(vrshdr->vrs_sig,"VRS1",4)) {
		fprintf(stderr,"Not a VRS file\n");
		return 1;
	}

	printf("VRS header:\n");
	printf("    Resident size:          %lu\n",(unsigned long)vrshdr->resident_size);
	printf("    Offset of VRS list:     %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_VRS_LIST]);
	printf("    Offset of sprite IDs:   %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_ID_LIST]);
	printf("    Offset of sprite names: %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_NAME_LIST]);
	printf("    Offset of anim list:    %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_LIST]);
	printf("    Offset of anim IDs:     %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_ID_LIST]);
	printf("    Offset of anim names:   %lu\n",(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_NAME_LIST]);

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_VRS_LIST]) != 0UL) {
		if ((offs+4UL) <= sz) {
			uint32_t *lst = (uint32_t*)(buffer + offs);
			uint32_t *fnc = (uint32_t*)(fence + 1 - sizeof(uint32_t));
			unsigned char *chk;
			unsigned int c=0;

			printf("*VRS offsets: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 16) {
					c = 0;
					printf("\n");
				}
				printf("%lu ",(unsigned long)(*lst));

				if ((*lst) >= (sz - 8UL)) {
					printf("*ERROR offset out of bounds\n");
					c = 0;
				}
				else {
					chk = buffer + *lst;
					if (memcmp(chk,"VRL1",4)) {
						printf("*ERROR entry not a VRL sprite\n");
						c = 0;
					}
				}

				lst++;
			} while (1);
			printf("\n");
		}
		else {
			printf("*VRS list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_ID_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			uint16_t *lst = (uint16_t*)(buffer + offs);
			uint16_t *fnc = (uint16_t*)(fence + 1 - sizeof(uint16_t));
			unsigned int c=0;

			printf("*Sprite IDs: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 32) {
					c = 0;
					printf("\n");
				}
				printf("%u ",(unsigned int)(*lst++));
			} while (1);
			printf("\n");
		}
		else {
			printf("*sprite ID list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_SPRITE_NAME_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			/* an array of C strings terminated by a NUL string (0x00 0x00) */
			char *s = (char*)buffer + offs;
			char *f = (char*)fence;
			unsigned int c=0;
			char *name;

			printf("*Sprite names: ");
			do {
				if (s > f) printf("*ERROR list overflow\n");
				if (s >= f) break;

				name = s;
				if (*name == 0) break;

				/* scan until NUL */
				while (s < f && *s != 0) s++;
				if (s == f) {
					printf("*ERROR list overflow, string not terminated\n");
					break;
				}
				if (*s == 0) s++; // then step over NUL

				if ((++c) >= 8) {
					c = 0;
					printf("\n");
				}
				printf("\"%s\" ",name);
			} while (1);
			printf("\n");
		}
		else {
			printf("*sprite name list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_LIST]) != 0UL) {
		if ((offs+4UL) <= sz) {
			uint32_t *lst = (uint32_t*)(buffer + offs);
			uint32_t *fnc = (uint32_t*)(fence + 1 - sizeof(uint32_t));
			unsigned int c=0;

			printf("*Animation list offsets: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 16) {
					c = 0;
					printf("\n");
				}
				printf("%lu ",(unsigned long)(*lst));

				if ((*lst) >= (sz - 8UL)) {
					printf("*ERROR offset out of bounds\n");
					c = 0;
				}

				lst++;
			} while (1);
			printf("\n");
		}
		else {
			printf("*Animation list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_ID_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			uint16_t *lst = (uint16_t*)(buffer + offs);
			uint16_t *fnc = (uint16_t*)(fence + 1 - sizeof(uint16_t));
			unsigned int c=0;

			printf("*Animation IDs: ");
			do {
				if (lst >= fnc) {
					printf("*ERROR list overflow\n");
					break;
				}
				if (*lst == 0) break;
				if ((++c) >= 32) {
					c = 0;
					printf("\n");
				}
				printf("%u ",(unsigned int)(*lst++));
			} while (1);
			printf("\n");
		}
		else {
			printf("*Animation ID list offset out range!\n");
			/* error condition */
		}
	}

	if ((offs=(unsigned long)vrshdr->offset_table[VRS_HEADER_OFFSET_ANIMATION_NAME_LIST]) != 0UL) {
		if ((offs+2UL) <= sz) {
			/* an array of C strings terminated by a NUL string (0x00 0x00) */
			char *s = (char*)buffer + offs;
			char *f = (char*)fence;
			unsigned int c=0;
			char *name;

			printf("*Animation names: ");
			do {
				if (s > f) printf("*ERROR list overflow\n");
				if (s >= f) break;

				name = s;
				if (*name == 0) break;

				/* scan until NUL */
				while (s < f && *s != 0) s++;
				if (s == f) {
					printf("*ERROR list overflow, string not terminated\n");
					break;
				}
				if (*s == 0) s++; // then step over NUL

				if ((++c) >= 8) {
					c = 0;
					printf("\n");
				}
				printf("\"%s\" ",name);
			} while (1);
			printf("\n");
		}
		else {
			printf("*Animation name list offset out range!\n");
			/* error condition */
		}
	}

	close(fd);
	return 0;
}

