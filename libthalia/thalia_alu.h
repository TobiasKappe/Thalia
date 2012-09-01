#ifndef __THALIA_ALU_H__
#define __THALIA_ALU_H__

#include <glib.h>
#include "thalia_gb.h"

// Jump conditions. Assigned values correspond with opcode encoding.
typedef enum {
	THALIA_COND_NZ = 0,
	THALIA_COND_Z  = 1,
	THALIA_COND_NC = 2,
	THALIA_COND_C  = 3
} thalia_cond_t;

// Conditions
gboolean thalia_alu_condition_satisfied(ThaliaGB* gb, thalia_cond_t cond);

// Addition
guint8 thalia_alu_add(ThaliaGB* gb, guint8 a, guint8 b, gboolean update_carry);
guint8 thalia_alu_add_carry(ThaliaGB* gb, guint8 a, guint8 b);
guint16 thalia_alu_add_16bit(ThaliaGB* gb, guint16 a, guint16 b);
guint16 thalia_alu_add_16bit_mixed(ThaliaGB* gb, guint16 a, gint8 b);

// Subtraction
guint8 thalia_alu_sub(ThaliaGB* gb, guint8 a, guint8 b, gboolean update_carry);
guint8 thalia_alu_sub_carry(ThaliaGB* gb, guint8 a, guint8 b);

// Bitwise boolean operators
guint8 thalia_alu_and(ThaliaGB* gb, guint8 a, guint8 b);
guint8 thalia_alu_xor(ThaliaGB* gb, guint8 a, guint8 b);
guint8 thalia_alu_or(ThaliaGB* gb, guint8 a, guint8 b);
guint8 thalia_alu_daa(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_cpl(ThaliaGB* gb, guint8 a);

// Carry flag
void thalia_alu_scf(ThaliaGB* gb);
void thalia_alu_ccf(ThaliaGB* gb);

// Shifting
guint8 thalia_alu_rrc(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_rlc(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_rr(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_rl(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_sra(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_sla(ThaliaGB* gb, guint8 a);
guint8 thalia_alu_srl(ThaliaGB* gb, guint8 a);

// Bit manipulation
guint8 thalia_alu_swap(ThaliaGB* gb, guint8 a);
void thalia_alu_bit(ThaliaGB* gb, guint8 a, guint8 bit);
guint8 thalia_alu_res(ThaliaGB* gb, guint8 a, guint8 bit);
guint8 thalia_alu_set(ThaliaGB* gb, guint8 a, guint8 bit);

#endif
