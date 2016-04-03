
#include <stdint.h>

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

