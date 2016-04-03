
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

#pragma pack(push,1)
struct vrl_header {
	uint8_t			vrl_sig[4];		// +0x00  "VRL1"
	uint8_t			fmt_sig[4];		// +0x04  "VGAX"
	uint16_t		height;			// +0x08  Sprite height
	uint16_t		width;			// +0x0A  Sprite width
	int16_t			hotspot_x;		// +0x0C  Hotspot offset (X) for programmer's reference
	int16_t			hotspot_y;		// +0x0E  Hotspot offset (Y) for programmer's reference
};							// =0x10
#pragma pack(pop)

#pragma pack(push,1)
// NOTE: VRS sheets intended for use with 16-bit segmented DOS programs must not exceed 64KB in any way.
//       Otherwise, the format permits sprite sheets to be any size they need to be.
//
//       It is *strongly recommended* that when reading the file information, that the program first validate
//       the offsets are valid, then, parse the contents cautiously. If that cannot be done for performance
//       reasons, then you should at least design debug builds of your program to validate it.
struct vrs_header {
	uint8_t			vrs_sig[4];		// +0x00  "VRS1"
	uint32_t		resident_size;		// +0x04  size of data intended to be resident in memory (i.e. sprite sheets, sprite IDs, animation loops)
							//        this must be less than or equal to the file size. data after this size is "nonresident" meaning
							//        it is not kept in memory.
	uint32_t		offset_table[16];	// +0x08  table of offsets, pointing to various data. Type is determined by index, see enum.
	// data follows					// +0x48  data
};
#pragma pack(pop)

#pragma pack(push,1)
struct vrs_list_offset_t {
	uint32_t		offset;			// file offset to locate VRL sprite. zero means to stop reading the array
};

struct vrs_list_sprite_id_entry_t {
	uint16_t		sprite_id;		// the index of this array entry, matching up to the index of the sprite offset array, to map sprite ID -> sprite. zero means to stop reading the array
};

struct vrs_list_sprite_name_entry_t {
	uint32_t		offset;			// file offset to locate sprite name (ASCIIZ string). zero means to stop reading the array
};

struct vrs_list_animation_list_entry_t {
	uint32_t		offset;			// file offset to locate animation list. zero means to stop reading the array
};

struct vrs_list_animation_name_list_entry_t {
	uint32_t		offset;			// file offset to locate animation name (ASCIIZ ztring). zero means to stop reading the array
};

struct vrs_animation_list_entry_t {			// one entry of the animation list.. zero sprite ID means to stop reading the array
	uint16_t		sprite_id;		// sprite ID to display for this frame.
	uint16_t		delay;			// if nonzero, delay this many ticks. if zero, stop animation until triggered to animate again by game engine.
	uint16_t		event_id;		// if nonzero, game-specific event to trigger when entering the animation frame
};
#pragma pack(pop)

enum vrs_header_offset_type_t { // offset table indexes
	VRS_HEADER_OFFSET_VRS_LIST=0,			// offset points to array of sprite offsets (32-bit). seek to sprite offset to locate VRL sprite. Array ends at first zero entry.

	VRS_HEADER_OFFSET_SPRITE_ID_LIST=1,		// offset points to array of sprite IDs (16-bit). one entry per VRL sprite. Array ends at first zero entry.

	VRS_HEADER_OFFSET_SPRITE_NAME_LIST=2,		// offset points to array of sprite name offsets (32-bit). Array ends at first zero entry. Offset points to ASCIIZ string. OPTIONAL.

	VRS_HEADER_OFFSET_ANIMATION_LIST=3,		// offset points to array of animation list offsets (32-bit). Offset points to array of (sprite ID, delay in ticks, event ID) for animation.
							//     The array ends at first zero entry. Animation list at the offset ends at first zero sprite ID.
							//     delay is in units of time determined by your game, not defined by this file format.
							//     if delay is zero, animation should stop on this frame until triggered by something in your game to continue.
							//     When your game hits the end of the array, it should step back to the first entry and repeat the animation.
							//     event ID is provided so that events or actions could be triggered upon entering or leaving a particular frame of animation.

	VRS_HEADER_OFFSET_ANIMATION_ID_LIST=4,		// offset points to array of animation IDs (16-bit). one entry per animation. Array ends at first zero entry.

	VRS_HEADER_OFFSET_ANIMATION_NAME_LIST=5		// offset points to array of animation name offsets (32-bit). Array ends at first zero entry. Offset points to ASCIIZ string. OPTIONAL.
};

struct vrl_spritesheetentry_t {
	uint16_t		x,y,w,h;
	uint16_t		sprite_id;
	char			sprite_name[9];		// 8 chars + NUL

	uint32_t		fileoffset;		// when compiling
};

struct vrl_animation_frame_t {
	char			sprite_name[9];		// sprite it refers to by name if not NUL
	uint16_t		sprite_id;		// sprite it refers to by ID if nonzero
	uint16_t		delay;			// delay (in whatever units of time the game uses). 0 means to stop animation on this frame.
	uint16_t		event_id;		// event ID (for use by the game) if nonzero
};

#define MAX_ANIMATION_LIST_FRAMES	128
struct vrl_animation_list_t {
	char			animation_name[32];
	uint16_t		animation_frames;
	uint16_t		animation_id;
	struct vrl_animation_frame_t animation_frame[MAX_ANIMATION_LIST_FRAMES]; // max 128 frames

	uint32_t		fileoffset;		// when compiling
};

#define MAX_CUTREGIONS			1024
struct vrl_spritesheetentry_t		cutregion[MAX_CUTREGIONS];
int					cutregions = 0;

#define MAX_ANIMATION_LISTS		256
struct vrl_animation_list_t		animlist[MAX_ANIMATION_LISTS];
int					animlists = 0;

static unsigned char			tempbuffer[8192];

static void chomp(char *line) {
	char *s = line+strlen(line)-1;
	while (s >= line && (*s == '\n' || *s == '\r')) *s-- = 0;
}

static void take_sprite_item(struct vrl_spritesheetentry_t *item) {
	unsigned int i;

	if (cutregions < MAX_CUTREGIONS) {
		if (item->sprite_id == 0)
			item->sprite_id = MAX_CUTREGIONS + cutregions; // auto-assign
		if (item->sprite_name[0] == 0)
			sprintf(item->sprite_name,"____%04u",item->sprite_id); // auto-assign

		// warn if sprite ID or sprite name already taken!
		for (i=0;i < cutregions;i++) {
			if (cutregion[i].sprite_id == item->sprite_id)
				fprintf(stderr,"WARNING for %s: sprite ID %u already taken by %s\n",
					item->sprite_name,item->sprite_id,cutregion[i].sprite_name);
			if (!strcasecmp(cutregion[i].sprite_name,item->sprite_name))
				fprintf(stderr,"WARNING for %s: sprite name already taken\n",
					item->sprite_name);
		}

		if (item->w != 0 && item->h != 0)
			cutregion[cutregions++] = *item;
	}

	memset(item,0,sizeof(*item));
}

static void take_anim_item_frame(struct vrl_animation_list_t *item) {
	struct vrl_animation_frame_t *frame = item->animation_frame + item->animation_frames;

	if (frame->sprite_name[0] != 0 || frame->sprite_id != 0) {
		item->animation_frames++;
	}
}

static void anim_validate_list(struct vrl_animation_list_t *item) {
	struct vrl_animation_frame_t *animframe;
	unsigned int i,frame;

	for (frame=0;frame < item->animation_frames;frame++) {
		animframe = item->animation_frame + frame;

		if (animframe->sprite_id == 0 && animframe->sprite_name[0] != 0) {
			for (i=0;i < cutregions;i++) {
				if (cutregion[i].sprite_id != 0) {
					if (!strcmp(cutregion[i].sprite_name,animframe->sprite_name)) {
						animframe->sprite_id = cutregion[i].sprite_id;
						break;
					}
				}
			}
		}
		else if (animframe->sprite_id != 0 && animframe->sprite_name[0] == 0) {
			for (i=0;i < cutregions;i++) {
				if (cutregion[i].sprite_name[0] != 0) {
					if (cutregion[i].sprite_id == animframe->sprite_id) {
						strcpy(animframe->sprite_name,cutregion[i].sprite_name);
					}
				}
			}
		}
	}
}

static void take_anim_item(struct vrl_animation_list_t *item) {
	unsigned int i;

	take_anim_item_frame(item);
	anim_validate_list(item); // validate entries, and fill in sprite IDs and names
	if (animlists < MAX_ANIMATION_LISTS && item->animation_frames != 0) {
		if (item->animation_id == 0)
			item->animation_id = MAX_ANIMATION_LISTS + animlists; // auto-assign

		// warn if sprite ID or sprite name already taken!
		for (i=0;i < cutregions;i++) {
			if (animlist[i].animation_id == item->animation_id)
				fprintf(stderr,"WARNING for %s: animation ID %u already taken by %s\n",
					item->animation_name,item->animation_id,animlist[i].animation_name);
			if (item->animation_name[0] != 0 && !strcasecmp(animlist[i].animation_name,item->animation_name))
				fprintf(stderr,"WARNING for %s: sprite name already taken\n",
					item->animation_name);
		}

		animlist[animlists++] = *item;
	}

	memset(item,0,sizeof(*item));
}

enum {
	SECTION_NONE=0,
	SECTION_SPRITESHEET,
	SECTION_ANIMATION
};

static int parse_script_file(const char *path) {
	struct vrl_spritesheetentry_t sprite_item;
	struct vrl_animation_list_t anim_item;
	unsigned char start_item = 0;
	unsigned char in_section = 0;
	unsigned char OK = 1;
	char line[512],*s,*d;
	FILE *fp;

	fp = fopen(path,"r");
	if (!fp) return 0;

	memset(&anim_item,0,sizeof(anim_item));
	memset(&sprite_item,0,sizeof(sprite_item));
	while (!feof(fp) && !ferror(fp)) {
		if (fgets(line,sizeof(line)-1,fp) == NULL) break;

		/* eat the newline at the end. "chomp" refers to the function of the same name in Perl */
		chomp(line);

		/* begin scanning */
		s = line;
		if (*s == 0/*end of string*/ || *s == '#'/*comment*/) continue;
		while (*s == ' ' || *s == '\t') s++;

		/* cut the string at the first pound sign, to allow comments */
		{
			char *c = strchr(s,'#');
			if (c != NULL) *c = 0; // ASCIIZ snip
		}

		if (*s == '*') { // beginning of section
			if (start_item) {
				if (in_section == SECTION_SPRITESHEET)
					take_sprite_item(&sprite_item);
				else if (in_section == SECTION_ANIMATION)
					take_anim_item(&anim_item);

				start_item = 0;
			}

			s++;
			if (!strcmp(s,"spritesheet")) {
				in_section = SECTION_SPRITESHEET;
			}
			else if (!strcmp(s,"animation")) {
				in_section = SECTION_ANIMATION;
			}
			else {
				in_section = 0; // ignore the rest.
			}
		}
		else if (*s == '+') { // starting another item
			s++;
			if (in_section == SECTION_SPRITESHEET) {
				if (start_item)
					take_sprite_item(&sprite_item);

				if (cutregions >= MAX_CUTREGIONS) {
					fprintf(stderr,"Too many cut regions!\n");
					OK = 0;
					break;
				}

				// New sprite (no ID or name)
				// +
				//
				// New sprite with name (8 char max)
				// +PRUSSIA
				//
				// New sprite with ID
				// +@400
				//
				// New sprite with name and ID
				// +PRUSSIA@400
				//
				// ID part can be octal or hexadecimal according to strtoul() rules
				// +PRUSSIA@0x158
				// +PRUSSIA@0777
				start_item = 1;
				while (*s == ' ') s++;
				d = sprite_item.sprite_name;

				// sprite name (optional)
				while (*s != 0 && *s != '@') {
					if (d < (sprite_item.sprite_name+8) && (isdigit(*s) || isalpha(*s) || *s == '_' || *s == '+'))
						*d++ = *s++;
					else
						s++;
				}
				*d = 0;//ASCIIZ snip
				// @sprite ID
				if (*s == '@') {
					s++;
					sprite_item.sprite_id = (uint16_t)strtoul(s,&s,0); // whatever base you want, and set 's' to point after number
				}
			}
			else if (in_section == SECTION_ANIMATION) {
				if (start_item)
					take_anim_item(&anim_item);

				if (animlists >= MAX_ANIMATION_LISTS) {
					fprintf(stderr,"Too many animation lists!\n");
					OK = 0;
					break;
				}

				// New sprite (no ID or name)
				// +
				//
				// New sprite with name (8 char max)
				// +PRUSSIA
				//
				// New sprite with ID
				// +@400
				//
				// New sprite with name and ID
				// +PRUSSIA@400
				//
				// ID part can be octal or hexadecimal according to strtoul() rules
				// +PRUSSIA@0x158
				// +PRUSSIA@0777
				start_item = 1;
				while (*s == ' ') s++;
				d = anim_item.animation_name;

				// sprite name (optional)
				while (*s != 0 && *s != '@') {
					if (d < (anim_item.animation_name+32) && (isdigit(*s) || isalpha(*s) || *s == '_' || *s == '+'))
						*d++ = *s++;
					else
						s++;
				}
				*d = 0;//ASCIIZ snip
				// @sprite ID
				if (*s == '@') {
					s++;
					anim_item.animation_id = (uint16_t)strtoul(s,&s,0); // whatever base you want, and set 's' to point after number
				}
			}
		}
		else if (*s == '-') {
			s++;

			if (start_item) {
				if (in_section == SECTION_ANIMATION) {
					if (!strcmp(s,"newframe")) {
						if ((anim_item.animation_frames+1) >= MAX_ANIMATION_LIST_FRAMES) {
							fprintf(stderr,"Too many frames in animation list!\n");
							OK = 0;
							break;
						}

						take_anim_item_frame(&anim_item);
					}
				}
			}
		}
		else {
			if (start_item) {
				// item params are name=value
				char *name = s;
				char *value = strchr(s,'=');
				if (value != NULL) {
					*value++ = 0; // snip

					if (in_section == SECTION_SPRITESHEET) {
						if (!strcmp(name,"xy")) {
							// xy=x,y
							sprite_item.x = (uint16_t)strtoul(value,&value,0);
							if (*value == ',') {
								value++;
								sprite_item.y = (uint16_t)strtoul(value,&value,0);
							}
						}
						else if (!strcmp(name,"wh")) {
							// wh=w,h
							sprite_item.w = (uint16_t)strtoul(value,&value,0);
							if (*value == ',') {
								value++;
								sprite_item.h = (uint16_t)strtoul(value,&value,0);
							}
						}
						else if (!strcmp(name,"x")) {
							// x=x
							sprite_item.x = (uint16_t)strtoul(value,&value,0);
						}
						else if (!strcmp(name,"y")) {
							// y=y
							sprite_item.y = (uint16_t)strtoul(value,&value,0);
						}
						else if (!strcmp(name,"w")) {
							// w=w
							sprite_item.w = (uint16_t)strtoul(value,&value,0);
						}
						else if (!strcmp(name,"h")) {
							// h=h
							sprite_item.h = (uint16_t)strtoul(value,&value,0);
						}
					}
					else if (in_section == SECTION_ANIMATION) {
						struct vrl_animation_frame_t *frame = &anim_item.animation_frame[anim_item.animation_frames];

						if (!strcmp(name,"event")) {
							// @ID
							// events do not have names, yet
							s = value;
							while (*s == ' ') s++;
							while (*s != 0 && *s != '@') s++;
							// @event ID
							if (*s == '@') {
								s++;
								frame->event_id = (uint16_t)strtoul(s,&s,0); // whatever base you want, and set 's' to point after number
							}
						}
						else if (!strcmp(name,"sprite")) {
							// name
							// @ID
							// name@ID
							s = value;
							while (*s == ' ') s++;
							d = frame->sprite_name;

							// sprite name (optional)
							while (*s != 0 && *s != '@') {
								if (d < (frame->sprite_name+8) && (isdigit(*s) || isalpha(*s) || *s == '_' || *s == '+'))
									*d++ = *s++;
								else
									s++;
							}
							*d = 0;//ASCIIZ snip
							// @sprite ID
							if (*s == '@') {
								s++;
								frame->sprite_id = (uint16_t)strtoul(s,&s,0); // whatever base you want, and set 's' to point after number
							}
						}
						else if (!strcmp(name,"delay")) {
							s = value;
							frame->delay = (uint16_t)strtoul(s,&s,0);
						}
						else {
							fprintf(stderr,"ANIMATION SECTION: Unknown value %s=%s\n",name,s);
						}
					}
				}
			}
		}
	}

	if (start_item) {
		if (in_section == SECTION_SPRITESHEET)
			take_sprite_item(&sprite_item);
		else if (in_section == SECTION_ANIMATION)
			take_anim_item(&anim_item);

		start_item = 0;
	}

	fclose(fp);
	return OK;
}

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

