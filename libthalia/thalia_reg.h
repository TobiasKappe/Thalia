#ifndef __THALIA_REG_H__
#define __THALIA_REG_H__

#include <glib.h>
#include "thalia_gb.h"

// Word register indices, correspond with opcode encoding.
typedef enum {
    THALIA_REG_BC = 0,
    THALIA_REG_DE = 1,
    THALIA_REG_HL = 2,
    THALIA_REG_AF = 3,
    THALIA_REG_SP = 3 // May use this index depending on context
} thalia_regname_double_t;

// Byte register indices, correspond with opcode encoding.
typedef enum {
    THALIA_REG_B   = 0,
    THALIA_REG_C   = 1,
    THALIA_REG_D   = 2,
    THALIA_REG_E   = 3,
    THALIA_REG_H   = 4,
    THALIA_REG_L   = 5,
    THALIA_REG_IHL = 6,
    THALIA_REG_F   = 6,
    THALIA_REG_A   = 7
} thalia_regname_single_t;

// Register substructure, laid out to allow for indexed access.
typedef union {
    guint8 indexed[8];
    struct {
        guint8 b, c;
        guint8 d, e;
        guint8 h, l;
      guint8 dummy : 4;
      gboolean flag_carry : 1;
      gboolean flag_halfcarry : 1;
      gboolean flag_operation : 1;
      gboolean flag_zero : 1;
        guint8 a; // Accumulator
    } named;
} thalia_reg_t;
#endif

#ifdef __THALIA_GB_T__
guint8 thalia_reg_read_single(ThaliaGB* gb, const thalia_regname_single_t reg);
guint16 thalia_reg_read_double(ThaliaGB* gb, const thalia_regname_double_t reg, gboolean format);
void thalia_reg_write_single(ThaliaGB* gb, const thalia_regname_single_t reg, const guint8 value);
void thalia_reg_write_double(ThaliaGB* gb, const thalia_regname_double_t reg,
guint16 value, gboolean format);
#endif
