/***************************************************************
 * sig.c : handling signal stuff (linux os only?)
 * 2009/11/07 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#include "common.h"
#include "sig.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/
#define PSTACK	"/usr/bin/pstack"



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct sigfunctable {
	union {
		SIGFUNC	func[32];
		void (*logger)(const char *);		/* func[0] is log handler */
	};
} sigfunctable_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
static sigfunctable_t g_now, g_old;



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
/* common functions */
NBR_INLINE
void sig_write_log(const char *buf)
{
	if (g_now.logger) {
		g_now.logger(buf);
	}
	else {
		fprintf(stderr, "%s\n", buf);
	}
}

NBR_INLINE void
sig_write_signal_log(int signum, SIGFUNC fn)
{
	char buf[32] = "[SIG";

	switch (signum) {
	  case SIGHUP:		strcat(buf, "HUP");		break;
	  case SIGINT:		strcat(buf, "INT");		break;
	  case SIGQUIT:		strcat(buf, "QUIT");	break;
	  case SIGILL:		strcat(buf, "ILL");		break;
	  case SIGTRAP:		strcat(buf, "TRAP");	break;
	  case SIGABRT:		strcat(buf, "ABRT");	break;
	  case SIGBUS:		strcat(buf, "BUS");		break;
	  case SIGFPE:		strcat(buf, "FPE");		break;
	  case SIGKILL:		strcat(buf, "KILL");	break;
	  case SIGUSR1:		strcat(buf, "USR1");	break;
	  case SIGSEGV:		strcat(buf, "SEGV");	break;
	  case SIGUSR2:		strcat(buf, "USR2");	break;
	  case SIGPIPE:		strcat(buf, "PIPE");	break;
	  case SIGALRM:		strcat(buf, "ALRM");	break;
	  case SIGTERM:		strcat(buf, "TERM");	break;
	  case SIGSTKFLT:	strcat(buf, "STKFLT");	break;
	  case SIGCHLD:		strcat(buf, "CHLD");	break;
	  case SIGCONT:		strcat(buf, "CONT");	break;
	  case SIGSTOP:		strcat(buf, "STOP");	break;
	  case SIGTSTP:		strcat(buf, "TSTP");	break;
	  case SIGTTIN:		strcat(buf, "TTIN");	break;
	  case SIGTTOU:		strcat(buf, "TTOU");	break;
	  case SIGURG:		strcat(buf, "URG");		break;
	  case SIGXCPU:		strcat(buf, "XCPU");	break;
	  case SIGXFSZ:		strcat(buf, "XFSZ");	break;
	  case SIGVTALRM:	strcat(buf, "VTALRM");	break;
	  case SIGPROF:		strcat(buf, "PROF");	break;
	  case SIGWINCH:	strcat(buf, "WINCH");	break;
	  case SIGIO:		strcat(buf, "IO");		break;
	/*case SIGLOST:		strcat(buf, "LOST");	break;*/
	  case SIGPWR:		strcat(buf, "PWR");		break;
	  case SIGUNUSED:	strcat(buf, "UNUSED");	break;
	}

	strcat(buf, "]...");
	if (fn == SIG_IGN) {
		strcat(buf, "ignore");
	}
	sig_write_log(buf);
}

NBR_INLINE void
sig_write_stack_log()
{
	char 	buf[256], *ptr;
	FILE	*fp;
	SIGFUNC	sh_chld;
	if (access(PSTACK, X_OK) == 0) {
		sprintf(buf, PSTACK" %u", getpid());
		(void)putenv("LD_PRELOAD=");

		sh_chld = signal(SIGCHLD, SIG_IGN);
		if ((fp = popen(buf, "r"))) {
			while (!fgets(buf, sizeof(buf), fp)) {
				if ((ptr = strchr(buf, '\n'))) {
					*ptr = '\0';
				}
				sig_write_log(buf);
			}
			pclose(fp);
		}
		signal(SIGCHLD, sh_chld);
	}
}


/* SIGNAL HANDLERS */
static void
sig_ignore_handler(int signum)
{
	sig_write_signal_log(signum, SIG_IGN);
}

static void
sig_term_handler(int signum)
{
	if (g_now.func[signum]) {
		sig_write_signal_log(signum, g_now.func[signum]);
		g_now.func[signum](signum);
	}
	else {
		sig_write_signal_log(signum, g_old.func[signum]);
		signal(signum, g_old.func[signum]);
		raise(signum);	/* default */
	}
}

static void
sig_fault_handler(int signum)
{
	sig_write_signal_log(signum, g_old.func[signum]);
	sig_write_stack_log();
	if (g_now.func[signum]) {
		g_now.func[signum](signum);
	}
	signal(signum, g_old.func[signum]);
	raise(signum);	/* default */
}

static void
sig_stop_handler(int signum)
{
	sig_write_signal_log(signum, g_now.func[signum]);
	if (g_now.func[signum]) {
		g_now.func[signum](signum);
	}
	signal(signum, g_old.func[signum]);
	raise(signum);	/* default */
}



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int nbr_sig_init()
{
	int signum;
	memset(&g_old, 0, sizeof(sigfunctable_t));
	memset(&g_now, 0, sizeof(sigfunctable_t));
	for (signum = 1; signum < 32; signum++) {
		if (signum == SIGKILL || signum == SIGSTOP) {
			/* cannot catch */
			continue;
		}
		switch (signum) {
		case SIGHUP:
		case SIGINT:
		/*case SIGKILL:*/
		case SIGPIPE:
		case SIGALRM:
		case SIGTERM:
		case SIGUSR1:
		case SIGUSR2:
		case SIGPROF:
		case SIGVTALRM:
		case SIGSTKFLT:
		case SIGIO:
		case SIGPWR:
		/*case SIGLOST:*/
		case SIGUNUSED:
		/* terminate the process */
			g_old.func[signum] = signal(signum, sig_term_handler);
			break;

		case SIGCHLD:
		case SIGCONT:
		case SIGURG:
		case SIGWINCH:
			/* ignore the signal */
			g_old.func[signum] = signal(signum, sig_ignore_handler);
			break;

		case SIGQUIT:
		case SIGILL:
		case SIGABRT:
		case SIGBUS:
		case SIGFPE:
		case SIGSEGV:
		case SIGTRAP:
		case SIGXCPU:
		case SIGXFSZ:
			/* dump core */
			g_old.func[signum] = signal(signum, sig_fault_handler);
			break;

		/*case SIGSTOP:*/
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			/* stop the process */
			g_old.func[signum] = signal(signum, sig_stop_handler);
			break;
		}
	}
	/* for default, we redirect signal log to err.c */
	nbr_sig_set_logger(nbr_err_sig);
	/* for default, I want to ignore SIGPIPE and SIGHUP... */
	nbr_sig_set_handler(SIGPIPE, SIG_IGN);
	nbr_sig_set_handler(SIGHUP, SIG_IGN);
	return 0;
}

NBR_API void
nbr_sig_set_logger(void (*logger)(const char*))
{
	g_now.logger = logger;
}

NBR_API int
nbr_sig_set_handler(int signum, SIGFUNC fn)
{
	if (signum <= 32) {
		g_now.func[signum] = fn;
		return NBR_OK;
	}
	return NBR_EINVAL;
}

NBR_API void
nbr_sig_set_ignore_handler(SIGFUNC fn)
{
	g_now.func[SIGCHLD] =
	g_now.func[SIGCONT] =
	g_now.func[SIGURG] =
	g_now.func[SIGWINCH] =	fn;
}

NBR_API void
nbr_sig_set_intr_handler(SIGFUNC fn)
{
	g_now.func[SIGHUP] =
	g_now.func[SIGINT] =
	g_now.func[SIGPIPE] =
	g_now.func[SIGALRM] =
	g_now.func[SIGTERM] =
	g_now.func[SIGUSR1] =
	g_now.func[SIGUSR2] =
	g_now.func[SIGPROF] =
	g_now.func[SIGVTALRM] =
	g_now.func[SIGSTKFLT] =
	g_now.func[SIGIO] =
	g_now.func[SIGPWR] =
	g_now.func[SIGUNUSED] =	fn;
}

NBR_API void
nbr_sig_set_fault_handler(SIGFUNC fn)
{
	g_now.func[SIGQUIT] =
	g_now.func[SIGILL] =
	g_now.func[SIGABRT] =
	g_now.func[SIGBUS] =
	g_now.func[SIGFPE] =
	g_now.func[SIGSEGV] =
	g_now.func[SIGTRAP] =
	g_now.func[SIGXCPU] =
	g_now.func[SIGXFSZ] =	fn;
}

NBR_API void
nbr_sig_set_stop_handler(SIGFUNC fn)
{
	g_now.func[SIGTSTP] =
	g_now.func[SIGTTIN] =
	g_now.func[SIGTTOU] =	fn;
}



