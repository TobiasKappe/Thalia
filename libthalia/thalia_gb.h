#ifndef __THALIA_GB_H__
#define __THALIA_GB_H__

#include <glib.h>
#include <glib-object.h>

#include "thalia_reg.h"
#include "thalia_gpu.h"
#include "thalia_mmu.h"
#include "thalia_keypad.h"
#include "thalia_timer.h"

// Macros for use by the GObject type system
#define THALIA_TYPE_GB            (thalia_gb_get_type())
#define THALIA_GB(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                   THALIA_TYPE_GB, ThaliaGB))
#define THALIA_IS_GB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                   THALIA_TYPE_GB))
#define THALIA_GB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  \
                                   THALIA_TYPE_GB, ThaliaGBClass))
#define THALIA_IS_GB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  \
                                   THALIA_TYPE_GB))
#define THALIA_GB_GET_CLASS       (G_TYPE_INSTANCE_GET_CLASS((obj),  \
                                   THALIA_TYPE_GB, ThaliaGBClass))
#define THALIA_ERROR              (thalia_error_quark())

// Error conditions
typedef enum {
	THALIA_ERROR_INVALID_ROM,
	THALIA_ERROR_BANK_SIZE,
	THALIA_ERROR_UNKNOWN_CARTRIDGE,
	THALIA_ERROR_INVALID_CHECKSUM,
} thalia_error_t;

// Cartridge types, correspond with values in ROM header.
typedef enum {
	THALIA_CARTRIDGE_ROMONLY = 0x00,
	THALIA_CARTRIDGE_MBC1    = 0x01,
	THALIA_CARTRIDGE_MBC2    = 0x02,
} thalia_cartridge_t;

// ROM header offsets of relevant data.
typedef enum {
	THALIA_HEADER_CARTRIDGE = 0x0147,
	THALIA_HEADER_ROMSIZE   = 0x0148,
	THALIA_HEADER_START     = 0x0134,
	THALIA_HEADER_END       = 0x014C,
	THALIA_HEADER_CHECKSUM  = 0x014D
} thalia_header_loc_t;

// Main gameboy type. Represents a complete state of execution.
typedef struct {
	GObject parent_instance;

	thalia_cartridge_t cartridge; // Cartridge type

	gboolean halted;
	gboolean stopped;
	gboolean interrupts;          // Whether interrupts are enabled
	guint16 pc;                   // Program counter
	guint16 sp;                   // Stack pointer

	thalia_reg_t reg;             // Registry
	thalia_mmu_t* mmu;            // Memory
	thalia_gpu_t gpu;             // Graphics
	thalia_keypad_t keypad;       // Keypad I/O
	thalia_timer_t timer;         // Timer
	guint32 cycles;               // Current clock count

	guint8 enable_interrupts_in;  // Opcodes to go before enabling interrupts
	guint8 disable_interrupts_in; // Ditto, before disabling interrupts.
} ThaliaGB;
#define __THALIA_GB_T__

typedef struct {
	GObjectClass parent_class;
	void (* loadRom) (ThaliaGB*, const gchar* path, GError **error);
	void (* run) (ThaliaGB*);
	void (* renderScreen) (void); // render callback
} ThaliaGBClass;

GType thalia_gb_get_type();
GQuark thalia_error_quark();

// Exported methods
ThaliaGB* thalia_gb_new();
void thalia_gb_destroy();
void thalia_gb_load_rom(ThaliaGB* gb, const gchar* path, GError** error);
void thalia_gb_run(ThaliaGB* gb);
#endif
