#include <glib.h>
#include "thalia_gb.h"
#include "thalia_keypad.h"

// Locks the keypad.
void thalia_keypad_lock(ThaliaGB* gb)
{
    g_mutex_lock(&gb->keypad.mutex);
}

// Unlocks the keypad.
void thalia_keypad_unlock(ThaliaGB* gb)
{
    g_mutex_unlock(&gb->keypad.mutex);
}

// Synthesises the keypad register contents from the keypad state.
guint8 thalia_keypad_read(ThaliaGB* gb)
{
    thalia_keypad_lock(gb);
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

    thalia_keypad_unlock(gb);
    return ret;
}

// Record the selected keypad region.
void thalia_keypad_write(ThaliaGB* gb, guint8 value)
{
    thalia_keypad_lock(gb);
    gb->keypad.region = (value >> 4) & 0x03;
    thalia_keypad_unlock(gb);
}
