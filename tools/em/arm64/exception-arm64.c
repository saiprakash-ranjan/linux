/* 2016-11-10: File added and changed by Sony Corporation */
/*
 *  exception-arm64.c  - arm64 specific part of Exception Monitor
 *  Based on exception-arm.c
 *
 */

/* With non GPL files, use following license */
/*
 * Copyright 2005,2008,2009,2016 Sony Corporation.
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
 *  Copyright 2005,2008,2009,2016 Sony Corporation.
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
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include "exception.h"
#include "demangle.h"
#ifdef CONFIG_SNSC_ALT_BACKTRACE
#include <linux/snsc_backtrace.h>
#endif
#ifdef CONFIG_SNSC_EM_DEMANGLE
#include <demangle.h>
#endif

//#define DEBUG
#ifdef DEBUG
#define dbg(fmt, argv...) em_dump_write(fmt, ##argv)
#else
#define dbg(fmt, argv...) do{}while(0)
#endif

#define ARRAY_NUM(x) (sizeof(x)/sizeof((x)[0]))
#define ALIGN4(x) (((x) + 0x3) & 0xfffffffc)
#define FILE int

#ifdef CONFIG_SNSC_EM_DISASSEMBLE
extern int print_insn(FILE *, unsigned long, unsigned);
#endif

extern struct pt_regs *em_regs;

#ifdef CONFIG_SNSC_EM_INITDUMP_PROC_NOTIFY
#define BT_MAX 100

struct stackframe_entry {
	struct stackframe entry;
	unsigned int depth;
};

static void em_dump_write_buffer(const char *format, ...)
{
	char buf[WRITE_BUF_SZ];
	va_list args;

	va_start(args, format);
	vsnprintf(buf, WRITE_BUF_SZ, format, args);
	va_end(args);
	buf[WRITE_BUF_SZ - 1] = '\0';

	em_dump_string_to_buffer(buf, strlen(buf));
}

/*
 * This function is from a following file.
 * arch/arm64/kernel/traps.c (dump_mem())
 */
static void em_dump_exception_stack(unsigned long bottom, unsigned long top)
{
	unsigned long first;
	mm_segment_t fs;
	int i;

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	em_dump_write_buffer("Exception stack(0x%016lx to 0x%016lx)\n", bottom, top);

	for (first = bottom & ~31; first < top; first += 32) {
		unsigned long p;
		char str[sizeof(" 12345678") * 8 + 1];

		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';

		for (p = first, i = 0; i < (32/8) && p < top; i++, p += 8) {
			if (p >= bottom && p < top) {
				unsigned long val;
				if (__get_user(val, (unsigned long *)p) == 0)
					scnprintf(str + i * 17, 17, " %016lx", val);
				else
					scnprintf(str + i * 17, 17, " ????????????????");
			}
		}
		em_dump_write_buffer("%04lx:%s\n", first & 0xffff, str);
	}

	set_fs(fs);
}

/*
 * This function is from a following file.
 * arch/arm64/kernel/trap.c (dump_backtrace_entry())
 */
static void em_dump_backtrace_entry(unsigned long where,
				    unsigned long from, unsigned long frame)
{
	struct pt_regs *regs;
	int i, top_reg;
	u64 lr, sp;

#ifdef CONFIG_KALLSYMS
	char sym1[KSYM_SYMBOL_LEN], sym2[KSYM_SYMBOL_LEN];
	sprint_symbol(sym1, where);
	sprint_symbol(sym2, from);

	em_dump_write_buffer("[<%08lx>] (%s) from [<%08lx>] (%s)\n", where, sym1, from, sym2);
#else
	em_dump_write_buffer("Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif

	if (in_exception_text(where)) {
		em_dump_exception_stack(frame + 4, frame + 4 + sizeof(struct pt_regs));
		regs = (struct pt_regs *)(frame + 4);

		if (compat_user_mode(regs)) {
			lr = regs->compat_lr;
			sp = regs->compat_sp;
			top_reg = 12;
		} else {
			lr = regs->regs[30];
			sp = regs->sp;
			top_reg = 29;
		}

		show_regs_print_info(KERN_DEFAULT);
		em_dump_write("pc : [<0x%016llx>] lr : [<0x%016llx>] pstate: 0x%016llx\n",
		       regs->pc, lr, regs->pstate);
		em_dump_write("sp : 0x%016llx\n", sp);
		for (i = top_reg; i >= 0; i--) {
			em_dump_write("x%-2d: 0x%016llx ", i, regs->regs[i]);
			if (i % 2 == 0)
				em_dump_write("\n");
		}
		em_dump_write("\n");
	}
}

static int report_trace(struct stackframe *frame, void *d)
{
	struct stackframe_entry *p = d;

	/* skip the first one */
	if (p->depth != BT_MAX)
		em_dump_backtrace_entry(p->entry.pc, frame->pc, frame->sp - 4);

	p->depth--;
	p->entry.fp = frame->fp;
	p->entry.sp = frame->sp;
	p->entry.pc = frame->pc;

	return p->depth == 0;
}

/* Simillar to dump_backtrace() in arch/arm64/kernel/traps.c */
static void em_initdump_bt(struct pt_regs *_regs)
{
	struct stackframe frame;
	struct stackframe_entry ent;
	register unsigned long current_sp asm ("sp");

	if (!is_initdumping)
		return;

	if (_regs) {
		/* kstack_regs */
		frame.fp = _regs->regs[29];
		frame.sp = _regs->sp;
		frame.pc = _regs->pc;
	} else {
		/* kstack */
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.pc = (unsigned long)em_initdump_bt;
	}

	ent.depth = BT_MAX;

	walk_stackframe(&frame, report_trace, &ent);

	return;
}

static void em_initdump_usr_regs(struct pt_regs *usr_regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (!is_initdumping)
		return;

	em_dump_write("\n[user register dump]\n");

	if (compat_user_mode(usr_regs)) {
		lr = usr_regs->compat_lr;
		sp = usr_regs->compat_sp;
		top_reg = 12;
	} else {
		lr = usr_regs->regs[30];
		sp = usr_regs->sp;
		top_reg = 29;
	}

	show_regs_print_info(KERN_DEFAULT);
	em_dump_write("pc : [<0x%016llx>] lr : [<0x%016llx>] pstate: 0x%016llx\n",
		usr_regs->pc, lr, usr_regs->pstate);
	em_dump_write("sp : 0x%016llx\n", sp);
	for (i = top_reg; i >= 0; i--) {
		em_dump_write("x%-2d: 0x%016llx ", i, usr_regs->regs[i]);
		if (i % 2 == 0)
			em_dump_write("\n");
	}
	em_dump_write("\n");
}
#else
static void em_initdump_bt(struct pt_regs *_regs) {}
static void em_initdump_usr_regs(struct pt_regs *usr_regs) {}
#endif

/*
 * for callstack
 */
#define LIB_NAME_SIZE 64
static char libname[LIB_MAX][LIB_NAME_SIZE];

#define ELF_INFO_MAX (1 + LIB_MAX)
static struct _elf_info elf_info[ELF_INFO_MAX];

/*
 * for demangle
 */
#ifdef CONFIG_SNSC_EM_DEMANGLE
/* static int demangle_flag = DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE | DMGL_TYPES; */
static int demangle_flag = DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE;
#endif

static void em_close_elffile(unsigned int index)
{
	if (elf_info[index].file) {
		filp_close(elf_info[index].file, NULL);
		elf_info[index].file = NULL;
	}
}

static void em_close_elffiles(int elf_cnt)
{
	int i;

	for (i = 0; i < elf_cnt; i++) {
		em_close_elffile(i);
	}
}

static int em_open_elffile(unsigned int index)
{
	int i;
	int strip = 2;
	Elf32_Ehdr ehdr;
	Elf32_Shdr shdr;
	Elf32_Shdr sh_shstrtab;
	Elf32_Phdr phdr;
	char *shstrtab;
	struct file *fp;

	/*
	 * open elf file
	 */
	elf_info[index].file =
	    filp_open(elf_info[index].filename, O_RDONLY, 0444);

	if (IS_ERR(elf_info[index].file)) {
		elf_info[index].file = NULL;
		goto fail;
	}
	fp = elf_info[index].file;

	if (!fp->f_op || !fp->f_op->read)
		goto close_fail;

	/*
	 * read elf header
	 */
	fp->f_op->read(fp, (char *)&ehdr, sizeof(Elf32_Ehdr), &fp->f_pos);
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
		goto close_fail;
	if (!elf_check_arch(&ehdr))
		goto close_fail;

	/*
	 * read program header
	 */
	vfs_llseek(fp, ehdr.e_phoff, 0);
	elf_info[index].vaddr = 0;
	for (i = 0; i < ehdr.e_phnum; i++) {
		fp->f_op->read(fp, (char *)&phdr, sizeof(Elf32_Phdr),
			       &fp->f_pos);
		if (phdr.p_type == PT_LOAD) { /* first PT_LOAD segment */
			elf_info[index].vaddr = phdr.p_vaddr;
			break;
		}
	}

	/*
	 * read section header table
	 */
	vfs_llseek(fp,
		   ehdr.e_shoff + sizeof(Elf32_Shdr) * ehdr.e_shstrndx,
		   0);
	fp->f_op->read(fp, (char *)&sh_shstrtab, sizeof(Elf32_Shdr),
		       &fp->f_pos);
	shstrtab = (char *)kmalloc(sh_shstrtab.sh_size, GFP_ATOMIC);
	if(shstrtab == NULL){
		goto close_fail;
	}
	vfs_llseek(fp, sh_shstrtab.sh_offset, 0);
	fp->f_op->read(fp, shstrtab, sh_shstrtab.sh_size, &fp->f_pos);

	/*
	 * read shsymtab
	 */
	vfs_llseek(fp, ehdr.e_shoff, 0);
	for (i = 0; i < ehdr.e_shnum; i++) {
		fp->f_op->read(fp, (char *)&shdr, sizeof(Elf32_Shdr),
			       &fp->f_pos);
		if (strcmp(&shstrtab[shdr.sh_name], ".dynsym") == 0)
			elf_info[index].sh_dynsym = shdr;
		else if (strcmp(&shstrtab[shdr.sh_name], ".dynstr") == 0)
			elf_info[index].sh_dynstr = shdr;
		else if (strcmp(&shstrtab[shdr.sh_name], ".symtab") == 0) {
			elf_info[index].sh_symtab = shdr;
			strip--;
		} else if (strcmp(&shstrtab[shdr.sh_name], ".strtab") == 0) {
			elf_info[index].sh_strtab = shdr;
			strip--;
		}
	}

	if (!strip)
		elf_info[index].strip = strip;

	kfree(shstrtab);
	return 0;

      close_fail:
	em_close_elffile(index);
      fail:
	return -1;
}

static void init_struct_elfinfo(void)
{
	int i;

	for (i = 0; i < ELF_INFO_MAX; i++) {
		elf_info[i].filename = 0;
		elf_info[i].sh_dynsym.sh_size = 0;
		elf_info[i].sh_dynstr.sh_size = 0;
		elf_info[i].sh_symtab.sh_size = 0;
		elf_info[i].sh_strtab.sh_size = 0;
		elf_info[i].addr_offset = 0;
		elf_info[i].addr_end = 0;
		elf_info[i].strip = 1;
	}

}

static int em_open_elffiles(void)
{
	char *path;
	char buf[LIB_NAME_SIZE];
	struct vm_area_struct *vm = NULL;
	char *short_name;
	int elf_cnt = 0;
	int i;

	/*
	 * initialize
	 */
	init_struct_elfinfo();

	if (!not_interrupt)
		goto out;

	/*
	 * set elf_info
	 */
	elf_info[0].filename = em_get_execname();
	short_name = elf_info[0].filename;
	for (i = 0; elf_info[0].filename[i]; i++)
		if (elf_info[0].filename[i] == '/')
			short_name = &elf_info[0].filename[i + 1];
	if (current->mm != NULL)
		vm = current->mm->mmap;
	for (; vm != NULL; vm = vm->vm_next) {
		if (vm->vm_flags & VM_WRITE)
			continue;
		if (vm->vm_file == NULL)
			continue;
		if (vm->vm_file->f_path.dentry) {
			if (strcmp
			    (vm->vm_file->f_path.dentry->d_name.name,
			     short_name) == 0) {
				elf_info[0].addr_offset = vm->vm_start;
				elf_info[0].addr_end = vm->vm_end;
			}
		}
	}

	elf_cnt = 1;

	if (current->mm != NULL)
		vm = current->mm->mmap;
	for (i = 0; i < ARRAY_NUM(libname) && vm != NULL; vm = vm->vm_next) {
		if ((vm->vm_flags & (VM_READ | VM_EXEC)) != (VM_READ | VM_EXEC))
			continue;
		if (vm->vm_flags & VM_WRITE)	/* assume text is r-x and text
						   seg addr is base addr */
			continue;
		if (current->mm->exe_file != NULL)
			continue;
		if (vm->vm_file == NULL)
			continue;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
		path = d_path(&vm->vm_file->f_path, buf, sizeof(buf));
#else
		path = d_path(vm->vm_file->f_path.dentry, vm->vm_file->f_vfsmnt,
			      buf, sizeof(buf));
#endif
		buf[sizeof(buf) - 1] = '\0';

		if (path == NULL || access_ok(VERIFY_READ, (unsigned long)path, sizeof(buf)-1)) {
			continue;
		}
		if (strcmp(path, "/lib/ld-linux.so.2") == 0)
			continue;
		if (strcmp(path, "/devel/lib/ld-linux.so.2") == 0)
			continue;
		if (strcmp(path, "/lib/sonyld.so") == 0)
			continue;

		strncpy(libname[i], path, LIB_NAME_SIZE);
		libname[i][LIB_NAME_SIZE - 1] = 0;

		elf_info[elf_cnt].filename = libname[i];
		elf_info[elf_cnt].addr_offset = vm->vm_start;
		elf_info[elf_cnt].addr_end = vm->vm_end;
		elf_cnt++;
		i++;
	}

	for (i = 0; i < elf_cnt; i++) {
		if (em_open_elffile(i) == -1)
			em_dump_write("\n\tWARNING: file not found: %s\n",
				      elf_info[i].filename);

		dbg("file : %s (%08lx %08lx)\n",
			 elf_info[i].filename, elf_info[i].addr_offset, elf_info[i].addr_end);
	}

out:
	return elf_cnt;
}

static inline ulong em_get_user(ulong *p)
{
	ulong v;

	if (__get_user(v, p))
		v = 0;

	return v;
}

static inline ulong em_put_user(ulong v, ulong *p)
{
	return __put_user(v, p);
}

static inline ulong arch_stack_pointer(ulong *frame)
{
	return em_get_user(frame - 2);
}

static inline ulong arch_caller_address(ulong *frame)
{
	return em_get_user(frame - 1);
}

static inline ulong *arch_prev_frame(ulong *frame)
{
	return (ulong *)em_get_user(frame - 3);
}

void em_get_callstack(void)
{
	int elf_cnt;

	elf_cnt = em_open_elffiles();

	em_close_elffiles(elf_cnt);

}
void em_dump_regs(int argc, char **argv)
{
	char *mode;
	char mode_list[][5] = {"EL0t","EL1t","EL1h","EL2t","EL2h"
			       ,"EL3t","EL3h","???"};
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(em_regs)) {
		lr = em_regs->compat_lr;
		sp = em_regs->compat_sp;
		top_reg = 12;
	} else {
		lr = em_regs->regs[30];
		sp = em_regs->sp;
		top_reg = 29;
	}

	em_dump_write("\n[register dump]\n");
	show_regs_print_info(KERN_DEFAULT);
	em_dump_write("pc : [<0x%016llx>] lr : [<0x%016llx>] pstate: 0x%016llx\n",
	       em_regs->pc, lr, em_regs->pstate);
	em_dump_write("sp : 0x%016llx\n", sp);
	for (i = top_reg; i >= 0; i--) {
		em_dump_write("x%-2d: 0x%016llx ", i, em_regs->regs[i]);
		if (i % 2 == 0)
			em_dump_write("\n");
	}
	em_dump_write("\n");

	switch (em_regs->pstate & PSR_MODE_MASK) {
	case PSR_MODE_EL0t: mode = mode_list[0]; break;
	case PSR_MODE_EL1t: mode = mode_list[1]; break;
	case PSR_MODE_EL1h: mode = mode_list[2]; break;
	case PSR_MODE_EL2t: mode = mode_list[3]; break;
	case PSR_MODE_EL2h: mode = mode_list[4]; break;
	case PSR_MODE_EL3t: mode = mode_list[5]; break;
	case PSR_MODE_EL3h: mode = mode_list[6]; break;
	default: mode = mode_list[7];break;
	}
	em_dump_write("pstate: 0x%016lx: Condition Flags: %c%c%c%c, "
		      "IRQ: O%s, FIQ: O%s, \n Execution state: %s, "
		      " Debug mask: %smasked System Error: %smasked Mode: %s\n",
		      em_regs->pstate,
		      (em_regs->pstate & PSR_N_BIT) ? 'N' : 'n',
		      (em_regs->pstate & PSR_Z_BIT) ? 'Z' : 'z',
		      (em_regs->pstate & PSR_C_BIT) ? 'C' : 'c',
		      (em_regs->pstate & PSR_V_BIT) ? 'V' : 'v',
		      (em_regs->pstate & PSR_I_BIT) ? "FF" : "N",
		      (em_regs->pstate & PSR_F_BIT) ? "FF" : "N",
		      (em_regs->pstate & PSR_MODE32_BIT) ? "ARM" : "A64",
		      (em_regs->pstate & PSR_D_BIT) ? "" : "Un",
		      (em_regs->pstate & PSR_A_BIT) ? "" : "Un",
		      mode);

	return;
}

#ifdef CONFIG_SNSC_EM_DUMP_VFP_REGISTER
/* Print the the current VFP state from the provided structures. */
void em_dump_vfp(int argc, char **argv)
{
	struct fpsimd_state *hwstate;
	struct task_struct *tsk = current;
	int i;

	/* Ensure that the saved uregs is up-to-date. */
	fpsimd_restore_current_state();
	hwstate = &tsk->thread.fpsimd_state;

	em_dump_write("\n[FPSIMD register dump]\n");

#ifdef CONFIG_SMP
	em_dump_write("cpu:%d\n", hwstate->cpu);
#endif
	for (i = 32; i >= 0; i--) {
		em_dump_write("V%-2d: 0x%032llx ", i, hwstate->vregs[i]);
		if (i % 2 == 0)
			em_dump_write("\n");
	}

	em_dump_write("fpsr:0x%08x:\n\tCondition Flags: %c%c%c%c%s\n"
		      "\tException bits: IDC:O%s, IXC:O%s, UFC:O%s, OFC:O%s, DZC:O%s, IOC:O%s\n",
		      hwstate->fpcr,
		      (hwstate->fpsr & PSR_N_BIT) ? 'N' : 'n',
		      (hwstate->fpsr & PSR_Z_BIT) ? 'Z' : 'z',
		      (hwstate->fpsr & PSR_C_BIT) ? 'C' : 'c',
		      (hwstate->fpsr & PSR_V_BIT) ? 'V' : 'v',
		      (hwstate->fpsr & PSR_Q_BIT) ? "Q" : "q",
		      (hwstate->fpsr & (1 << 7)) ? "FF" : "N",
		      (hwstate->fpsr & (1 << 4)) ? "FF" : "N",
		      (hwstate->fpsr & (1 << 3)) ? "FF" : "N",
		      (hwstate->fpsr & (1 << 2)) ? "FF" : "N",
		      (hwstate->fpsr & (1 << 1)) ? "FF" : "N",
		      (hwstate->fpsr & (1 << 0)) ? "FF" : "N");

	em_dump_write("fpcr:0x%08x:\n\tRMode:%d FZ:%sabled DN:O%s AHP:O%s\n",
			hwstate->fpcr, hwstate->fpcr & (0x3 << 22),
			hwstate->fpcr & (0x1 << 24) ? "En" : "Dis",
			hwstate->fpcr & (0x1 << 25) ? "FF" : "N",
			hwstate->fpcr & (0x1 << 26) ? "FF" : "N");
}
#endif

void em_dump_regs_detail(int argc, char **argv)
{
	em_dump_regs(1, NULL);
	return;
}

static void em_dump_till_end_of_page(unsigned long *sp)
{
	unsigned long *tail = sp;
	unsigned long stackdata;
	int i = 0;
	char buf[33];

	tail = (unsigned long *)(((unsigned long)sp + PAGE_SIZE - 1) & PAGE_MASK);

#define MIN_STACK_LEN 2048
	if (((unsigned long)tail - (unsigned long)sp) < MIN_STACK_LEN)
		tail = (unsigned long *)((unsigned long) sp + MIN_STACK_LEN);

	buf[32] = 0;
	while (sp < tail) {

		if ((i % 4) == 0) {
			em_dump_write("0x%016lx : ", (unsigned long)sp);
		}
		if (__get_user(stackdata, sp++)) {
			em_dump_write(" (Bad stack address)\n");
			break;
		}
		em_dump_write(" 0x%016lx", (unsigned long)stackdata);
		buf[(i % 4) * 8]     = em_convert_char(stackdata);
		buf[(i % 4) * 8 + 1] = em_convert_char(stackdata >> 8);
		buf[(i % 4) * 8 + 2] = em_convert_char(stackdata >> 16);
		buf[(i % 4) * 8 + 3] = em_convert_char(stackdata >> 24);
		buf[(i % 4) * 8 + 4] = em_convert_char(stackdata >> 32);
		buf[(i % 4) * 8 + 5] = em_convert_char(stackdata >> 40);
		buf[(i % 4) * 8 + 6] = em_convert_char(stackdata >> 48);
		buf[(i % 4) * 8 + 7] = em_convert_char(stackdata >> 56);

		if ((i % 4) == 3) {
			em_dump_write(" : %s\n", buf);
		}

		++i;
	}
}

void em_dump_stack(int argc, char **argv)
{
	unsigned long *sp = (unsigned long *)(em_regs->sp & ~0x03);
	unsigned long *fp = (unsigned long *)(em_regs->regs[29]& ~0x03);
	unsigned long *tail;
	unsigned long backchain;
	unsigned long stackdata;
	int frame = 1;

	tail = sp + PAGE_SIZE / 4;

	em_dump_write("\n[stack dump]\n");

	backchain = arch_stack_pointer(fp);
	while (sp < tail) {
		if (backchain == (unsigned long)sp) {
			em_dump_write("|");
			fp = arch_prev_frame(fp);
			if (!fp)
				break;

			backchain = arch_stack_pointer(fp);
			if (!backchain)
				break;
		} else {
			em_dump_write(" ");
		}

		if (backchain < (unsigned long)sp) {
			break;
		}

		if (__get_user(stackdata, sp)) {
			em_dump_write("\n (bad stack address)\n");
			break;
		}

		if (((unsigned long)tail-(unsigned long)sp) % 0x10 == 0) {
			if (frame) {
				em_dump_write("\n0x%08x:|", sp);
				frame = 0;
			} else {
				em_dump_write("\n0x%08x: ", sp);
			}
		}

		em_dump_write("0x%08x", stackdata);

		sp++;
	}

	em_dump_write("\n");

	em_dump_write("\n #################em_dump_till_end_of_page###########\n");
	em_dump_till_end_of_page(sp);
	em_dump_write("\n");
}

static struct fsr_info {
	const char *name;
} fsr_info[] = {
	{"ttbr address size fault"	},
	{"level 1 address size fault"	},
	{"level 2 address size fault"	},
	{"level 3 address size fault"	},
	{"input address range fault"	},
	{"level 1 translation fault"	},
	{"level 2 translation fault"	},
	{"level 3 translation fault"	},
	{"reserved access flag fault"	},
	{"level 1 access flag fault"	},
	{"level 2 access flag fault"	},
	{"level 3 access flag fault"	},
	{"reserved permission fault"	},
	{"level 1 permission fault"	},
	{"level 2 permission fault"	},
	{"level 3 permission fault"	},
	{"synchronous external abort"	},
	{"asynchronous external abort"	},
	{"unknown 18"			},
	{"unknown 19"			},
	{"synchronous abort (translation table walk)" },
	{"synchronous abort (translation table walk)" },
	{"synchronous abort (translation table walk)" },
	{"synchronous abort (translation table walk)" },
	{"synchronous parity error"	},
	{"asynchronous parity error"	},
	{"unknown 26"			},
	{"unknown 27"			},
	{"synchronous parity error (translation table walk" },
	{"synchronous parity error (translation table walk" },
	{"synchronous parity error (translation table walk" },
	{"synchronous parity error (translation table walk" },
	{"unknown 32"			},
	{"alignment fault"		},
	{"debug event"			},
	{"unknown 35"			},
	{"unknown 36"			},
	{"unknown 37"			},
	{"unknown 38"			},
	{"unknown 39"			},
	{"unknown 40"			},
	{"unknown 41"			},
	{"unknown 42"			},
	{"unknown 43"			},
	{"unknown 44"			},
	{"unknown 45"			},
	{"unknown 46"			},
	{"unknown 47"			},
	{"unknown 48"			},
	{"unknown 49"			},
	{"unknown 50"			},
	{"unknown 51"			},
	{"implementation fault (lockdown abort)" },
	{"unknown 53"			},
	{"unknown 54"			},
	{"unknown 55"			},
	{"unknown 56"			},
	{"unknown 57"			},
	{"implementation fault (coprocessor abort)" },
	{"unknown 59"			},
	{"unknown 60"			},
	{"unknown 61"			},
	{"unknown 62"			},
	{"unknown 63"			},
};

void em_show_syndrome(void)
{
	unsigned long fsr, far;
	struct fsr_info *inf;
	struct task_struct *tsk = current;

	em_dump_write("\n\n[exception syndrome]\n");
	fsr = tsk->thread.fault_code;
	far = tsk->thread.fault_address;
	inf = fsr_info + (fsr & 63);
	em_dump_write("tp_value:	0x%-20lx pc:\t\t0x%08lx\n",
			tsk->thread.tp_value, em_regs->pc);
	em_dump_write("fault_code:	0x%-20lx fault_address:	0x%08lx"
			"\nfault:\t\t%s\n", fsr, far, inf->name);
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

/* variables needed during disasseble of instruction  */
unsigned long func_start, exception_addr;

#ifdef CONFIG_SNSC_EM_DISASSEMBLE
extern int disasm_command;
/* function to print only insns and not the disasm information */
static void dump_only_insns(int disasm_size, unsigned long **disasm_point)
{
	unsigned long insn;
	int i = 0;
	unsigned long *point = (unsigned long *)*disasm_point;
	point -= 8;

	for (i=0; i<disasm_size; i++) {
		if (__get_user(insn, point)) {
			point++;
			continue;
		}

		/* ARM instructions */
		if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
			em_dump_write("0x%08x:\t%08x \n", point, insn);
		}
		else { /* Thumb2 instructions */
			unsigned long tmp_insn = insn & 0xffff;
			em_dump_write("0x%08x:\t%04x ", point, tmp_insn);
			tmp_insn = ((insn >> 16)& 0xffff);
			em_dump_write("%04x\n", tmp_insn);
		}
		point++;
	}

	disasm_command = 1;
	*disasm_point = point;
}

void em_dump_disasm(int argc, char **argv, int *disasm_size,
		    unsigned long **disasm_point)
{
	unsigned long insn;
	int size = *disasm_size;
	unsigned long *point = (unsigned long *)*disasm_point;
	int i;
	unsigned long *end_addr;
	int extra_print_size = 0;

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

	/* called from EM normal flow and not from exception prompt */
	if(disasm_command == 0) {
		if((unsigned long)point <= USER_DS) {
			/* codition where retreiving addsymlist info failed */
			if ((func_start == 0) || (exception_addr == 0)) {
				dump_only_insns(*disasm_size, disasm_point);
				return;
			}
			point = (unsigned long *)func_start;
			size = ((exception_addr - func_start)+(sizeof(unsigned long))-1) / (sizeof(unsigned long));
		}
		else {
			/* called for kernel exception, start addr should be 8 instr before pc
			   we just moved the logic from em_dump_exception to here */
			point -= 8;
		}
	}
	else {
		/* No need to get the alligned adderss for ARM case, as it is always 4 byte alligned */
		if(em_regs->ARM_cpsr & PSR_T_BIT)
			while (1) {
				if (__get_user(insn, point--)) {
					em_dump_write("(bad data address)\n");
					point++;
					break;
				}
				else {
					/* to get the proper starting address for thumb2 instruction */
					if((insn & 0xF800) == 0xF800 ||
					  (insn & 0xF800) == 0xF000 ||
					  (insn & 0xF800) == 0xE800) {
						insn = ((insn >> 16)& 0xffff);
						if((insn & 0xF800) == 0xF800 ||
						  (insn & 0xF800) == 0xF000 ||
						  (insn & 0xF800) == 0xE800){
							continue;
						}
						else {
							point = (unsigned long *)((unsigned long)point+2);
							break;
						}
					}
					else {
						point++;
						break;
					}
				}
			}
	}

	end_addr = point + size;
	/* disassemble size should not be more than 16, */
	if(disasm_command == 0) {
		if (size < *disasm_size) {
			extra_print_size = (*disasm_size - size)/2;
			point = point - extra_print_size;
			for (i=0; i<extra_print_size; i++) {
				if (__get_user(insn, point)) {
					point++;
					continue;
				}

				/* ARM instructions */
				if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
					em_dump_write("0x%08x:\t%08x \n", point, insn);
				}
				else { /* Thumb2 instructions */
					unsigned long tmp_insn = insn & 0xffff;
					em_dump_write("0x%08x:\t%04x ", point, tmp_insn);
					tmp_insn = ((insn >> 16)& 0xffff);
					em_dump_write("%04x\n", tmp_insn);
				}
				point++;
			}
		}
		size = *disasm_size;

		while ((end_addr - point) > *disasm_size) {
			if (__get_user(insn, point++))
				continue;
			if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
				point = end_addr - *disasm_size;
				break;
			}
			else {
				unsigned long tmp_insn = insn & 0xffff;
				tmp_insn = ((insn >> 16)& 0xffff);

				if ((tmp_insn & 0xF800) == 0xF800 ||
				    (tmp_insn & 0xF800) == 0xF000 ||
				    (tmp_insn & 0xF800) == 0xE800)
					point = (unsigned long *)((unsigned long)point-2);
			}
		}
	}

	while (point <= end_addr) {
		em_dump_write("0x%08x:\t", point);
		if (__get_user(insn, point)) {
			em_dump_write("(bad data address)\n");
			point++;
			continue;
		}
		if (print_insn(NULL, insn, (unsigned)point++) == 0xFF)
			point = (unsigned long *)((unsigned long)point-2);
	}
	*disasm_point = point;
	*disasm_size = size;

	if (disasm_command == 1)
		em_dump_write("disassembly may display invalid information sometimes !!! \n");
	else{
		disasm_command = 1;
		if (extra_print_size) {
			for (i=0; i<extra_print_size; i++) {
				if (__get_user(insn, point)) {
					point++;
					continue;
				}

				/* ARM instructions */
				if (!(em_regs->ARM_cpsr & PSR_T_BIT)) {
					em_dump_write("0x%08x:\t%08x \n", point, insn);
				}
				else { /* Thumb2 instructions */
					unsigned long tmp_insn = insn & 0xffff;
					em_dump_write("0x%08x:\t%04x ", point, tmp_insn);
					tmp_insn = ((insn >> 16)& 0xffff);
					em_dump_write("%04x\n", tmp_insn);
				}
				point++;
			}
		}
	}
}
#endif

int em_bt_ustack_callback(struct bt_arch_callback_arg *cbarg, void *user)
{
	static bool copy_addr = 0;
	em_dump_write("[0x%016lx] ", cbarg->ba_addr);
	if (bt_status_is_error(cbarg->ba_status)) {
		em_dump_write("stop backtracing: %s\n", cbarg->ba_str);

		/* if PC is invalid/zero, unwind/disassemble may tried with the valid LR.
		   CPSR bit need to be adjusted as per the LR register
		   if return addr is thumb2, last bit of LR is always 1
		   So set the T bit in cpsr regsiter. */
		/*
		 * In AARCH64 mode
		 * The nRW bit indicates the execution state of the CPU
		 * 1 is 32bit mode (AARCH32 and T32 modes)
		 * 0 is 64bit mode (AARCH64)
		 */
		if ((copy_addr == 0) && (compat_user_mode(em_regs))) {
			/*
			 * In AARCH32 mode the behaviour is exactly similar to ARMv7
			 * CPSR 5th bit indicates the
			 * 1 is T32 mode
			 * 0 is ARM mode
			 * Set the bit accordingly in the pstate.
			 */
			if (em_regs->compat_lr & 1)
				em_regs->pstate |= COMPAT_PSR_T_BIT;
			else
				em_regs->pstate &= ~COMPAT_PSR_T_BIT;
		}
		return 0;
	}

	/* copying the func start addr and pc, needed during disassebly
	   disassembly need func start address as a starting point to avoid arm/thumb2 confusion */
	if (copy_addr == 0) {
		unsigned int insn_size = 4;
		exception_addr = (cbarg->ba_addr/insn_size) * insn_size;
		func_start = (cbarg->ba_sym_start/insn_size) * insn_size;
		copy_addr = 1;
	}

	if (cbarg->ba_str[0]) {
		em_print_symbol(cbarg->ba_str);
	}
	/* condition for missing addsymlist information */
	else if (cbarg->ba_sym_start == 0) {
		em_dump_write("stripped (%s +%#lx) (%s hash:00000000)\n",
			      bt_file_name(cbarg->ba_file),
			      cbarg->ba_addr - cbarg->ba_adjust,
			      bt_file_name(cbarg->ba_file));
		return 0;
	}
	else {
		em_dump_write("0x%016lx", cbarg->ba_sym_start);
	}
	if (bt_hash_valid(cbarg)) {
		/* by symlist section */
		const unsigned char *hash = (unsigned char *)cbarg->ba_hash;
		em_dump_write("+%#lx (%s hash:%02x%02x%02x%02x adj:%ld)\n",
			      cbarg->ba_addr - cbarg->ba_sym_start,
			      bt_file_name(cbarg->ba_file),
			      hash[0], hash[1], hash[2], hash[3],
			      cbarg->ba_adjust);
	}
	else {
		/* by symtab section */
		em_dump_write("+%#lx/%#lx (%s +%#lx)\n",
			      cbarg->ba_addr - cbarg->ba_sym_start,
			      cbarg->ba_sym_size,
			      bt_file_name(cbarg->ba_file),
                  cbarg->ba_addr - cbarg->ba_adjust);
	}
	return 0;
}

static void em_dump_callstack_ustack(const char *mode)
{
	/* cant pass em_regs here, it contains kernel register values during kernel exception */
	struct pt_regs *usr_regs = task_pt_regs(current);
	em_initdump_usr_regs(usr_regs);
	em_dump_write("\n[call stack (ustack)]\n");
	bt_ustack(mode, !not_interrupt, usr_regs, em_bt_ustack_callback, NULL);
	em_dump_write("\n");
}

int em_bt_kstack_callback(struct bt_arch_callback_arg *cbarg, void *user)
{
	em_dump_write("[0x%08lx] ", cbarg->ba_addr);
	if (!cbarg->ba_str) {
		em_dump_write("0x%08lx\n", cbarg->ba_addr);
		return 0;
	}
	em_dump_write("%s+%#lx/%#lx", cbarg->ba_str,
		      cbarg->ba_addr - cbarg->ba_sym_start,
		      cbarg->ba_sym_size);
	if (cbarg->ba_modname)
		em_dump_write(" [%s]\n", cbarg->ba_modname);
	else
		em_dump_write("\n");
	return 0;
}

static void em_dump_callstack_kstack(const char *mode)
{
	em_dump_write("\n[call stack (kstack)]\n");
	bt_kstack_current(mode, em_bt_kstack_callback, NULL);
	em_initdump_bt(NULL);
	em_dump_write("\n");
}

static void em_dump_callstack_kstack_regs(const char *mode)
{
	em_dump_write("\n[call stack (kstack_regs)]\n");
	bt_kstack_regs(current, em_regs, em_bt_kstack_callback, NULL, 1);
	em_initdump_bt(em_regs);
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
	em_dump_write("\n[em_callstack_mode]\n mode : %s\n\n", mode);
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

/*
 * Show task states
 */
struct stacktrace_state {
	unsigned int depth;
};

static int report_traces(struct stackframe *frame, void *d)
{
	struct stacktrace_state *sts = d;

	if (sts->depth) {
	em_dump_write("  pc: %p (%pF), sp %p, fp %p\n",
				frame->pc, frame->pc,
				frame->sp, frame->fp);
		sts->depth--;
		return 0;
	}
	em_dump_write("  ...\n");

	return sts->depth == 0;
}

static void em_sched_show_task(struct pt_regs *regs, struct task_struct *p)
{
	unsigned task_state;
	struct stackframe frame;
	static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
	struct stacktrace_state sts;

	register unsigned long current_sp asm ("sp");

	pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, p);

	/* show task state */
	task_state = p->state ? __ffs(p->state) + 1 : 0;
	em_dump_write("%-13.13s %c", p->comm,
		task_state < sizeof(stat_nam) - 1 ? stat_nam[task_state] : '?');
	if (task_state == TASK_RUNNING)
		em_dump_write(" running  ");
	else
		em_dump_write(" %08lx ", thread_saved_pc(p));
	em_dump_write("tgid %5d ", task_tgid_nr(p));
	em_dump_write("pid %5d ", task_pid_nr(p));
	em_dump_write("parent %6d ", task_pid_nr(p->real_parent));
	em_dump_write("flags 0x%08lx ",
				(unsigned long)task_thread_info(p)->flags);
	em_dump_write("cpu %d\n",task_cpu(p));

	/* show backtrace */
	if (regs) {
		frame.fp = regs->regs[29];
		frame.sp = regs->sp;
		frame.lr = regs->regs[30];
		/* PC might be corrupted, use LR in that case. */
		frame.pc = kernel_text_address(regs->pc)
			? regs->pc : regs->regs[30];
	} else if (p == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)em_sched_show_task;
	} else {
		/* task blocked in __switch_to */
		frame.fp = thread_saved_fp(p);
		frame.sp = thread_saved_sp(p);
		/*
		 * The function calling __switch_to cannot be a leaf function
		 * so LR is recovered from the stack.
		 */
		frame.lr = 0;
		frame.pc = thread_saved_pc(p);
	}

	sts.depth = 100;
	walk_stackframe(&frame, report_traces, &sts);
}

void em_task_log(int argc, char **argv)
{
	struct task_struct *g, *p;

	/* show each task's state */
	do_each_thread(g, p) {
		em_sched_show_task(NULL, p);
	} while_each_thread(g, p);
}
