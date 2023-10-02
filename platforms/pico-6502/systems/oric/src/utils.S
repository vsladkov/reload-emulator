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

// 04 03 02 01  08 07 06 05  12 11 10 09
// 01 02 03 04  05 06 07 08  09 10 11 12

.macro decode_pixel_data_3x rd1, rd2, rd3
	ldr r7, [r2, #POP0_OFFS]	
	mov \rd2, r7
	lsls r7, #8
	orrs \rd2, r7
	lsls r7, #16
	mov \rd1, r7
	ldr r7, [r2, #POP0_OFFS]
	orrs \rd1, r7
	lsls r7, #8
	orrs \rd1, r7
	lsls r7, #8
	orrs \rd1, r7
	ldr r7, [r2, #POP0_OFFS]	
	lsls r7, #8
	mov \rd3, r7
	lsls r7, #8
	orrs \rd3, r7
	lsls r7, #8
	orrs \rd3, r7
	ldr r7, [r2, #POP0_OFFS]	
	orrs \rd3, r7
	lsls r7, #16
	orrs \rd2, r7
	lsls r7, #8
	orrs \rd2, r7
.endm

decl_func oric_render_scanline_3x
	push {r3-r7, lr}
	
	lsls r2, #1
	mov r3, r2
	lsls r2, #1
	add r2, r3
	add r2, r1
	mov ip, r2
	ldr r2, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)

.align 2
1:
	ldmia r0!, {r7}
	str r7, [r2, #ACCUM0_OFFS]
	str r7, [r2, #ACCUM1_OFFS]
	decode_pixel_data_3x r3, r4, r5
    stmia r1!, {r3, r4, r5}
	decode_pixel_data_3x r3, r4, r5
    stmia r1!, {r3, r4, r5}
	cmp r1, ip
    bne 1b

	pop {r3-r7, pc}


.macro decode_pixel_data_2x rd
	ldr r7, [r2, #POP0_OFFS]
	lsls r7, #16
	mov \rd, r7
	lsls r7, #8
	orrs \rd, r7
	ldr r7, [r2, #POP0_OFFS]
	orrs \rd, r7
	lsls r7, #8
	orrs \rd, r7
.endm

decl_func oric_render_scanline_2x
	push {r3-r7, lr}
	
	lsls r2, #2
	add r2, r1
	mov ip, r2
	ldr r2, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)

.align 2
1:
	ldmia r0!, {r7}
	str r7, [r2, #ACCUM0_OFFS]
	str r7, [r2, #ACCUM1_OFFS]
	decode_pixel_data_2x r3
	decode_pixel_data_2x r4
	decode_pixel_data_2x r5
	decode_pixel_data_2x r6
    stmia r1!, {r3, r4, r5, r6}
	cmp r1, ip
    bne 1b

	pop {r3-r7, pc}
