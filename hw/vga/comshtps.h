
#define MAX_ANIMATION_LIST_FRAMES	128
#define MAX_ANIMATION_LISTS		256
#define MAX_CUTREGIONS			1024

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

struct vrl_animation_list_t {
	char			animation_name[32];
	uint16_t		animation_frames;
	uint16_t		animation_id;
	struct vrl_animation_frame_t animation_frame[MAX_ANIMATION_LIST_FRAMES]; // max 128 frames

	uint32_t		fileoffset;		// when compiling
};

enum {
	SECTION_NONE=0,
	SECTION_SPRITESHEET,
	SECTION_ANIMATION
};

extern struct vrl_spritesheetentry_t		cutregion[MAX_CUTREGIONS];
extern int					cutregions;

extern struct vrl_animation_list_t		animlist[MAX_ANIMATION_LISTS];
extern int					animlists;

int parse_script_file(const char *path);

