#include <glib.h>
#include "thalia_gb.h"
#include "thalia_alu.h"
#include "thalia_proc.h"
#include "thalia_mmu.h"
#include "thalia_reg.h"

guint16 prev_pc;

// Handles the end of opcode execution, increasing the cycle count by 'cycles'
// and the program counter by 1 + 'pc_increase'. Note that for jumps, a value of
// -1 is advised for this parameter.
static inline void thalia_proc_end_opcode(ThaliaGB* gb, guint8 cycles,
 guint16 pc_increase)
{
	gb->cycles += cycles;
	gb->pc += pc_increase + 1;
}

// Processes the "NOP" instruction (No operation).
static inline void thalia_proc_nop(ThaliaGB* gb)
{
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "LD (n), SP" instruction (Load SP into immediate address n).
static inline void thalia_proc_ld_n_sp(ThaliaGB* gb, guint16 immediate)
{
	thalia_mmu_write_word(gb, immediate, gb->sp);
	thalia_proc_end_opcode(gb, 5, 2);
}

// Processes the "LD r, n" instruction (Load immediate word n into register r).
static inline void thalia_proc_ld_r_n(ThaliaGB* gb, thalia_regname_double_t reg,
 guint16 immediate)
{
	thalia_reg_write_double(gb, reg, immediate, FALSE);
	thalia_proc_end_opcode(gb, 3, 2);
}

// Processes the "ADD HL, r" instruction (Add register r to HL).
static inline void thalia_proc_add_hl_r(ThaliaGB* gb, thalia_regname_double_t reg)
{
	thalia_reg_write_double(
		gb,
		THALIA_REG_HL,
		thalia_alu_add_16bit(
			gb,
			thalia_reg_read_double(gb, THALIA_REG_HL, TRUE),
			thalia_reg_read_double(gb, reg, FALSE)
		),
		TRUE
	);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "LD (r), A" opcode (Load A into address in register r).
static inline void thalia_proc_ld_r_a(ThaliaGB* gb, thalia_regname_double_t reg)
{
	guint16 offset = thalia_reg_read_double(gb, reg, FALSE);
	thalia_mmu_write_byte(gb, offset, gb->reg.named.a);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "LD A, (r)" opcode (Load A from address in register r).
static inline void thalia_proc_ld_a_r(ThaliaGB* gb, thalia_regname_double_t reg)
{
	guint16 offset = thalia_reg_read_double(gb, reg, FALSE);
	gb->reg.named.a = thalia_mmu_read_byte(gb, offset);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "INC r" opcode (Increment register r).
static inline void thalia_proc_inc_r(ThaliaGB* gb, thalia_regname_double_t reg)
{
	// circumvent ALU, no registers change
	guint16 contents = thalia_reg_read_double(gb, reg, FALSE);
	thalia_reg_write_double(gb, reg, contents+1, FALSE);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "DEC r" opcode (Decrement register r).
static inline void thalia_proc_dec_r(ThaliaGB* gb, thalia_regname_double_t reg)
{
	// circumvent ALU, no registers change
	guint16 contents = thalia_reg_read_double(gb, reg, FALSE);
	thalia_reg_write_double(gb, reg, contents-1, FALSE);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "INC d" opcode (Increment register d).
static inline void thalia_proc_inc_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 result = thalia_alu_add(gb, thalia_reg_read_single(gb, reg), 1, FALSE);
	thalia_reg_write_single(gb, reg, result);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "DEC d" opcode (Decrement register d).
static inline void thalia_proc_dec_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 result = thalia_alu_sub(gb, thalia_reg_read_single(gb, reg), 1, FALSE);
	thalia_reg_write_single(gb, reg, result);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "LD d, n" opcode (Load immedate byte into register d).
static inline void thalia_proc_ld_d_n(ThaliaGB* gb, thalia_regname_single_t reg,
 guint8 immediate)
{
	thalia_reg_write_single(gb, reg, immediate);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "RdCA" opcode (Rotate left/right with carry register A).
static inline void thalia_proc_rdca(ThaliaGB* gb, thalia_dir_t direction)
{
	switch(direction) {
	case THALIA_DIR_RIGHT:
		gb->reg.named.a = thalia_alu_rrc(gb, gb->reg.named.a);
		break;
	case THALIA_DIR_LEFT:
		gb->reg.named.a = thalia_alu_rlc(gb, gb->reg.named.a);
		break;
	}
	// Contrary to RdC A, this operation resets the zero flag.
	gb->reg.named.flag_zero = FALSE;
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "RdA" opcode (Rotate left/right register A).
static inline void thalia_proc_rda(ThaliaGB* gb, thalia_dir_t direction)
{
	switch(direction) {
	case THALIA_DIR_RIGHT:
		gb->reg.named.a = thalia_alu_rr(gb, gb->reg.named.a);
		break;
	case THALIA_DIR_LEFT:
		gb->reg.named.a = thalia_alu_rl(gb, gb->reg.named.a);
		break;
	}
	// Contrary to Rd A, this operation resets the zero flag.
	gb->reg.named.flag_zero = FALSE;
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "STOP" opcode (Stop and wait for keypad input).
static inline void thalia_proc_stop(ThaliaGB* gb)
{
	// TODO: Proper implementation
	gb->stopped = TRUE;
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "JR n" opcode (Relative jump).
static inline void thalia_proc_jr_n(ThaliaGB* gb, guint8 immediate)
{
	gb->pc += (gint8) immediate;
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "JR f, n" opcode (Conditional relative jump).
static inline void thalia_proc_jr_f_n(ThaliaGB* gb, thalia_cond_t cond,
 guint8 immediate)
{
	if(thalia_alu_condition_satisfied(gb, cond))
		gb->pc += (gint8) immediate;
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "LDo (HL), A" opcode.
// (Load A into address pointed at by HL, then in- or decrease HL).
static inline void thalia_proc_ldo_hl_a(ThaliaGB* gb, thalia_operation_t op)
{
	guint16 hl = thalia_reg_read_double(gb, THALIA_REG_HL, TRUE);
	thalia_mmu_write_byte(gb, hl, gb->reg.named.a);
	switch(op) {
	case THALIA_OPERATION_INC: hl++; break;
	case THALIA_OPERATION_DEC: hl--; break;
	}
	thalia_reg_write_double(gb, THALIA_REG_HL, hl, TRUE);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "LDo A, (HL)" opcode.
// (Load A from address pointed at by HL, then in- or decrease HL).
static inline void thalia_proc_ldo_a_hl(ThaliaGB* gb, thalia_operation_t op)
{
	guint16 hl = thalia_reg_read_double(gb, THALIA_REG_HL, TRUE);
	gb->reg.named.a = thalia_mmu_read_byte(gb, hl);
	switch(op) {
	case THALIA_OPERATION_INC: hl++; break;
	case THALIA_OPERATION_DEC: hl--; break;
	}
	thalia_reg_write_double(gb, THALIA_REG_HL, hl, TRUE);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "DAA" opcode (Perform BCD adjustment on register A).
static inline void thalia_proc_daa(ThaliaGB* gb)
{
	gb->reg.named.a = thalia_alu_daa(gb, gb->reg.named.a);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "CPL" opcode (Perform bitwise complement on A).
static inline void thalia_proc_cpl(ThaliaGB* gb)
{
	gb->reg.named.a = thalia_alu_cpl(gb, gb->reg.named.a);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "SCF" opcode (Set carry flag).
static inline void thalia_proc_scf(ThaliaGB* gb)
{
	thalia_alu_scf(gb);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "CCF" opcode (Complement carry flag).
static inline void thalia_proc_ccf(ThaliaGB* gb)
{
	thalia_alu_ccf(gb);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "HALT" opcode (Stop execution until interrupt occurs).
static inline void thalia_proc_halt(ThaliaGB* gb)
{
	gb->halted = TRUE;
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "LD d, d" opcode (Load one register from another).
static inline void thalia_proc_ld_d_d(ThaliaGB* gb, thalia_regname_single_t treg,
 thalia_regname_single_t sreg)
{
	thalia_reg_write_single(gb, treg, thalia_reg_read_single(gb, sreg));
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "ADD A, d" opcode (Add register d to register A).
static inline void thalia_proc_add_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_add(gb, gb->reg.named.a, other, TRUE);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "ADC A, d" opcode (Add register d to register A with carry).
static inline void thalia_proc_adc_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_add_carry(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "SUB A, d" opcode (Subtract register d from register A).
static inline void thalia_proc_sub_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_sub(gb, gb->reg.named.a, other, TRUE);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "SBC A, d" opcode
// (Subtract register d and carry from register A).
static inline void thalia_proc_sbc_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_sub_carry(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "AND A, d" opcode (Bitwise AND of registers A and d).
static inline void thalia_proc_and_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_and(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "XOR A, d" opcode (Bitwise XOR of registers A and d).
static inline void thalia_proc_xor_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_xor(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "OR A, d" opcode (Bitwise OR of registers A and d).
static inline void thalia_proc_or_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	gb->reg.named.a = thalia_alu_or(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "CP A, d" opcode (Compare register A against register d).
static inline void thalia_proc_cp_a_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 other = thalia_reg_read_single(gb, reg);
	thalia_alu_sub(gb, gb->reg.named.a, other, TRUE);
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "ADD A, n" opcode (Add immediate to register A).
static inline void thalia_proc_add_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_add(gb, gb->reg.named.a, other, TRUE);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "ADC A, N" opcode (Add immediate and carry to register A).
static inline void thalia_proc_adc_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_add_carry(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "SUB A, n" opcode (Subtract immediate from register A).
static inline void thalia_proc_sub_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_sub(gb, gb->reg.named.a, other, TRUE);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "SBC A, n" opcode
// (Subtract immediate with carry from register A).
static inline void thalia_proc_sbc_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_sub_carry(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "AND A, n" opcode (Bitwise AND of register A and immediate).
static inline void thalia_proc_and_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_and(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "XOR A, n" opcode (Bitwise XOR of register A and immediate).
static inline void thalia_proc_xor_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_xor(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "OR A, n" opcode (Bitwise OR of register A and immediate).
static inline void thalia_proc_or_a_n(ThaliaGB* gb, guint8 other)
{
	gb->reg.named.a = thalia_alu_or(gb, gb->reg.named.a, other);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "CP A, n" opcode (Compare register A against immediate).
static inline void thalia_proc_cp_a_n(ThaliaGB* gb, guint8 other)
{
	thalia_alu_sub(gb, gb->reg.named.a, other, TRUE);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "POP r" opcode (Pop word from stack into register r)
static inline void thalia_proc_pop_r(ThaliaGB* gb, thalia_regname_double_t reg)
{
	thalia_reg_write_double(gb, reg, thalia_mmu_pop_word(gb), TRUE);
	thalia_proc_end_opcode(gb, 3, 0);
}

// Processes the "PUSH r" opcode (Push register r into stack).
static inline void thalia_proc_push_r(ThaliaGB* gb, thalia_regname_double_t reg)
{
	thalia_mmu_push_word(gb, thalia_reg_read_double(gb, reg, TRUE));
	thalia_proc_end_opcode(gb, 4, 0);
}

// Processes the "RST n" opcode (Call code at n << 3).
static inline void thalia_proc_rst_n(ThaliaGB* gb, guint8 offset)
{
	thalia_mmu_push_word(gb, gb->pc + 1);
	gb->pc = offset << 3;
	thalia_proc_end_opcode(gb, 8, -1);
}

// Processes the "RET f" opcode (Conditional return).
static inline void thalia_proc_ret_f(ThaliaGB* gb, thalia_cond_t condition)
{
	if(thalia_alu_condition_satisfied(gb, condition)) {
		gb->pc = thalia_mmu_pop_word(gb);
		thalia_proc_end_opcode(gb, 2, -1);
	} else
		thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "RET(I)" opcode
// (Unconditional return, possibly enabling interrupts).
static inline void thalia_proc_ret(ThaliaGB* gb, gboolean interrupts)
{
	gb->pc = thalia_mmu_pop_word(gb);
	if(interrupts)
		gb->interrupts = TRUE;
	thalia_proc_end_opcode(gb, 3, -1);
}

// Processes the "JP f, n" opcode (Conditional absolute jump).
static inline void thalia_proc_jp_f_n(ThaliaGB* gb, thalia_cond_t condition,
 guint16 immediate)
{
	if(thalia_alu_condition_satisfied(gb, condition)) {
		gb->pc = immediate;
		thalia_proc_end_opcode(gb, 3, -1);
	} else
		thalia_proc_end_opcode(gb, 3, 2);
}

// Processes the "JP n" opcode (Absolute jump).
static inline void thalia_proc_jp_n(ThaliaGB* gb, guint16 immediate)
{
	gb->pc = immediate;
	thalia_proc_end_opcode(gb, 3, -1);
}

// Processes the "CALL f, n" opcode (Conditional call).
static inline void thalia_proc_call_f_n(ThaliaGB* gb, thalia_cond_t condition,
 guint16 immediate)
{
	if(thalia_alu_condition_satisfied(gb, condition)) {
		thalia_mmu_push_word(gb, gb->pc + 3);
		gb->pc = immediate;
		thalia_proc_end_opcode(gb, 3, -1);
	} else
		thalia_proc_end_opcode(gb, 3, 2);
}

// Processes the "CALL n" opcode (Unconditional call).
static inline void thalia_proc_call_n(ThaliaGB* gb, guint16 immediate)
{
	thalia_mmu_push_word(gb, gb->pc + 3);
	gb->pc = immediate;
	thalia_proc_end_opcode(gb, 3, -1);
}

// Processes the "ADD SP, n" opcode (Add immediate to SP).
static inline void thalia_proc_add_sp_n(ThaliaGB* gb, guint8 immediate)
{
	gb->sp = thalia_alu_add_16bit_mixed(gb, gb->sp, immediate);
	thalia_proc_end_opcode(gb, 4, 1);
}

// Processes the "LD HL, SP+n" opcode (Store SP+n in HL).
static inline void thalia_proc_ld_hl_sp_n(ThaliaGB* gb, guint8 immediate)
{
	guint16 res = thalia_alu_add_16bit_mixed(gb, gb->sp, immediate);
	thalia_reg_write_double(gb, THALIA_REG_HL, res, FALSE);
	thalia_proc_end_opcode(gb, 3, 1);
}

// Processes the "LD (0xFF00+n), A" opcode
// (Load register A from address 0xFF00+immediate).
static inline void thalia_proc_ld_ff00_n_a(ThaliaGB* gb, guint8 immediate)
{
	thalia_mmu_write_byte(gb, 0xFF00 + immediate, gb->reg.named.a);
	thalia_proc_end_opcode(gb, 3, 1);
}

// Processes the "LD A, (0xFF00+n)" opcode.
// (Store register A in address 0xFF00+immediate).
static inline void thalia_proc_ld_a_ff00_n(ThaliaGB* gb, guint8 immediate)
{
	gb->reg.named.a = thalia_mmu_read_byte(gb, 0xFF00 + immediate);
	thalia_proc_end_opcode(gb, 3, 1);
}

// Processes the "LD A, (0xFF00+C)" opcode.
// (Load register A from address 0xFF00+register C)
static inline void thalia_proc_ld_a_c(ThaliaGB* gb)
{
	gb->reg.named.a = thalia_mmu_read_byte(gb, 0xFF00+gb->reg.named.c);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "LD (0xFF00+C), A" opcode.
// (Store register A at address 0xFF00+register C)
static inline void thalia_proc_ld_c_a(ThaliaGB* gb)
{
	thalia_mmu_write_byte(gb, 0xFF00+gb->reg.named.c, gb->reg.named.a);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "LD (n), A" opcode (Store register A at immediate address).
static inline void thalia_proc_ld_n_a(ThaliaGB* gb, guint16 immediate)
{
	thalia_mmu_write_byte(gb, immediate, gb->reg.named.a);
	thalia_proc_end_opcode(gb, 4, 2);
}

// Processes the "LD A, (n)" opcode (Load A from immediate address).
static inline void thalia_proc_ld_a_n(ThaliaGB* gb, guint16 immediate)
{
	gb->reg.named.a = thalia_mmu_read_byte(gb, immediate);
	thalia_proc_end_opcode(gb, 4, 2);
}

// Processes the "JP HL" opcode (Jump to address in register HL).
static inline void thalia_proc_jp_hl(ThaliaGB* gb)
{
	gb->pc = thalia_reg_read_double(gb, THALIA_REG_HL, TRUE);
	thalia_proc_end_opcode(gb, 1, -1);
}

// Processes the "LD SP, HL" opcode (Set SP to HL).
static inline void thalia_proc_ld_sp_hl(ThaliaGB* gb)
{
	gb->sp = thalia_reg_read_double(gb, THALIA_REG_HL, TRUE);
	thalia_proc_end_opcode(gb, 2, 0);
}

// Processes the "DI" opcode (Disable interrupts after next instruction).
static inline void thalia_proc_di(ThaliaGB* gb)
{
	gb->disable_interrupts_in = 2;
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "EI" opcode (Enable interrupts after next instruction).
static inline void thalia_proc_ei(ThaliaGB* gb)
{
	gb->enable_interrupts_in = 2;
	thalia_proc_end_opcode(gb, 1, 0);
}

// Processes the "RdC d" opcode (Rotate left/right with carry register d).
static inline void thalia_proc_rdc_d(ThaliaGB* gb, thalia_dir_t dir,
 thalia_regname_single_t reg)
{
	guint8 res;
	switch(dir) {
	case THALIA_DIR_RIGHT:
	  res = thalia_alu_rrc(gb, thalia_reg_read_single(gb, reg));
		break;
	case THALIA_DIR_LEFT:
		res = thalia_alu_rlc(gb, thalia_reg_read_single(gb, reg));
		break;
	default:
		g_error("Unknown shift direction");
		return;
	}

	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "Rd d" opcode (Rotate left/right register d).
static inline void thalia_proc_rd_d(ThaliaGB* gb, thalia_dir_t dir,
 thalia_regname_single_t reg)
{
	guint8 res;
	switch(dir) {
	case THALIA_DIR_RIGHT:
		res = thalia_alu_rr(gb, thalia_reg_read_single(gb, reg));
		break;
	case THALIA_DIR_LEFT:
		res = thalia_alu_rl(gb, thalia_reg_read_single(gb, reg));
		break;
	default:
		g_error("Unknown shift direction");
		return;
	}

	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "SdA d" opcode (Shift register d left/right arithmetically).
static inline void thalia_proc_sda_d(ThaliaGB* gb, thalia_dir_t dir,
 thalia_regname_single_t reg)
{
	guint8 res;
	switch(dir) {
	case THALIA_DIR_RIGHT:
		res = thalia_alu_sra(gb, thalia_reg_read_single(gb, reg));
		break;
	case THALIA_DIR_LEFT:
		res = thalia_alu_sla(gb, thalia_reg_read_single(gb, reg));
		break;
	default:
		g_error("Unknown shift direction");
		return;
	}

	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "SWAP d" opcode (Swap nibbles in register d).
static inline void thalia_proc_swap_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 res = thalia_alu_swap(gb, thalia_reg_read_single(gb, reg));
	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "SRL" d opcode (Shift register d right logically).
static inline void thalia_proc_srl_d(ThaliaGB* gb, thalia_regname_single_t reg)
{
	guint8 res = thalia_alu_srl(gb, thalia_reg_read_single(gb, reg));
	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "BIT n, d" opcode (Test for bit n to be set in register d).
static inline void thalia_proc_bit_n_d(ThaliaGB* gb, guint8 bit,
 thalia_regname_single_t reg)
{
	thalia_alu_bit(gb, thalia_reg_read_single(gb, reg), bit);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "RES n, d" opcode (Reset bit n in register d).
static inline void thalia_proc_res_n_d(ThaliaGB* gb, guint8 bit,
 thalia_regname_single_t reg)
{
	guint8 res = thalia_alu_res(gb, thalia_reg_read_single(gb,reg), bit);
	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Processes the "SET n, d" opcode (Set bit n in register d).
static inline void thalia_proc_set_n_d(ThaliaGB* gb, guint8 bit,
 thalia_regname_single_t reg)
{
	guint8 res = thalia_alu_set(gb, thalia_reg_read_single(gb,reg), bit);
	thalia_reg_write_single(gb, reg, res);
	thalia_proc_end_opcode(gb, 2, 1);
}

// Decodes an extended opcode.
static inline void thalia_proc_decode_extended(ThaliaGB* gb, guint8 opcode)
{
	switch(opcode) {
	// High 0
	case 0x00: thalia_proc_rdc_d(gb, 0, 0); return;
	case 0x01: thalia_proc_rdc_d(gb, 0, 1); return;
	case 0x02: thalia_proc_rdc_d(gb, 0, 2); return;
	case 0x03: thalia_proc_rdc_d(gb, 0, 3); return;
	case 0x04: thalia_proc_rdc_d(gb, 0, 4); return;
	case 0x05: thalia_proc_rdc_d(gb, 0, 5); return;
	case 0x06: thalia_proc_rdc_d(gb, 0, 6); return;
	case 0x07: thalia_proc_rdc_d(gb, 0, 7); return;
	case 0x08: thalia_proc_rdc_d(gb, 1, 0); return;
	case 0x09: thalia_proc_rdc_d(gb, 1, 1); return;
	case 0x0A: thalia_proc_rdc_d(gb, 1, 2); return;
	case 0x0B: thalia_proc_rdc_d(gb, 1, 3); return;
	case 0x0C: thalia_proc_rdc_d(gb, 1, 4); return;
	case 0x0D: thalia_proc_rdc_d(gb, 1, 5); return;
	case 0x0E: thalia_proc_rdc_d(gb, 1, 6); return;
	case 0x0F: thalia_proc_rdc_d(gb, 1, 7); return;

	// High 1
	case 0x10: thalia_proc_rd_d(gb, 0, 0); return;
	case 0x11: thalia_proc_rd_d(gb, 0, 1); return;
	case 0x12: thalia_proc_rd_d(gb, 0, 2); return;
	case 0x13: thalia_proc_rd_d(gb, 0, 3); return;
	case 0x14: thalia_proc_rd_d(gb, 0, 4); return;
	case 0x15: thalia_proc_rd_d(gb, 0, 5); return;
	case 0x16: thalia_proc_rd_d(gb, 0, 6); return;
	case 0x17: thalia_proc_rd_d(gb, 0, 7); return;
	case 0x18: thalia_proc_rd_d(gb, 1, 0); return;
	case 0x19: thalia_proc_rd_d(gb, 1, 1); return;
	case 0x1A: thalia_proc_rd_d(gb, 1, 2); return;
	case 0x1B: thalia_proc_rd_d(gb, 1, 3); return;
	case 0x1C: thalia_proc_rd_d(gb, 1, 4); return;
	case 0x1D: thalia_proc_rd_d(gb, 1, 5); return;
	case 0x1E: thalia_proc_rd_d(gb, 1, 6); return;
	case 0x1F: thalia_proc_rd_d(gb, 1, 7); return;

	// High 2
	case 0x20: thalia_proc_sda_d(gb, 0, 0); return;
	case 0x21: thalia_proc_sda_d(gb, 0, 1); return;
	case 0x22: thalia_proc_sda_d(gb, 0, 2); return;
	case 0x23: thalia_proc_sda_d(gb, 0, 3); return;
	case 0x24: thalia_proc_sda_d(gb, 0, 4); return;
	case 0x25: thalia_proc_sda_d(gb, 0, 5); return;
	case 0x26: thalia_proc_sda_d(gb, 0, 6); return;
	case 0x27: thalia_proc_sda_d(gb, 0, 7); return;
	case 0x28: thalia_proc_sda_d(gb, 1, 0); return;
	case 0x29: thalia_proc_sda_d(gb, 1, 1); return;
	case 0x2A: thalia_proc_sda_d(gb, 1, 2); return;
	case 0x2B: thalia_proc_sda_d(gb, 1, 3); return;
	case 0x2C: thalia_proc_sda_d(gb, 1, 4); return;
	case 0x2D: thalia_proc_sda_d(gb, 1, 5); return;
	case 0x2E: thalia_proc_sda_d(gb, 1, 6); return;
	case 0x2F: thalia_proc_sda_d(gb, 1, 7); return;

	// High 3
	case 0x30: thalia_proc_swap_d(gb, 0); return;
	case 0x31: thalia_proc_swap_d(gb, 1); return;
	case 0x32: thalia_proc_swap_d(gb, 2); return;
	case 0x33: thalia_proc_swap_d(gb, 3); return;
	case 0x34: thalia_proc_swap_d(gb, 4); return;
	case 0x35: thalia_proc_swap_d(gb, 5); return;
	case 0x36: thalia_proc_swap_d(gb, 6); return;
	case 0x37: thalia_proc_swap_d(gb, 7); return;
	case 0x38: thalia_proc_srl_d(gb, 0); return;
	case 0x39: thalia_proc_srl_d(gb, 1); return;
	case 0x3A: thalia_proc_srl_d(gb, 2); return;
	case 0x3B: thalia_proc_srl_d(gb, 3); return;
	case 0x3C: thalia_proc_srl_d(gb, 4); return;
	case 0x3D: thalia_proc_srl_d(gb, 5); return;
	case 0x3E: thalia_proc_srl_d(gb, 6); return;
	case 0x3F: thalia_proc_srl_d(gb, 7); return;

	// High 4
	case 0x40: thalia_proc_bit_n_d(gb, 0, 0); return;
	case 0x41: thalia_proc_bit_n_d(gb, 0, 1); return;
	case 0x42: thalia_proc_bit_n_d(gb, 0, 2); return;
	case 0x43: thalia_proc_bit_n_d(gb, 0, 3); return;
	case 0x44: thalia_proc_bit_n_d(gb, 0, 4); return;
	case 0x45: thalia_proc_bit_n_d(gb, 0, 5); return;
	case 0x46: thalia_proc_bit_n_d(gb, 0, 6); return;
	case 0x47: thalia_proc_bit_n_d(gb, 0, 7); return;
	case 0x48: thalia_proc_bit_n_d(gb, 1, 0); return;
	case 0x49: thalia_proc_bit_n_d(gb, 1, 1); return;
	case 0x4A: thalia_proc_bit_n_d(gb, 1, 2); return;
	case 0x4B: thalia_proc_bit_n_d(gb, 1, 3); return;
	case 0x4C: thalia_proc_bit_n_d(gb, 1, 4); return;
	case 0x4D: thalia_proc_bit_n_d(gb, 1, 5); return;
	case 0x4E: thalia_proc_bit_n_d(gb, 1, 6); return;
	case 0x4F: thalia_proc_bit_n_d(gb, 1, 7); return;

	// High 5
	case 0x50: thalia_proc_bit_n_d(gb, 2, 0); return;
	case 0x51: thalia_proc_bit_n_d(gb, 2, 1); return;
	case 0x52: thalia_proc_bit_n_d(gb, 2, 2); return;
	case 0x53: thalia_proc_bit_n_d(gb, 2, 3); return;
	case 0x54: thalia_proc_bit_n_d(gb, 2, 4); return;
	case 0x55: thalia_proc_bit_n_d(gb, 2, 5); return;
	case 0x56: thalia_proc_bit_n_d(gb, 2, 6); return;
	case 0x57: thalia_proc_bit_n_d(gb, 2, 7); return;
	case 0x58: thalia_proc_bit_n_d(gb, 3, 0); return;
	case 0x59: thalia_proc_bit_n_d(gb, 3, 1); return;
	case 0x5A: thalia_proc_bit_n_d(gb, 3, 2); return;
	case 0x5B: thalia_proc_bit_n_d(gb, 3, 3); return;
	case 0x5C: thalia_proc_bit_n_d(gb, 3, 4); return;
	case 0x5D: thalia_proc_bit_n_d(gb, 3, 5); return;
	case 0x5E: thalia_proc_bit_n_d(gb, 3, 6); return;
	case 0x5F: thalia_proc_bit_n_d(gb, 3, 7); return;

	// High 6
	case 0x60: thalia_proc_bit_n_d(gb, 4, 0); return;
	case 0x61: thalia_proc_bit_n_d(gb, 4, 1); return;
	case 0x62: thalia_proc_bit_n_d(gb, 4, 2); return;
	case 0x63: thalia_proc_bit_n_d(gb, 4, 3); return;
	case 0x64: thalia_proc_bit_n_d(gb, 4, 4); return;
	case 0x65: thalia_proc_bit_n_d(gb, 4, 5); return;
	case 0x66: thalia_proc_bit_n_d(gb, 4, 6); return;
	case 0x67: thalia_proc_bit_n_d(gb, 4, 7); return;
	case 0x68: thalia_proc_bit_n_d(gb, 5, 0); return;
	case 0x69: thalia_proc_bit_n_d(gb, 5, 1); return;
	case 0x6A: thalia_proc_bit_n_d(gb, 5, 2); return;
	case 0x6B: thalia_proc_bit_n_d(gb, 5, 3); return;
	case 0x6C: thalia_proc_bit_n_d(gb, 5, 4); return;
	case 0x6D: thalia_proc_bit_n_d(gb, 5, 5); return;
	case 0x6E: thalia_proc_bit_n_d(gb, 5, 6); return;
	case 0x6F: thalia_proc_bit_n_d(gb, 5, 7); return;

	// High 7
	case 0x70: thalia_proc_bit_n_d(gb, 6, 0); return;
	case 0x71: thalia_proc_bit_n_d(gb, 6, 1); return;
	case 0x72: thalia_proc_bit_n_d(gb, 6, 2); return;
	case 0x73: thalia_proc_bit_n_d(gb, 6, 3); return;
	case 0x74: thalia_proc_bit_n_d(gb, 6, 4); return;
	case 0x75: thalia_proc_bit_n_d(gb, 6, 5); return;
	case 0x76: thalia_proc_bit_n_d(gb, 6, 6); return;
	case 0x77: thalia_proc_bit_n_d(gb, 6, 7); return;
	case 0x78: thalia_proc_bit_n_d(gb, 7, 0); return;
	case 0x79: thalia_proc_bit_n_d(gb, 7, 1); return;
	case 0x7A: thalia_proc_bit_n_d(gb, 7, 2); return;
	case 0x7B: thalia_proc_bit_n_d(gb, 7, 3); return;
	case 0x7C: thalia_proc_bit_n_d(gb, 7, 4); return;
	case 0x7D: thalia_proc_bit_n_d(gb, 7, 5); return;
	case 0x7E: thalia_proc_bit_n_d(gb, 7, 6); return;
	case 0x7F: thalia_proc_bit_n_d(gb, 7, 7); return;

	// High 8
	case 0x80: thalia_proc_res_n_d(gb, 0, 0); return;
	case 0x81: thalia_proc_res_n_d(gb, 0, 1); return;
	case 0x82: thalia_proc_res_n_d(gb, 0, 2); return;
	case 0x83: thalia_proc_res_n_d(gb, 0, 3); return;
	case 0x84: thalia_proc_res_n_d(gb, 0, 4); return;
	case 0x85: thalia_proc_res_n_d(gb, 0, 5); return;
	case 0x86: thalia_proc_res_n_d(gb, 0, 6); return;
	case 0x87: thalia_proc_res_n_d(gb, 0, 7); return;
	case 0x88: thalia_proc_res_n_d(gb, 1, 0); return;
	case 0x89: thalia_proc_res_n_d(gb, 1, 1); return;
	case 0x8A: thalia_proc_res_n_d(gb, 1, 2); return;
	case 0x8B: thalia_proc_res_n_d(gb, 1, 3); return;
	case 0x8C: thalia_proc_res_n_d(gb, 1, 4); return;
	case 0x8D: thalia_proc_res_n_d(gb, 1, 5); return;
	case 0x8E: thalia_proc_res_n_d(gb, 1, 6); return;
	case 0x8F: thalia_proc_res_n_d(gb, 1, 7); return;

	// High 9
	case 0x90: thalia_proc_res_n_d(gb, 2, 0); return;
	case 0x91: thalia_proc_res_n_d(gb, 2, 1); return;
	case 0x92: thalia_proc_res_n_d(gb, 2, 2); return;
	case 0x93: thalia_proc_res_n_d(gb, 2, 3); return;
	case 0x94: thalia_proc_res_n_d(gb, 2, 4); return;
	case 0x95: thalia_proc_res_n_d(gb, 2, 5); return;
	case 0x96: thalia_proc_res_n_d(gb, 2, 6); return;
	case 0x97: thalia_proc_res_n_d(gb, 2, 7); return;
	case 0x98: thalia_proc_res_n_d(gb, 3, 0); return;
	case 0x99: thalia_proc_res_n_d(gb, 3, 1); return;
	case 0x9A: thalia_proc_res_n_d(gb, 3, 2); return;
	case 0x9B: thalia_proc_res_n_d(gb, 3, 3); return;
	case 0x9C: thalia_proc_res_n_d(gb, 3, 4); return;
	case 0x9D: thalia_proc_res_n_d(gb, 3, 5); return;
	case 0x9E: thalia_proc_res_n_d(gb, 3, 6); return;
	case 0x9F: thalia_proc_res_n_d(gb, 3, 7); return;

	// High A
	case 0xA0: thalia_proc_res_n_d(gb, 4, 0); return;
	case 0xA1: thalia_proc_res_n_d(gb, 4, 1); return;
	case 0xA2: thalia_proc_res_n_d(gb, 4, 2); return;
	case 0xA3: thalia_proc_res_n_d(gb, 4, 3); return;
	case 0xA4: thalia_proc_res_n_d(gb, 4, 4); return;
	case 0xA5: thalia_proc_res_n_d(gb, 4, 5); return;
	case 0xA6: thalia_proc_res_n_d(gb, 4, 6); return;
	case 0xA7: thalia_proc_res_n_d(gb, 4, 7); return;
	case 0xA8: thalia_proc_res_n_d(gb, 5, 0); return;
	case 0xA9: thalia_proc_res_n_d(gb, 5, 1); return;
	case 0xAA: thalia_proc_res_n_d(gb, 5, 2); return;
	case 0xAB: thalia_proc_res_n_d(gb, 5, 3); return;
	case 0xAC: thalia_proc_res_n_d(gb, 5, 4); return;
	case 0xAD: thalia_proc_res_n_d(gb, 5, 5); return;
	case 0xAE: thalia_proc_res_n_d(gb, 5, 6); return;
	case 0xAF: thalia_proc_res_n_d(gb, 5, 7); return;

	// High B
	case 0xB0: thalia_proc_res_n_d(gb, 6, 0); return;
	case 0xB1: thalia_proc_res_n_d(gb, 6, 1); return;
	case 0xB2: thalia_proc_res_n_d(gb, 6, 2); return;
	case 0xB3: thalia_proc_res_n_d(gb, 6, 3); return;
	case 0xB4: thalia_proc_res_n_d(gb, 6, 4); return;
	case 0xB5: thalia_proc_res_n_d(gb, 6, 5); return;
	case 0xB6: thalia_proc_res_n_d(gb, 6, 6); return;
	case 0xB7: thalia_proc_res_n_d(gb, 6, 7); return;
	case 0xB8: thalia_proc_res_n_d(gb, 7, 0); return;
	case 0xB9: thalia_proc_res_n_d(gb, 7, 1); return;
	case 0xBA: thalia_proc_res_n_d(gb, 7, 2); return;
	case 0xBB: thalia_proc_res_n_d(gb, 7, 3); return;
	case 0xBC: thalia_proc_res_n_d(gb, 7, 4); return;
	case 0xBD: thalia_proc_res_n_d(gb, 7, 5); return;
	case 0xBE: thalia_proc_res_n_d(gb, 7, 6); return;
	case 0xBF: thalia_proc_res_n_d(gb, 7, 7); return;

	// High C
	case 0xC0: thalia_proc_set_n_d(gb, 0, 0); return;
	case 0xC1: thalia_proc_set_n_d(gb, 0, 1); return;
	case 0xC2: thalia_proc_set_n_d(gb, 0, 2); return;
	case 0xC3: thalia_proc_set_n_d(gb, 0, 3); return;
	case 0xC4: thalia_proc_set_n_d(gb, 0, 4); return;
	case 0xC5: thalia_proc_set_n_d(gb, 0, 5); return;
	case 0xC6: thalia_proc_set_n_d(gb, 0, 6); return;
	case 0xC7: thalia_proc_set_n_d(gb, 0, 7); return;
	case 0xC8: thalia_proc_set_n_d(gb, 1, 0); return;
	case 0xC9: thalia_proc_set_n_d(gb, 1, 1); return;
	case 0xCA: thalia_proc_set_n_d(gb, 1, 2); return;
	case 0xCB: thalia_proc_set_n_d(gb, 1, 3); return;
	case 0xCC: thalia_proc_set_n_d(gb, 1, 4); return;
	case 0xCD: thalia_proc_set_n_d(gb, 1, 5); return;
	case 0xCE: thalia_proc_set_n_d(gb, 1, 6); return;
	case 0xCF: thalia_proc_set_n_d(gb, 1, 7); return;

	// High D
	case 0xD0: thalia_proc_set_n_d(gb, 2, 0); return;
	case 0xD1: thalia_proc_set_n_d(gb, 2, 1); return;
	case 0xD2: thalia_proc_set_n_d(gb, 2, 2); return;
	case 0xD3: thalia_proc_set_n_d(gb, 2, 3); return;
	case 0xD4: thalia_proc_set_n_d(gb, 2, 4); return;
	case 0xD5: thalia_proc_set_n_d(gb, 2, 5); return;
	case 0xD6: thalia_proc_set_n_d(gb, 2, 6); return;
	case 0xD7: thalia_proc_set_n_d(gb, 2, 7); return;
	case 0xD8: thalia_proc_set_n_d(gb, 3, 0); return;
	case 0xD9: thalia_proc_set_n_d(gb, 3, 1); return;
	case 0xDA: thalia_proc_set_n_d(gb, 3, 2); return;
	case 0xDB: thalia_proc_set_n_d(gb, 3, 3); return;
	case 0xDC: thalia_proc_set_n_d(gb, 3, 4); return;
	case 0xDD: thalia_proc_set_n_d(gb, 3, 5); return;
	case 0xDE: thalia_proc_set_n_d(gb, 3, 6); return;
	case 0xDF: thalia_proc_set_n_d(gb, 3, 7); return;

	// High E
	case 0xE0: thalia_proc_set_n_d(gb, 4, 0); return;
	case 0xE1: thalia_proc_set_n_d(gb, 4, 1); return;
	case 0xE2: thalia_proc_set_n_d(gb, 4, 2); return;
	case 0xE3: thalia_proc_set_n_d(gb, 4, 3); return;
	case 0xE4: thalia_proc_set_n_d(gb, 4, 4); return;
	case 0xE5: thalia_proc_set_n_d(gb, 4, 5); return;
	case 0xE6: thalia_proc_set_n_d(gb, 4, 6); return;
	case 0xE7: thalia_proc_set_n_d(gb, 4, 7); return;
	case 0xE8: thalia_proc_set_n_d(gb, 5, 0); return;
	case 0xE9: thalia_proc_set_n_d(gb, 5, 1); return;
	case 0xEA: thalia_proc_set_n_d(gb, 5, 2); return;
	case 0xEB: thalia_proc_set_n_d(gb, 5, 3); return;
	case 0xEC: thalia_proc_set_n_d(gb, 5, 4); return;
	case 0xED: thalia_proc_set_n_d(gb, 5, 5); return;
	case 0xEE: thalia_proc_set_n_d(gb, 5, 6); return;
	case 0xEF: thalia_proc_set_n_d(gb, 5, 7); return;

	// High F
	case 0xF0: thalia_proc_set_n_d(gb, 6, 0); return;
	case 0xF1: thalia_proc_set_n_d(gb, 6, 1); return;
	case 0xF2: thalia_proc_set_n_d(gb, 6, 2); return;
	case 0xF3: thalia_proc_set_n_d(gb, 6, 3); return;
	case 0xF4: thalia_proc_set_n_d(gb, 6, 4); return;
	case 0xF5: thalia_proc_set_n_d(gb, 6, 5); return;
	case 0xF6: thalia_proc_set_n_d(gb, 6, 6); return;
	case 0xF7: thalia_proc_set_n_d(gb, 6, 7); return;
	case 0xF8: thalia_proc_set_n_d(gb, 7, 0); return;
	case 0xF9: thalia_proc_set_n_d(gb, 7, 1); return;
	case 0xFA: thalia_proc_set_n_d(gb, 7, 2); return;
	case 0xFB: thalia_proc_set_n_d(gb, 7, 3); return;
	case 0xFC: thalia_proc_set_n_d(gb, 7, 4); return;
	case 0xFD: thalia_proc_set_n_d(gb, 7, 5); return;
	case 0xFE: thalia_proc_set_n_d(gb, 7, 6); return;
	case 0xFF: thalia_proc_set_n_d(gb, 7, 7); return;

	default:
		g_error("Unhandled extended opcode 0x%02X @ 0x%04X\r\n", opcode, gb->pc);
	}
}

// Decodes a normal opcode.
gboolean thalia_proc_decode(ThaliaGB* gb, guint8 opcode)
{
	// Caution: big ugly switch up ahead.
	switch(opcode) {
	// High 0
	case 0x00: thalia_proc_nop(gb); return TRUE;
	case 0x01:
		thalia_proc_ld_r_n(gb, 0, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0x02: thalia_proc_ld_r_a(gb, 0); return TRUE;
	case 0x03: thalia_proc_inc_r(gb, 0); return TRUE;
	case 0x04: thalia_proc_inc_d(gb, 0); return TRUE;
	case 0x05: thalia_proc_dec_d(gb, 0); return TRUE;
	case 0x06:
		thalia_proc_ld_d_n(gb, 0, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x07: thalia_proc_rdca(gb, 0); return TRUE;
	case 0x08:
		thalia_proc_ld_n_sp(gb, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0x09: thalia_proc_add_hl_r(gb, 0); return TRUE;
	case 0x0A: thalia_proc_ld_a_r(gb, 0); return TRUE;
	case 0x0B: thalia_proc_dec_r(gb, 0); return TRUE;
	case 0x0C: thalia_proc_inc_d(gb, 1); return TRUE;
	case 0x0D: thalia_proc_dec_d(gb, 1); return TRUE;
	case 0x0E:
		thalia_proc_ld_d_n(gb, 1, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x0F: thalia_proc_rdca(gb, 1); return TRUE;

	// High 1
	case 0x10: thalia_proc_stop(gb); return TRUE;
	case 0x11:
		thalia_proc_ld_r_n(gb, 1, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0x12: thalia_proc_ld_r_a(gb, 1); return TRUE;
	case 0x13: thalia_proc_inc_r(gb, 1); return TRUE;
	case 0x14: thalia_proc_inc_d(gb, 2); return TRUE;
	case 0x15: thalia_proc_dec_d(gb, 2); return TRUE;
	case 0x16:
		thalia_proc_ld_d_n(gb, 2, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x17: thalia_proc_rda(gb, 0); return TRUE;
	case 0x18: thalia_proc_jr_n(gb, thalia_mmu_immediate_byte(gb)); return TRUE;
	case 0x19: thalia_proc_add_hl_r(gb, 1); return TRUE;
	case 0x1A: thalia_proc_ld_a_r(gb, 1); return TRUE;
	case 0x1B: thalia_proc_dec_r(gb, 1); return TRUE;
	case 0x1C: thalia_proc_inc_d(gb, 3); return TRUE;
	case 0x1D: thalia_proc_dec_d(gb, 3); return TRUE;
	case 0x1E:
		thalia_proc_ld_d_n(gb, 3, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x1F: thalia_proc_rda(gb, 1); return TRUE;

	// High 2
	case 0x20:
		thalia_proc_jr_f_n(gb, 0, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x21:
		thalia_proc_ld_r_n(gb, 2, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0x22: thalia_proc_ldo_hl_a(gb, 0); return TRUE;
	case 0x23: thalia_proc_inc_r(gb, 2); return TRUE;
	case 0x24: thalia_proc_inc_d(gb, 4); return TRUE;
	case 0x25: thalia_proc_dec_d(gb, 4); return TRUE;
	case 0x26:
		thalia_proc_ld_d_n(gb, 4, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x27: thalia_proc_daa(gb); return TRUE;
	case 0x28:
		thalia_proc_jr_f_n(gb, 1, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x29: thalia_proc_add_hl_r(gb, 2); return TRUE;
	case 0x2A: thalia_proc_ldo_a_hl(gb, 0); return TRUE;
	case 0x2B: thalia_proc_dec_r(gb, 2); return TRUE;
	case 0x2C: thalia_proc_inc_d(gb, 5); return TRUE;
	case 0x2D: thalia_proc_dec_d(gb, 5); return TRUE;
	case 0x2E:
		thalia_proc_ld_d_n(gb, 5, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x2F: thalia_proc_cpl(gb); return TRUE;

	// High 3
	case 0x30:
		thalia_proc_jr_f_n(gb, 2, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x31:
		thalia_proc_ld_r_n(gb, 3, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0x32: thalia_proc_ldo_hl_a(gb, 1); return TRUE;
	case 0x33: thalia_proc_inc_r(gb, 3); return TRUE;
	case 0x34: thalia_proc_inc_d(gb, 6); return TRUE;
	case 0x35: thalia_proc_dec_d(gb, 6); return TRUE;
	case 0x36:
		thalia_proc_ld_d_n(gb, 6, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x37: thalia_proc_scf(gb); return TRUE;
	case 0x38:
		thalia_proc_jr_f_n(gb, 3, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x39: thalia_proc_add_hl_r(gb, 3); return TRUE;
	case 0x3A: thalia_proc_ldo_a_hl(gb, 1); return TRUE;
	case 0x3B: thalia_proc_dec_r(gb, 3); return TRUE;
	case 0x3C: thalia_proc_inc_d(gb, 7); return TRUE;
	case 0x3D: thalia_proc_dec_d(gb, 7); return TRUE;
	case 0x3E:
		thalia_proc_ld_d_n(gb, 7, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0x3F: thalia_proc_ccf(gb); return TRUE;

	// High 4
	case 0x40: thalia_proc_ld_d_d(gb, 0, 0); return TRUE;
	case 0x41: thalia_proc_ld_d_d(gb, 0, 1); return TRUE;
	case 0x42: thalia_proc_ld_d_d(gb, 0, 2); return TRUE;
	case 0x43: thalia_proc_ld_d_d(gb, 0, 3); return TRUE;
	case 0x44: thalia_proc_ld_d_d(gb, 0, 4); return TRUE;
	case 0x45: thalia_proc_ld_d_d(gb, 0, 5); return TRUE;
	case 0x46: thalia_proc_ld_d_d(gb, 0, 6); return TRUE;
	case 0x47: thalia_proc_ld_d_d(gb, 0, 7); return TRUE;
	case 0x48: thalia_proc_ld_d_d(gb, 1, 0); return TRUE;
	case 0x49: thalia_proc_ld_d_d(gb, 1, 1); return TRUE;
	case 0x4A: thalia_proc_ld_d_d(gb, 1, 2); return TRUE;
	case 0x4B: thalia_proc_ld_d_d(gb, 1, 3); return TRUE;
	case 0x4C: thalia_proc_ld_d_d(gb, 1, 4); return TRUE;
	case 0x4D: thalia_proc_ld_d_d(gb, 1, 5); return TRUE;
	case 0x4E: thalia_proc_ld_d_d(gb, 1, 6); return TRUE;
	case 0x4F: thalia_proc_ld_d_d(gb, 1, 7); return TRUE;

	// High 5
	case 0x50: thalia_proc_ld_d_d(gb, 2, 0); return TRUE;
	case 0x51: thalia_proc_ld_d_d(gb, 2, 1); return TRUE;
	case 0x52: thalia_proc_ld_d_d(gb, 2, 2); return TRUE;
	case 0x53: thalia_proc_ld_d_d(gb, 2, 3); return TRUE;
	case 0x54: thalia_proc_ld_d_d(gb, 2, 4); return TRUE;
	case 0x55: thalia_proc_ld_d_d(gb, 2, 5); return TRUE;
	case 0x56: thalia_proc_ld_d_d(gb, 2, 6); return TRUE;
	case 0x57: thalia_proc_ld_d_d(gb, 2, 7); return TRUE;
	case 0x58: thalia_proc_ld_d_d(gb, 3, 0); return TRUE;
	case 0x59: thalia_proc_ld_d_d(gb, 3, 1); return TRUE;
	case 0x5A: thalia_proc_ld_d_d(gb, 3, 2); return TRUE;
	case 0x5B: thalia_proc_ld_d_d(gb, 3, 3); return TRUE;
	case 0x5C: thalia_proc_ld_d_d(gb, 3, 4); return TRUE;
	case 0x5D: thalia_proc_ld_d_d(gb, 3, 5); return TRUE;
	case 0x5E: thalia_proc_ld_d_d(gb, 3, 6); return TRUE;
	case 0x5F: thalia_proc_ld_d_d(gb, 3, 7); return TRUE;

	// High 6
	case 0x60: thalia_proc_ld_d_d(gb, 4, 0); return TRUE;
	case 0x61: thalia_proc_ld_d_d(gb, 4, 1); return TRUE;
	case 0x62: thalia_proc_ld_d_d(gb, 4, 2); return TRUE;
	case 0x63: thalia_proc_ld_d_d(gb, 4, 3); return TRUE;
	case 0x64: thalia_proc_ld_d_d(gb, 4, 4); return TRUE;
	case 0x65: thalia_proc_ld_d_d(gb, 4, 5); return TRUE;
	case 0x66: thalia_proc_ld_d_d(gb, 4, 6); return TRUE;
	case 0x67: thalia_proc_ld_d_d(gb, 4, 7); return TRUE;
	case 0x68: thalia_proc_ld_d_d(gb, 5, 0); return TRUE;
	case 0x69: thalia_proc_ld_d_d(gb, 5, 1); return TRUE;
	case 0x6A: thalia_proc_ld_d_d(gb, 5, 2); return TRUE;
	case 0x6B: thalia_proc_ld_d_d(gb, 5, 3); return TRUE;
	case 0x6C: thalia_proc_ld_d_d(gb, 5, 4); return TRUE;
	case 0x6D: thalia_proc_ld_d_d(gb, 5, 5); return TRUE;
	case 0x6E: thalia_proc_ld_d_d(gb, 5, 6); return TRUE;
	case 0x6F: thalia_proc_ld_d_d(gb, 5, 7); return TRUE;

	// High 7
	case 0x70: thalia_proc_ld_d_d(gb, 6, 0); return TRUE;
	case 0x71: thalia_proc_ld_d_d(gb, 6, 1); return TRUE;
	case 0x72: thalia_proc_ld_d_d(gb, 6, 2); return TRUE;
	case 0x73: thalia_proc_ld_d_d(gb, 6, 3); return TRUE;
	case 0x74: thalia_proc_ld_d_d(gb, 6, 4); return TRUE;
	case 0x75: thalia_proc_ld_d_d(gb, 6, 5); return TRUE;
	case 0x76: thalia_proc_halt(gb); return TRUE;
	case 0x77: thalia_proc_ld_d_d(gb, 6, 7); return TRUE;
	case 0x78: thalia_proc_ld_d_d(gb, 7, 0); return TRUE;
	case 0x79: thalia_proc_ld_d_d(gb, 7, 1); return TRUE;
	case 0x7A: thalia_proc_ld_d_d(gb, 7, 2); return TRUE;
	case 0x7B: thalia_proc_ld_d_d(gb, 7, 3); return TRUE;
	case 0x7C: thalia_proc_ld_d_d(gb, 7, 4); return TRUE;
	case 0x7D: thalia_proc_ld_d_d(gb, 7, 5); return TRUE;
	case 0x7E: thalia_proc_ld_d_d(gb, 7, 6); return TRUE;
	case 0x7F: thalia_proc_ld_d_d(gb, 7, 7); return TRUE;

	// High 8
	case 0x80: thalia_proc_add_a_d(gb, 0); return TRUE;
	case 0x81: thalia_proc_add_a_d(gb, 1); return TRUE;
	case 0x82: thalia_proc_add_a_d(gb, 2); return TRUE;
	case 0x83: thalia_proc_add_a_d(gb, 3); return TRUE;
	case 0x84: thalia_proc_add_a_d(gb, 4); return TRUE;
	case 0x85: thalia_proc_add_a_d(gb, 5); return TRUE;
	case 0x86: thalia_proc_add_a_d(gb, 6); return TRUE;
	case 0x87: thalia_proc_add_a_d(gb, 7); return TRUE;
	case 0x88: thalia_proc_adc_a_d(gb, 0); return TRUE;
	case 0x89: thalia_proc_adc_a_d(gb, 1); return TRUE;
	case 0x8A: thalia_proc_adc_a_d(gb, 2); return TRUE;
	case 0x8B: thalia_proc_adc_a_d(gb, 3); return TRUE;
	case 0x8C: thalia_proc_adc_a_d(gb, 4); return TRUE;
	case 0x8D: thalia_proc_adc_a_d(gb, 5); return TRUE;
	case 0x8E: thalia_proc_adc_a_d(gb, 6); return TRUE;
	case 0x8F: thalia_proc_adc_a_d(gb, 7); return TRUE;

	// High 9
	case 0x90: thalia_proc_sub_a_d(gb, 0); return TRUE;
	case 0x91: thalia_proc_sub_a_d(gb, 1); return TRUE;
	case 0x92: thalia_proc_sub_a_d(gb, 2); return TRUE;
	case 0x93: thalia_proc_sub_a_d(gb, 3); return TRUE;
	case 0x94: thalia_proc_sub_a_d(gb, 4); return TRUE;
	case 0x95: thalia_proc_sub_a_d(gb, 5); return TRUE;
	case 0x96: thalia_proc_sub_a_d(gb, 6); return TRUE;
	case 0x97: thalia_proc_sub_a_d(gb, 7); return TRUE;
	case 0x98: thalia_proc_sbc_a_d(gb, 0); return TRUE;
	case 0x99: thalia_proc_sbc_a_d(gb, 1); return TRUE;
	case 0x9A: thalia_proc_sbc_a_d(gb, 2); return TRUE;
	case 0x9B: thalia_proc_sbc_a_d(gb, 3); return TRUE;
	case 0x9C: thalia_proc_sbc_a_d(gb, 4); return TRUE;
	case 0x9D: thalia_proc_sbc_a_d(gb, 5); return TRUE;
	case 0x9E: thalia_proc_sbc_a_d(gb, 6); return TRUE;
	case 0x9F: thalia_proc_sbc_a_d(gb, 7); return TRUE;

	// High A
	case 0xA0: thalia_proc_and_a_d(gb, 0); return TRUE;
	case 0xA1: thalia_proc_and_a_d(gb, 1); return TRUE;
	case 0xA2: thalia_proc_and_a_d(gb, 2); return TRUE;
	case 0xA3: thalia_proc_and_a_d(gb, 3); return TRUE;
	case 0xA4: thalia_proc_and_a_d(gb, 4); return TRUE;
	case 0xA5: thalia_proc_and_a_d(gb, 5); return TRUE;
	case 0xA6: thalia_proc_and_a_d(gb, 6); return TRUE;
	case 0xA7: thalia_proc_and_a_d(gb, 7); return TRUE;
	case 0xA8: thalia_proc_xor_a_d(gb, 0); return TRUE;
	case 0xA9: thalia_proc_xor_a_d(gb, 1); return TRUE;
	case 0xAA: thalia_proc_xor_a_d(gb, 2); return TRUE;
	case 0xAB: thalia_proc_xor_a_d(gb, 3); return TRUE;
	case 0xAC: thalia_proc_xor_a_d(gb, 4); return TRUE;
	case 0xAD: thalia_proc_xor_a_d(gb, 5); return TRUE;
	case 0xAE: thalia_proc_xor_a_d(gb, 6); return TRUE;
	case 0xAF: thalia_proc_xor_a_d(gb, 7); return TRUE;

	// High B
	case 0xB0: thalia_proc_or_a_d(gb, 0); return TRUE;
	case 0xB1: thalia_proc_or_a_d(gb, 1); return TRUE;
	case 0xB2: thalia_proc_or_a_d(gb, 2); return TRUE;
	case 0xB3: thalia_proc_or_a_d(gb, 3); return TRUE;
	case 0xB4: thalia_proc_or_a_d(gb, 4); return TRUE;
	case 0xB5: thalia_proc_or_a_d(gb, 5); return TRUE;
	case 0xB6: thalia_proc_or_a_d(gb, 6); return TRUE;
	case 0xB7: thalia_proc_or_a_d(gb, 7); return TRUE;
	case 0xB8: thalia_proc_cp_a_d(gb, 0); return TRUE;
	case 0xB9: thalia_proc_cp_a_d(gb, 1); return TRUE;
	case 0xBA: thalia_proc_cp_a_d(gb, 2); return TRUE;
	case 0xBB: thalia_proc_cp_a_d(gb, 3); return TRUE;
	case 0xBC: thalia_proc_cp_a_d(gb, 4); return TRUE;
	case 0xBD: thalia_proc_cp_a_d(gb, 5); return TRUE;
	case 0xBE: thalia_proc_cp_a_d(gb, 6); return TRUE;
	case 0xBF: thalia_proc_cp_a_d(gb, 7); return TRUE;

	// High C
	case 0xC0: thalia_proc_ret_f(gb, 0); return TRUE;
	case 0xC1: thalia_proc_pop_r(gb, 0); return TRUE;
	case 0xC2:
		thalia_proc_jp_f_n(gb, 0, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xC3: thalia_proc_jp_n(gb, thalia_mmu_immediate_word(gb)); return TRUE;
	case 0xC4:
		thalia_proc_call_f_n(gb, 0, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xC5: thalia_proc_push_r(gb, 0); return TRUE;
	case 0xC6:
		thalia_proc_add_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xC7: thalia_proc_rst_n(gb, 0); return TRUE;
	case 0xC8: thalia_proc_ret_f(gb, 1); return TRUE;
	case 0xC9: thalia_proc_ret(gb, 0); return TRUE;
	case 0xCA:
		thalia_proc_jp_f_n(gb, 1, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xCC:
		thalia_proc_call_f_n(gb, 1, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xCB: // Extended opcode
		thalia_proc_decode_extended(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xCD:
		thalia_proc_call_n(gb, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xCE:
		thalia_proc_adc_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xCF: thalia_proc_rst_n(gb, 1); return TRUE;

	// High D
	case 0xD0: thalia_proc_ret_f(gb, 2); return TRUE;
	case 0xD1: thalia_proc_pop_r(gb, 1); return TRUE;
	case 0xD2:
		thalia_proc_jp_f_n(gb, 2, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xD4:
		thalia_proc_call_f_n(gb, 2, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xD5: thalia_proc_push_r(gb, 1); return TRUE;
	case 0xD6:
		thalia_proc_sub_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xD7: thalia_proc_rst_n(gb, 2); return TRUE;
	case 0xD8: thalia_proc_ret_f(gb, 3); return TRUE;
	case 0xD9: thalia_proc_ret(gb, 1); return TRUE;
	case 0xDA:
		thalia_proc_jp_f_n(gb, 3, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xDC:
		thalia_proc_call_f_n(gb, 3, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xDE:
		thalia_proc_sbc_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xDF: thalia_proc_rst_n(gb, 3); return TRUE;

	// High E
	case 0xE0:
		thalia_proc_ld_ff00_n_a(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xE1: thalia_proc_pop_r(gb, 2); return TRUE;
	case 0xE2: thalia_proc_ld_c_a(gb); return TRUE;
	case 0xE5: thalia_proc_push_r(gb, 2); return TRUE;
	case 0xE6:
		thalia_proc_and_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xE7: thalia_proc_rst_n(gb, 4); return TRUE;
	case 0xE8:
		thalia_proc_add_sp_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xE9: thalia_proc_jp_hl(gb); return TRUE;
	case 0xEA:
		thalia_proc_ld_n_a(gb, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xEE:
		thalia_proc_xor_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xEF: thalia_proc_rst_n(gb, 5); return TRUE;

	// High F
	case 0xF0:
		thalia_proc_ld_a_ff00_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xF1: thalia_proc_pop_r(gb, 3); return TRUE;
	case 0xF2: thalia_proc_ld_a_c(gb); return TRUE;
	case 0xF3: thalia_proc_di(gb); return TRUE;
	case 0xF5: thalia_proc_push_r(gb, 3); return TRUE;
	case 0xF6:
		thalia_proc_or_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xF7: thalia_proc_rst_n(gb, 6); return TRUE;
	case 0xF8:
		thalia_proc_ld_hl_sp_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xF9: thalia_proc_ld_sp_hl(gb); return TRUE;
	case 0xFA:
		thalia_proc_ld_a_n(gb, thalia_mmu_immediate_word(gb));
		return TRUE;
	case 0xFB: thalia_proc_ei(gb); return TRUE;
	case 0xFE:
		thalia_proc_cp_a_n(gb, thalia_mmu_immediate_byte(gb));
		return TRUE;
	case 0xFF: thalia_proc_rst_n(gb, 7); return TRUE;

	default:
		g_error("Unhandled opcode 0x%02X @ 0x%04X\r\n", opcode, gb->pc);
		return FALSE;
	}
}
