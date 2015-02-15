#include <glib.h>
#include "thalia_gb.h"
#include "thalia_alu.h"

// Tells us whether condition code 'cond' is satisfied.
gboolean thalia_alu_condition_satisfied(ThaliaGB* gb, thalia_cond_t cond)
{
    switch(cond) {
    case THALIA_COND_Z:
        return gb->reg.named.flag_zero ? TRUE : FALSE;
    case THALIA_COND_NZ:
        return gb->reg.named.flag_zero ? FALSE : TRUE;
    case THALIA_COND_C:
        return gb->reg.named.flag_carry ? TRUE : FALSE;
    case THALIA_COND_NC:
        return gb->reg.named.flag_carry ? FALSE : TRUE;
    }

    g_error("Unknown condition code: %d\r\n", cond);
}

// Performs an 8bit addition on the ALU.
guint8 thalia_alu_add(ThaliaGB* gb, guint8 a, guint8 b, gboolean update_carry)
{
    guint8 res = a + b;
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = (a & 0x0F) + (b & 0x0F) > 0x0F;
    if(update_carry)
        gb->reg.named.flag_carry = a > G_MAXUINT8 - b;
    return res;
}

// Performs an 8bit addition with carry on the ALU.
guint8 thalia_alu_add_carry(ThaliaGB* gb, guint8 a, guint8 b)
{
    // The ternary operator is necessary because the bitfield is read as though
    // it were a signed byte. This causes TRUE to be read as -1.
    guint8 carry = (gb->reg.named.flag_carry ? 1 : 0);
    guint8 res = a + b + carry;
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;

    // We cannot pass control to thalia_alu_add because the flags are
    // calculated differently with regard to overflow
    gb->reg.named.flag_halfcarry = (a & 0x0F) + (b & 0x0F) + carry > 0x0F;
    gb->reg.named.flag_carry = a > G_MAXUINT8 - b - carry;
    return res;
}

// Performs a 16bit addition of a 16bit number and an 8bit signed number.
guint16 thalia_alu_add_16bit_mixed(ThaliaGB* gb, guint16 a, gint8 b)
{
    guint16 other = (guint16) (((gint16) (b << 8)) >> 8);
    gb->reg.named.flag_zero = FALSE;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = (a & 0x000F) + (other & 0x000F) > 0x000F;
    gb->reg.named.flag_carry = (a & 0x00FF) + (other & 0x00FF) > 0x00FF;
    return a + other;
}

// Performs a 16bit addition on the ALU.
guint16 thalia_alu_add_16bit(ThaliaGB* gb, guint16 a, guint16 b)
{
    gb->reg.named.flag_operation = FALSE;
    // Note how the halfcarry is from bit 11, not bit 7 as you might expect.
    gb->reg.named.flag_halfcarry = (a & 0x07FF) + (b & 0x07FF) > 0x07FF;
    gb->reg.named.flag_carry = a > G_MAXUINT16 - b;
    return a + b;
}

// Performs an 8bit subtraction on the ALU.
guint8 thalia_alu_sub(ThaliaGB* gb, guint8 a, guint8 b, gboolean update_carry)
{
    guint8 res = a - b;
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = TRUE;
    gb->reg.named.flag_halfcarry = (a & 0x0F) < (b & 0x0F);
    if(update_carry)
        gb->reg.named.flag_carry = a < b;
    return res;
}

// Performs an 8bit subtraction with carry on the ALU.
guint8 thalia_alu_sub_carry(ThaliaGB* gb, guint8 a, guint8 b)
{
    // See thalia_alu_add_carry for comment on the ternary operator.
    guint8 carry = (gb->reg.named.flag_carry ? 1 : 0);
    guint8 res = a - (b + carry);
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = TRUE;
    gb->reg.named.flag_halfcarry = (a & 0x0F) < (b & 0x0F) + carry;
    gb->reg.named.flag_carry = a < b + carry;
    return res;
}

// Performs an 8bit bitwise AND on the ALU.
guint8 thalia_alu_and(ThaliaGB* gb, guint8 a, guint8 b)
{
    guint8 res = a & b;
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = TRUE;
    gb->reg.named.flag_carry = FALSE;
    return res;
}

// Performs an 8bit bitwise XOR on the ALU.
guint8 thalia_alu_xor(ThaliaGB* gb, guint8 a, guint8 b)
{
    guint8 res = a ^ b;
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = FALSE;
    gb->reg.named.flag_carry = FALSE;
    return res;
}

// Performs an 8bit bitwise OR on the ALU.
guint8 thalia_alu_or(ThaliaGB* gb, guint8 a, guint8 b)
{
    guint8 res = a | b;
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = FALSE;
    gb->reg.named.flag_carry = FALSE;
    return res;
}

// Adjusts the operand for binary-coded decimal representation on the ALU.
guint8 thalia_alu_daa(ThaliaGB* gb, guint8 a)
{
    guint16 adjust = gb->reg.named.flag_carry ? 0x60 : 0x00;
    if(gb->reg.named.flag_halfcarry)
        adjust |= 0x06;
    if(!gb->reg.named.flag_operation) {
        if((a & 0x0F) > 0x09)
            adjust |= 0x06;
        if(a > 0x99)
            adjust |= 0x60;
        a += adjust;
    } else
        a -= adjust;

    gb->reg.named.flag_carry = adjust >= 0x60 ? TRUE : FALSE;
  gb->reg.named.flag_halfcarry = FALSE;
  gb->reg.named.flag_zero = a == 0 ? TRUE : FALSE;
  return a;
}

// Performs an 8bit complement on the ALU.
guint8 thalia_alu_cpl(ThaliaGB* gb, guint8 a)
{
    gb->reg.named.flag_operation = TRUE;
    gb->reg.named.flag_halfcarry = TRUE;
    return ~a;
}

// Sets the carry flag on the ALU.
void thalia_alu_scf(ThaliaGB* gb)
{
    gb->reg.named.flag_carry = TRUE;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = FALSE;
}

// Complements the carry flag on the ALU.
void thalia_alu_ccf(ThaliaGB* gb)
{
    gb->reg.named.flag_carry = !gb->reg.named.flag_carry;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = FALSE;
}

// Updates the flags register after a shift operation, with 'res' being the
// result of said operation and 'leaving' representing the leaving bit.
guint8 thalia_alu_update_flags_shift(ThaliaGB* gb, guint8 res,
                                     gboolean leaving)
{
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = FALSE;
    // Ternary operator necessary due to bitfield behavior.
    gb->reg.named.flag_carry = leaving ? TRUE : FALSE;
    return res;
}

// Performs a rotate right with carry on the ALU.
guint8 thalia_alu_rrc(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x01;
    guint8 res = (a >> 1) | (leaving << 7);
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Performs a rotate left with carry on the ALU.
guint8 thalia_alu_rlc(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x80;
    guint8 res = (a << 1) | (leaving >> 7);
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Performs a rotate right on the ALU.
guint8 thalia_alu_rr(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x01;
    guint8 res = (a >> 1) | (gb->reg.named.flag_carry ? 0x80 : 0x00);
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Performs a rotate left on the ALU.
guint8 thalia_alu_rl(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x80;
    guint8 res = (a << 1) | (gb->reg.named.flag_carry ? 0x01 : 0x00);
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Performs an arithmetic shift right on the ALU (preserving sign bit).
guint8 thalia_alu_sra(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x01;
#if (-2 >> 1) == -1
    // Right shift on signed numbers implemented as an arithmetic shift right.
    // We can utilize this by casting to a signed type and shifting. This is
    // the case for GCC and clang on x86-ish architectures.
    guint8 res = ((gint8) a) >> 1;
#else
#error Your build target does not seem to perform an arithmetic right shift   \
       on signed numbers. Comment out this error directive if you want to try \
       out an *untested* implementation of the arithmetic right shift.
    // Right shift on signed numbers implemented as a logical shift right.
    // We can work around this by manually copying the most significant bit.
    guint8 res = (a >> 1) | (a & 0x80);
#endif
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Performs a logical shift left on the ALU (shifting in zeroes).
guint8 thalia_alu_sla(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x80;
    guint8 res = a << 1;
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Performs a logical shift right on the ALU (shifting in zeroes).
guint8 thalia_alu_srl(ThaliaGB* gb, guint8 a)
{
    guint8 leaving = a & 0x01;
    guint8 res = a >> 1;
    return thalia_alu_update_flags_shift(gb, res, leaving);
}

// Swaps the high and low nibbles (4 bits) of the target on the ALU.
guint8 thalia_alu_swap(ThaliaGB* gb, guint8 a)
{
    guint8 res = (a >> 4) | (a << 4);
    gb->reg.named.flag_zero = res == 0;
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = FALSE;
    gb->reg.named.flag_carry = FALSE;
    return res;
}

// Tests whether 'bit' is set in the target on the ALU.
void thalia_alu_bit(ThaliaGB* gb, guint8 a, guint8 bit)
{
    gb->reg.named.flag_zero = !(a & (1 << bit));
    gb->reg.named.flag_operation = FALSE;
    gb->reg.named.flag_halfcarry = TRUE;
}

// Resets 'bit' in the target on the ALU.
guint8 thalia_alu_res(ThaliaGB* gb, guint8 a, guint8 bit)
{
    return a & ~(1 << bit);
}

// Sets 'bit' in the target on the ALU.
guint8 thalia_alu_set(ThaliaGB* gb, guint8 a, guint8 bit)
{
    return a | (1 << bit);
}
