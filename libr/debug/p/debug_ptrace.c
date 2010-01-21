/* radare - LGPL - Copyright 2009 pancake<nopcode.org> */

#include <r_userconf.h>
#include <r_debug.h>
#include <r_asm.h>
#include <r_reg.h>
#include <r_lib.h>

#if __linux__
#include <sys/user.h>
#include <limits.h>
#endif

#if __WINDOWS__
struct r_debug_handle_t r_debug_plugin_ptrace = {
	.name = "ptrace",
};
#else

#if DEBUGGER

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

static int r_debug_ptrace_step(int pid)
{
	int ret;
	ut32 addr = 0; /* should be eip */
	//ut32 data = 0;
	//printf("NATIVE STEP over PID=%d\n", pid);
	ret = ptrace (PTRACE_SINGLESTEP, pid, addr, 0); //addr, data);
	if (ret == -1)
		perror("ptrace-singlestep");
	return R_TRUE;
}

static int r_debug_ptrace_attach(int pid)
{
	void *addr = 0;
	void *data = 0;
	int ret = ptrace (PTRACE_ATTACH, pid, addr, data);
	return (ret != -1)?R_TRUE:R_FALSE;
}

static int r_debug_ptrace_detach(int pid)
{
	void *addr = 0;
	void *data = 0;
	return ptrace (PTRACE_DETACH, pid, addr, data);
}

static int r_debug_ptrace_continue(int pid, int sig)
{
	void *addr = NULL; // eip for BSD
	void *data = NULL;
	if (sig != -1)
		data = (void*)(size_t)sig;
	return ptrace (PTRACE_CONT, pid, addr, data);
}

static int r_debug_ptrace_wait(int pid)
{
	int ret, status = -1;
	//printf("prewait\n");
	ret = waitpid(pid, &status, 0);
	//printf("status=%d (return=%d)\n", status, ret);
	return status;
}

// TODO: why strdup here?
static const char *r_debug_ptrace_reg_profile()
{
	return strdup(
	"gpr	eip	.32	48	0\n"
	"gpr	ip	.16	48	0\n"
	"gpr	oeax	.32	44	0\n"
	"gpr	eax	.32	24	0\n"
	"gpr	ax	.16	24	0\n"
	"gpr	ah	.8	24	0\n"
	"gpr	al	.8	25	0\n"
	"gpr	ebx	.32	0	0\n"
	"gpr	bx	.16	0	0\n"
	"gpr	bh	.8	0	0\n"
	"gpr	bl	.8	1	0\n"
	"gpr	ecx	.32	4	0\n"
	"gpr	cx	.16	4	0\n"
	"gpr	ch	.8	4	0\n"
	"gpr	cl	.8	5	0\n"
	"gpr	edx	.32	8	0\n"
	"gpr	dx	.16	8	0\n"
	"gpr	dh	.8	8	0\n"
	"gpr	dl	.8	9	0\n"
	"gpr	esp	.32	60	0\n"
	"gpr	sp	.16	60	0\n"
	"gpr	ebp	.32	20	0\n"
	"gpr	bp	.16	20	0\n"
	"gpr	esi	.32	12	0\n"
	"gpr	si	.16	12	0\n"
	"gpr	edi	.32	16	0\n"
	"gpr	di	.16	16	0\n"
	"seg	xfs	.32	36	0\n"
	"seg	xgs	.32	40	0\n"
	"seg	xcs	.32	52	0\n"
	"seg	cs	.16	52	0\n"
	"seg	xss	.32	52	0\n"
	"gpr	eflags	.32	56	0\n"
	"gpr	flags	.16	56	0\n"
	"\n"
	"# base address is 448bit\n"
	"flg	carry	.1	.448	0\n"
	"flg	flag_p	.1	.449	0\n"
	"flg	flag_a	.1	.450	0\n"
	"flg	zero	.1	.451	0\n"
	"flg	sign	.1	.452	0\n"
	"flg	flag_t	.1	.453	0\n"
	"flg	flag_i	.1	.454	0\n"
	"flg	flag_d	.1	.455	0\n"
	"flg	flag_o	.1	.456	0\n"
	"flg	flag_r	.1	.457	0\n"
	);
}

// TODO: what about float and hardware regs here ???
// TODO: add flag for type
static int r_debug_ptrace_reg_read(struct r_debug_t *dbg, int type, ut8 *buf, int size)
{
	int ret; 
	int pid = dbg->pid;
	if (type == R_REG_TYPE_GPR) {
// XXX this must be defined somewhere else
#if __linux__ && (__i386__ || __x86_64__)
		struct user_regs_struct regs;
		memset(&regs, 0, sizeof(regs));
		memset(buf, 0, size);
		ret = ptrace(PTRACE_GETREGS, pid, 0, &regs);
		if (sizeof(regs) < size)
			size = sizeof(regs);
		if (ret != 0)
			return R_FALSE;
		memcpy(buf, &regs, size);
		return sizeof(regs);
		//r_reg_set_bytes(reg, &regs, sizeof(struct user_regs));
#else
#warning dbg-ptrace not supported for this platform
	return 0;
#endif
	}

	return 0;
}

static int r_debug_ptrace_reg_write(int pid, int type, const ut8* buf, int size) {
	int ret;
	// XXX use switch or so
printf("reg_write\n");
	if (type == R_REG_TYPE_GPR) {
#if __linux__ && (__i386__ || __x86_64__)
printf("reg_write_real\n");
		ret = ptrace(PTRACE_SETREGS, pid, 0, buf);
		if (sizeof(struct user_regs_struct) < size)
			size = sizeof(struct user_regs_struct);
		if (ret != 0)
			return R_FALSE;
		return R_TRUE;
#else
		#warning r_debug_ptrace_reg_write not implemented
#endif
	} else
printf("reg_write_non-gpr (%d)\n", type);
	return R_FALSE;
}

// TODO: deprecate???
#if 0
static int r_debug_ptrace_bp_write(int pid, ut64 addr, int size, int hw, int rwx) {
	if (hw) {
		/* implement DRx register handling here */
		return R_TRUE;
	}
	return R_FALSE;
}

/* TODO: rethink */
static int r_debug_ptrace_bp_read(int pid, ut64 addr, int hw, int rwx)
{
	return R_TRUE;
}
#endif

static int r_debug_get_arch()
{
	return R_ASM_ARCH_X86;
#if 0
#if __WORDSIZE == 64
	return R_ASM_ARCH_X86_64;
#else
	return R_ASM_ARCH_X86;
#endif
#endif
}

#if 0
static int r_debug_ptrace_import(struct r_debug_handle_t *from)
{
	//int pid = from->export(R_DEBUG_GET_PID);
	//int maps = from->export(R_DEBUG_GET_MAPS);
	return R_FALSE;
}
#endif
#if __WORDSIZE == 64
const char *archlist[3] = { "x86", "x86-32", "x86-64", 0 };
#else
const char *archlist[3] = { "x86", "x86-32", 0 };
#endif

struct r_debug_handle_t r_debug_plugin_ptrace = {
	.name = "ptrace",
	.archs = (const char **)archlist,
	.step = &r_debug_ptrace_step,
	.cont = &r_debug_ptrace_continue,
	.attach = &r_debug_ptrace_attach,
	.detach = &r_debug_ptrace_detach,
	.wait = &r_debug_ptrace_wait,
	.get_arch = &r_debug_get_arch,
	//.bp_write = &r_debug_ptrace_bp_write,
	.reg_profile = (void *)&r_debug_ptrace_reg_profile,
	.reg_read = &r_debug_ptrace_reg_read,
	.reg_write = (void *)&r_debug_ptrace_reg_write,
	//.bp_read = &r_debug_ptrace_bp_read,
};

#endif
#endif

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_DBG,
	.data = &r_debug_plugin_ptrace
};
#endif