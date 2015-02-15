#include <glib.h>
#include "thalia_gb.h"
#include "thalia_proc.h"
#include "thalia_mmu.h"
#include "thalia_gpu.h"
#include "thalia_reg.h"
#include "thalia_timer.h"

// Export the ThaliaGB type for external bindings.
G_DEFINE_TYPE(ThaliaGB, thalia_gb, G_TYPE_OBJECT);

// The error quark used by this namespace.
GQuark thalia_error_quark()
{
    return g_quark_from_static_string("thalia-error-quark");
}

// Creates a new ThaliaGB instance.
ThaliaGB* thalia_gb_new()
{
    return THALIA_GB(g_object_new(THALIA_TYPE_GB, NULL));
}

// Destroys ThaliaGB instance in 'gb'.
void thalia_gb_destroy(ThaliaGB* gb)
{
    g_object_unref(gb);
}

// Finalizes the ThaliaGB instance encapsulated by 'obj'.
static void thalia_gb_finalize (GObject *obj)
{
    ThaliaGB* gb = THALIA_GB(obj);
    guint i;
    // Free the space allocated for memory banks.
    if(gb->mmu->rom_banks) {
        for(i = 0; i < THALIA_MMU_MAX_BANK_COUNT; i++)
            if(gb->mmu->rom_banks[i])
                g_free(gb->mmu->rom_banks[i]);
        g_free(gb->mmu->rom_banks);
    }

    g_free(gb->mmu);
    g_object_unref(gb->gpu.screen);

    // Pass on finalization to the parent class.
    G_OBJECT_CLASS(thalia_gb_parent_class)->finalize(obj);
}

// Initialize 'gb' with memory and register values as set by the BIOS.
// Also prepare structures for memory and graphics emulation.
static void thalia_gb_init(ThaliaGB* gb)
{
    // Values set by boot sequence.
    gb->reg.named.c = 0x13;
    gb->reg.named.e = 0xD8;
    gb->reg.named.h = 0x01;
    gb->reg.named.l = 0x4D;
    gb->sp = 0xFFFE;
    gb->pc = 0x0100;

    gb->mmu = g_new0(thalia_mmu_t, 1);
    gb->mmu->ram_io.unpacked.lcd_operation = TRUE;
    gb->mmu->ram_io.unpacked.lcd_wd_display = TRUE;
    gb->mmu->ram_io.unpacked.lcd_bg_display = TRUE;
    gb->mmu->ram_io.unpacked.lcd_tile_data = TRUE;

    gb->interrupts = TRUE;
    gb->mmu->mbc.ram_bank = 0;
    gb->mmu->mbc.rom_bank = 1;
    gb->mmu->mbc.enable_ext_ram = TRUE;

    // Create a pixel buffer to blit pixels in, start with a black screen
    gb->gpu.screen = gdk_pixbuf_new(
        GDK_COLORSPACE_RGB,
        FALSE, 8,
        THALIA_GPU_SCREEN_WIDTH,
        THALIA_GPU_SCREEN_HEIGHT
    );
    gdk_pixbuf_fill(gb->gpu.screen, 0x00000000);
}

// Initialize the ThaliaGB class by setting up methods and signals.
static void thalia_gb_class_init(ThaliaGBClass* klass)
{
    klass->loadRom = thalia_gb_load_rom;
    klass->run = thalia_gb_run;
    G_OBJECT_CLASS(klass)->finalize = thalia_gb_finalize;

    // Signal tied to vblank, when the display can be redrawn.
    g_signal_new("thalia-render-screen",
                 THALIA_TYPE_GB,
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(ThaliaGBClass, renderScreen),
                 NULL,
                 NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0
    );
}

// Grab useful values from the ROM header.
static void thalia_gb_decode_header(ThaliaGB* gb)
{
    gb->cartridge = gb->mmu->rom_bank0[THALIA_HEADER_CARTRIDGE];
    gb->mmu->rom_size = 1 << (15 + gb->mmu->rom_bank0[THALIA_HEADER_ROMSIZE]);
}

// Check whether the checksum included in the ROM header matches the data.
static gboolean thalia_gb_check_header(ThaliaGB* gb)
{
    return 1;
    guint16 sum = 0;
    guint16 i;
    for(i = THALIA_HEADER_START; i <= THALIA_HEADER_END; i++)
        sum -= thalia_mmu_read_byte(gb, i)+1;
    return thalia_mmu_read_byte(gb, THALIA_HEADER_CHECKSUM) == (sum & 0xFF);
}

// Loads the ROM from file 'path'.
void thalia_gb_load_rom(ThaliaGB* gb, const gchar* path, GError** error)
{
    GIOChannel* channel;
    channel = g_io_channel_new_file(path, "r", error);
    if(*error)
        return;

    // Switch to raw encoding, allowing us to read non-ASCII characters.
    g_io_channel_set_encoding(channel, NULL, error);
    if(*error)
        return;

    // Bank 0 is always allocated
    gb->mmu->rom_banks = g_new0(guint8*, THALIA_MMU_MAX_BANK_COUNT);
    gb->mmu->rom_banks[0] = g_new0(guint8, THALIA_MMU_BANK_SIZE);
    gb->mmu->rom_bank0 = gb->mmu->rom_banks[0];
    thalia_mmu_read_bank(channel, gb->mmu->rom_banks[0], error);
    if(*error)
        return;

    if(!thalia_gb_check_header(gb)) {
        // Do not continue if we have an invalid checksum.
        g_set_error(
            error,
            THALIA_ERROR,
            THALIA_ERROR_INVALID_CHECKSUM,
            "Invalid ROM header checksum"
        );
        return;
    }

    thalia_gb_decode_header(gb);

    // Now that we know the rom size, we should load as many banks from the
    // source ROM file into subsequent banks.
    guint8 i;
    for(i = 1; i < gb->mmu->rom_size / THALIA_MMU_BANK_SIZE; i++) {
        // TODO: It would appear that multitudes of 32 are mapped to bank zero.
        // Will need a ROM that uses >32 banks to investigate.
        gb->mmu->rom_banks[i] = g_new0(guint8, THALIA_MMU_BANK_SIZE);
        thalia_mmu_read_bank(channel, gb->mmu->rom_banks[i], error);
        if(*error)
            return;
    }

    // Employ different behavior dependent on cartridge type.
    switch(gb->cartridge) {
    case THALIA_CARTRIDGE_ROMONLY:
    case THALIA_CARTRIDGE_MBC1:
    case THALIA_CARTRIDGE_MBC2:
        gb->mmu->rom_bankn = gb->mmu->rom_banks[1];
        break;
    default:
        g_set_error(
            error,
            THALIA_ERROR,
            THALIA_ERROR_UNKNOWN_CARTRIDGE,
            "Cartridge type 0x%02X unimplemented",
            gb->cartridge
        );
        return;
    }

    g_io_channel_shutdown(channel, TRUE, error);
    if(*error)
        return;
    g_io_channel_unref(channel);
    return;
}

// Prepares an interrupt to start at address 'addr'.
static void thalia_gb_start_interrupt(ThaliaGB* gb, guint16 int_addr)
{
    gb->interrupts = FALSE;
    thalia_mmu_push_word(gb, gb->pc);
    gb->pc = int_addr;
}

// Handles interrupts for the ThaliaGB instance.
static void thalia_gb_handle_interrupts(ThaliaGB* gb)
{
    // Check if we are due to enable or disable interrupts at this point.
    if(gb->disable_interrupts_in > 0) {
        gb->disable_interrupts_in--;
        if(gb->disable_interrupts_in == 0) {
            gb->interrupts = FALSE;
        }
    }
    if(gb->enable_interrupts_in > 0) {
        gb->enable_interrupts_in--;
        if(gb->enable_interrupts_in == 0)
            gb->interrupts = TRUE;
    }

    // Stop if the master switch for interrupts is off.
    if(!gb->interrupts && !gb->halted)
        return;

    // Check for pending interrupts that are enabled.
    if(gb->mmu->ram_io.unpacked.int_flag_vblank && \
       gb->mmu->ram_page0.unpacked.int_enable_vblank) {
      if(gb->interrupts) {
            gb->mmu->ram_io.unpacked.int_flag_vblank = FALSE;
            thalia_gb_start_interrupt(gb, 0x0040);
        } else
            gb->halted = FALSE;
        return;
    }
    if(gb->mmu->ram_io.unpacked.int_flag_lcd && \
       gb->mmu->ram_page0.unpacked.int_enable_lcd) {
      if(gb->interrupts) {
            gb->mmu->ram_io.unpacked.int_flag_lcd = FALSE;
            thalia_gb_start_interrupt(gb, 0x0048);
        } else
            gb->halted = FALSE;
        return;
    }
    if(gb->mmu->ram_io.unpacked.int_flag_timer && \
       gb->mmu->ram_page0.unpacked.int_enable_timer) {
      if(gb->interrupts) {
          gb->mmu->ram_io.unpacked.int_flag_timer = FALSE;
          thalia_gb_start_interrupt(gb, 0x0050);
        } else
            gb->halted = FALSE;
      return;
    }
}

// Runs the gameboy program in the instance indefinitely.
void thalia_gb_run(ThaliaGB* gb)
{
    while(TRUE) {
        // Fetch an opcode and execute it if we're not halted.
        if(!gb->halted) {
            guint8 opcode = thalia_mmu_read_byte(gb, gb->pc);
            if(!thalia_proc_decode(gb, opcode))
                return;
        } else
            gb->cycles++; // If we are, just spin idly waiting for interrupts.

        // Allow hardware emulation to adjust to the new machine state.
        thalia_gpu_step(gb);
        thalia_timer_step(gb);
        thalia_gb_handle_interrupts(gb);

        // TODO: Slow down execution to real-time speed taking the amount of
        // cycles used for each instruction into account, with optional
        // throttle.
    }
}
