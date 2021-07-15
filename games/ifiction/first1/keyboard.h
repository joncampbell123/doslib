
#include "simplequeue.h"

/* raw key event */
struct IFEKeyEvent {
	uint32_t		raw_code;	/* platform and driver-specific */
	uint32_t		code;		/* universal code within framework */
	uint32_t		flags;		/* flags */
};

/* cooked key event (i.e. for text entry).
 * depending on the platform, this is either taken from the raw key event (MS-DOS),
 * or provided by the environment (Windows, SDL2) */
struct IFECookedKeyEvent {
	uint32_t		code;		/* Unicode key code */
	uint32_t		flags;		/* flags */
};

/* key is down, else not down */
#define IFEKeyEvent_FLAG_DOWN				(1u << 0u)
/* some keys do not have a down or up state, but only a toggle state. DOWN is set if toggled on. */
#define IFEKeyEvent_FLAG_TOGGLE				(1u << 1u)

#define IFEKeyQueueSize		256
#define IFECookedKeyQueueSize	256

extern IFESimpleQueue<IFEKeyEvent,IFEKeyQueueSize>			IFEKeyQueue;
extern IFESimpleQueue<IFECookedKeyEvent,IFECookedKeyQueueSize>		IFECookedKeyQueue;

void IFEKeyQueueEmptyAll(void);

/* ripped from SDL2 scan codes, development time is short */
enum IFEKeyCode {
    IFEKEY_UNKNOWN = 0,

    /**
     *  \name Usage page 0x07
     *
     *  These values are from usage page 0x07 (USB keyboard page).
     */
    /* @{ */

    IFEKEY_A = 4,
    IFEKEY_B = 5,
    IFEKEY_C = 6,
    IFEKEY_D = 7,
    IFEKEY_E = 8,
    IFEKEY_F = 9,
    IFEKEY_G = 10,
    IFEKEY_H = 11,
    IFEKEY_I = 12,
    IFEKEY_J = 13,
    IFEKEY_K = 14,
    IFEKEY_L = 15,
    IFEKEY_M = 16,
    IFEKEY_N = 17,
    IFEKEY_O = 18,
    IFEKEY_P = 19,
    IFEKEY_Q = 20,
    IFEKEY_R = 21,
    IFEKEY_S = 22,
    IFEKEY_T = 23,
    IFEKEY_U = 24,
    IFEKEY_V = 25,
    IFEKEY_W = 26,
    IFEKEY_X = 27,
    IFEKEY_Y = 28,
    IFEKEY_Z = 29,

    IFEKEY_1 = 30,
    IFEKEY_2 = 31,
    IFEKEY_3 = 32,
    IFEKEY_4 = 33,
    IFEKEY_5 = 34,
    IFEKEY_6 = 35,
    IFEKEY_7 = 36,
    IFEKEY_8 = 37,
    IFEKEY_9 = 38,
    IFEKEY_0 = 39,

    IFEKEY_RETURN = 40,
    IFEKEY_ESCAPE = 41,
    IFEKEY_BACKSPACE = 42,
    IFEKEY_TAB = 43,
    IFEKEY_SPACE = 44,

    IFEKEY_MINUS = 45,
    IFEKEY_EQUALS = 46,
    IFEKEY_LEFTBRACKET = 47,
    IFEKEY_RIGHTBRACKET = 48,
    IFEKEY_BACKSLASH = 49, /**< Located at the lower left of the return
                                  *   key on ISO keyboards and at the right end
                                  *   of the QWERTY row on ANSI keyboards.
                                  *   Produces REVERSE SOLIDUS (backslash) and
                                  *   VERTICAL LINE in a US layout, REVERSE
                                  *   SOLIDUS and VERTICAL LINE in a UK Mac
                                  *   layout, NUMBER SIGN and TILDE in a UK
                                  *   Windows layout, DOLLAR SIGN and POUND SIGN
                                  *   in a Swiss German layout, NUMBER SIGN and
                                  *   APOSTROPHE in a German layout, GRAVE
                                  *   ACCENT and POUND SIGN in a French Mac
                                  *   layout, and ASTERISK and MICRO SIGN in a
                                  *   French Windows layout.
                                  */
    IFEKEY_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
                                  *   instead of 49 for the same key, but all
                                  *   OSes I've seen treat the two codes
                                  *   identically. So, as an implementor, unless
                                  *   your keyboard generates both of those
                                  *   codes and your OS treats them differently,
                                  *   you should generate IFEKEY_BACKSLASH
                                  *   instead of this code. As a user, you
                                  *   should not rely on this code because SDL
                                  *   will never generate it with most (all?)
                                  *   keyboards.
                                  */
    IFEKEY_SEMICOLON = 51,
    IFEKEY_APOSTROPHE = 52,
    IFEKEY_GRAVE = 53, /**< Located in the top left corner (on both ANSI
                              *   and ISO keyboards). Produces GRAVE ACCENT and
                              *   TILDE in a US Windows layout and in US and UK
                              *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                              *   and NOT SIGN in a UK Windows layout, SECTION
                              *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                              *   layouts on ISO keyboards, SECTION SIGN and
                              *   DEGREE SIGN in a Swiss German layout (Mac:
                              *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                              *   DEGREE SIGN in a German layout (Mac: only on
                              *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                              *   French Windows layout, COMMERCIAL AT and
                              *   NUMBER SIGN in a French Mac layout on ISO
                              *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                              *   SIGN in a Swiss German, German, or French Mac
                              *   layout on ANSI keyboards.
                              */
    IFEKEY_COMMA = 54,
    IFEKEY_PERIOD = 55,
    IFEKEY_SLASH = 56,

    IFEKEY_CAPSLOCK = 57,

    IFEKEY_F1 = 58,
    IFEKEY_F2 = 59,
    IFEKEY_F3 = 60,
    IFEKEY_F4 = 61,
    IFEKEY_F5 = 62,
    IFEKEY_F6 = 63,
    IFEKEY_F7 = 64,
    IFEKEY_F8 = 65,
    IFEKEY_F9 = 66,
    IFEKEY_F10 = 67,
    IFEKEY_F11 = 68,
    IFEKEY_F12 = 69,

    IFEKEY_PRINTSCREEN = 70,
    IFEKEY_SCROLLLOCK = 71,
    IFEKEY_PAUSE = 72,
    IFEKEY_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
                                   does send code 73, not 117) */
    IFEKEY_HOME = 74,
    IFEKEY_PAGEUP = 75,
    IFEKEY_DELETE = 76,
    IFEKEY_END = 77,
    IFEKEY_PAGEDOWN = 78,
    IFEKEY_RIGHT = 79,
    IFEKEY_LEFT = 80,
    IFEKEY_DOWN = 81,
    IFEKEY_UP = 82,

    IFEKEY_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
                                     */
    IFEKEY_KP_DIVIDE = 84,
    IFEKEY_KP_MULTIPLY = 85,
    IFEKEY_KP_MINUS = 86,
    IFEKEY_KP_PLUS = 87,
    IFEKEY_KP_ENTER = 88,
    IFEKEY_KP_1 = 89,
    IFEKEY_KP_2 = 90,
    IFEKEY_KP_3 = 91,
    IFEKEY_KP_4 = 92,
    IFEKEY_KP_5 = 93,
    IFEKEY_KP_6 = 94,
    IFEKEY_KP_7 = 95,
    IFEKEY_KP_8 = 96,
    IFEKEY_KP_9 = 97,
    IFEKEY_KP_0 = 98,
    IFEKEY_KP_PERIOD = 99,

    IFEKEY_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
                                        *   keyboards have over ANSI ones,
                                        *   located between left shift and Y.
                                        *   Produces GRAVE ACCENT and TILDE in a
                                        *   US or UK Mac layout, REVERSE SOLIDUS
                                        *   (backslash) and VERTICAL LINE in a
                                        *   US or UK Windows layout, and
                                        *   LESS-THAN SIGN and GREATER-THAN SIGN
                                        *   in a Swiss German, German, or French
                                        *   layout. */
    IFEKEY_APPLICATION = 101, /**< windows contextual menu, compose */
    IFEKEY_POWER = 102, /**< The USB document says this is a status flag,
                               *   not a physical key - but some Mac keyboards
                               *   do have a power key. */
    IFEKEY_KP_EQUALS = 103,
    IFEKEY_F13 = 104,
    IFEKEY_F14 = 105,
    IFEKEY_F15 = 106,
    IFEKEY_F16 = 107,
    IFEKEY_F17 = 108,
    IFEKEY_F18 = 109,
    IFEKEY_F19 = 110,
    IFEKEY_F20 = 111,
    IFEKEY_F21 = 112,
    IFEKEY_F22 = 113,
    IFEKEY_F23 = 114,
    IFEKEY_F24 = 115,
    IFEKEY_EXECUTE = 116,
    IFEKEY_HELP = 117,
    IFEKEY_MENU = 118,
    IFEKEY_SELECT = 119,
    IFEKEY_STOP = 120,
    IFEKEY_AGAIN = 121,   /**< redo */
    IFEKEY_UNDO = 122,
    IFEKEY_CUT = 123,
    IFEKEY_COPY = 124,
    IFEKEY_PASTE = 125,
    IFEKEY_FIND = 126,
    IFEKEY_MUTE = 127,
    IFEKEY_VOLUMEUP = 128,
    IFEKEY_VOLUMEDOWN = 129,
/* not sure whether there's a reason to enable these */
/*     IFEKEY_LOCKINGCAPSLOCK = 130,  */
/*     IFEKEY_LOCKINGNUMLOCK = 131, */
/*     IFEKEY_LOCKINGSCROLLLOCK = 132, */
    IFEKEY_KP_COMMA = 133,
    IFEKEY_KP_EQUALSAS400 = 134,

    IFEKEY_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    IFEKEY_INTERNATIONAL2 = 136,
    IFEKEY_INTERNATIONAL3 = 137, /**< Yen */
    IFEKEY_INTERNATIONAL4 = 138,
    IFEKEY_INTERNATIONAL5 = 139,
    IFEKEY_INTERNATIONAL6 = 140,
    IFEKEY_INTERNATIONAL7 = 141,
    IFEKEY_INTERNATIONAL8 = 142,
    IFEKEY_INTERNATIONAL9 = 143,
    IFEKEY_LANG1 = 144, /**< Hangul/English toggle */
    IFEKEY_LANG2 = 145, /**< Hanja conversion */
    IFEKEY_LANG3 = 146, /**< Katakana */
    IFEKEY_LANG4 = 147, /**< Hiragana */
    IFEKEY_LANG5 = 148, /**< Zenkaku/Hankaku */
    IFEKEY_LANG6 = 149, /**< reserved */
    IFEKEY_LANG7 = 150, /**< reserved */
    IFEKEY_LANG8 = 151, /**< reserved */
    IFEKEY_LANG9 = 152, /**< reserved */

    IFEKEY_ALTERASE = 153, /**< Erase-Eaze */
    IFEKEY_SYSREQ = 154,
    IFEKEY_CANCEL = 155,
    IFEKEY_CLEAR = 156,
    IFEKEY_PRIOR = 157,
    IFEKEY_RETURN2 = 158,
    IFEKEY_SEPARATOR = 159,
    IFEKEY_OUT = 160,
    IFEKEY_OPER = 161,
    IFEKEY_CLEARAGAIN = 162,
    IFEKEY_CRSEL = 163,
    IFEKEY_EXSEL = 164,

    IFEKEY_KP_00 = 176,
    IFEKEY_KP_000 = 177,
    IFEKEY_THOUSANDSSEPARATOR = 178,
    IFEKEY_DECIMALSEPARATOR = 179,
    IFEKEY_CURRENCYUNIT = 180,
    IFEKEY_CURRENCYSUBUNIT = 181,
    IFEKEY_KP_LEFTPAREN = 182,
    IFEKEY_KP_RIGHTPAREN = 183,
    IFEKEY_KP_LEFTBRACE = 184,
    IFEKEY_KP_RIGHTBRACE = 185,
    IFEKEY_KP_TAB = 186,
    IFEKEY_KP_BACKSPACE = 187,
    IFEKEY_KP_A = 188,
    IFEKEY_KP_B = 189,
    IFEKEY_KP_C = 190,
    IFEKEY_KP_D = 191,
    IFEKEY_KP_E = 192,
    IFEKEY_KP_F = 193,
    IFEKEY_KP_XOR = 194,
    IFEKEY_KP_POWER = 195,
    IFEKEY_KP_PERCENT = 196,
    IFEKEY_KP_LESS = 197,
    IFEKEY_KP_GREATER = 198,
    IFEKEY_KP_AMPERSAND = 199,
    IFEKEY_KP_DBLAMPERSAND = 200,
    IFEKEY_KP_VERTICALBAR = 201,
    IFEKEY_KP_DBLVERTICALBAR = 202,
    IFEKEY_KP_COLON = 203,
    IFEKEY_KP_HASH = 204,
    IFEKEY_KP_SPACE = 205,
    IFEKEY_KP_AT = 206,
    IFEKEY_KP_EXCLAM = 207,
    IFEKEY_KP_MEMSTORE = 208,
    IFEKEY_KP_MEMRECALL = 209,
    IFEKEY_KP_MEMCLEAR = 210,
    IFEKEY_KP_MEMADD = 211,
    IFEKEY_KP_MEMSUBTRACT = 212,
    IFEKEY_KP_MEMMULTIPLY = 213,
    IFEKEY_KP_MEMDIVIDE = 214,
    IFEKEY_KP_PLUSMINUS = 215,
    IFEKEY_KP_CLEAR = 216,
    IFEKEY_KP_CLEARENTRY = 217,
    IFEKEY_KP_BINARY = 218,
    IFEKEY_KP_OCTAL = 219,
    IFEKEY_KP_DECIMAL = 220,
    IFEKEY_KP_HEXADECIMAL = 221,

    IFEKEY_LCTRL = 224,
    IFEKEY_LSHIFT = 225,
    IFEKEY_LALT = 226, /**< alt, option */
    IFEKEY_LGUI = 227, /**< windows, command (apple), meta */
    IFEKEY_RCTRL = 228,
    IFEKEY_RSHIFT = 229,
    IFEKEY_RALT = 230, /**< alt gr, option */
    IFEKEY_RGUI = 231, /**< windows, command (apple), meta */

    IFEKEY_MODE = 257,    /**< I'm not sure if this is really not covered
                                 *   by any of the above, but since there's a
                                 *   special KMOD_MODE for it I'm adding it here
                                 */

    /* @} *//* Usage page 0x07 */

    /**
     *  \name Usage page 0x0C
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     */
    /* @{ */

    IFEKEY_AUDIONEXT = 258,
    IFEKEY_AUDIOPREV = 259,
    IFEKEY_AUDIOSTOP = 260,
    IFEKEY_AUDIOPLAY = 261,
    IFEKEY_AUDIOMUTE = 262,
    IFEKEY_MEDIASELECT = 263,
    IFEKEY_WWW = 264,
    IFEKEY_MAIL = 265,
    IFEKEY_CALCULATOR = 266,
    IFEKEY_COMPUTER = 267,
    IFEKEY_AC_SEARCH = 268,
    IFEKEY_AC_HOME = 269,
    IFEKEY_AC_BACK = 270,
    IFEKEY_AC_FORWARD = 271,
    IFEKEY_AC_STOP = 272,
    IFEKEY_AC_REFRESH = 273,
    IFEKEY_AC_BOOKMARKS = 274,

    /* @} *//* Usage page 0x0C */

    /**
     *  \name Walther keys
     *
     *  These are values that Christian Walther added (for mac keyboard?).
     */
    /* @{ */

    IFEKEY_BRIGHTNESSDOWN = 275,
    IFEKEY_BRIGHTNESSUP = 276,
    IFEKEY_DISPLAYSWITCH = 277, /**< display mirroring/dual display
                                           switch, video mode switch */
    IFEKEY_KBDILLUMTOGGLE = 278,
    IFEKEY_KBDILLUMDOWN = 279,
    IFEKEY_KBDILLUMUP = 280,
    IFEKEY_EJECT = 281,
    IFEKEY_SLEEP = 282,

    IFEKEY_APP1 = 283,
    IFEKEY_APP2 = 284,

    /* @} *//* Walther keys */

    /**
     *  \name Usage page 0x0C (additional media keys)
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     */
    /* @{ */

    IFEKEY_AUDIOREWIND = 285,
    IFEKEY_AUDIOFASTFORWARD = 286,

    /* @} *//* Usage page 0x0C (additional media keys) */

    /* Add any other keys here. */

    IFEKEY_MAX = 512 /**< not a key, just marks the number of scancodes
                                 for array bounds */
};

