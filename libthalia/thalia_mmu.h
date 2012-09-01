#ifndef __THALIA_MMU_H__
#define __THALIA_MMU_H__

#include <glib.h>
#include "thalia_gb.h"
#include "thalia_gpu.h"

#define THALIA_MMU_MAX_BANK_COUNT 0x80
#define THALIA_MMU_BANK_SIZE (0x4000)

// I/O region mapping, allows for easy access to often used registers.
// CAUTION: This memory layout will _not_ work on big-endian platforms!
typedef struct {
	guint8 keypad;                 // Key register, handled by thalia_keypad.c

	// Offset 0xFF01
	guint8 sio_data;

	// Offset 0xFF02
	gboolean sio_start : 1;
	guint8             : 6;        // Padding
	gboolean sioclock  : 1;

	guint8 : 8;                    // Padding

	guint8 time_divider;           // Offset 0xFF04
	guint8 time_count;             // Offset 0xFF05
	guint8 time_modulo;            // Offset 0xFF06

	// Offset 0xFF07
	guint8 time_clock   : 2;
	gboolean time_start : 1;
	guint8              : 5;       // Padding
	guint8 padding_1[0x07];        // Offset 0xFF07

	// Offset 0xFF0F
	gboolean int_flag_vblank : 1;
	gboolean int_flag_lcd    : 1;
	gboolean int_flag_timer  : 1;
	gboolean int_flag_serial : 1;
	gboolean int_flag_joypad : 1;
	guint8 : 3;                    // Padding

	guint8 padding_2[0x30];        // Offset 0xFF10

	// Offset 0xFF40
	gboolean lcd_bg_display   : 1;
	gboolean lcd_transparency : 1;
	gboolean lcd_sprite_size  : 1;
	gboolean lcd_bg_tile      : 1;
	gboolean lcd_tile_data    : 1;
	gboolean lcd_wd_display   : 1;
	gboolean lcd_wd_tile      : 1;
	gboolean lcd_operation    : 1;

	// Offset 0xFF41
	thalia_gpu_mode_t gpu_mode        : 2;
	gboolean int_scanline_coincidence : 1;
	gboolean int_scan_hblank          : 1;
	gboolean int_scan_vblank          : 1;
	gboolean int_scan_oam             : 1;
	gboolean scanline_coincidence     : 1;
	guint8                            : 1; // Padding

	guint8 scroll_y;                 // Offset FF42
	guint8 scroll_x;                 // Offset FF43
	guint8 line_cur;                 // Offset FF44
	guint8 line_cmp;                 // Offset FF45
	guint8 dma_cont;                 // Offset FF46
	guint8 pal_bg;                   // Offset FF47
	guint8 pal_obj0;                 // Offset FF48
	guint8 pal_obj1;                 // Offset FF49
	guint8 window_y;                 // Offset FF4A
	guint8 window_x;                 // Offset FF4B
	guint8 padding[34];
} thalia_io_t;

typedef struct {
	guint8* rom_bank0;                // Offset 0x0000
	guint8* rom_bankn;                // Offset 0x4000
	union {
		guint8 packed[0x2000];
		struct {
			guint8 tileset_1[128][8][2];  // offset 0x8000
			guint8 tileset_s[128][8][2];  // offset 0x8800
			guint8 tileset_0[128][8][2];  // offset 0x9000
			guint8 tilemap_0[32][32];
			guint8 tilemap_1[32][32];
		} unpacked;
	} ram_gpu;                        // Offset 0x8000
	guint8 ram_ext[0x2000];           // Offset 0xA000
	guint8 ram_int[0x2000];           // Offset 0xC000
	union {
		guint8 packed[0x80];
		thalia_io_t unpacked;
	} ram_io;                         // Offset 0xFF00
	union {
		guint8 packed[0xA0];
		thalia_sprite_t unpacked[THALIA_GPU_N_SPRITES];
	} ram_oam;                        // Offset 0xFE00
	union {
		guint8 packed[0x80];
		struct {
			guint8 padding[0x7F];
			// Offset 0xFFFF
			gboolean int_enable_vblank : 1;
			gboolean int_enable_lcd    : 1;
			gboolean int_enable_timer  : 1;
			gboolean int_enable_serial : 1;
			gboolean int_enable_pins   : 1;
			guint8                     : 3;  // Padding
		} unpacked;
	} ram_page0; // Offset 0xFF80
	gint rom_size;
	struct {
		gboolean enable_ext_ram;
		gboolean mode;
		guint8 rom_bank;
		guint8 ram_bank;
	} mbc;
	guint8** rom_banks;
} thalia_mmu_t;
#endif

#ifdef __THALIA_GB_T__
void thalia_mmu_read_bank(GIOChannel* channel, guint8* dest, GError** error);

// Generic reading/writing
guint8 thalia_mmu_read_byte(ThaliaGB* gb, guint16 addr);
guint16 thalia_mmu_read_word(ThaliaGB* gb, guint16 addr);

// Stack management
void thalia_mmu_write_byte(ThaliaGB* gb, guint16 addr, guint8 val);
void thalia_mmu_write_word(ThaliaGB* gb, guint16 addr, guint16 val);
void thalia_mmu_push_word(ThaliaGB* gb, guint16 val);
guint16 thalia_mmu_pop_word(ThaliaGB* gb);

// Immediate fetching
guint16 thalia_mmu_immediate_word(ThaliaGB* gb);
guint8 thalia_mmu_immediate_byte(ThaliaGB* gb);
#endif
