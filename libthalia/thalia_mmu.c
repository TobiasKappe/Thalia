#include <glib.h>
#include "thalia_gb.h"
#include "thalia_mmu.h"
#include "thalia_keypad.h"
#include "thalia_gpu.h"

// Auxiliary function to read a bank from 'channel' into 'dest'.
void thalia_mmu_read_bank(GIOChannel* channel, guint8* dest, GError** error)
{
	gsize read;
	g_io_channel_read_chars(
		channel,
		(gchar*) dest,
		THALIA_MMU_BANK_SIZE,
		&read,
		error
	);

	// We want to read the exact amount of data requested to fill a bank.
	if(read < THALIA_MMU_BANK_SIZE)
		g_set_error(
			error,
			THALIA_ERROR,
			THALIA_ERROR_BANK_SIZE,
			"Could not read entire bank, is the file a ROM?"
		);
}

// Reads a byte from 'addr', performing mapping and I/O triggers.
guint8 thalia_mmu_read_byte(ThaliaGB* gb, guint16 addr)
{
	guint8 ret = 0;

	switch(addr & 0xF000) {
	case 0x0000: case 0x1000:
	case 0x2000: case 0x3000:
		ret = gb->mmu->rom_bank0[addr];
		break;
	case 0x4000: case 0x5000:
	case 0x6000: case 0x7000:
		// The pointer points to the current bank mapped to this range.
		ret = gb->mmu->rom_bankn[addr - 0x4000];
		break;
	case 0x8000: case 0x9000:
		ret = gb->mmu->ram_gpu.packed[addr - 0x8000];
		break;
	case 0xA000: case 0xB000:
		ret = gb->mmu->ram_ext[addr - 0xA000];
		break;
	case 0xC000: case 0xD000:
		ret = gb->mmu->ram_int[addr - 0xC000];
		break;
	case 0xE000:
		ret = gb->mmu->ram_int[addr - 0xE000];
	case 0xF000:
		switch(addr & 0x0F00) {
		case 0x0000: case 0x0100:
		case 0x0200: case 0x0300:
		case 0x0400: case 0x0500:
		case 0x0600: case 0x0700:
		case 0x0800: case 0x0900:
		case 0x0A00: case 0x0B00:
		case 0x0C00: case 0x0D00:
			// The first 0x0E00 bytes in this range are a copy of internal RAM
			ret = gb->mmu->ram_int[addr - 0xE000];
			break;
		case 0x0E00:
			// Only the first 0xA0 bytes in this range are meaningful
			ret = addr < 0xFEA0 ? gb->mmu->ram_oam.packed[addr - 0xFE00] : 0;
			break;
		case 0x0F00:
			if(addr == 0xFF00) {
				// The keypad register status is synthesised from gb->keypad
				ret = thalia_keypad_read(gb);
			} else {
				// Lower 0x80 bits are I/O RAM, upper 0x80 are zero-page RAM.
				ret = addr < 0xFF80 ? gb->mmu->ram_io.packed[addr - 0xFF00] :
				 gb->mmu->ram_page0.packed[addr - 0xFF80];
			}
			break;
		}
		break;
	}

#ifdef THALIA_DEBUG_MMU
	g_debug("(0x%04X) READ  @ 0x%04X: 0x%02X", gb->pc, addr, ret);
#endif
	return ret;
}

// Reads a words from 'addr' in little-endian format.
guint16 thalia_mmu_read_word(ThaliaGB* gb, guint16 addr)
{
	guint16 ret = thalia_mmu_read_byte(gb, addr) |
	 thalia_mmu_read_byte(gb, addr + 1) << 8;
#ifdef THALIA_DEBUG_MMU
	g_debug("(0x%04X) READ  @ 0x%04X: 0x%04X", gb->pc, addr, ret);
#endif
	return ret;
}

// Writes 'val' to 'addr', performing mapping and I/O steps.
void thalia_mmu_write_byte(ThaliaGB* gb, guint16 addr, guint8 val)
{
#ifdef THALIA_DEBUG_MMU
	g_debug("(0x%04X) WRITE @ 0x%04X: 0x%02X", gb->pc, addr, val);
#endif
	switch(addr & 0xF000) {
	case 0x0000: case 0x1000:
		gb->mmu->mbc.enable_ext_ram = val == 0xA0;
		return;
	case 0x2000: case 0x3000:
		val = val & 0x1F;
		if(val == 0) val = 1;
		// Writes to this range cause a change in the lower five bits of the ROM
		// bank number, which is mapped to 0x4000-0x7FFF.
		gb->mmu->mbc.rom_bank = (gb->mmu->mbc.rom_bank & 0x60) | val;
		gb->mmu->rom_bankn = gb->mmu->rom_banks[gb->mmu->mbc.rom_bank];
		return;
	case 0x4000: case 0x5000:
		if(gb->mmu->mbc.mode) {
			// Writing to this range while in RAM mode causes a change in the RAM
			// bank number, which is mapped to 0xA000-0xBFFF
			g_warning("Switching RAM bank (not implemented!)");
			gb->mmu->mbc.ram_bank = val & 4;
		} else {
			// Writing to this range while in ROM mode causes a change in the upper
			// two bits of the ROM bank number.
			gb->mmu->mbc.rom_bank = ((val & 3) << 5) | (gb->mmu->mbc.rom_bank & 0x1F);
			gb->mmu->rom_bankn = gb->mmu->rom_banks[gb->mmu->mbc.rom_bank];
		}
		return;
	case 0x6000: case 0x7000:
		// Writes to this range change the MBC addressing mode to RAM-mode
		// (least significant bit 1) or ROM-mode (least significant bit 0)
		gb->mmu->mbc.mode = val & 0x01;
		return;
	case 0x8000: case 0x9000:
		gb->mmu->ram_gpu.packed[addr - 0x8000] = val;
		thalia_gpu_mark_change(gb);
		return;
	case 0xA000: case 0xB000:
		gb->mmu->ram_ext[addr - 0xA000] = val;
		return;
	case 0xC000: case 0xD000:
		gb->mmu->ram_int[addr - 0xC000] = val;
		return;
	case 0xE000:
		gb->mmu->ram_int[addr - 0xE000] = val;
	case 0xF000:
		switch(addr & 0x0F00) {
		case 0x0000: case 0x0100:
		case 0x0200: case 0x0300:
		case 0x0400: case 0x0500:
		case 0x0600: case 0x0700:
		case 0x0800: case 0x0900:
		case 0x0A00: case 0x0B00:
		case 0x0C00: case 0x0D00:
			// The first 0x0E00 bytes in this range are a copy of internal RAM.
			gb->mmu->ram_int[addr - 0xE000] = val;
			return;
		case 0x0E00:
			// Only the first 0xA0 bytes contain information, the rest are ignored.
			if(addr <= 0xFEA0) {
				gb->mmu->ram_oam.packed[addr - 0xFE00] = val;
				thalia_gpu_mark_change(gb);
			}
			return;
		case 0x0F00:
			switch(addr) {
			case 0xFF00:
				// Writes to the keypad trigger selection of key columns.
				thalia_keypad_write(gb, val);
				return;
			case 0xFF46:
				// Writes to this address trigger DMA
				thalia_gpu_handle_dma(gb, val);
				thalia_gpu_mark_change(gb);
				return;
			case 0xFF40: case 0xFF41:
			case 0xFF42: case 0xFF43:
			case 0xFF44: case 0xFF47:
			case 0xFF48: case 0xFF49:
			case 0xFF4A: case 0xFF4B:
				// Fallthrough
				thalia_gpu_mark_change(gb);
			default:
				if(addr < 0xFF80)
					gb->mmu->ram_io.packed[addr - 0xFF00] = val;
				else
					gb->mmu->ram_page0.packed[addr - 0xFF80] = val;
				return;
			}
		}
	}
}

// Writes word 'val' to 'addr' in little-endian format.
void thalia_mmu_write_word(ThaliaGB* gb, guint16 addr, guint16 val)
{
#ifdef THALIA_DEBUG_MMU
	g_debug("(0x%04X) WRITE @ 0x%04X: 0x%04X", gb->pc, addr, val);
#endif
	thalia_mmu_write_byte(gb, addr, val & 0xFF);
	thalia_mmu_write_byte(gb, addr + 1, val >> 8);
}

// Pushes 'val' onto the stack.
void thalia_mmu_push_word(ThaliaGB* gb, guint16 val)
{
	gb->sp -= 2;
#ifdef THALIA_DEBUG_STACK
	g_debug("(0x%04X) PUSHING 0x%04X @ 0x%04X", gb->pc, val, gb->sp);
#endif
	thalia_mmu_write_word(gb, gb->sp, val);
}

// Pops a word from the stack.
guint16 thalia_mmu_pop_word(ThaliaGB* gb)
{
	guint16 ret = thalia_mmu_read_word(gb, gb->sp);
#ifdef THALIA_DEBUG_STACK
	g_debug("(0x%04X) POPPING 0x%04X @ 0x%04X", gb->pc, ret, gb->sp);
#endif
	gb->sp += 2;
	return ret;
}

// Reads the byte next to the program counter.
guint16 thalia_mmu_immediate_word(ThaliaGB* gb)
{
	return thalia_mmu_read_word(gb, gb->pc + 1);
}

// Reads the word next to the program counter.
guint8 thalia_mmu_immediate_byte(ThaliaGB* gb)
{
	return thalia_mmu_read_byte(gb, gb->pc + 1);
}
