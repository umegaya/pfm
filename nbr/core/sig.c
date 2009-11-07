/*
 *	signal_handler.c:	extend signal handlers
 *
 *	$Revision: 1 $
 *	$Date: 07/11/29 20:57 $
 *	$Id: $
 */


#include "sig.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CMD_PSTACK	"/usr/bin/pstack"

static signal_handler_t s_signal_handler, s_old_signal_handler;

/* common functions */
static void _write_log(const char *buf)
{
	if (s_signal_handler.func_loghandler) {
		s_signal_handler.func_loghandler(buf);
	}
	else {
		fprintf(stderr, "%s\n", buf);
	}
}

static void _write_signal_log(int signum, __sighandler_t sighandler)
{
	char buf[32] = "Caught SIG";

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

	strcat(buf, " ...");
	if (sighandler == SIG_IGN) {
		strcat(buf, " ignored.");
	}
	_write_log(buf);
}

static void _write_stack_log()
{
	if (access(CMD_PSTACK, X_OK) == 0) {
		char 			buf[256];
		FILE			*fp;
		int				drop_flag = 1;
		__sighandler_t	sh_chld;

		sprintf(buf, CMD_PSTACK " %d", (int)getpid());
		/* undefine LD_PRELOAD to avoid 64-bit problems */
		(void)putenv("LD_PRELOAD=");

		sh_chld = signal(SIGCHLD, SIG_IGN);
		if ((fp = popen(buf, "r")) != NULL) {
			char *ptr;
			while (1) {
				fgets(buf, sizeof(buf), fp);
				if (feof(fp)) {
					break;
				}
				if (drop_flag) {
					char cmd[8];
					sscanf(buf, "%*p: %6s", cmd);
					if (strcmp(cmd, "killpg") == 0) {
						drop_flag = 0;
					}
					continue;
				}
				ptr = strchr(buf, '\n');
				if (ptr != NULL) {
					*ptr = '\0';
				}
				_write_log(buf);
			}
			pclose(fp);
		}
		signal(SIGCHLD, sh_chld);
	}
}


/* signal handlers */

static void _sig_ign_handler(int signum)
{
	_write_signal_log(signum, SIG_IGN);
	signal(signum, _sig_ign_handler);	/* ignore */
}

static void _terminate_handler(int signum)
{
	if (s_signal_handler.func[signum]) {
		_write_signal_log(signum, SIG_IGN);
		s_signal_handler.func[signum](signum);
		signal(signum, _terminate_handler);	/* ignore */
	}
	else {
		_write_signal_log(signum, s_old_signal_handler.func[signum]);
		signal(signum, s_old_signal_handler.func[signum]);
		raise(signum);	/* default */
	}
}

static void _ignore_handler(int signum)
{
	_write_signal_log(signum, SIG_IGN);
	if (s_signal_handler.func[signum]) {
		s_signal_handler.func[signum](signum);
	}
	signal(signum, _ignore_handler);	/* ignore */
}

static void _dump_core_handler(int signum)
{
	_write_signal_log(signum, s_old_signal_handler.func[signum]);
	_write_stack_log();
	if (s_signal_handler.func[signum]) {
		s_signal_handler.func[signum](signum);
	}
	signal(signum, s_old_signal_handler.func[signum]);
	raise(signum);	/* default */
}

static void _stop_handler(int signum)
{
	_write_signal_log(signum, s_old_signal_handler.func[signum]);
	if (s_signal_handler.func[signum]) {
		s_signal_handler.func[signum](signum);
	}
	signal(signum, s_old_signal_handler.func[signum]);
	raise(signum);	/* default */
}

int overlay_signal_handler(signal_handler_t* sht)
{
	int signum;

	memcpy(&s_signal_handler, sht, sizeof(signal_handler_t));
	for (signum = 1; signum < 32; signum++) {
		if (signum == SIGKILL || signum == SIGSTOP) {
			/* cannot catch */
			continue;
		}
		if (sht->func[signum] == SIG_IGN) {
			s_old_signal_handler.func[signum] = signal(signum, _sig_ign_handler);
		}
		else {
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
				s_old_signal_handler.func[signum] = signal(signum, _terminate_handler);
				break;

			  case SIGCHLD:
			  case SIGCONT:
			  case SIGURG:
			  case SIGWINCH:
				/* ignore the signal */
				s_old_signal_handler.func[signum] = signal(signum, _ignore_handler);
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
				s_old_signal_handler.func[signum] = signal(signum, _dump_core_handler);
				break;

			/*case SIGSTOP:*/
			  case SIGTSTP:
			  case SIGTTIN:
			  case SIGTTOU:
				/* stop the process */
				s_old_signal_handler.func[signum] = signal(signum, _stop_handler);
				break;
			}
		}
	}

	return 0;
}
