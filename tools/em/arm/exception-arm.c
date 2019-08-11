/*
 *  exception-arm.c  - arm specific part of Exception Monitor
 *
 */

/* With non GPL files, use following license */
/*
 * Copyright 2005,2008,2009 Sony Corporation.
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
 *  Copyright 2005,2008,2009 Sony Corporation.
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
#ifdef CONFIG_SNSC_EM_DEMANGLE
#include "demangle.h"
#endif
#ifdef CONFIG_SNSC_ALT_BACKTRACE
#include <linux/snsc_backtrace.h>
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
 * arch/arm/kernel/trap.c (dump_mem())
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

	em_dump_write_buffer("Exception stack(0x%08lx to 0x%08lx)\n", bottom, top);

	for (first = bottom & ~31; first < top; first += 32) {
		unsigned long p;
		char str[sizeof(" 12345678") * 8 + 1];

		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';

		for (p = first, i = 0; i < 8 && p < top; i++, p += 4) {
			if (p >= bottom && p < top) {
				unsigned long val;
				if (__get_user(val, (unsigned long *)p) == 0)
					sprintf(str + i * 9, " %08lx", val);
				else
					sprintf(str + i * 9, " ????????");
			}
		}
		em_dump_write_buffer("%04lx:%s\n", first & 0xffff, str);
	}

	set_fs(fs);
}

/*
 * This function is from a following file.
 * arch/arm/kernel/trap.c (dump_backtrace_entry())
 */
static void em_dump_backtrace_entry(unsigned long where,
				    unsigned long from, unsigned long frame)
{
	struct pt_regs *regs;
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
		em_dump_write_buffer(
			"\na1: r0: 0x%08lx  a2: r1: 0x%08lx  a3: r2: 0x%08lx  a4: r3: 0x%08lx\n",
			regs->ARM_r0, regs->ARM_r1,
			regs->ARM_r2, regs->ARM_r3);
		em_dump_write_buffer(
			"v1: r4: 0x%08lx  v2: r5: 0x%08lx  v3: r6: 0x%08lx  v4: r7: 0x%08lx\n",
			regs->ARM_r4, regs->ARM_r5,
			regs->ARM_r6, regs->ARM_r7);
		em_dump_write_buffer(
			"v5: r8: 0x%08lx  v6: r9: 0x%08lx  v7:r10: 0x%08lx  fp:r11: 0x%08lx\n",
			regs->ARM_r8, regs->ARM_r9,
			regs->ARM_r10, regs->ARM_fp);
		em_dump_write_buffer(
			"ip:r12: 0x%08lx  sp:r13: 0x%08lx  lr:r14: 0x%08lx  pc:r15: 0x%08lx\n",
			regs->ARM_ip, regs->ARM_sp,
			regs->ARM_lr, regs->ARM_pc);
		em_dump_write_buffer("cpsr:r16: 0x%08lx\n", regs->ARM_cpsr);
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
	p->entry.lr = frame->lr;
	p->entry.pc = frame->pc;

	return p->depth == 0;
}

static void em_initdump_bt(struct pt_regs *_regs)
{
	struct stackframe frame;
	struct stackframe_entry ent;
	register unsigned long current_sp asm ("sp");

	if (!is_initdumping)
		return;

	if (_regs) {
		/* kstack_regs */
		frame.fp = _regs->ARM_fp;
		frame.sp = _regs->ARM_sp;
		frame.lr = _regs->ARM_lr;
		/* PC might be corrupted, use LR in that case. */
		frame.pc = kernel_text_address(_regs->ARM_pc)
			 ? _regs->ARM_pc : _regs->ARM_lr;
	} else {
		/* kstack */
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)em_initdump_bt;
	}

	ent.depth = BT_MAX;

	walk_stackframe(&frame, report_trace, &ent);

	return;
}

static void em_initdump_usr_regs(struct pt_regs *usr_regs)
{
	if (!is_initdumping)
		return;

	em_dump_write_buffer(
		"\na1: r0: 0x%08lx  a2: r1: 0x%08lx  a3: r2: 0x%08lx  a4: r3: 0x%08lx\n",
		usr_regs->ARM_r0, usr_regs->ARM_r1,
		usr_regs->ARM_r2, usr_regs->ARM_r3);
	em_dump_write_buffer(
		"v1: r4: 0x%08lx  v2: r5: 0x%08lx  v3: r6: 0x%08lx  v4: r7: 0x%08lx\n",
		usr_regs->ARM_r4, usr_regs->ARM_r5,
		usr_regs->ARM_r6, usr_regs->ARM_r7);
	em_dump_write_buffer(
		"v5: r8: 0x%08lx  v6: r9: 0x%08lx  v7:r10: 0x%08lx  fp:r11: 0x%08lx\n",
		usr_regs->ARM_r8, usr_regs->ARM_r9,
		usr_regs->ARM_r10, usr_regs->ARM_fp);
	em_dump_write_buffer(
		"ip:r12: 0x%08lx  sp:r13: 0x%08lx  lr:r14: 0x%08lx  pc:r15: 0x%08lx\n",
		usr_regs->ARM_ip, usr_regs->ARM_sp,
		usr_regs->ARM_lr, usr_regs->ARM_pc);
	em_dump_write_buffer("cpsr:r16: 0x%08lx\n", usr_regs->ARM_cpsr);
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
		if (vm->vm_file->f_dentry) {
			if (strcmp
			    (vm->vm_file->f_dentry->d_name.name,
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
		path = d_path(vm->vm_file->f_dentry, vm->vm_file->f_vfsmnt,
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

static long arm_pc_adjustment(void){
	static long ad;
	long tmp1,tmp2;

	if (ad)
		return ad;

	__asm__ __volatile__ (
		"1: stmfd sp!, {pc}\n"
		"   adr %1, 1b\n"
		"   ldmfd sp!, {%2}\n"
		"   sub %0, %2, %1\n"
		: "=r"(ad),"=r"(tmp1),"=r"(tmp2)
		);
	return ad;
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

static inline ulong arch_entry_address(ulong *frame)
{
	ulong val = em_get_user(frame);

	if (val)
		val -= (arm_pc_adjustment() + 4);
	return val;
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
	char mode_list[][4] = {"USR","FIQ","IRQ","SVC","ABT"
			       ,"UND","SYS","???"};

	em_dump_write("\n[register dump]\n");

	em_dump_write(
          "a1: r0: 0x%08x  a2: r1: 0x%08x  a3: r2: 0x%08x  a4: r3: 0x%08x\n",
		      em_regs->ARM_r0, em_regs->ARM_r1,
		      em_regs->ARM_r2, em_regs->ARM_r3);
	em_dump_write(
	  "v1: r4: 0x%08x  v2: r5: 0x%08x  v3: r6: 0x%08x  v4: r7: 0x%08x\n",
		      em_regs->ARM_r4, em_regs->ARM_r5,
		      em_regs->ARM_r6, em_regs->ARM_r7);
	em_dump_write(
	  "v5: r8: 0x%08x  v6: r9: 0x%08x  v7:r10: 0x%08x  fp:r11: 0x%08x\n",
		      em_regs->ARM_r8, em_regs->ARM_r9,
		      em_regs->ARM_r10, em_regs->ARM_fp);
	em_dump_write(
	  "ip:r12: 0x%08x  sp:r13: 0x%08x  lr:r14: 0x%08x  pc:r15: 0x%08x\n",
		      em_regs->ARM_ip, em_regs->ARM_sp,
		      em_regs->ARM_lr, em_regs->ARM_pc);

#define PSR_MODE_MASK 0x0000001f
	switch (em_regs->ARM_cpsr & PSR_MODE_MASK) {
	case USR_MODE: mode = mode_list[0]; break;
	case FIQ_MODE: mode = mode_list[1]; break;
	case IRQ_MODE: mode = mode_list[2]; break;
	case SVC_MODE: mode = mode_list[3]; break;
	case ABT_MODE: mode = mode_list[4]; break;
	case UND_MODE: mode = mode_list[5]; break;
	case SYSTEM_MODE: mode = mode_list[6]; break;
	default: mode = mode_list[7];break;
	}
	em_dump_write("cpsr: 0x%08x: Flags: %c%c%c%c, "
		      "IRQ: o%s, FIQ: o%s, Thumb: o%s, Mode: %s\n",
		      em_regs->ARM_cpsr,
		      (em_regs->ARM_cpsr & PSR_N_BIT) ? 'N' : 'n',
		      (em_regs->ARM_cpsr & PSR_Z_BIT) ? 'Z' : 'z',
		      (em_regs->ARM_cpsr & PSR_C_BIT) ? 'C' : 'c',
		      (em_regs->ARM_cpsr & PSR_V_BIT) ? 'V' : 'v',
		      (em_regs->ARM_cpsr & PSR_I_BIT) ? "ff" : "n",
		      (em_regs->ARM_cpsr & PSR_F_BIT) ? "ff" : "n",
		      (em_regs->ARM_cpsr & PSR_T_BIT) ? "n" : "ff",
		      mode);
}

/* Print the the current VFP state from the provided structures. */
void em_dump_vfp (int argc, char **argv)
{
	struct thread_info *thread = current_thread_info();
	struct vfp_hard_struct *hwstate;

	/* Ensure that the saved hwstate is up-to-date. */
	vfp_sync_hwstate(thread);
	hwstate = &thread->vfpstate.hard;

	em_dump_write("\n[vfp register dump]\n");

	em_dump_write(
		"d0[s0:s1]    0x%016llx     d1[s2:s3]    0x%016llx\n",
			hwstate->fpregs[0], hwstate->fpregs[1]);
	em_dump_write(
		"d2[s4:s5]    0x%016llx     d3[s6:s7]    0x%016llx\n",
			hwstate->fpregs[2], hwstate->fpregs[3]);
	em_dump_write(
		"d4[s8:s9]    0x%016llx     d5[s10:s11]  0x%016llx\n",
			hwstate->fpregs[4], hwstate->fpregs[5]);
	em_dump_write(
		"d6[s12:s13]  0x%016llx     d7[s14:s15]  0x%016llx\n",
			hwstate->fpregs[6], hwstate->fpregs[7]);
	em_dump_write(
		"d8[s16:s17]  0x%016llx     d9[s18:s19]  0x%016llx\n",
			hwstate->fpregs[8], hwstate->fpregs[9]);
	em_dump_write(
		"d10[s20:s21] 0x%016llx     d11[s22:s23] 0x%016llx\n",
			hwstate->fpregs[10], hwstate->fpregs[11]);
	em_dump_write(
		"d12[s24:s25] 0x%016llx     d13[s26:s27] 0x%016llx\n",
			hwstate->fpregs[12], hwstate->fpregs[13]);
	em_dump_write(
		"d14[s28:s29] 0x%016llx     d15[s30:s31] 0x%016llx\n\n",
			hwstate->fpregs[14], hwstate->fpregs[15]);
	em_dump_write(
		"d16: 0x%016llx d17: 0x%016llx  d18: 0x%016llx  d19: 0x%016llx\n",
			hwstate->fpregs[16], hwstate->fpregs[17],
			hwstate->fpregs[18], hwstate->fpregs[19]);
	em_dump_write(
		"d20: 0x%016llx d21: 0x%016llx  d22: 0x%016llx  d23: 0x%016llx\n",
			hwstate->fpregs[20], hwstate->fpregs[21],
			hwstate->fpregs[22], hwstate->fpregs[23]);
	em_dump_write(
		"d24: 0x%016llx d25: 0x%016llx  d26: 0x%016llx  d27: 0x%016llx\n",
			hwstate->fpregs[24], hwstate->fpregs[25],
			hwstate->fpregs[26], hwstate->fpregs[27]);
	em_dump_write(
		"d28: 0x%016llx d29: 0x%016llx  d30: 0x%016llx  d31: 0x%016llx\n\n",
			hwstate->fpregs[28], hwstate->fpregs[29],
			hwstate->fpregs[30], hwstate->fpregs[31]);
	em_dump_write(
		"fpex:%08x fpscr:0x%08x fpint:0x%08x fpint2:0x%08x\n",
			hwstate->fpexc,hwstate->fpscr,
			hwstate->fpinst,hwstate->fpinst2);
#ifdef CONFIG_SMP
	em_dump_write("cpu:%d\n",hwstate->cpu);
#endif
}

void em_dump_regs_detail(int argc, char **argv)
{
	em_dump_regs(1, NULL);
	em_dump_write("\n[cp15 register dump]\n\n");

	/* FIXME: This is ARM926EJ-S specific... */
	{
		unsigned long id, cache, tcm, control, trans,
			dac, d_fsr, i_fsr, far, d_lock, i_lock,
			d_tcm, i_tcm, tlb_lock, fcse, context;
		char size_list[][8] = {
			"  0", "  4", "  8", " 16", " 32", " 64",
			"128", "256", "512", "1024", "???"};
		char *dsiz, *isiz;
		char fault_stat[][32] = {
			"Alignment", "External abort on translation",
			"Translation", "Domain", "Permission",
			"External abort", "???" };
		char *stat;
		char alloc[][8] = {"locked", "opened"};

		asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (id));
		asm volatile ("mrc p15, 0, %0, c0, c0, 1" : "=r" (cache));
		asm volatile ("mrc p15, 0, %0, c0, c0, 2" : "=r" (tcm));
		asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (control));
		asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (trans));
		asm volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r" (dac));
		asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (d_fsr));
		asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r" (i_fsr));
		asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r" (far));
		asm volatile ("mrc p15, 0, %0, c9, c0, 0" : "=r" (d_lock));
		asm volatile ("mrc p15, 0, %0, c9, c0, 1" : "=r" (i_lock));
		asm volatile ("mrc p15, 0, %0, c9, c1, 0" : "=r" (d_tcm));
		asm volatile ("mrc p15, 0, %0, c9, c1, 1" : "=r" (i_tcm));
		asm volatile ("mrc p15, 0, %0, c10, c0, 0" : "=r" (tlb_lock));
		asm volatile ("mrc p15, 0, %0, c13, c0, 0" : "=r" (fcse));
		asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r" (context));

#define MASK_ASCII  0xff000000
#define MASK_SPEC   0x00f00000
#define MASK_ARCH   0x000f0000
#define MASK_PART   0x0000fff0
#define MASK_LAYOUT 0x0000000f
		em_dump_write("* ID code: %08x:  "
			      "tm: %c, spec: %1x, arch: %1x, "
			      "part: %3x, layout: %1x\n",
			      id,
			      ((id & MASK_ASCII)  >> 24),
			      ((id & MASK_SPEC)   >> 20),
			      ((id & MASK_ARCH)   >> 16),
			      ((id & MASK_PART)   >>  4),
			      ((id & MASK_LAYOUT)));

#define MASK_CTYPE 0x1e000000
#define MASK_S_BIT 0x01000000
#define MASK_DSIZE 0x00fff000
#define MASK_ISIZE 0x00000fff
#define MASK_SIZE  0x000003c0
#define MASK_ASSOC 0x00000038
#define MASK_LEN   0x00000003

		switch ((((cache & MASK_DSIZE) >> 12) & MASK_SIZE) >> 6) {
		case 0x3: dsiz = size_list[1]; break;
		case 0x4: dsiz = size_list[2]; break;
		case 0x5: dsiz = size_list[3]; break;
		case 0x6: dsiz = size_list[4]; break;
		case 0x7: dsiz = size_list[5]; break;
		case 0x8: dsiz = size_list[6]; break;
		default:  dsiz = size_list[10]; break;
		}
		switch (((cache & MASK_ISIZE) & MASK_SIZE) >> 6) {
		case 0x3: isiz = size_list[1]; break;
		case 0x4: isiz = size_list[2]; break;
		case 0x5: isiz = size_list[3]; break;
		case 0x6: isiz = size_list[4]; break;
		case 0x7: isiz = size_list[5]; break;
		case 0x8: isiz = size_list[6]; break;
		default:  isiz = size_list[10]; break;
		}
		em_dump_write("* Cache Type: %08x:  %s, %s,\n"
			      "\tDCache: %sKB, %s-way, line: %s word\n"
			      "\tICache: %sKB, %s-way, line: %s word\n",
			      cache,
			      (((cache & MASK_CTYPE) >> 25) == 0xe) ?
			      "write-back" : "write-???",
			      (cache & MASK_S_BIT) ?
			      "harvard" : "unified",
			      dsiz,
			      ((((cache & MASK_DSIZE) >> 12) & MASK_ASSOC >> 3)
			       == 0x2) ? "4" : "?",
			      ((((cache & MASK_DSIZE) >>12 ) & MASK_LEN)
			       == 0x2) ? "8" : "?",
			      isiz,
			      (((cache & MASK_ISIZE) & MASK_ASSOC >> 3)
			       == 0x2) ? "4" : "?",
			      (((cache & MASK_ISIZE) & MASK_LEN)
			       == 0x2) ? "8" : "?");

#define MASK_DTCM 0x00010000
#define MASK_ITCM 0x00000001
		em_dump_write("* TCM Status: %08x: "
			      "DTCM %spresent, ITCM %spresent\n",
			      tcm,
			      (tcm & MASK_DTCM) ? "" : "not ",
			      (tcm & MASK_ITCM) ? "" : "not ");

#define MASK_L4     0x00008000
#define MASK_RR     0x00004000
#define MASK_VEC    0x00002000
#define MASK_ICACHE 0x00001000
#define MASK_ROM    0x00000200
#define MASK_SYS    0x00000100
#define MASK_END    0x00000080
#define MASK_DCACHE 0x00000004
#define MASK_ALIGN  0x00000002
#define MASK_MMU    0x00000001
		em_dump_write("* Control: %08x: L4: %s, Cache: %s replace\n"
			      "\texception vector at %s endian: %s\n"
			      "\tICache %sabled, DCache %sabled, "
			      "Align %sabled, MMU %sabled\n"
			      "\tROM protection: %s, system protection: %s\n",
			      control,
			      (control & MASK_L4) ? "1" : "0",
			      (control & MASK_RR) ? "round robin" : "random",
			      (control & MASK_VEC) ?
			      "ffff00{00-1c}" : "000000{00-1c}",
			      (control & MASK_END) ? "big" : "little",
			      (control & MASK_ICACHE) ? "en" : "dis",
			      (control & MASK_DCACHE) ? "en" : "dis",
			      (control & MASK_ALIGN) ? "en" : "dis",
			      (control & MASK_MMU) ? "en" : "dis",
			      (control & MASK_ROM) ? "1" : "0",
			      (control & MASK_SYS) ? "1" : "0");

		em_dump_write("* Translation Table Base: %08x\n", trans);
		em_dump_write("* Domain Access Control: %08x\n", dac);

#define MASK_DOMAIN 0x000000f0
#define MASK_STATUS 0x0000000f
		switch (d_fsr & MASK_STATUS) {
		case 0x1: case 0x3: stat = fault_stat[0]; break;
		case 0xc: case 0xe: stat = fault_stat[1]; break;
		case 0x5: case 0x7: stat = fault_stat[2]; break;
		case 0x9: case 0xb: stat = fault_stat[3]; break;
		case 0xd: case 0xf: stat = fault_stat[4]; break;
		case 0x8: case 0xa: stat = fault_stat[5]; break;
		default:            stat = fault_stat[6]; break;
		}
		em_dump_write("* Fault Status: data: %08x, inst: %08x\n"
			      "\tat domain: %x, status: %s\n",
			      d_fsr, i_fsr,
			      ((d_fsr & MASK_DOMAIN) >> 4), stat);

		em_dump_write("* Fault Address: %08x\n", far);

#define MASK_WAY3 0x00000008
#define MASK_WAY2 0x00000004
#define MASK_WAY1 0x00000002
#define MASK_WAY0 0x00000001
		em_dump_write("* Cache Lockdown: DCache: %08x, ICache: %08x\n"
			      "\tDCache: way 3: %s, 2: %s, 1: %s, 0: %s\n"
			      "\tICache: way 3: %s, 2: %s, 1: %s, 0: %s\n",
			      d_lock, i_lock,
			      (d_lock & MASK_WAY3) ? alloc[0] : alloc[1],
			      (d_lock & MASK_WAY2) ? alloc[0] : alloc[1],
			      (d_lock & MASK_WAY1) ? alloc[0] : alloc[1],
			      (d_lock & MASK_WAY0) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY3) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY2) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY1) ? alloc[0] : alloc[1],
			      (i_lock & MASK_WAY0) ? alloc[0] : alloc[1]);

#define MASK_BASE   0xfffff000
#undef  MASK_SIZE
#define MASK_SIZE   0x0000003c
#define MASK_ENABLE 0x00000001
		switch ((d_tcm & MASK_SIZE) >> 2) {
		case 0x0: dsiz = size_list[0]; break;
		case 0x3: dsiz = size_list[1]; break;
		case 0x4: dsiz = size_list[2]; break;
		case 0x5: dsiz = size_list[3]; break;
		case 0x6: dsiz = size_list[4]; break;
		case 0x7: dsiz = size_list[5]; break;
		case 0x8: dsiz = size_list[6]; break;
		case 0x9: dsiz = size_list[7]; break;
		case 0xa: dsiz = size_list[8]; break;
		case 0xb: dsiz = size_list[9]; break;
		default:  dsiz = size_list[10]; break;
		}
		switch ((i_tcm & MASK_SIZE) >> 2) {
		case 0x0: isiz = size_list[0]; break;
		case 0x3: isiz = size_list[1]; break;
		case 0x4: isiz = size_list[2]; break;
		case 0x5: isiz = size_list[3]; break;
		case 0x6: isiz = size_list[4]; break;
		case 0x7: isiz = size_list[5]; break;
		case 0x8: isiz = size_list[6]; break;
		case 0x9: isiz = size_list[7]; break;
		case 0xa: isiz = size_list[8]; break;
		case 0xb: isiz = size_list[9]; break;
		default:  isiz = size_list[10]; break;
		}
		em_dump_write("* TCM Region: data: %08x, inst: %08x\n"
			      "\tDTCM: Base addr: %08x, size: %sKB, %sabled\n"
			      "\tITCM: Base addr: %08x, size: %sKB, %sabled\n",
			      d_tcm, i_tcm,
			      ((d_tcm & MASK_BASE) >> 12), dsiz,
			      (d_tcm & MASK_ENABLE) ? "en" : "dis",
			      ((i_tcm & MASK_BASE) >> 12), isiz,
			      (i_tcm & MASK_ENABLE) ? "en" : "dis");

#define MASK_VICT 0x1c000000
#define MASK_PBIT 0x00000001
		em_dump_write("* TLB Lockdown: %08x: "
			      "victim: %x, preserve: %s\n",
			      tlb_lock,
			      ((tlb_lock & MASK_VICT) >> 26),
			      (tlb_lock & MASK_PBIT) ? alloc[0] : alloc[1]);

#define MASK_FCSE 0xfe000000
		em_dump_write("* FCSE PID: %08x: pid: %x\n",
			      fcse, ((fcse & MASK_FCSE) >> 25));

		em_dump_write("* Context ID: %08x\n", context);
	}
	em_dump_write("\n");
}

static void em_dump_till_end_of_page(unsigned long *sp)
{
	unsigned long *tail = sp;
	unsigned long stackdata;
	int i = 0;
	char buf[17];

	tail = (unsigned long *)(((unsigned long)sp + PAGE_SIZE - 1) & PAGE_MASK);

#define MIN_STACK_LEN 2048
	if (((unsigned long)tail - (unsigned long)sp) < MIN_STACK_LEN)
		tail = (unsigned long *)((unsigned long) sp + MIN_STACK_LEN);

	buf[16] = 0;
	while (sp < tail) {

		if ((i % 4) == 0) {
			em_dump_write("%08x : ", (unsigned long)sp);
		}
		if (__get_user(stackdata, sp++)) {
			em_dump_write(" (Bad stack address)\n");
			break;
		}
		em_dump_write(" 0x%08x", (unsigned long)stackdata);
		buf[(i % 4) * 4]     = em_convert_char(stackdata);
		buf[(i % 4) * 4 + 1] = em_convert_char(stackdata >> 8);
		buf[(i % 4) * 4 + 2] = em_convert_char(stackdata >> 16);
		buf[(i % 4) * 4 + 3] = em_convert_char(stackdata >> 24);

		if ((i % 4) == 3) {
			em_dump_write(" : %s\n", buf);
		}

		++i;
	}
}

void em_dump_stack(int argc, char **argv)
{
	unsigned long *sp = (unsigned long *)(em_regs->ARM_sp & ~0x03);
	unsigned long *fp = (unsigned long *)(em_regs->ARM_fp & ~0x03);
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
	/*
	 * The following are the standard ARMv3 and ARMv4 aborts.  ARMv5
	 * defines these to be "precise" aborts.
	 */
	{ "vector exception"		   },
	{ "alignment exception"		   },
	{ "terminal exception"		   },
	{ "alignment exception"		   },
	{ "external abort on linefetch"	   },
	{ "section translation fault"	   },
	{ "external abort on linefetch"	   },
	{ "page translation fault"	   },
	{ "external abort on non-linefetch" },
	{ "section domain fault"		   },
	{ "external abort on non-linefetch" },
	{ "page domain fault"		   },
	{ "external abort on translation"   },
	{ "section permission fault"	   },
	{ "external abort on translation"   },
	{ "page permission fault"	   },
	/*
	 * The following are "imprecise" aborts, which are signalled by bit
	 * 10 of the FSR, and may not be recoverable.  These are only
	 * supported if the CPU abort handler supports bit 10.
	 */
	{ "unknown 16"			   },
	{ "unknown 17"			   },
	{ "unknown 18"			   },
	{ "unknown 19"			   },
	{ "lock abort"			   }, /* xscale */
	{ "unknown 21"			   },
	{ "imprecise external abort"	   }, /* xscale */
	{ "unknown 23"			   },
	{ "dcache parity error"		   }, /* xscale */
	{ "unknown 25"			   },
	{ "unknown 26"			   },
	{ "unknown 27"			   },
	{ "unknown 28"			   },
	{ "unknown 29"			   },
	{ "unknown 30"			   },
	{ "unknown 31"			   }
};

void em_show_syndrome(void)
{
	unsigned long fsr,far;
	const struct fsr_info *inf;
	struct task_struct *tsk = current;

	em_dump_write("\n\n[Exception Syndrome]\n");

	switch(tsk->thread.trap_no){
	case 6:
		em_dump_write("Illegal Instruction at 0x%08lx\n",
			      em_regs->ARM_pc);

		break;
	case 14:
	default:
		fsr = tsk->thread.error_code;
		far = tsk->thread.address;
		inf = fsr_info + (fsr & 15) + ((fsr & (1 << 10)) >> 6);

		em_dump_write("%s (0x%03x) at 0x%08lx\n",
			      inf->name, fsr, far);

		break;
	}
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
	em_dump_write("[0x%08lx] ", cbarg->ba_addr);
	if (bt_status_is_error(cbarg->ba_status)) {
		em_dump_write("stop backtracing: %s\n", cbarg->ba_str);

		/* if PC is invalid/zero, unwind/disassemble may tried with the valid LR.
		   CPSR bit need to be adjusted as per the LR register
		   if return addr is thumb2, last bit of LR is always 1
		   So set the T bit in cpsr regsiter. */
		if (copy_addr == 0){
			if(em_regs->ARM_lr & 1)
				em_regs->ARM_cpsr |= PSR_T_BIT;
			else
				em_regs->ARM_cpsr &= ~PSR_T_BIT;
		}
		return 0;
	}

	/* copying the func start addr and pc, needed during disassebly
	   disassembly need func start address as a starting point to avoid arm/thumb2 confusion */
	if (copy_addr == 0) {
		unsigned int insn_size = 4;
		if(em_regs->ARM_cpsr & PSR_T_BIT)
			insn_size = 2;
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
		em_dump_write("0x%08lx", cbarg->ba_sym_start);
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

	printk(
		"\na1: r0: 0x%08lx  a2: r1: 0x%08lx  a3: r2: 0x%08lx  a4: r3: 0x%08lx\n",
		usr_regs->ARM_r0, usr_regs->ARM_r1,
		usr_regs->ARM_r2, usr_regs->ARM_r3);
	printk(
		"v1: r4: 0x%08lx  v2: r5: 0x%08lx  v3: r6: 0x%08lx  v4: r7: 0x%08lx\n",
		usr_regs->ARM_r4, usr_regs->ARM_r5,
		usr_regs->ARM_r6, usr_regs->ARM_r7);
	printk(
		"v5: r8: 0x%08lx  v6: r9: 0x%08lx  v7:r10: 0x%08lx  fp:r11: 0x%08lx\n",
		usr_regs->ARM_r8, usr_regs->ARM_r9,
		usr_regs->ARM_r10, usr_regs->ARM_fp);
	printk(
		"ip:r12: 0x%08lx  sp:r13: 0x%08lx  lr:r14: 0x%08lx  pc:r15: 0x%08lx\n",
		usr_regs->ARM_ip, usr_regs->ARM_sp,
		usr_regs->ARM_lr, usr_regs->ARM_pc);
	printk("cpsr:r16: 0x%08lx\n", usr_regs->ARM_cpsr);

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
	em_dump_write("  pc: %p (%pF), lr %p (%pF), sp %p, fp %p\n",
				frame->pc, frame->pc, frame->lr, frame->lr,
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
		frame.fp = regs->ARM_fp;
		frame.sp = regs->ARM_sp;
		frame.lr = regs->ARM_lr;
		/* PC might be corrupted, use LR in that case. */
		frame.pc = kernel_text_address(regs->ARM_pc)
			? regs->ARM_pc : regs->ARM_lr;
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
