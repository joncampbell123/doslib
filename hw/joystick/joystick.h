
#include <stdint.h>

#define JOYSTICK_BUTTON_BITFIELD(x) (1U << (x))

struct joystick_t {
	uint16_t		port;		// there is only ONE joystick port!
	uint8_t			buttons;	// button states (last read), bitfield for buttons 1-4, read from bits 5-8 of the I/O port
	uint16_t		reading[4];	// joystick readings, in 8254 timer ticks. It is up to *you* to calibrate and make meaning of these values.
						// furthermore, the values may change scale quite a bit if this code is run in DOSBox and timed intervals are
						// turned off, and the user changes the CPU cycles count.
};

enum joystick_button_t {
	JOYSTICK_BUTTON_1=0x01,
	JOYSTICK_BUTTON_2=0x02,
	JOYSTICK_BUTTON_3=0x04,
	JOYSTICK_BUTTON_4=0x08
};

enum joystick_reading_t {
	JOYSTICK_READING_A_X_AXIS=0,		// Joystick A, X axis
	JOYSTICK_READING_A_Y_AXIS=1,		// Joystick A, Y axis
	JOYSTICK_READING_B_X_AXIS=2,		// Joystick B, X axis
	JOYSTICK_READING_B_Y_AXIS=3		// Joystick B, Y axis
};

static inline void joystick_update_button_state(struct joystick_t *joy,const uint8_t b) {
	joy->buttons = ((~b) >> 4U) & 0xFU;
}

static inline void trigger_joystick(struct joystick_t *joy) {
	outp(joy->port,0xFF);
}

void init_joystick(struct joystick_t *joy);
void read_joystick_buttons(struct joystick_t *joy);
void read_joystick_positions(struct joystick_t *joy,uint8_t which/*bitmask of axes to read*/);
int probe_joystick(struct joystick_t *joy,uint16_t port);

// NTS: When reading joystick axis, try to read only the ones you care about. If the joystick is a standard two-axis joystick
//      then read_joystick_positions() will end up wasting time waiting for the 2nd axis to change from 0 to 1, which never
//      happens unless the joystick is explicitly 4-axis.

