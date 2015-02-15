#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include "thalia_gb.h"
#include "thalia_gpu.h"
#include "thalia_mmu.h"

// TODO: Implement this using a mutex in a non-broken glib
G_LOCK_DEFINE(thalia_gpu);

// Locks the GPU.
void thalia_gpu_lock()
{
    G_LOCK(thalia_gpu);
}

// Unlocks the GPU.
void thalia_gpu_unlock()
{
    G_UNLOCK(thalia_gpu);
}

// Handles a DMA (direct memory access) request.
void thalia_gpu_handle_dma(ThaliaGB* gb, guint8 addr_msb)
{
    guint16 addr = addr_msb << 8;
    guint8 i;

    // Copy data into OAM from the written address.
    for(i = 0; i < 0xA0; i++)
        thalia_mmu_write_byte(
            gb,
            0xFE00 + i,
            thalia_mmu_read_byte(gb, addr + i)
        );
    gb->cycles += 0xA0; // TODO: Figure out clock penalty
}

// Check for a scanline coincidence interrupt using the current line and
// compare line I/O registers and schedule the interrupt if one has occured.
static void thalia_gpu_check_scanline_interrupt(ThaliaGB* gb)
{
    if(gb->mmu->ram_io.unpacked.int_scanline_coincidence &&
       gb->mmu->ram_io.unpacked.line_cur ==
       gb->mmu->ram_io.unpacked.line_cmp) {
      gb->mmu->ram_io.unpacked.scanline_coincidence = TRUE;
      gb->mmu->ram_io.unpacked.int_flag_lcd = TRUE;
    } else
        gb->mmu->ram_io.unpacked.scanline_coincidence = FALSE;
}

// Check for an interrupt on the GPU status and schedule one if necessary.
static void thalia_gpu_check_status_interrupt(ThaliaGB* gb)
{
    switch(gb->mmu->ram_io.unpacked.gpu_mode) {
    case THALIA_GPU_MODE_HBLANK:
        if(!gb->mmu->ram_io.unpacked.int_scan_hblank)
            return;
    case THALIA_GPU_MODE_VBLANK:
        if(!gb->mmu->ram_io.unpacked.int_scan_vblank)
            return;
    case THALIA_GPU_MODE_SCAN_OAM:
        if(!gb->mmu->ram_io.unpacked.int_scan_oam)
            return;
    default:
        return;
    }

    // An LCD interrupt can be scheduled at this point.
    gb->mmu->ram_io.unpacked.scanline_coincidence = FALSE;
    gb->mmu->ram_io.unpacked.int_flag_lcd = TRUE;
    return;
}

void thalia_gpu_mark_change(ThaliaGB* gb)
{
    gb->gpu.last_change = gb->gpu.periods;
}

static void thalia_gpu_render_line_background(ThaliaGB* gb, gint screen_ypos,
                                              guchar* pixel, gint n_channels)
{
    gint i;
    guint8 (*wd_tmap)[32][32];
    guint8 (*bg_tmap)[32][32];
    guint8 screen_xpos;

    // Determine which maps are the base window and background tile map.
    if(gb->mmu->ram_io.unpacked.lcd_wd_tile)
        wd_tmap = &gb->mmu->ram_gpu.unpacked.tilemap_1;
    else
        wd_tmap = &gb->mmu->ram_gpu.unpacked.tilemap_0;
    if(gb->mmu->ram_io.unpacked.lcd_bg_tile)
        bg_tmap = &gb->mmu->ram_gpu.unpacked.tilemap_1;
    else
        bg_tmap = &gb->mmu->ram_gpu.unpacked.tilemap_0;

    if(gb->mmu->ram_io.unpacked.lcd_bg_display ||
       gb->mmu->ram_io.unpacked.lcd_wd_display) {
        // First render the background and the window.
        for(screen_xpos = 0;
            screen_xpos < THALIA_GPU_SCREEN_WIDTH;
            screen_xpos++) {

            guint8 in_buffer_x;
            guint8 in_buffer_y;
            guint8 (*tmap)[32][32];
            guint8 (*tile)[8][2];
            guint8 palette = gb->mmu->ram_io.unpacked.pal_bg;

            // Inside the window, we need to render from a different tilemap.
            if(screen_ypos >= gb->mmu->ram_io.unpacked.window_y &&
               screen_xpos >= gb->mmu->ram_io.unpacked.window_x &&
               gb->mmu->ram_io.unpacked.lcd_wd_display) {
              // Buffer coordinates are offset by the window position.
                in_buffer_x = screen_xpos - gb->mmu->ram_io.unpacked.window_x;
                in_buffer_y = screen_ypos - gb->mmu->ram_io.unpacked.window_y;
                tmap = wd_tmap;
            } else {
                // If we're not, select the default tile map.
                in_buffer_x = screen_xpos + gb->mmu->ram_io.unpacked.scroll_x;
                in_buffer_y = screen_ypos + gb->mmu->ram_io.unpacked.scroll_y;
                tmap = bg_tmap;
            }

            // Grab the tile number from the map.
            guint8 tile_no = (*tmap)[in_buffer_y >> 3][in_buffer_x >> 3];
            guint8 tile_x = in_buffer_x & 0x07;
            guint8 tile_y = in_buffer_y & 0x07;

            // Get the tile from the tileset as indicated in the lcd control
            // register.
            if(gb->mmu->ram_io.unpacked.lcd_tile_data) {
                if(tile_no < 0x80)
                    tile = &gb->mmu->ram_gpu.unpacked.tileset_1[tile_no];
                else
                    tile = &gb->mmu->ram_gpu.unpacked.tileset_s[
                        tile_no - 0x80
                    ];
            } else {
                // The tile number is signed when this set is used.
                gint8 tile_no_signed = (gint8)tile_no;
                if(tile_no_signed < 0)
                    tile = &gb->mmu->ram_gpu.unpacked.tileset_s[
                        0x80+tile_no_signed
                    ];
                else
                    tile = &gb->mmu->ram_gpu.unpacked.tileset_0[tile_no];
            }

            // Now that we have the right tile, get the color code at this
            // position.
            guint8 code = 0;
            code |= ((*tile)[tile_y][0] & (0x80 >> tile_x)) ? 1 : 0;
            code |= ((*tile)[tile_y][1] & (0x80 >> tile_x)) ? 2 : 0;

            // Pull the color code through the palette to get the actual color.
            guint8 color = (palette & (3 << (code*2))) >> (code*2);

            // Paint said color in the buffer.
            for(i = 0; i < n_channels; i++)
                pixel[i] = 0xFF-85*color;
            pixel += n_channels;
        }
    } else {
        // If window and background are both disabled, render a white line.
        for(screen_xpos = 0;
            screen_xpos < THALIA_GPU_SCREEN_WIDTH;
            screen_xpos++) {

            for(i = 0; i < n_channels; i++)
                pixel[i] = 0xFF;
            pixel += n_channels;
        }
    }
}

static void thalia_gpu_render_line_sprites(ThaliaGB* gb, gint screen_ypos,
 guchar* pixel_base, gint n_channels)
{
    gint i;
    gint sprite_index;
    guint8 sprites_drawn = 0;

    // Enumerate all sprites to check for those to be drawn on this line.
    for(sprite_index = THALIA_GPU_N_SPRITES-1; sprite_index >= 0 &&
        sprites_drawn < THALIA_GPU_MAX_SPRITES_ON_LINE; sprite_index--) {
      // First grab the sprite object at this location from OAM.
        thalia_sprite_t sprite = gb->mmu->ram_oam.unpacked[sprite_index];
        guint8 tile_y;
        guint8 tile_xoff;
        guint8 tile_yoff;
        guint8 (*tile)[8][2];
        guint8 palette;
        gint16 real_ypos = sprite.ypos - 16;
        gint16 real_xpos = sprite.xpos - 8;
        guchar* pixel_cur;

        // Skip this sprite if it does not intersect with the scanline.
        if(real_ypos > screen_ypos || real_ypos+8 <= screen_ypos)
            continue;

        // Determine the pallette we're using for this sprite.
        if(sprite.palette)
            palette = gb->mmu->ram_io.unpacked.pal_obj1;
        else
            palette = gb->mmu->ram_io.unpacked.pal_obj0;

        // Grab the tile to be rendered from.
        if(sprite.tileno < 0x80)
            tile = &gb->mmu->ram_gpu.unpacked.tileset_1[sprite.tileno];
        else
            tile = &gb->mmu->ram_gpu.unpacked.tileset_s[sprite.tileno-0x80];

        // Determine offset within the tile and possibly flip Y coordinates.
        tile_yoff = screen_ypos - real_ypos;
        tile_y = sprite.yflip ? 7-tile_yoff : tile_yoff;

        // Start rendering the line intersecting with the line to be rendered.
        pixel_cur = pixel_base + real_xpos*n_channels;
        for(tile_xoff = 0; tile_xoff < 8; tile_xoff++) {
            guint8 code = 0;
            // Flip X coordinates if the sprite attributes demand so.
            guint8 tile_x = sprite.xflip ? 7-tile_xoff : tile_xoff;

            // Get the color from the palette like we do for the background.
            code |= ((*tile)[tile_y][0] & 0x80 >> tile_x) ? 1 : 0;
            code |= ((*tile)[tile_y][1] & 0x80 >> tile_x) ? 2 : 0;

            // Code zero means transparent, so we don't render that particular
            // pixel. Also, pixels of non-priority sprites are only rendered if
            // the current color at that position is white.
            if(code && (!sprite.priority || (pixel_cur[0] == 0xFF &&
                 pixel_cur[1] == 0xFF && pixel_cur[2] == 0xFF))) {
                guint8 color = (palette & (3 << (code*2))) >> (code*2);
                for(i = 0; i < n_channels; i++)
                    pixel_cur[i] = 0xFF-85*color;
            }
            pixel_cur += n_channels;
        }

        // Keep track of the number of sprites on the line, there's a maximum.
        sprites_drawn++;
    }
}

// Renders a line on the main pixel buffer.
static void thalia_gpu_render_line(ThaliaGB* gb)
{
    gint rowstride;
    gint n_channels;
    guint8 screen_ypos;
    guchar* pixel_base;

    if(gb->gpu.last_change - gb->gpu.periods > 2)
        return; // TODO: overflow?

    n_channels = gdk_pixbuf_get_n_channels(gb->gpu.screen);
    rowstride = gdk_pixbuf_get_rowstride(gb->gpu.screen);
    screen_ypos = gb->mmu->ram_io.unpacked.line_cur;
    pixel_base = gdk_pixbuf_get_pixels(gb->gpu.screen) + rowstride*screen_ypos;

    g_assert(screen_ypos <= THALIA_GPU_SCREEN_HEIGHT);
    g_assert(n_channels == 3);

    // First render the background of this line.
    thalia_gpu_render_line_background(gb, screen_ypos, pixel_base, n_channels);

    // Then look for sprites we might need to draw on top.
    thalia_gpu_render_line_sprites(gb, screen_ypos, pixel_base, n_channels);
}

// Emulates the end of a horizontal blanking period.
static void thalia_gpu_hblank(ThaliaGB* gb)
{
    gb->gpu.done += THALIA_GPU_DURATION_HBLANK;

    if(gb->mmu->ram_io.unpacked.line_cur < THALIA_GPU_SCREEN_HEIGHT &&
       gb->mmu->ram_io.unpacked.lcd_operation) {
        // If we're not at the end of the screen, go back to do another line.
        gb->mmu->ram_io.unpacked.gpu_mode = THALIA_GPU_MODE_SCAN_OAM;
        thalia_gpu_render_line(gb);
    } else {
        // If we are, go into vertical blanking mode.
        gb->mmu->ram_io.unpacked.int_flag_vblank = TRUE;
        gb->mmu->ram_io.unpacked.gpu_mode = THALIA_GPU_MODE_VBLANK;
    }

    // Skip one line ahead and check for possible interrupts.
    gb->mmu->ram_io.unpacked.line_cur++;
    thalia_gpu_check_scanline_interrupt(gb);
    thalia_gpu_check_status_interrupt(gb);
}

// Emulates the end of a vertical blanking period.
static void thalia_gpu_vblank(ThaliaGB* gb)
{
    gb->gpu.done += THALIA_GPU_DURATION_VBLANK;
    gb->mmu->ram_io.unpacked.line_cur++;
    thalia_gpu_check_scanline_interrupt(gb);

    if(gb->mmu->ram_io.unpacked.line_cur >= THALIA_GPU_SCREEN_HEIGHT_EXTRA) {
        // If at the end of the extra lines, go back to the top of the screen.
        gb->mmu->ram_io.unpacked.line_cur = 0;
        gb->mmu->ram_io.unpacked.gpu_mode = THALIA_GPU_MODE_SCAN_OAM;
        thalia_gpu_check_status_interrupt(gb);

        if(gb->gpu.periods - gb->gpu.last_change < 2) {
            // Signal the consuming code that the screen may now be rendered.
            // Note that we can only continue once the consuming code
            // relinquishes its lock. This is done to prevent the emulation
            // thread from continuing to render, causing screen tearing.
            thalia_gpu_unlock();
            g_signal_emit_by_name(G_OBJECT(gb), "thalia-render-screen");
            thalia_gpu_lock();
        } else
            gb->gpu.periods++;
    }
}

// Emulates the end of a scan of object attribute memory.
static void thalia_gpu_scan_oam(ThaliaGB* gb)
{
    gb->gpu.done += THALIA_GPU_DURATION_SCAN_OAM;
    gb->mmu->ram_io.unpacked.gpu_mode = THALIA_GPU_MODE_SCAN_VRAM;
    thalia_gpu_check_status_interrupt(gb);
}

// Emulates the end of a scan of visual RAM.
static void thalia_gpu_scan_vram(ThaliaGB* gb)
{
    gb->gpu.done += THALIA_GPU_DURATION_SCAN_VRAM;
    gb->mmu->ram_io.unpacked.gpu_mode = THALIA_GPU_MODE_HBLANK;
    thalia_gpu_check_status_interrupt(gb);
}

// Adjusts the GPU state to the machine state after opcode execution.
void thalia_gpu_step(ThaliaGB* gb)
{
    guint32 left;
    gb->mmu->ram_io.unpacked.int_flag_vblank = FALSE;

    // 'left' holds the cycles that the GPU has yet to emulate. If there's
    // enough time accumulated to reach the end of a mode, kick off a relevant
    // function.
    while((left = gb->cycles - gb->gpu.done) > 0) {
        switch(gb->mmu->ram_io.unpacked.gpu_mode) {
        case THALIA_GPU_MODE_HBLANK:
            if(left >= THALIA_GPU_DURATION_HBLANK)
                thalia_gpu_hblank(gb);
            else
                return;
            break;
        case THALIA_GPU_MODE_VBLANK:
            if(left >= THALIA_GPU_DURATION_VBLANK)
                thalia_gpu_vblank(gb);
            else
                return;
            break;
        case THALIA_GPU_MODE_SCAN_OAM:
            if(left >= THALIA_GPU_DURATION_SCAN_OAM)
                thalia_gpu_scan_oam(gb);
            else
                return;
            break;
        case THALIA_GPU_MODE_SCAN_VRAM:
            if(left >= THALIA_GPU_DURATION_SCAN_VRAM)
                thalia_gpu_scan_vram(gb);
            else
                return;
            break;
        }
    }
}
