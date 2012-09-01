#include <glib.h>
#include "thalia_gb.h"
#include "thalia_reg.h"
#include "thalia_mmu.h"

// Reads byte register 'reg'.
guint8 thalia_reg_read_single(ThaliaGB* gb, const thalia_regname_single_t reg)
{
#ifdef THALIA_DEBUG_REGS
	switch(reg) {
	case THALIA_REG_B:
		g_debug("(0x%04X) READ B: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	case THALIA_REG_C:
		g_debug("(0x%04X) READ C: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	case THALIA_REG_D:
		g_debug("(0x%04X) READ D: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	case THALIA_REG_E:
		g_debug("(0x%04X) READ E: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	case THALIA_REG_H:
		g_debug("(0x%04X) READ H: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	case THALIA_REG_L:
		g_debug("(0x%04X) READ L: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	case THALIA_REG_IHL:
		g_debug("(0x%04X) READ (HL): 0x%02X", gb->pc,
		 thalia_mmu_read_word(gb, thalia_reg_read_word(gb, THALIA_REG_HL, TRUE)));
		break;
	case THALIA_REG_A:
		g_debug("(0x%04X) READ A: 0x%02X", gb->pc, gb->reg.indexed[reg]);
		break;
	}
#endif

	if(reg == THALIA_REG_IHL) {
		// Read from (HL)
		guint16 offset = thalia_reg_read_double(gb, THALIA_REG_HL, TRUE);
		return thalia_mmu_read_byte(gb, offset);
		gb->cycles++;
	}
	else return gb->reg.indexed[reg];
}

// Reads word register 'reg'. If 'type' is TRUE, the index for SP is read as AF.
guint16 thalia_reg_read_double(ThaliaGB* gb, const thalia_regname_double_t reg,
 gboolean type)
{
	switch(reg) {
	case THALIA_REG_BC:
		return (gb->reg.named.b << 8) | gb->reg.named.c;
	case THALIA_REG_DE:
		return (gb->reg.named.d << 8) | gb->reg.named.e;
	case THALIA_REG_HL:
		return (gb->reg.named.h << 8) | gb->reg.named.l;
	case THALIA_REG_SP:
		return type ? ((gb->reg.named.a << 8) | gb->reg.indexed[6]) : gb->sp;
	}

	g_error("Unknown double register: %d", reg);
}

// Writes 'val' to byte register 'reg'.
void thalia_reg_write_single(ThaliaGB* gb, const thalia_regname_single_t reg,
 const guint8 value)
{
#ifdef THALIA_DEBUG_REGS
	switch(reg) {
	case THALIA_REG_B:
		g_debug("(0x%04X) WRITE B: 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_C:
		g_debug("(0x%04X) WRITE C: 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_D:
		g_debug("(0x%04X) WRITE D: 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_E:
		g_debug("(0x%04X) WRITE E: 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_H:
		g_debug("(0x%04X) WRITE H: 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_L:
		g_debug("(0x%04X) WRITE L: 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_IHL:
		g_debug("(0x%04X) WRITE (HL): 0x%02X", gb->pc, value);
		break;
	case THALIA_REG_A:
		g_debug("(0x%04X) WRITE A: 0x%02X", gb->pc, value);
		break;
	}
#endif

	if(reg == THALIA_REG_IHL) {
		// Write to (HL)
		guint16 offset = thalia_reg_read_double(gb, THALIA_REG_HL, TRUE);
		thalia_mmu_write_byte(gb, offset, value);
		gb->cycles++;
	}
	else gb->reg.indexed[reg] = value;
}

// Write 'val' to word register 'reg'; 'type' works the same as when reading.
void thalia_reg_write_double(ThaliaGB* gb, const thalia_regname_double_t reg,
 const guint16 value, gboolean type)
{
#ifdef THALIA_DEBUG_REGS
	switch(reg) {
	case THALIA_REG_BC:
		g_debug("(0x%04X) WRITE BC: 0x%04X", gb->pc, value);
		break;
	case THALIA_REG_DE:
		g_debug("(0x%04X) WRITE DE: 0x%04X", gb->pc, value);
		break;
	case THALIA_REG_HL:
		g_debug("(0x%04X) WRITE HL: 0x%04X", gb->pc, value);
		break;
	case THALIA_REG_SP:
		if(!type)
			g_debug("(0x%04X) WRITE SP: 0x%04X", gb->pc, value);
		else g_debug("(0x%04X) WRITE AF: 0x%04X", gb->pc, value);
		break;
	}
#endif

	// Save the word into individual byte registers.
	switch(reg) {
	case THALIA_REG_BC:
		gb->reg.named.b = (value >> 8) & 0xFF;
		gb->reg.named.c = value & 0xFF;
		break;
	case THALIA_REG_DE:
		gb->reg.named.d = (value >> 8) & 0xFF;
		gb->reg.named.e = value & 0xFF;
		break;
	case THALIA_REG_HL:
		gb->reg.named.h = (value >> 8) & 0xFF;
		gb->reg.named.l = value & 0xFF;
		break;
	case THALIA_REG_SP:
		if(type) {
			gb->reg.named.a = (value >> 8) & 0xFF;
			gb->reg.indexed[6] = value & 0xF0;
		} else {
			gb->sp = value;
		}
	}
}
