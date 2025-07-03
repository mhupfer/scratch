#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum {
   QNX_SIGHUP = 1,
   QNX_SIGINT,
   QNX_SIGQUIT,
   QNX_SIGILL,
   QNX_SIGTRAP,
   QNX_SIGABRT,
   QNX_SIGEMT,
   QNX_SIGFPE,
   QNX_SIGKILL,
   QNX_SIGBUS,
   QNX_SIGSEGV,
   QNX_SIGSYS,
   QNX_SIGPIPE,
   QNX_SIGALRM,
   QNX_SIGTERM,
   QNX_SIGUSR1,
   QNX_SIGUSR2,
   QNX_SIGCHLD,
   QNX_SIGPWR,
   QNX_SIGWINCH,
   QNX_SIGURG,
   QNX_SIGPOLL,
   QNX_SIGSTOP,
   QNX_SIGTSTP,
   QNX_SIGCONT,
   QNX_SIGTTIN,
   QNX_SIGTTOU,
   QNX_SIGVTALRM,
   QNX_SIGPROF,
   QNX_SIGXCPU,
   QNX_SIGXFSZ
};
#define SIG(x) #x
char *tags[] = {
   SIG(SIGHUP),
   SIG(SIGINT),
   SIG(SIGQUIT),
   SIG(SIGILL),
   SIG(SIGTRAP),
   SIG(SIGABRT),
   SIG(SIGEMT),
   SIG(SIGFPE),
   SIG(SIGKILL),
   SIG(SIGBUS),
   SIG(SIGSEGV),
   SIG(SIGSYS),
   SIG(SIGPIPE),
   SIG(SIGALRM),
   SIG(SIGTERM),
   SIG(SIGUSR1),
   SIG(SIGUSR2),
   SIG(SIGCHLD),
   SIG(SIGPWR),
   SIG(SIGWINCH),
   SIG(SIGURG),
   SIG(SIGPOLL),
   SIG(SIGSTOP),
   SIG(SIGTSTP),
   SIG(SIGCONT),
   SIG(SIGTTIN),
   SIG(SIGTTOU),
   SIG(SIGVTALRM),
   SIG(SIGPROF),
   SIG(SIGXCPU),
   SIG(SIGXFSZ)
};

int decode(int sig, int code, int flt) {
    char *ssig = tags[sig-1];
	char *scode = "<Unknown/machine specific>";
    char *sflt = "unknown fault";

	switch (sig) {
	case QNX_SIGILL:
		switch (code) {
		case 1:  scode = "Illegal Opcode"; break;
		case 2:  scode = "Illegal Operand"; break;
		case 3:  scode = "Illegal addressing mode"; break;
		case 4:  scode = "Illegal Trap"; break;
		case 5:  scode = "Privileged Opcode"; break;
		case 6:  scode = "Privileged Register"; break;
		case 7:  scode = "Coprocessor Error"; break;
		case 8:  scode = "Bad Stack"; break;
		}
		break;
	case QNX_SIGSEGV:
		switch (code) {
		case 1: scode = "Address Not Mapped"; break;
		case 2: scode = "No Permissions"; break;
		case 3: scode = "Stack Exception"; break;
		case 4: scode = "General Protection"; break;
		case 5: scode = "Interrupt Handler Fault"; break;
		}
		break;
	case QNX_SIGBUS:
		switch (code) {
		case 1: scode = "Address Not Aligned"; break;
		case 2: scode = "Invalid Phys Addr"; break;
		case 3: scode = "Object Specificy hardware Error"; break;
		case 4: scode = "Bad Page"; break;
		case 5: scode = "End Object"; break;
		case 6: scode = "Other Translation Fault"; break;
		case 7: scode = "No Such Entry"; break;
		case 8: scode = "Transient Fault"; break;
		case 9: scode = "Out of Memory"; break;
		case 10: scode = "Error Fault"; break;
		case 11: scode = "Error Invalid"; break;
		case 12: scode = "Error Access"; break;
		case 13: scode = "Bad File Desc"; break;
		case 50: scode = "Obscure Server Error"; break;
		}
		break;
	case QNX_SIGCHLD:
		switch (code) {
        case 1: scode = "Child exitted"; break;
        case 2: scode = "Child killed"; break;
        case 3: scode = "Child Abend"; break;
        case 4: scode = "Traced Child Stop"; break;
        case 5: scode = "Child Stopped"; break;
        case 6: scode = "Stopped Child Continued"; break;
		}
		break;
	case QNX_SIGTRAP:
		switch (code) {
        case 1: scode = "Break Point"; break;
        case 2: scode = "Trace"; break;
        case 3: scode = "Kdebug BreakPoint"; break;
        case 4: scode = "Crash"; break;
		}
		break;
    case QNX_SIGFPE:
		switch (code) {
		case 1: scode = "Integer divide by zero"; break;
		case 2: scode = "Integer overflow"; break;
		case 3: scode = "Float divide by zero"; break;
		case 4: scode = "Float overflow"; break;
		case 5: scode = "Float underflow"; break;
		case 6: scode = "Fload inexact"; break;
		case 7: scode = "Invalid Float operation"; break;
		case 8: scode = "Subscript out of range"; break;
		case 9: scode = "No Float support"; break;
		case 10: scode = "No space for FPU"; break;
		}
		break;
    default:
        scode = "No signal code";
        break;
    }
	switch (flt) {
    case 1:  sflt = "Illegal Instruction"; break;
    case 2:  sflt = "Privileged Instruction"; break;
    case 3:  sflt = "Breakpoint Instruction"; break;
    case 4:  sflt = "Trace trap"; break;
    case 5:  sflt = "Memory Access"; break;
    case 6:  sflt = "Memory Bounds"; break;
    case 7:  sflt = "Integer overflow"; break;
    case 8:  sflt = "Integer zero divide"; break;
    case 9:  sflt = "Float exception"; break;
    case 10:  sflt = "Irrecoverable Stack Fault"; break;
    case 11:  sflt = "Recoverable Page Fault"; break;
	default:
#define _FIRST_CPU_FAULT 32
		switch (flt) {
		case _FIRST_CPU_FAULT + 0: sflt = "NMI(x86)/NoFPU(arm)"; break;
		case _FIRST_CPU_FAULT + 1: sflt = "SysError(Double Fault)"; break;
		case _FIRST_CPU_FAULT + 2: sflt = "SysError(Old Float)"; break;
		case _FIRST_CPU_FAULT + 3: sflt = "SysError(Bad TSS)"; break;
		case _FIRST_CPU_FAULT + 4: sflt = "SysError(Bad Seg)"; break;
		case _FIRST_CPU_FAULT + 5: sflt = "SysError(MissingCause)"; break;
		case _FIRST_CPU_FAULT + 6: sflt = "SysError(IntelReserved)"; break;
		case _FIRST_CPU_FAULT + 7: sflt = "No Floatin Point Support"; break;
		case _FIRST_CPU_FAULT + 8: sflt = "NMachine Check"; break;
        }
        break;
    }
    printf("%s / %s / %s\n", ssig, scode, sflt);
}

void Show(char *a, char *b, char *c) {
	decode(strtol(a, 0, 0), strtol(b, 0, 0), strtol(c, 0, 0));
}

int main(int ac, char **av) {
	switch (ac) {
    default:
		fprintf(stderr, "usage: %s [s/c/f| s c f]\n", av[0]);
		exit(1);
	case 2: {
        char *a, *b, *c;
		a = av[1];
		b = strchr(av[1], '/')+1;
		c = strchr(b, '/')+1;
		Show(a, b, c);
	} break;
	case 4:
		Show(av[1], av[2], av[3]);
		break;
	}
	return 0;
}
