/*
 * Copyright 2013-16 Board of Trustees of Stanford University
 * Copyright 2013-16 Ecole Polytechnique Federale Lausanne (EPFL)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


/*
 * syscallmap.c - posix table-driven parameter checking and tracing
 *
 * table based system call interposition
 * replaces functionality from apps/sandbox/trap.c that handles system call interposition
 *
 * scm_ = syscallmap_
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <asm/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/times.h>

#include <dune.h>

#include "syscallmap.h"

#define MAX_SYSCALL 425  // allow "new" system calls for libOS

static struct {
	const char *name;
	const char *arglist;
	const char *checklist;
	int fixedlen;
} scmtable[MAX_SYSCALL];


/**
 * syscallmap_trace
 *
 * this code assumes V=P.
 */
void syscallmap_trace(int syscall_nr, struct dune_tf *tf, pid_t tid)
{
	assert(syscall_nr >= 0 && syscall_nr < MAX_SYSCALL);
	if (scmtable[syscall_nr].name) {
		// TODO: make this conditional
		char fmt[256];
		sprintf(fmt, "%04d-SYSCALL %%lx <- %s(%s) rip=%lx\n", tid, scmtable[syscall_nr].name, scmtable[syscall_nr].arglist, tf->rip);
		//log_info(fmt, tf->rax, ARG0(tf), ARG1(tf), ARG2(tf), ARG3(tf), ARG4(tf));
	} else {
		//log_info("SYSCALL unknown %d\n", syscall_nr);
	}
}

static uint64_t *scm_arg(struct dune_tf *tf, char c)
{

	switch (c) {
	case '0':
		return &ARG0(tf);
	case '1':
		return &ARG1(tf);
	case '2':
		return &ARG2(tf);
	case '3':
		return &ARG3(tf);
	case '4':
		return &ARG4(tf);
	case '5':
		return &ARG5(tf);
	default:
		panic("scm_arg - bad argument %d\n", c);
	}
	return (uint64_t *)0;
}



/**
 * syscallmap_checkparams
 *
 *
 */

int
syscallmap_checkparams(struct dune_tf *tf,
		       syscallmap_checkmem_cb cb)
{
	int nr = tf->rax;
	assert(nr >= 0 && nr < MAX_SYSCALL);
	uintptr_t *addr_ptr;
	uintptr_t len;
	int extra = 0;
	int ret = 0;


	if (scmtable[nr].name) {

		//log_info("syscallmap_checkparams %s\n",scmtable[nr].name);

	} else {

		//log_err("[syscallmap] not implemented %d\n", nr);
		panic("syscallmap_checkparams\n");
	}


	const char *check = scmtable[nr].checklist;
	if (check == NULL)
		return 0;

	while (check[0]) {
		switch (check[0]) {
		case 'S':  /* string */
			addr_ptr = scm_arg(tf, check[1]);
			if (*addr_ptr)
				ret = cb(tf, addr_ptr, 0);
			check += 2;
			break;
		case 'P': /* pointer/len */
			addr_ptr = scm_arg(tf, check[1]);
			len = *scm_arg(tf, check[2]);
			if (*addr_ptr)
				ret = cb(tf, addr_ptr, len);
			check += 3;
			break;
		case 'I': /* IOV - s/g */
			//log_err("[syscall_map] Indirect S/G vector for sys %s (NOT IMPLEMENTED)\n", scmtable[nr].name);
			addr_ptr = scm_arg(tf, check[1]);
			len = *scm_arg(tf, check[2]);
			// first check the IOV itself
			if (*addr_ptr)
				ret = cb(tf, addr_ptr, len);
			// then read the IOV vector (not implemnented)
			// then check the arguments of the IOV (not implemented)
			panic("not implemented - IOV SG\n");
			check += 3;
			break;
		case 'F': /* fixed sized pointer */
			addr_ptr = scm_arg(tf, check[1]);
			len = scmtable[nr].fixedlen;
			if (*addr_ptr)
				ret = cb(tf, addr_ptr, len);
			check += 2;
			break;
		case 'X':
			//log_err("[syscallmap] %s SPECIAL UNPARSED\n", scmtable[nr].name);
			extra = 1;
			check += 1;
			break;
		default:
			panic("syscallmap: unknonwn case\n");

		}
		if (ret)
			return ret;

	}
	if (extra) {
		switch (nr) {
		case SYS_arch_prctl:
			if (ARG0(tf) == ARCH_GET_FS)
				ret = cb(tf, scm_arg(tf, '1'), sizeof(unsigned long));
			break;
		default:
			// implement extra special system calls here
			//log_err("unhandled extra condition for syscall %d\n", nr);
			panic("bad");
			;
		}
	}
	return ret;

}


#define ASSERT_IS_ARG(_x) assert((_x)>='0' && (_x)<='5')

static void scm_defsignature(int nr, const char *name,
			     const char *arglist,
			     const char *checklist, int fixedlen)
{
	assert(scmtable[nr].name == NULL);
	assert(scmtable[nr].arglist == NULL);
	assert(nr >= 0 && nr < MAX_SYSCALL);
	scmtable[nr].name = name;
	scmtable[nr].arglist = arglist;
	scmtable[nr].checklist = checklist;
	scmtable[nr].fixedlen = fixedlen;
	/* checklist regular expression:
	 *   { ( Sd | Pdd | Idd ) }*
	 * Sd = null-terminated string at argument d
	 * Pdd = pointer at address d with len d
	 * Idd = iovec at address d with len d
	 * Fd = fixed-sized structure at addr d (if non-zero)
	 * X = requires special processing
	 */


	if (checklist) {
		const char *s = checklist;
		while (*s) {
			switch (*s) {
			case 'S':
				ASSERT_IS_ARG(s[1]);
				s += 2;
				break;
			case 'P':
			case 'I':
				ASSERT_IS_ARG(s[1]);
				ASSERT_IS_ARG(s[2]);
				s += 3;
				break;
			case 'F':
				ASSERT_IS_ARG(s[1]);
				assert(fixedlen > 0);
				s += 2;
				break;
			case 'X':
				s++;
				break;
			default:
				panic("syscallmap - bad option\n");
			}
		}
	}
}

void syscallmap_init(void)
{
	//log_info("Initializing system call table\n");

	/* 0 */ scm_defsignature(SYS_read, "read", "fd=%d,buf=0x%lx,cnt=%ld", "P12", 0);
	/* 1 */ scm_defsignature(SYS_write, "write", "fd=%d,buf=0x%lx,cnt=%ld", "P12", 0);
	/* 2 */ scm_defsignature(SYS_open, "open", "f=\"%s\",flags=0x%lx", "S0", 0);
	/* 3 */ scm_defsignature(SYS_close, "close", "fd=%d", NULL, 0);
	/* 4 */ scm_defsignature(SYS_stat, "stat", "pathname=%s buf=%lx", "S0F1", sizeof(struct stat));
	/* 5 */ scm_defsignature(SYS_fstat, "fstat", "f=%d,buf=0x%lx", "F1", sizeof(struct stat));

	/* 9 */ scm_defsignature(SYS_mmap, "mmap", "addr=0x%lx,len=0x%lx,prot=0x%lx,flags=0x%lx,fd=%d,off=0x%lx", NULL, 0);
	/* 10 */ scm_defsignature(SYS_mprotect, "mprotect", "addr=0x%lx,len=0x%lx,prot=0x%lx", NULL, 0);
	/* 11 */ scm_defsignature(SYS_munmap, "munmap", "addr=0x%lx, len=0x%lx", NULL, 0);
	/* 12 */ scm_defsignature(SYS_brk, "brk", "brk=0x%lx", NULL, 0);
	/* 13 */ scm_defsignature(SYS_rt_sigaction, "rt_sigaction", "signr=%d,ptr=0x%lx,old=0x%lx\n",
				  "F1F2", sizeof(struct sigaction));

	/* 20 */ scm_defsignature(SYS_rt_sigprocmask , "rt_sigprocmask", "how=%d set=0x%lx oldset=0x%lx", "F1F2", sizeof(sigset_t));

	int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

	/* 20 */ scm_defsignature(SYS_writev, "writev", "f=%d,sg=0x%lx,len=0x%lx", "X", 0);

	/* 21 */ scm_defsignature(SYS_access, "access", "f=\"%s\",mode=0x%lx", "S0", 0);

	/* 28 */ scm_defsignature(SYS_madvise, "madvise", "addr=%lx len=%lx advice=%lx", NULL, 0);

	/* 39 */ scm_defsignature(SYS_getpid, "getpid", "", NULL, 0);

	/* 42 */ scm_defsignature(SYS_connect, "connect", "sockfd=%d,addr=%lx,len=%d", "P12", 0);

	/* 49 */ scm_defsignature(SYS_bind, "bind", "sockfd=%d,addr=%lx,len=%d", "P12", 0);

	/* 56 */ scm_defsignature(SYS_clone, "clone", "fn=%0xlx stack=0x%lx flag=%lx args=%lx", "", 0);

	/* 63 */ scm_defsignature(SYS_uname, "uname", "buf=%lx", "F0", sizeof(struct utsname));

	/* 72 */ scm_defsignature(SYS_fcntl, "fcntl", "fd=%d cmd=%d", NULL, 0);


	/* 78 */ scm_defsignature(SYS_getdents, "getdents", "fd=%d,buf=0x%lx,sz=%d", "P12", 0);
	/* 79 */ scm_defsignature(SYS_getcwd, "getcwd", "buf=\"%s\",len=%d", "P01", 0);

	/* 83 */ scm_defsignature(SYS_mkdir, "mkdir", "pathame=%s mode=%d", "S0", 0);
	/* 85 */ scm_defsignature(SYS_creat, "creat", "pathname=\"%s\",mode=%x", "S0", 0);
	/* 86 */ scm_defsignature(SYS_link, "link", "old=\"%s\",new=\"%s\"", "S0S1", 0);
	/* 87 */ scm_defsignature(SYS_unlink, "unlink", "path=\"%s\"", "S0", 0);

	/* 97 */  scm_defsignature(SYS_getrlimit, "getrlimit", "resource=%d,rlimit=%0lx", "F1", sizeof(struct rlimit));

	/* 100 */ scm_defsignature(SYS_times, "times", "buf=0x%lx", "F0", sizeof(struct tms));


	/* 158 */ scm_defsignature(SYS_arch_prctl, "arch_prctl", "code=%d addr=%lx", "X", 0);

	/* 202 */ scm_defsignature(SYS_futex, "futex", "INCOMPLETE uaddr=0x%lx,op=%d,val=%d,timeout=0x%lx,uaddr2=0x%lx,val3=%d",
				   "", 0);

	/* 204 */ scm_defsignature(SYS_sched_getaffinity, "sched_getaffinity", "pid=%d,cpusetsize=%d mask=0x%lx\n",
				   "F2", sizeof(cpu_set_t));


	/* 218 */ scm_defsignature(SYS_set_tid_address, "set_tid_address", "tidptr=0x%lx", "F0", sizeof(int));

	/* 228 */ scm_defsignature(SYS_clock_gettime, "clock_gettime", "clk_id=%d tp=0x%lx", "F1", sizeof(struct timespec));

	/* 231 */ scm_defsignature(SYS_exit_group, "exit_group", "status=%d", NULL, 0);

	/* 257 */ scm_defsignature(SYS_openat, "openat", "dirfd=%d,path=\"%s\",flags=0x%x,mode=0x%x", "S1", 0);

	/* 273 */ scm_defsignature(SYS_set_robust_list, "set_robust_list", "UNKNOWN", "", 0);

	/* 400 */ scm_defsignature(400, "backos_init", "strategy=%d", NULL, 0);
	/* 401 */ scm_defsignature(401, "backos_guess", "n=%d", NULL, 0);
}