#include <glib.h>
#include "thalia_gb.h"
#include "thalia_keypad.h"

// TODO: Implement this using a mutex in a non-broken glib
G_LOCK_DEFINE(thalia_keypad);

// Locks the keypad.
void thalia_keypad_lock()
{
	G_LOCK(thalia_keypad);
}

// Unlocks the keypad.
void thalia_keypad_unlock()
{
	G_UNLOCK(thalia_keypad);
}

// Synthesises the keypad register contents from the keypad state.
guint8 thalia_keypad_read(ThaliaGB* gb)
{
	thalia_keypad_lock();
	guint8 ret = 0x0F | (gb->keypad.region << 4);

	// Reset the bits for the keys that are down in the selected region(s).
	if(gb->keypad.region & THALIA_KEY_REGION_LOW) {
		if(gb->keypad.key_a)
			ret &= ~THALIA_KEY_A;
		if(gb->keypad.key_b)
			ret &= ~THALIA_KEY_B;
		if(gb->keypad.key_select)
			ret &= ~THALIA_KEY_SELECT;
		if(gb->keypad.key_start)
			ret &= ~THALIA_KEY_START;
	}
	if(gb->keypad.region & THALIA_KEY_REGION_HIGH) {
		if(gb->keypad.key_right)
			ret &= ~THALIA_KEY_RIGHT;
		if(gb->keypad.key_left)
			ret &= ~THALIA_KEY_LEFT;
		if(gb->keypad.key_up)
			ret &= ~THALIA_KEY_UP;
		if(gb->keypad.key_down)
			ret &= ~THALIA_KEY_DOWN;
	}

	thalia_keypad_unlock();
	return ret;
}

// Record the selected keypad region.
void thalia_keypad_write(ThaliaGB* gb, guint8 value)
{
	thalia_keypad_lock();
	gb->keypad.region = (value >> 4) & 0x03;
	thalia_keypad_unlock();
}
