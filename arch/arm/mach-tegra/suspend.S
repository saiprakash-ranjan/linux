#include <linux/linkage.h>
#include <asm/memory.h>
#include <asm/cache.h>
#include <asm/vfpmacros.h>
#include <asm/pgtable.h>
#include "power.h"
#include <asm/assembler.h>

#define PHYS_OFFSET 0x80000000

#define TTB_S           (1 << 1)
#define TTB_RGN_NC      (0 << 3)
#define TTB_RGN_OC_WBWA (1 << 3)
#define TTB_RGN_OC_WT   (2 << 3)
#define TTB_RGN_OC_WB   (3 << 3)
#define TTB_NOS         (1 << 5)
#define TTB_IRGN_NC     ((0 << 0) | (0 << 6))
#define TTB_IRGN_WBWA   ((0 << 0) | (1 << 6))
#define TTB_IRGN_WT     ((1 << 0) | (0 << 6))
#define TTB_IRGN_WB     ((1 << 0) | (1 << 6))

/* PTWs cacheable, inner WB not shareable, outer WB not shareable */
#define TTB_FLAGS_UP    TTB_IRGN_WB|TTB_RGN_OC_WB
#define PMD_FLAGS_UP    PMD_SECT_WB

/* PTWs cacheable, inner WBWA shareable, outer WBWA not shareable */
#define TTB_FLAGS_SMP   TTB_IRGN_WBWA|TTB_S|TTB_NOS|TTB_RGN_OC_WBWA
#define PMD_FLAGS_SMP   PMD_SECT_WBWA|PMD_SECT_S

.equ    PRRR,   0xff0a81a8
.equ    NMRR,   0x40e040e0

/*
 * spooled CPU context is 1KB / CPU
 */
#define CTX_SP		0
#define CTX_CPSR	4
#define CTX_SPSR	8
#define CTX_CPACR	12
#define CTX_CSSELR	16
#define CTX_SCTLR	20
#define CTX_ACTLR	24
#define CTX_PCTLR	28

#define CTX_FPEXC	32
#define CTX_FPSCR	36

#define CTX_TTBR0	48
#define CTX_TTBR1	52
#define CTX_TTBCR	56
#define CTX_DACR	60
#define CTX_PAR		64
#define CTX_PRRR	68
#define CTX_NMRR	72
#define CTX_VBAR	76
#define CTX_CONTEXTIDR	80
#define CTX_TPIDRURW	84
#define CTX_TPIDRURO	88
#define CTX_TPIDRPRW	92

#define CTX_SVC_SP	0
#define CTX_SVC_LR	-1	@ stored on stack
#define CTX_SVC_SPSR	8

#define CTX_SYS_SP	96
#define CTX_SYS_LR	100

#define CTX_ABT_SPSR	112
#define CTX_ABT_SP	116
#define CTX_ABT_LR	120

#define CTX_UND_SPSR	128
#define CTX_UND_SP	132
#define CTX_UND_LR	136

#define CTX_IRQ_SPSR	144
#define CTX_IRQ_SP	148
#define CTX_IRQ_LR	152

#define CTX_FIQ_SPSR	160
#define CTX_FIQ_R8	164
#define CTX_FIQ_R9	168
#define CTX_FIQ_R10	172
#define CTX_FIQ_R11	178
#define CTX_FIQ_R12	180
#define CTX_FIQ_SP	184
#define CTX_FIQ_LR	188

#define CTX_VFP_REGS	256
#define CTX_VFP_SIZE	(32 * 8)

#define CTX_DIAG_CTR	512

.macro mov32, reg, val
	movw	\reg, #:lower16:\val
	movt	\reg, #:upper16:\val
.endm

/*
 * Suspend/resume routine
 */
	.text
        .align L1_CACHE_SHIFT
ENTRY(jetson_suspend)

	push	{r3-r12, lr}
	mov	lr, r0			@ keep function address to call in 'lr'

	mov32	r8, suspend_context_area

	mov	r0, sp
	mrs	r1, cpsr
	mrs	r2, spsr

	mrc	p15, 0, r3, c1, c0, 2	@ cpacr
	stmia	r8, {r0-r3}
	mrc	p15, 2, r0, c0, c0, 0	@ csselr
	mrc	p15, 0, r1, c1, c0, 0	@ sctlr
	mrc	p15, 0, r2, c1, c0, 1	@ actlr
	mrc	p15, 0, r4, c15, c0, 0	@ pctlr
	add	r9, r8, #CTX_CSSELR
	stmia	r9, {r0-r2, r4}

#ifdef CONFIG_VFPv3
	orr	r2, r3, #0xF00000
	mcr	p15, 0, r2, c1, c0, 2	@ enable access to FPU
	VFPFMRX	r2, FPEXC
	str	r2, [r8, #CTX_FPEXC]
	mov	r1, #0x40000000		@ enable access to FPU
	VFPFMXR	FPEXC, r1
	VFPFMRX	r1, FPSCR
	str	r1, [r8, #CTX_FPSCR]
	isb
	add	r9, r8, #CTX_VFP_REGS

	VFPFSTMIA r9, r12	@ save out (16 or 32)*8B of FPU registers
	VFPFMXR	FPEXC, r2
	mcr	p15, 0, r3, c1, c0, 2	@ restore original FPEXC/CPACR
#endif
	mrc	p15, 0, r0, c15, c0, 1	@ diagnostic register (Cortex-A9)
	str	r0, [r8, #CTX_DIAG_CTR]

	add	r9, r8, #CTX_TTBR0
	mrc	p15, 0, r0, c2, c0, 0	@ TTBR0
	mrc	p15, 0, r1, c2, c0, 1	@ TTBR1
	mrc	p15, 0, r2, c2, c0, 2	@ TTBCR
	mrc	p15, 0, r3, c3, c0, 0	@ domain access control reg
	mrc	p15, 0, r4, c7, c4, 0	@ PAR
	mrc	p15, 0, r5, c10, c2, 0	@ PRRR
	mrc	p15, 0, r6, c10, c2, 1	@ NMRR
	mrc	p15, 0, r7, c12, c0, 0	@ VBAR
	stmia	r9!, {r0-r7}
	mrc	p15, 0, r0, c13, c0, 1	@ CONTEXTIDR
	mrc	p15, 0, r1, c13, c0, 2	@ TPIDRURW
	mrc	p15, 0, r2, c13, c0, 3	@ TPIDRURO
	mrc	p15, 0, r3, c13, c0, 4	@ TPIDRPRW
	stmia	r9, {r0-r3}

	cps	0x1f			@ SYS mode
	add	r9, r8, #CTX_SYS_SP
	stmia	r9, {sp,lr}

	cps	0x17			@ Abort mode
	mrs	r12, spsr
	add	r9, r8, #CTX_ABT_SPSR
	stmia	r9, {r12,sp,lr}

	cps	0x12			@ IRQ mode
	mrs	r12, spsr
	add	r9, r8, #CTX_IRQ_SPSR
	stmia	r9, {r12,sp,lr}

	cps	0x1b			@ Undefined mode
	mrs	r12, spsr
	add	r9, r8, #CTX_UND_SPSR
	stmia	r9, {r12,sp,lr}

	mov	r0, r8
	add	r1, r8, #CTX_FIQ_SPSR
	cps	0x11			@ FIQ mode
	mrs	r7, spsr
	stmia	r1, {r7-r12,sp,lr}

	cps	0x13			@ back to SVC

	/*
	 * call the function kept in 'lr', with the resume address.
	 */
	ldr	r0, =jetson_resume
	blx	lr

	pop	{r3-r12, lr}
	mov	pc, lr

	.align L1_CACHE_SHIFT
ENTRY(jetson_resume)

	mov32	r0, suspend_context_area
	sub	r0, r0, #PAGE_OFFSET
	add	r0, r0, #PHYS_OFFSET
	mov	r8, r0

	cps	0x11			@ FIQ mode
	add	r1, r0, #CTX_FIQ_SPSR
	ldmia	r1, {r7-r12,sp,lr}
	msr	spsr_fsxc, r7

	cps	0x12			@ IRQ mode
	add	r1, r0, #CTX_IRQ_SPSR
	ldmia	r1, {r12, sp, lr}
	msr	spsr_fsxc, r12

	cps	0x17			@ abort mode
	add	r1, r0, #CTX_ABT_SPSR
	ldmia	r1, {r12, sp, lr}
	msr	spsr_fsxc, r12

	cps	0x1f			@ SYS mode
	add	r1, r0, #CTX_SYS_SP
	ldmia	r1, {sp, lr}

	cps	0x1b			@ Undefined mode
	add	r1, r0, #CTX_UND_SPSR
	ldmia	r1, {r12, sp, lr}
	msr	spsr_fsxc, r12

	cps	0x13			@ back to SVC
	mov	r8, r0

        mov     ip, #0
        mcr     p15, 0, ip, c7, c5, 0   @ invalidate I cache
        mcr     p15, 0, ip, c13, c0, 1  @ set reserved context ID

	add	r9, r8, #CTX_ACTLR
	ldr	r6, [r9]

        mrc     p15, 0, r4, c1, c0, 1   @ Read Auxiliary control register
        teq     r4, r6                  @ Is it already set?
        mcrne   p15, 0, r6, c1, c0, 1   @ No, so write it

	add	r9, r8, #CTX_DACR
	ldr	r6, [r9]

	add	r9, r8, #CTX_TTBR0
	ldmia	r9!, {r1-r3}

#ifdef CONFIG_MMU
        mcr     p15, 0, ip, c8, c7, 0   @ invalidate TLBs
        mcr     p15, 0, r6, c3, c0, 0   @ Domain ID
#ifndef CONFIG_ARM_LPAE
        ALT_SMP(orr     r1, r1, #TTB_FLAGS_SMP)
        ALT_UP(orr      r1, r1, #TTB_FLAGS_UP)
#endif
        mcr     p15, 0, r1, c2, c0, 0   @ TTB 0
        mcr     p15, 0, r2, c2, c0, 1   @ TTB 1
        mcr     p15, 0, r3, c2, c0, 2  @ TTB control register
        ldr     r4, =PRRR               @ PRRR
        ldr     r5, =NMRR               @ NMRR
        mcr     p15, 0, r4, c10, c2, 0  @ write PRRR
        mcr     p15, 0, r5, c10, c2, 1  @ write NMRR
#endif  /* CONFIG_MMU */

	add	r9, r8, #CTX_VBAR
	ldmia	r9, {r1-r5}

	mcr	p15, 0, r1, c12, c0, 0	@ VBAR
	mcr	p15, 0, r3, c13, c0, 2	@ TPIDRURW
	mcr	p15, 0, r4, c13, c0, 3	@ TPIDRURO
	mcr	p15, 0, r5, c13, c0, 4	@ TPIDRPRW
	isb
	mcr	p15, 0, r2, c13, c0, 1	@ CONTEXTIDR
	isb

#ifdef CONFIG_VFPv3
	add	r9, r8, #CTX_CPACR
	ldr	r10, [r9]
        mcr     p15, 0, r10, c1, c0, 2  @ Co-processor access control
#endif

	add	r9, r8, #CTX_SCTLR
	ldr	r0, [r9]
	mov	r0, r0
        instr_sync
        mcr     p15, 0, r0, c1, c0, 0   @ turn on MMU, I-cache, etc
        instr_sync
        mov     r0, r0
        mov     r0, r0

##########
	ldmia	r8, {r5-r7, lr}
	/*
	 * now MMU is completely restored.
	 * we can touch kernel virtual addresses (stack, etc.)
	 */

	mov	sp, r5
	msr	cpsr_cxsf, r6
	msr	spsr_cxsf, r7

	/* finally, restore the stack and return */
	pop	{r3-r12, lr}

	/* return value */
	mov	r0, #0

	mov	pc, lr

ENDPROC(jetson_resume)
