#ifndef __THALIA_TIMER_H__
#define __THALIA_TIMER_H__

// Timer substructure.
typedef struct {
	guint64 base_ticks_done; // Keeps track of how many base ticks are processed.
} thalia_timer_t;

#endif

#ifdef __THALIA_GB_T__
void thalia_timer_step(ThaliaGB* gb);
#endif
