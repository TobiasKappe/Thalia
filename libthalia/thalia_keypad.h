#ifndef __THALIA_KEY_H__
#define __THALIA_KEY_H__

#include <glib.h>
#include "thalia_gb.h"

// Key masks, correspond with those expected by the program.
typedef enum {
	THALIA_KEY_A       = 1 << 0,
	THALIA_KEY_B       = 1 << 1,
	THALIA_KEY_SELECT  = 1 << 2,
	THALIA_KEY_START   = 1 << 3,
	THALIA_KEY_RIGHT   = 1 << 0,
	THALIA_KEY_LEFT    = 1 << 1,
	THALIA_KEY_UP      = 1 << 2,
	THALIA_KEY_DOWN    = 1 << 3
} thalia_key_mask_t;

// Key region selectors.
typedef enum {
	THALIA_KEY_REGION_HIGH = 0x02,
	THALIA_KEY_REGION_LOW  = 0x01
} thalia_key_region_t;

// Stores the state of the keyboard emulated.
typedef struct {
	gboolean key_a;
	gboolean key_b;
	gboolean key_select;
	gboolean key_start;
	gboolean key_right;
	gboolean key_left;
	gboolean key_up;
	gboolean key_down;
	thalia_key_region_t region;
} thalia_keypad_t;
#endif

#ifdef __THALIA_GB_T__
void thalia_keypad_lock();
void thalia_keypad_unlock();
guint8 thalia_keypad_read(ThaliaGB* gb);
void thalia_keypad_write(ThaliaGB* gb, guint8 value);
void thalia_keypad_event(ThaliaGB* gb, gboolean pressed);
#endif
