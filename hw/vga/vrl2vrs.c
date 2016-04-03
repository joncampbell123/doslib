
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
#include "comshtps.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static unsigned char			tempbuffer[8192];

static void help() {
	fprintf(stderr,"VRL2VRS sprite sheet compiler (C) 2016 Jonathan Campbell\n");
	fprintf(stderr,"Program will read multiple VRL files as directed by sprite sheet file\n");
	fprintf(stderr,"from the CURRENT WORKING DIRECTORY.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"vrl2vrs [options]\n");
	fprintf(stderr,"  -hp <string>                 With -hc, prefix for sprite defines\n");
	fprintf(stderr,"  -hc <filename>               Emit sprite names and IDs to C header\n");
	fprintf(stderr,"  -s <filename>                File on how to cut the sprite sheet\n");
	fprintf(stderr,"  -o <filename>                Output VRS file\n");
}

int main(int argc,char **argv) {
	const char *scr_file = NULL,*hdr_file = NULL,*hdr_prefix = NULL,*dst_file = NULL;
	struct vrs_header vrshdr;
	unsigned long foffset;
	unsigned int cut;
	char tmpname[14];
	int i,fd,srcfd;
	const char *a;
	int rdsz;

	for (i=1;i < argc;) {
		a = argv[i++];
		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"hp")) {
				hdr_prefix = argv[i++];
			}
			else if (!strcmp(a,"hc")) {
				hdr_file = argv[i++];
			}
			else if (!strcmp(a,"o")) {
				dst_file = argv[i++];
			}
			else if (!strcmp(a,"s")) {
				scr_file = argv[i++];
			}
			else {
				fprintf(stderr,"Unknown switch '%s'. Use --help\n",a);
				return 1;
			}
		}
		else {
			fprintf(stderr,"Unknown param %s\n",a);
			return 1;
		}
	}

	if (scr_file == NULL || dst_file == NULL) {
		help();
		return 1;
	}

	/* read the script file */
	if (!parse_script_file(scr_file)) {
		fprintf(stderr,"Script file (-s) parse error\n");
		return 1;
	}

	if (hdr_file != NULL) {
		struct vrl_animation_frame_t *animframe;
		struct vrl_spritesheetentry_t *cutreg;
		struct vrl_animation_list_t *anim;
		unsigned int frame;
		FILE *fp;

		fp = fopen(hdr_file,"w");
		if (fp == NULL) {
			fprintf(stderr,"Failed to open -hc file\n");
			return 1;
		}

		fprintf(fp,"// header file for sprite sheet. AUTO GENERATED, do not edit\n");
		fprintf(fp,"// \n");
		fprintf(fp,"// sheet script: %s\n",scr_file);
		fprintf(fp,"\n");

		fprintf(fp,"// sprite sheet (sprite IDs)\n");
		for (cut=0;cut < cutregions;cut++) {
			cutreg = cutregion+cut;

			fprintf(fp,"#define %s%s_sprite %uU\n",
				hdr_prefix != NULL ? hdr_prefix : "",
				cutreg->sprite_name,
				cutreg->sprite_id);
		}

		fprintf(fp,"// animation list (animation IDs)\n");
		for (cut=0;cut < animlists;cut++) {
			anim = animlist+cut;

			fprintf(fp,"#define %s%s_anim %uU /*",
				hdr_prefix != NULL ? hdr_prefix : "",
				anim->animation_name,
				anim->animation_id);
			fprintf(fp,"frames=%u ",
				anim->animation_frames);
			if (anim->animation_frames != 0) {
				fprintf(fp,"[ ");
				for (frame=0;frame < anim->animation_frames;frame++) {
					animframe = anim->animation_frame + frame;
					fprintf(fp,"%s@%u/event=%u/delay=%u ",animframe->sprite_name,animframe->sprite_id,
						animframe->event_id,animframe->delay);
				}
				fprintf(fp,"]");
			}
			fprintf(fp," */\n");
		}

		fprintf(fp,"\n");
		fprintf(fp,"// end list\n");
		fclose(fp);
	}

	fd = open(dst_file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
	if (fd < 0) {
		fprintf(stderr,"Unable to open dst file, %s\n",strerror(errno));
		return 1;
	}

	// dummy header, for now
	memset(&vrshdr,0,sizeof(vrshdr));
	memcpy(&vrshdr.vrs_sig,"VRS1",4);
	write(fd,&vrshdr,sizeof(vrshdr));
	foffset = sizeof(vrshdr);

	// write VRL sprites
	for (cut=0;cut < cutregions;cut++) {
		struct vrl_spritesheetentry_t *cutreg;

		cutreg = cutregion+cut;

		cutreg->fileoffset = foffset;

		sprintf(tmpname,"%s.VRL",cutreg->sprite_name);
		srcfd = open(tmpname,O_RDONLY|O_BINARY,0644);
		if (srcfd < 0) {
			fprintf(stderr,"Cannot read VRL sprite %s\n",tmpname);
			return 1;
		}
		while ((rdsz=read(srcfd,tempbuffer,sizeof(tempbuffer))) > 0) {
			write(fd,tempbuffer,rdsz);
			foffset += rdsz;
		}
		close(srcfd);
		if (rdsz < 0) {
			fprintf(stderr,"Error reading VRL sprite %s\n",tmpname);
			return 1;
		}
	}

	// update header
	vrshdr.offset_table[VRS_HEADER_OFFSET_VRS_LIST] = foffset;
	// sprite offsets
	for (cut=0;cut < cutregions;cut++) {
		struct vrl_spritesheetentry_t *cutreg;

		cutreg = cutregion+cut;

		*((uint32_t*)tempbuffer) = cutreg->fileoffset;
		write(fd,tempbuffer,sizeof(uint32_t));
		foffset += sizeof(uint32_t);
	}
	*((uint32_t*)tempbuffer) = 0;
	write(fd,tempbuffer,sizeof(uint32_t));
	foffset += sizeof(uint32_t);

	// update header
	vrshdr.offset_table[VRS_HEADER_OFFSET_SPRITE_ID_LIST] = foffset;
	// sprite IDs
	for (cut=0;cut < cutregions;cut++) {
		struct vrl_spritesheetentry_t *cutreg;

		cutreg = cutregion+cut;

		*((uint16_t*)tempbuffer) = cutreg->sprite_id;
		write(fd,tempbuffer,sizeof(uint16_t));
		foffset += sizeof(uint16_t);
	}
	*((uint16_t*)tempbuffer) = 0;
	write(fd,tempbuffer,sizeof(uint16_t));
	foffset += sizeof(uint16_t);

	// update header
	vrshdr.offset_table[VRS_HEADER_OFFSET_SPRITE_NAME_LIST] = foffset;
	// sprite names (TODO: make this optional)
	for (cut=0;cut < cutregions;cut++) {
		struct vrl_spritesheetentry_t *cutreg;
		size_t l;

		cutreg = cutregion+cut;

		l = strlen(cutreg->sprite_name);
		memcpy(tempbuffer,cutreg->sprite_name,l+1);
		write(fd,tempbuffer,l+1);
		foffset += l+1;
	}
	*((uint16_t*)tempbuffer) = 0;
	write(fd,tempbuffer,1);
	foffset += 1;

	// write animation lists
	for (cut=0;cut < animlists;cut++) {
		struct vrs_animation_list_entry_t animstruct;
		struct vrl_animation_frame_t *animframe;
		struct vrl_animation_list_t *anim;
		unsigned int frame;

		anim = animlist+cut;

		anim->fileoffset = foffset;

		for (frame=0;frame < anim->animation_frames;frame++) {
			animframe = anim->animation_frame + frame;
			animstruct.sprite_id = animframe->sprite_id;
			animstruct.event_id = animframe->event_id;
			animstruct.delay = animframe->delay;

			write(fd,&animstruct,sizeof(animstruct));
			foffset += sizeof(animstruct);
		}
		memset(&animstruct,0,sizeof(animstruct));
		write(fd,&animstruct,sizeof(animstruct));
		foffset += sizeof(animstruct);
	}
	vrshdr.offset_table[VRS_HEADER_OFFSET_ANIMATION_LIST] = foffset;
	for (cut=0;cut < animlists;cut++) {
		struct vrl_animation_list_t *anim;

		anim = animlist+cut;

		*((uint32_t*)tempbuffer) = anim->fileoffset;
		write(fd,tempbuffer,sizeof(uint32_t));
		foffset += sizeof(uint32_t);
	}
	*((uint32_t*)tempbuffer) = 0;
	write(fd,tempbuffer,sizeof(uint32_t));
	foffset += sizeof(uint32_t);

	// update header
	vrshdr.offset_table[VRS_HEADER_OFFSET_ANIMATION_ID_LIST] = foffset;
	// sprite IDs
	for (cut=0;cut < animlists;cut++) {
		struct vrl_animation_list_t *anim;

		anim = animlist+cut;

		*((uint16_t*)tempbuffer) = anim->animation_id;
		write(fd,tempbuffer,sizeof(uint16_t));
		foffset += sizeof(uint16_t);
	}
	*((uint16_t*)tempbuffer) = 0;
	write(fd,tempbuffer,sizeof(uint16_t));
	foffset += sizeof(uint16_t);

	// update header
	vrshdr.offset_table[VRS_HEADER_OFFSET_ANIMATION_NAME_LIST] = foffset;
	// sprite names (TODO: make this optional)
	for (cut=0;cut < animlists;cut++) {
		struct vrl_animation_list_t *anim;
		size_t l;

		anim = animlist+cut;

		l = strlen(anim->animation_name);
		memcpy(tempbuffer,anim->animation_name,l+1);
		write(fd,tempbuffer,l+1);
		foffset += l+1;
	}
	*((uint16_t*)tempbuffer) = 0;
	write(fd,tempbuffer,1);
	foffset += 1;

	// update header on disk
	vrshdr.resident_size = foffset;
	lseek(fd,0,SEEK_SET);
	write(fd,&vrshdr,sizeof(vrshdr));
	lseek(fd,foffset,SEEK_SET);

	// final warning for 16-bit segmented programs
	if (foffset >= 65536UL) {
		fprintf(stderr,"WARNING: VRS file exceeds 64KB, may not be usable by 16-bit DOS programs\n");
		return 1;
	}

	close(fd);
	return 0;
}

