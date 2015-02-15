#ifndef __THALIA_GPU_H__
#define __THALIA_GPU_H__

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "thalia_gb.h"

#define THALIA_GPU_DURATION_HBLANK 51
#define THALIA_GPU_DURATION_VBLANK 114
#define THALIA_GPU_DURATION_SCAN_OAM 20
#define THALIA_GPU_DURATION_SCAN_VRAM 43

#define THALIA_GPU_SCREEN_HEIGHT 144
#define THALIA_GPU_SCREEN_WIDTH 160
#define THALIA_GPU_SCREEN_HEIGHT_EXTRA 154
#define THALIA_GPU_N_SPRITES 40
#define THALIA_GPU_MAX_SPRITES_ON_LINE 10

// Sprite, corresponds with the memory layout in object attribute memory (OAM).
typedef struct {
    guint8 ypos;
    guint8 xpos;
    guint8 tileno;
    guint8 padding : 4;
    gboolean palette : 1;
    gboolean xflip : 1;
    gboolean yflip : 1;
    gboolean priority : 1;
} thalia_sprite_t;

// GPU modes, correspond with the lower two bits of the LCD status register.
typedef enum {
    THALIA_GPU_MODE_HBLANK = 0,
    THALIA_GPU_MODE_VBLANK = 1,
    THALIA_GPU_MODE_SCAN_OAM = 2,
    THALIA_GPU_MODE_SCAN_VRAM = 3
} thalia_gpu_mode_t;

typedef struct {
    guint32 done;      // Cycles the GPU has processed.
    GdkPixbuf* screen; // Pixel buffer to draw the screen on.
    gint64 last_change;
    gint64 periods;
    GMutex mutex;
} thalia_gpu_t;
#endif

#ifdef __THALIA_GB_T__
void thalia_gpu_lock(ThaliaGB* gb);
void thalia_gpu_unlock(ThaliaGB* gb);
void thalia_gpu_mark_change(ThaliaGB* gb);
void thalia_gpu_step(ThaliaGB* gb);
void thalia_gpu_handle_dma(ThaliaGB* gb, guint8 addr_msb);
#endif
