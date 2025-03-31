#include "pico/config.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sio.h"

// Offsets suitable for ldr/str (must be <= 0x7c):
#define ACCUM0_OFFS     (SIO_INTERP0_ACCUM0_OFFSET    - SIO_INTERP0_ACCUM0_OFFSET)
#define ACCUM1_OFFS     (SIO_INTERP0_ACCUM1_OFFSET    - SIO_INTERP0_ACCUM0_OFFSET)
#define POP0_OFFS       (SIO_INTERP0_POP_LANE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)

.syntax unified
.cpu cortex-m0plus
.thumb

.macro decl_func_x name
.section .scratch_x.\name, "ax"
.global \name
.type \name,%function
.thumb_func
\name:
.endm

.macro decl_func_y name
.section .scratch_y.\name, "ax"
.global \name
.type \name,%function
.thumb_func
\name:
.endm

#define decl_func decl_func_x

.macro decode_pixel_data rd
	ldr \rd, [r2, #POP0_OFFS]
	lsls \rd, #8
	ldr r7, [r2, #POP0_OFFS]
	orrs \rd, r7
	ldr r7, [r2, #POP0_OFFS]
	lsls r7, #24
	orrs \rd, r7
	ldr r7, [r2, #POP0_OFFS]
	lsls r7, #16
	orrs \rd, r7
.endm

decl_func apple2e_render_scanline
	push {r3-r7, lr}
	
	lsls r2, #1
	add r2, r1
	mov ip, r2
	ldr r2, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)

.align 2
1:
	ldmia r0!, {r7}
	str r7, [r2, #ACCUM0_OFFS]
	str r7, [r2, #ACCUM1_OFFS]
	decode_pixel_data r3
	decode_pixel_data r4
    stmia r1!, {r3, r4}
	cmp r1, ip
    bne 1b

	pop {r3-r7, pc}

//
decl_func copy_tmdsbuf
	push {r2-r7, lr}

#ifdef OLIMEX_NEO6502
.rept 200
#else
.rept 200
#endif
	ldmia r1!, {r2, r3, r4, r5, r6, r7}
	stmia r0!, {r2, r3, r4, r5, r6, r7}
.endr

	pop {r2-r7, pc}
