/*  exception-i686.c  - i686s specific part of Exception Monitor
 *
 */

/* With non GPL files, use following license */
/*
 * Copyright 2006,2008,2009 Sony Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Otherwise with GPL files, use following license */
/*
 *  Copyright 2006,2008,2009 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include "exception.h"
#ifdef CONFIG_SNSC_EM_DEMANGLE
#include "demangle.h"
#endif
#ifdef CONFIG_SNSC_ALT_BACKTRACE
#include <linux/snsc_backtrace.h>
#endif

extern struct pt_regs *em_regs;

/*
 * for disassemble
 */
#ifdef CONFIG_SNSC_EM_DISASSEMBLE
enum X86_SYMTAX {
  ATT_SYNTAX,
  INTEL_SYNTAX
};
#define ALIGN4(x) (((x) + 0x3) & 0xfffffffffffffffc)
static int x86_disas_syntax = ATT_SYNTAX;
/* same size as in i386-dis.c */
#define MAX_BUF 20
static unsigned char x86_disas_buf[MAX_BUF];

extern int print_insn_x86_64_att(unsigned char *);
extern int print_insn_x86_64_intel(unsigned char *);

#endif

/*
 * for demangle
 */
#ifdef CONFIG_SNSC_EM_DEMANGLE
/* static int demangle_flag = DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE | DMGL_TYPES; */
static int demangle_flag = DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE;
#endif

extern void show_registers (struct pt_regs *);

void em_dump_regs(int argc, char **argv)
{
	em_dump_write("\n[register dump]\n");

	em_dump_write("rax: 0x%016lx  rbx: 0x%016lx  rcx: 0x%016lx  rdx: 0x%016lx\n",
		      em_regs->ax, em_regs->bx, em_regs->cx, em_regs->dx);
	em_dump_write("rsi: 0x%016lx  rdi: 0x%016lx  rsp: 0x%016lx  rbp: 0x%016lx\n",
		      em_regs->si, em_regs->di, em_regs->sp, em_regs->bp);
	em_dump_write(" r8: 0x%016lx   r9: 0x%016lx  r10: 0x%016lx  r11: 0x%016lx\n",
		      em_regs->r8, em_regs->r9, em_regs->r10, em_regs->r11);
	em_dump_write("r12: 0x%016lx  r13: 0x%016lx  r14: 0x%016lx  r15: 0x%016lx\n",
		      em_regs->r12, em_regs->r13, em_regs->r14, em_regs->r15);

	em_dump_write(" cs: 0x%04x     ss: 0x%04x\n", /* ds/es/fs/gs not in ptregs*/
		      em_regs->cs & 0xffff, em_regs->ss & 0xffff);

	/* but gs can be obtained by savesegment(gs,gs) macro.
	 * this can only have meaningful value if entered in kernel mode. */
	em_dump_write("rip: 0x%016lx  eflags: 0x%08x\n",
		      em_regs->ip, em_regs->flags);

}

static void em_dump_till_end_of_page(unsigned char *sp)
{
	unsigned long data;
	unsigned long page_size = 0x0000000000001000;
	unsigned long page_mask = 0xfffffffffffff000;
	unsigned long next_page = ((unsigned long)sp + page_size) & page_mask;
	int i = 0;
	char buf[17];

	em_dump_write("stack pointer: 0x%016lx\n", sp);
	em_dump_write("next page address: 0x%016lx\n", next_page);

	if (((unsigned long)sp % 0x10) != 0x0) {
		unsigned long align_diff = ((unsigned long)sp % 0x10);
		unsigned char *sptmp = sp - align_diff;
		em_dump_write("\n0x%016lx: ", sptmp);
		while (sptmp < sp -1) {
			em_dump_write("   ");
			buf[i] = ' ';
			i++;
			sptmp++;
		}
		em_dump_write("   ");
		buf[i] = ' ';
		i++;
	}

#define MIN_STACK_LEN 2048
	if ((next_page - (unsigned long)sp) < MIN_STACK_LEN)
		next_page = ((unsigned long)sp + MIN_STACK_LEN);

	buf[16] = 0;
	while ((unsigned long)sp < next_page && page_size > 0) {

		if (i % 0x10 == 0) {
			em_dump_write("0x%016lx: ", sp);
		}
		if (__get_user(data, sp++)) {
			em_dump_write(" (bad stack address)\n");
			break;
		}
		em_dump_write("%02x ", data);
		buf[(i % 0x10)] = em_convert_char(data);

		if (i % 0x10 == 0x0f) {
			em_dump_write(" : %s\n", buf);
		}

		i++;
		page_size--;
	}
}

/*
 * Assume -fomit-frame-pointer is not enabled, and bp points to stack frame.
 */
/* MEMO:
 *  If -fomit-frame-pointer is enabled, and bp is not pointing to base stack
 *  frame address, then method for finding return address is needed.
 *  One way is to look instruction in .text backwards, and find how deep stack
 *  is. (Like on arm) Other simpler way is to search stack backwards, and look
 *  for the value that lies in .text segment (r-x attribute).
*/
void em_dump_stack(int argc, char **argv)
{
	unsigned char *sp = (unsigned char *)em_regs->sp;
	unsigned long *bp = (unsigned long *)em_regs->bp;
	unsigned char *frame = NULL;
	unsigned char *tail;
	unsigned char data;
	unsigned long prev;

	tail = sp + PAGE_SIZE;

	em_dump_write("\n[stack dump]\n");

	if (__get_user (prev, bp)) {
		em_dump_write("\n (bad BP stack frame register data)\n");
		em_dump_till_end_of_page (sp);
		return;
	}
	frame = (unsigned char *)bp + 16;

	if (((unsigned long)sp % 0x10) != 0x0) {
		unsigned long align_diff = ((unsigned long)sp % 0x10);
		unsigned char *sptmp = sp - align_diff;
		em_dump_write("\n0x%016lx: ", sptmp);
		while (sptmp < sp -1) {
			em_dump_write("   ");
			sptmp++;
		}
		em_dump_write("  ");
	}

	while (sp < tail) {
		if (prev < (unsigned long)sp) {
			em_dump_write ("\n (previous BP smaller than SP"
				       ", end of stack frame.)\n\n");
			em_dump_till_end_of_page (sp);
			break;
		}

		if (__get_user(data, sp)) {
			em_dump_write("\n (bad stack address)\n");
			break;
		}

		if ((unsigned long)sp % 0x10 == 0)
			em_dump_write("\n0x%016lx:", sp);

		if (frame == sp)
			em_dump_write("|");
		else
			em_dump_write(" ");

		if (prev == (unsigned long)sp) {
			if (__get_user(prev, (unsigned long *)sp)) {
				em_dump_write("\n (bad previous BP address)\n");
				em_dump_till_end_of_page(sp);
				break;
			}
			/* 1 word above previous bp is ret addr.
			 * stack frame is above ret addr, so add 2. */
			frame = sp + 16;
		}

		em_dump_write("%02x", data);

		sp++;
	}
	em_dump_write("\n");
}

struct _reg_stat {
	char *desc;
	unsigned mask;
};
static const struct _reg_stat eflags[] = {
	{"Carry Flag (CF)",                 0x00000001},
	{"Parity Flag (PF)",                0x00000004},
	{"Auxiliary Carry Flag (AF)",       0x00000010},
	{"Zero Flag (ZF)",                  0x00000040},
	{"Sign Flag (SF)",                  0x00000080},
	{"Trap Flag (TF)",                  0x00000100},
	{"Interrupt Enable Flag (IF)",      0x00000200},
	{"Direction Flag (DF)",             0x00000400},
	{"Overflow Flag (OF)",              0x00000800},
	{"I/O Privilege Level (IOPL)",      0x00003000}, /* 2 bits */
	{"Nested Task (NT)",                0x00004000},
	{"Resume Flag (RF)",                0x00010000},
	{"Virtual-8086 Mode (VM)",          0x00020000},
	{"Alignment Check (AC)",            0x00040000},
	{"Virtual Interrupt Flag (VIF)",    0x00080000},
	{"Virtual Interrupt Pending (VIP)", 0x00100000},
	{"ID Flag (ID)",                    0x00200000}
};

void em_dump_regs_detail(int argc, char **argv)
{
	int i;

	em_dump_regs(argc, argv);
	em_dump_write("EFLAGS: \n");
	for (i = 0; i < sizeof(eflags)/sizeof(eflags[0]); i++) {
		switch (i) {
		case 0: case 1: case 2: case 3: case 4: case 8:
			em_dump_write("\tStatus Flag: %s: %s\n",
				      eflags[i].desc,
				      em_regs->flags & eflags[i].mask ?
				      "Set" : "Clear");
			break;
		case 7:
			em_dump_write("\tControl Flag: %s: %s\n",
				      eflags[i].desc,
				      em_regs->flags & eflags[i].mask ?
				      "Set" : "Clear");
			break;
		case 5: case 6: case 10: case 11: case 12:
		case 13: case 14: case 15: case 16:
			em_dump_write("\tSystem Flag: %s: %s\n",
				      eflags[i].desc,
				      em_regs->flags & eflags[i].mask ?
				      "Set" : "Clear");
			break;
		case 9:
			em_dump_write("\tSystem Flag: %s: %d\n",
				      eflags[i].desc,
				      em_regs->flags & eflags[i].mask
				      >> 12);
		default:
			break;
		}
	}
}

void em_get_callstack(void)
{
}

void em_show_syndrome(void)
{
}

#ifdef CONFIG_SNSC_ALT_BACKTRACE
static void em_print_symbol(const char *str)
{
#ifdef CONFIG_SNSC_EM_DEMANGLE
	char *demangle = cplus_demangle_v3(str, demangle_flag);
	if (demangle) {
		em_dump_write("%s", demangle);
		kfree(demangle);
		return;
	}
#endif
	em_dump_write("%s", str);
}


int em_bt_ustack_callback(struct bt_arch_callback_arg *cbarg, void *user)
{
	em_dump_write("[0x%016lx] ", cbarg->ba_addr);
	if (bt_status_is_error(cbarg->ba_status)) {
		em_dump_write("stop backtracing: %s\n", cbarg->ba_str);
		return 0;
	}

	/* show reliability */
	em_dump_write("%s", cbarg->reliable ? "" : "? ");

	if (cbarg->ba_sym_start == 0) {
		/* No symbol information. Show some other information */
		if (cbarg->ba_file)
			em_dump_write("stripped (%s +%#lx) (%s)\n",
				      bt_file_name(cbarg->ba_file),
				      cbarg->ba_addr - cbarg->ba_adjust,
				      bt_file_name(cbarg->ba_file));
		else
			em_dump_write("0x%016lx\n", cbarg->ba_addr);
		return 0;
	} else if (cbarg->ba_str[0]) {
		/* print function name here */
		em_print_symbol(cbarg->ba_str);
	}
	else {
		em_dump_write("0x%016lx", cbarg->ba_sym_start);
	}

	/* by symtab section */
	em_dump_write("+%#lx/%#lx (%s)\n",
		      cbarg->ba_addr - cbarg->ba_sym_start,
		      cbarg->ba_sym_size,
		      bt_file_name(cbarg->ba_file));

	return 0;
}

static void em_dump_callstack_ustack(const char *mode)
{
	/*
	 * cant pass em_regs here,
	 * it contains kernel register values during kernel exception
	 */
	struct pt_regs *usr_regs = task_pt_regs(current);

	em_dump_write("rax: 0x%016lx  rbx: 0x%016lx  rcx: 0x%016lx  rdx: 0x%016lx\n",
		      usr_regs->ax, usr_regs->bx, usr_regs->cx, usr_regs->dx);
	em_dump_write("rsi: 0x%016lx  rdi: 0x%016lx  rsp: 0x%016lx  rbp: 0x%016lx\n",
		      usr_regs->si, usr_regs->di, usr_regs->sp, usr_regs->bp);
	em_dump_write(" r8: 0x%016lx   r9: 0x%016lx  r10: 0x%016lx  r11: 0x%016lx\n",
		      usr_regs->r8, usr_regs->r9, usr_regs->r10, usr_regs->r11);
	em_dump_write("r12: 0x%016lx  r13: 0x%016lx  r14: 0x%016lx  r15: 0x%016lx\n",
		      usr_regs->r12, usr_regs->r13, usr_regs->r14, usr_regs->r15);

	em_dump_write(" cs: 0x%04x     ss: 0x%04x\n", /* ds/es/fs/gs not in ptregs*/
		      usr_regs->cs & 0xffff, usr_regs->ss & 0xffff);

	/* but gs can be obtained by savesegment(gs,gs) macro.
	 * this can only have meaningful value if entered in kernel mode. */
	em_dump_write("rip: 0x%016lx  eflags: 0x%08x\n",
		      usr_regs->ip, usr_regs->flags);

	em_dump_write("\n[call stack (ustack)]\n");
	bt_ustack(mode, !not_interrupt, usr_regs, em_bt_ustack_callback, NULL);
	em_dump_write("\n");
}

int em_bt_kstack_callback(struct bt_arch_callback_arg *cbarg, void *user)
{
	em_dump_write(" [<%p>] %s%pB\n", (void *)cbarg->ba_addr,
		      cbarg->reliable ? "" : "? ", (void *)cbarg->ba_addr);

	return 0;
}

static void em_dump_callstack_kstack(const char *mode)
{
	em_dump_write("\n[call stack (kstack)]\n");
	bt_kstack_current(mode, em_bt_kstack_callback, NULL);
	em_dump_write("\n");
}

static void em_dump_callstack_kstack_regs(const char *mode)
{
	em_dump_write("\n[call stack (kstack_regs)]\n");
	bt_kstack_regs(current, em_regs, em_bt_kstack_callback, NULL, 1);
	em_dump_write("\n");
}
#endif

static int em_is_param_char(char c)
{
	return isalnum(c) || c == '_';
}

static const char *em_param_match(const char *param, const char *name)
{
	const char *from = strstr(param, name);
	const char *to;

	if (from == NULL)
		return NULL;
	if (from > param && em_is_param_char(from[-1])) /* suffix match */
		return NULL;
	to = from + strlen(name);
	if (em_is_param_char(*to))
		return NULL; /* prefix match */
	return to;
}

static int em_callstack_mode(const char *mode)
{
	int count = 0;
#ifdef CONFIG_SNSC_ALT_BACKTRACE
	if (em_param_match(mode, "kstack")) {
		em_dump_callstack_kstack(mode);
		count++;
	}
	if (!not_interrupt && em_param_match(mode, "kstack_regs")) {
		em_dump_callstack_kstack_regs(mode);
		count++;
	}
	if (current->mm && em_param_match(mode, "ustack")) {
		em_dump_callstack_ustack(mode);
		count++;
	}
#endif
	return count;
}

void em_dump_callstack(int argc, char **argv)
{
	int count;
	if (!argv || argc <= 1)
		count = em_callstack_mode(em_callstack_param);
	else
		count = em_callstack_mode(argv[1]);
	if (count == 0)
		em_dump_write("\n[call stack]\nno callstack selected\n\n");
}

#ifdef CONFIG_SNSC_EM_DISASSEMBLE
void em_dump_disasm(int argc, char **argv, int *disasm_size,
		    unsigned long **disasm_point)
{
	int n;
	int size = *disasm_size;
	unsigned long *point = (unsigned long *)*disasm_point;
	int i;
	unsigned char *pointc;

	for (i = 0; i < argc && argv != NULL; i++) {
		if (strncmp(argv[i], "-M", 2) == 0 ||
		    strncmp(argv[i], "-m", 2) == 0) {
			if ((i + 1) == argc) {
				em_dump_write("-M option needs syntax (intel or att) 1\n");
				return;
			}
			if (strncmp(argv[i+1], "intel", 5) == 0) {
				x86_disas_syntax = INTEL_SYNTAX;
			} else if (strncmp(argv[i+1], "att", 3) == 0) {
				x86_disas_syntax = ATT_SYNTAX;
			} else {
				em_dump_write("-M option syntax must be intel or att.\n");
				return;
			}
			/* wipe off x86 syntax args */
			i += 2;
			for (; i < argc; i++) {
				argv[i-2] = argv[i];
			}
			argc -= 2;
			break;
		}
	}

	switch (argc) {
	case 3:
		if ((argv[2][0] == '0') && (toupper(argv[2][1]) == 'X')) {
			argv[2] = &argv[2][2];
		}
		size = simple_strtoul(argv[2], NULL, 16);
	case 2:
		if ((argv[1][0] == '0') && (toupper(argv[1][1]) == 'X')) {
			argv[1] = &argv[1][2];
		}
		point =
		    (unsigned long *)ALIGN4(simple_strtoul(argv[1], NULL, 16));
		break;
	case 1:
		break;
	default:
		return;
	}

	pointc = (unsigned char *)point;
	n = size;
	while (n--) {
		em_dump_write("0x%016lx:\t", pointc);
		if (__copy_from_user(x86_disas_buf, pointc, MAX_BUF)) {
			em_dump_write("(bad data address)\n");
			pointc++;
			continue;
		}
		if (x86_disas_syntax == ATT_SYNTAX)
			pointc += print_insn_x86_64_att(x86_disas_buf);
		else if (x86_disas_syntax == INTEL_SYNTAX)
			pointc += print_insn_x86_64_intel(x86_disas_buf);
		else
			em_dump_write("invalid syntax");
		em_dump_write("\n");
	}
	point = (unsigned long *)pointc;
	*disasm_point = point;
	*disasm_size = size;
}
#endif
