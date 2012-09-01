#ifndef __THALIA_PROC_H__
#define __THALIA_PROC_H__

// Shifting direction, corresponds with opcode encoding.
typedef enum {
	THALIA_DIR_LEFT  = 0,
	THALIA_DIR_RIGHT = 1
} thalia_dir_t;

// Post-operation (inc/dec), corresponds with opcode encoding.
typedef enum {
	THALIA_OPERATION_INC = 0,
	THALIA_OPERATION_DEC = 1
} thalia_operation_t;
#endif

#ifdef __THALIA_GB_T__
gboolean thalia_proc_decode(ThaliaGB* gb, guint8 opcode);
#endif
