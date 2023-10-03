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

decl_func_y copy_audiobuf
	push {r2-r7, lr}

.rept 42
	ldmia r1!, {r2, r3, r4, r5, r6, r7}
	stmia r0!, {r2, r3, r4, r5, r6, r7}
.endr
	ldmia r1!, {r2, r3, r4, r5}
	stmia r0!, {r2, r3, r4, r5}

	pop {r2-r7, pc}
