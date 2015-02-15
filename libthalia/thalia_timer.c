#include <glib.h>
#include "thalia_gb.h"
#include "thalia_timer.h"

// Updates timer state to be in line with machine state after opcode execution.
void thalia_timer_step(ThaliaGB* gb)
{
    guint64 base_ticks = gb->cycles - gb->timer.base_ticks_done;
    while(base_ticks > 0) {
        base_ticks--;
        gb->timer.base_ticks_done++;
        // Increase the divider timer every 16 base ticks.
        if(gb->timer.base_ticks_done % 4 == 0)
            gb->mmu->ram_io.unpacked.time_divider++;

        // If the timer is running, check if a tick has elapsed.
        if(gb->mmu->ram_io.unpacked.time_start) {
            // Step size depends on clock speed selection.
            guint8 step = 1 << (2*gb->mmu->ram_io.unpacked.time_clock);
            if(gb->timer.base_ticks_done % step == 0) {
                // At this point we know that a tick has gone by.
                gb->mmu->ram_io.unpacked.time_count++;
                if(gb->mmu->ram_io.unpacked.time_count == 0x00) {
                    // Restore the contents from the modulo register.
                    gb->mmu->ram_io.unpacked.time_count = \
                     gb->mmu->ram_io.unpacked.time_modulo;
                    // Overflow happened, flag for interrupt.
                    gb->mmu->ram_io.unpacked.int_flag_timer = TRUE;
                }
            }
        }
    }
}
