
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

struct vrl_spritesheetentry_t		cutregion[MAX_CUTREGIONS];
int					cutregions = 0;

struct vrl_animation_list_t		animlist[MAX_ANIMATION_LISTS];
int					animlists = 0;

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

int parse_script_file(const char *path) {
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

