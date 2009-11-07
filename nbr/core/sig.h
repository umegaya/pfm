/*
 *	signal_handler.h:	extend signal handlers
 *
 *	$Revision: 1 $
 *	$Date: 07/11/29 20:57 $
 *	$Id: $
 */


#ifndef _SIGNAL_HANDLER_H_
#define _SIGNAL_HANDLER_H_

#include <signal.h>
#include <string.h>

/* type define of log handler */
typedef void (*__loghandler_t)(const char *);

/* */
#define	BEGIN_SIGNAL_HANDLER_MAP()		\
		{ signal_handler_t _sht; bzero(&_sht, sizeof(signal_handler_t));

#define	SIGNAL_HANDLER(_sig,_func)		\
		_sht.func[(_sig)] =		(__sighandler_t)(_func);

#define LOG_HANDLER(_func)				\
		_sht.func_loghandler =	(__loghandler_t)(_func);

#define	TERMINATE_SIGNAL_HANDLER(_func)	\
		_sht.func[SIGHUP] =		\
		_sht.func[SIGINT] =		\
		_sht.func[SIGPIPE] =	\
		_sht.func[SIGALRM] =	\
		_sht.func[SIGTERM] =	\
		_sht.func[SIGUSR1] =	\
		_sht.func[SIGUSR2] =	\
		_sht.func[SIGPROF] =	\
		_sht.func[SIGVTALRM] =	\
		_sht.func[SIGSTKFLT] =	\
		_sht.func[SIGIO] =		\
		_sht.func[SIGPWR] =		\
		_sht.func[SIGUNUSED] =	(__sighandler_t)(_func);

#define	IGNORE_SIGNAL_HANDLER(_func)	\
		_sht.func[SIGCHLD] =	\
		_sht.func[SIGCONT] =	\
		_sht.func[SIGURG] =		\
		_sht.func[SIGWINCH] =	(__sighandler_t)(_func);

#define	DUMP_CORE_SIGNAL_HANDLER(_func)	\
		_sht.func[SIGQUIT] =	\
		_sht.func[SIGILL] =		\
		_sht.func[SIGABRT] =	\
		_sht.func[SIGBUS] =		\
		_sht.func[SIGFPE] =		\
		_sht.func[SIGSEGV] =	\
		_sht.func[SIGTRAP] =	\
		_sht.func[SIGXCPU] =	\
		_sht.func[SIGXFSZ] =	(__sighandler_t)(_func);

#define	STOP_SIGNAL_HANDLER(_func)		\
		_sht.func[SIGTSTP] =	\
		_sht.func[SIGTTIN] =	\
		_sht.func[SIGTTOU] =	(__sighandler_t)(_func);

#define	END_SIGNAL_HANDLER_MAP()		\
		overlay_signal_handler(&_sht); }


/* type define and function */

typedef struct _signal_handler_t {
	union {
		__sighandler_t	func[32];
		__loghandler_t	func_loghandler;		/* func[0] is log handler */
	};
} signal_handler_t;

extern int overlay_signal_handler(signal_handler_t* sht);

#endif	/* _SIGNAL_HANDLER_H_ */
