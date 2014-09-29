/** \ingroup rpmio
 * \file rpmio/rpmsq.c
 */

#include "system.h"

#include <signal.h>
#if !defined(__QNX__)
#  include <sys/signal.h>
#endif
#include <sys/wait.h>
#include <search.h>

#include <rpmiotypes.h>

/* portability fallback for sighold(3) */
#if !defined(HAVE_SIGHOLD) && defined(HAVE_SIGPROCMASK) && defined(HAVE_SIGADDSET)
static int __RPM_sighold(int sig)
{
    sigset_t set;
    if (sigprocmask(SIG_SETMASK, NULL, &set) < 0)
        return -1;
    if (sigaddset(&set, sig) < 0)
        return -1;
    return sigprocmask(SIG_SETMASK, &set, NULL);
}
#define sighold(sig) __RPM_sighold(sig)
#endif

/* portability fallback for sigrelse(3) */
#if !defined(HAVE_SIGRELSE) && defined(HAVE_SIGPROCMASK) && defined(HAVE_SIGDELSET)
static int __RPM_sigrelse(int sig)
{
    sigset_t set;
    if (sigprocmask(SIG_SETMASK, NULL, &set) < 0)
        return -1;
    if (sigdelset(&set, sig) < 0)
        return -1;
    return sigprocmask(SIG_SETMASK, &set, NULL);
}
#define sigrelse(sig) __RPM_sigrelse(sig)
#endif

/* portability fallback for sigpause(3) */
#if !defined(HAVE_SIGPAUSE) && defined(HAVE_SIGEMPTYSET) && defined(HAVE_SIGADDSET) && defined(HAVE_SIGSUSPEND)
static int __RPM_sigpause(int sig)
{
    sigset_t set;
    if (sigemptyset(&set) < 0)
        return -1;
    if (sigaddset(&set, sig) < 0)
        return -1;
    return sigsuspend(&set);
}
#define sigpause(sig) __RPM_sigpause(sig)
#endif

#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__QNX__)
struct qelem {
    struct qelem *q_forw;
    struct qelem *q_back;
    char q_data[1];
};
#endif

static void __insque(struct qelem * elem, struct qelem * pred)
{
  RPM_GNUC_TM_ATOMIC {
    elem -> q_forw = pred -> q_forw;
    pred -> q_forw -> q_back = elem;
    elem -> q_back = pred;
    pred -> q_forw = elem;
  }
}

static void __remque(struct qelem * elem)
{
  RPM_GNUC_TM_ATOMIC {
    elem -> q_forw -> q_back = elem -> q_back;
    elem -> q_back -> q_forw = elem -> q_forw;
  }
}

#if defined(WITH_PTHREADS)

# if !defined(__QNX__)
/* XXX suggested in bugzilla #159024 */
#  if PTHREAD_MUTEX_DEFAULT != PTHREAD_MUTEX_NORMAL
#    error RPM expects PTHREAD_MUTEX_DEFAULT == PTHREAD_MUTEX_NORMAL
#  endif
# endif

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
static pthread_mutex_t rpmsigTbl_lock = PTHREAD_MUTEX_INITIALIZER;
#else
static pthread_mutex_t rpmsigTbl_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

#define	DO_LOCK()	pthread_mutex_lock(&rpmsigTbl_lock)
#define	DO_UNLOCK()	pthread_mutex_unlock(&rpmsigTbl_lock);
#define	INIT_LOCK()	\
    {	pthread_mutexattr_t attr; \
	(void) pthread_mutexattr_init(&attr); \
	(void) pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); \
	(void) pthread_mutex_init (&rpmsigTbl_lock, &attr); \
	(void) pthread_mutexattr_destroy(&attr); \
	rpmsigTbl_sigchld->active = 0; \
    }
#define	ADD_REF(__tbl)	(__tbl)->active++
#define	SUB_REF(__tbl)	--(__tbl)->active
#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr) \
    (void) pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, (__oldtypeptr));\
	pthread_cleanup_push((__handler), (__arg));
#define	CLEANUP_RESET(__execute, __oldtype) \
    pthread_cleanup_pop(__execute); \
    (void) pthread_setcanceltype ((__oldtype), &(__oldtype));

#define	SAME_THREAD(_a, _b)	pthread_equal(((pthread_t)_a), ((pthread_t)_b))

#define ME() __tid2vp(pthread_self())
static void *__tid2vp(pthread_t tid)
{
    union { pthread_t tid; void *vp; } u;
    u.tid = tid;
    return u.vp;
}

#else	/* WITH_PTHREADS */

#define	DO_LOCK() (0)
#define	DO_UNLOCK() (0)
#define	INIT_LOCK()
#define	ADD_REF(__tbl)	(0)
#define	SUB_REF(__tbl)	(0)
#define	CLEANUP_HANDLER(__handler, __arg, __oldtypeptr)
#define	CLEANUP_RESET(__execute, __oldtype)

#define	SAME_THREAD(_a, _b)	(42)

#define ME() __pid2vp(getpid())
static void *__pid2vp(pid_t pid)
{
    union { pid_t pid; void *vp; } u;
    u.pid = pid;
    return u.vp;
}

#endif	/* WITH_PTHREADS */

#define	_RPMSQ_INTERNAL
#include <rpmsq.h>

#include "debug.h"

int _rpmsq_debug;
#define	SPEW(_list)	if (_rpmsq_debug) fprintf _list

/* XXX __OpenBSD__ insque(3) needs rock->q_forw initialized. */
static struct rpmsqElem rpmsqRock = { &rpmsqRock, NULL };

rpmsq rpmsqQueue = &rpmsqRock;

int rpmsqInsert(void * elem, void * prev)
{
    rpmsq sq = (rpmsq) elem;
    int ret = -1;

    if (sq != NULL) {
SPEW((stderr, "    Insert(%p): %p\n", ME(), sq));
	ret = sighold(SIGCHLD);
	if (ret == 0) {
	    sq->child = 0;
	    sq->reaped = 0;
	    sq->status = 0;
	    /* ==> Set to 1 to catch SIGCHLD, set to 0 to use waitpid(2).  */
	    sq->reaper = 1;
	    sq->pipes[0] = sq->pipes[1] = -1;

	    sq->id = ME();
	    __insque(elem, (prev != NULL ? prev : rpmsqQueue));
	  }
	    ret = sigrelse(SIGCHLD);
    }
    return ret;
}

int rpmsqRemove(void * elem)
{
    rpmsq sq = (rpmsq) elem;
    int ret = -1;

    if (elem != NULL) {

if (_rpmsq_debug)
SPEW((stderr, "    Remove(%p): %p\n", ME(), sq));
	ret = sighold (SIGCHLD);
	if (ret == 0) {
	    __remque(elem);
	    sq->id = NULL;
	    if (sq->pipes[1] > 0)	ret = close(sq->pipes[1]);
	    if (sq->pipes[0] > 0)	ret = close(sq->pipes[0]);
	    sq->pipes[0] = sq->pipes[1] = -1;
#ifdef	NOTYET	/* rpmpsmWait debugging message needs */
	    sq->status = 0;
	    sq->reaped = 0;
	    sq->child = 0;
#endif
	    ret = sigrelse(SIGCHLD);
	}
    }
    return ret;
}

sigset_t rpmsqCaught;

static struct rpmsig_s {
    int signum;
    void (*handler) (int signum, void * info, void * context);
    int active;
    struct sigaction oact;
} rpmsigTbl[] = {
    { SIGINT,	rpmsqAction },
#define	rpmsigTbl_sigint	(&rpmsigTbl[0])
    { SIGQUIT,	rpmsqAction },
#define	rpmsigTbl_sigquit	(&rpmsigTbl[1])
    { SIGCHLD,	rpmsqAction },
#define	rpmsigTbl_sigchld	(&rpmsigTbl[2])
    { SIGHUP,	rpmsqAction },
#define	rpmsigTbl_sighup	(&rpmsigTbl[3])		/* XXX unused */
    { SIGTERM,	rpmsqAction },
#define	rpmsigTbl_sigterm	(&rpmsigTbl[4])		/* XXX unused */
    { SIGPIPE,	rpmsqAction },
#define	rpmsigTbl_sigpipe	(&rpmsigTbl[5])		/* XXX unused */

#ifdef	NOTYET		/* XXX todo++ */
#if defined(SIGXCPU)
    { SIGXCPU,	rpmsqAction },
#define	rpmsigTbl_sigxcpu	(&rpmsigTbl[6])		/* XXX unused */
#endif
#if defined(SIGXFSZ)
    { SIGXFSZ,	rpmsqAction },
#define	rpmsigTbl_sigxfsz	(&rpmsigTbl[7])		/* XXX unused */
#endif
#endif	/* NOTYET */

    { -1,	NULL },
};

int rpmsqIsCaught(int signum)
{
    return sigismember(&rpmsqCaught, signum);
}

void rpmsqAction(int signum, void * info, void * context)
{
    int save = errno;
    rpmsig tbl;

    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tbl->signum != signum)
	    continue;

	(void) sigaddset(&rpmsqCaught, signum);

	switch (signum) {
	case SIGCHLD:
	    while (1) {
		rpmsq sq;
		int status = 0;
		pid_t reaped = waitpid(0, &status, WNOHANG);

		/* XXX errno set to ECHILD/EINVAL/EINTR. */
		if (reaped <= 0)
		    break;

		for (sq = rpmsqQueue->q_forw;
		     sq != NULL && sq != rpmsqQueue;
		     sq = sq->q_forw)
		{
		    int ret;

		    if (sq->child != reaped)
			continue;
		    sq->reaped = reaped;
		    sq->status = status;

		    ret = close(sq->pipes[1]);	sq->pipes[1] = -1;

		    break;
		}
	    }
	    break;
	default:
	    break;
	}
	break;
    }
    errno = save;
}

int rpmsqEnable(int signum, rpmsqAction_t handler)
{
    int tblsignum = (signum >= 0 ? signum : -signum);
    struct sigaction sa;
    rpmsig tbl;
    int ret = (signum >= 0 ? 1 : 0);
    int xx;

    xx = DO_LOCK ();
    if (rpmsqQueue->id == NULL)
	rpmsqQueue->id = ME();
    for (tbl = rpmsigTbl; tbl->signum >= 0; tbl++) {
	if (tblsignum != tbl->signum)
	    continue;

	if (signum >= 0) {			/* Enable. */
	    if (ADD_REF(tbl) <= 0) {
		(void) sigdelset(&rpmsqCaught, tbl->signum);

		/* XXX Don't set a signal handler if already SIG_IGN */
		(void) sigaction(tbl->signum, NULL, &tbl->oact);
		if (tbl->oact.sa_handler == SIG_IGN)
		    continue;

		(void) sigemptyset (&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
#if defined(__LCLINT__)	/* XXX glibc has union to track handler prototype. */
		sa.sa_handler = (handler != NULL ? handler : tbl->handler);
#else
		sa.sa_sigaction = (void (*)(int, siginfo_t*, void*)) (handler != NULL ? handler : tbl->handler);
#endif
		if (sigaction(tbl->signum, &sa, &tbl->oact) < 0) {
		    xx = SUB_REF(tbl);
		    break;
		}
		tbl->active = 1;		/* XXX just in case */
		if (handler != NULL)
		    tbl->handler = handler;
	    }
	} else {				/* Disable. */
	    if (SUB_REF(tbl) <= 0) {
		if (sigaction(tbl->signum, &tbl->oact, NULL) < 0)
		    break;
		tbl->active = 0;		/* XXX just in case */
		tbl->handler = (handler != NULL ? handler : rpmsqAction);
	    }
	}
	ret = tbl->active;
	break;
    }
    xx = DO_UNLOCK ();
    return ret;
}

pid_t rpmsqFork(rpmsq sq)
{
    pid_t pid;
    int xx;

    if (sq->reaper) {
	xx = rpmsqInsert(sq, NULL);
SPEW((stderr, "    Enable(%p): %p\n", ME(), sq));
	xx = rpmsqEnable(SIGCHLD, NULL);
    }

    xx = pipe(sq->pipes);

    xx = sighold(SIGCHLD);

    pid = fork();
    if (pid < (pid_t) 0) {		/* fork failed.  */
	sq->child = (pid_t)-1;
	xx = close(sq->pipes[0]);
	xx = close(sq->pipes[1]);
	sq->pipes[0] = sq->pipes[1] = -1;
	goto out;
    } else if (pid == (pid_t) 0) {	/* Child. */
	int yy;

	/* Block to permit parent time to wait. */
	xx = close(sq->pipes[1]);
	if (sq->reaper)
	    xx = (int)read(sq->pipes[0], &yy, sizeof(yy));
	xx = close(sq->pipes[0]);
	sq->pipes[0] = sq->pipes[1] = -1;

SPEW((stderr, "     Child(%p): %p child %d\n", ME(), sq, (int)getpid()));

    } else {				/* Parent. */

	sq->child = pid;

SPEW((stderr, "    Parent(%p): %p child %d\n", ME(), sq, (int)sq->child));

    }

out:
    xx = sigrelse(SIGCHLD);
    return sq->child;
}

/**
 * Wait for child process to be reaped, and unregister SIGCHLD handler.
 * @todo Rewrite to use waitpid on helper thread.
 * @param sq		scriptlet queue element
 * @return		0 on success
 */
static int rpmsqWaitUnregister(rpmsq sq)
{
    int nothreads = 0;
    int ret = 0;
    int xx;

assert(sq->reaper);
    /* Protect sq->reaped from handler changes. */
    ret = sighold(SIGCHLD);

    /* Start the child, linux often runs child before parent. */
    if (sq->pipes[0] >= 0)
	xx = close(sq->pipes[0]);
    if (sq->pipes[1] >= 0)
	xx = close(sq->pipes[1]);

    /* Re-initialize the pipe to receive SIGCHLD receipt confirmation. */
    xx = pipe(sq->pipes);

    /* Put a stopwatch on the time spent waiting to measure performance gain. */
    (void) rpmswEnter(&sq->op, -1);

    /* Wait for handler to receive SIGCHLD. */
    while (ret == 0 && sq->reaped != sq->child) {
	if (nothreads)
	    /* Note that sigpause re-enables SIGCHLD. */
	    ret = sigpause(SIGCHLD);
	else {
	    xx = sigrelse(SIGCHLD);
	    
	    /* Signal handler does close(sq->pipes[1]) triggering 0b EOF read */
	    if (read(sq->pipes[0], &xx, sizeof(xx)) == 0) {
		xx = close(sq->pipes[0]);	sq->pipes[0] = -1;
		ret = 1;
	    }

	    xx = sighold(SIGCHLD);
	}
    }

    /* Accumulate stopwatch time spent waiting, potential performance gain. */
    sq->ms_scriptlets += rpmswExit(&sq->op, -1)/1000;

    xx = sigrelse(SIGCHLD);

SPEW((stderr, "      Wake(%p): %p child %d reaper %d ret %d\n", ME(), sq, (int)sq->child, sq->reaper, ret));

    /* Remove processed SIGCHLD item from queue. */
    xx = rpmsqRemove(sq);

    /* Disable SIGCHLD handler on refcount == 0. */
    xx = rpmsqEnable(-SIGCHLD, NULL);
SPEW((stderr, "   Disable(%p): %p\n", ME(), sq));

    return ret;
}

pid_t rpmsqWait(rpmsq sq)
{

SPEW((stderr, "      Wait(%p): %p child %d reaper %d\n", ME(), sq, (int)sq->child, sq->reaper));

    if (sq->reaper) {
	(void) rpmsqWaitUnregister(sq);
    } else {
	pid_t reaped;
	int status;
	do {
	    reaped = waitpid(sq->child, &status, 0);
	} while (reaped >= 0 && reaped != sq->child);
	sq->reaped = reaped;
	sq->status = status;
SPEW((stderr, "   Waitpid(%p): %p child %d reaped %d\n", ME(), sq, (int)sq->child, (int)sq->reaped));
    }

SPEW((stderr, "      Fini(%p): %p child %d status 0x%x\n", ME(), sq, (int)sq->child, sq->status));

    return sq->reaped;
}

void * rpmsqThread(void * (*start) (void * arg), void * arg)
{
#if defined(WITH_PTHREADS)
    pthread_t pth;
    int ret;

    ret = pthread_create(&pth, NULL, start, arg);
    return (ret == 0 ? (void *)pth : NULL);
#else
    (void) start;
    (void) arg; 
    return NULL;
#endif
}

int rpmsqJoin(void * thread)
{
#if defined(WITH_PTHREADS)
    pthread_t pth = (pthread_t) thread;
    if (thread == NULL)
	return EINVAL;
    return pthread_join(pth, NULL);
#else
    (void) thread;
    return EINVAL;
#endif
}

int rpmsqThreadEqual(void * thread)
{
#if defined(WITH_PTHREADS)
    pthread_t t1 = (pthread_t) thread;
    pthread_t t2 = pthread_self();
    return pthread_equal(t1, t2);
#else
    (void) thread;
    return 0;
#endif
}

/**
 * SIGCHLD cancellation handler.
 */
#if defined(WITH_PTHREADS)
static void
sigchld_cancel (void *arg)
{
    pid_t child = *(pid_t *) arg;
    pid_t result;
    int xx;

    xx = kill(child, SIGKILL);

    do {
	result = waitpid(child, NULL, 0);
    } while (result == (pid_t)-1 && errno == EINTR);

    xx = DO_LOCK ();
    if (SUB_REF (rpmsigTbl_sigchld) == 0) {
	xx = rpmsqEnable(-SIGQUIT, NULL);
	xx = rpmsqEnable(-SIGINT, NULL);
    }
    xx = DO_UNLOCK ();
}
#endif

/**
 * Execute a command, returning its status.
 */
int
rpmsqExecve (const char ** argv)
{
#if defined(WITH_PTHREADS)
    int oldtype;
#endif
    int status = -1;
    pid_t pid = 0;
    pid_t result;
    sigset_t newMask, oldMask;
    rpmsq sq = (rpmsq) memset(alloca(sizeof(*sq)), 0, sizeof(*sq));
    int xx;

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
    INIT_LOCK ();
#endif

    xx = DO_LOCK ();
    if (ADD_REF (rpmsigTbl_sigchld) == 0) {
	if (rpmsqEnable(SIGINT, NULL) < 0) {
	    xx = SUB_REF (rpmsigTbl_sigchld);
	    goto out;
	}
	if (rpmsqEnable(SIGQUIT, NULL) < 0) {
	    xx = SUB_REF (rpmsigTbl_sigchld);
	    goto out_restore_sigint;
	}
    }
    xx = DO_UNLOCK ();

    (void) sigemptyset (&newMask);
    (void) sigaddset (&newMask, SIGCHLD);
    if (sigprocmask (SIG_BLOCK, &newMask, &oldMask) < 0) {
	xx = DO_LOCK ();
	if (SUB_REF (rpmsigTbl_sigchld) == 0)
	    goto out_restore_sigquit_and_sigint;
	goto out;
    }

    CLEANUP_HANDLER(sigchld_cancel, &pid, &oldtype);

    pid = fork ();
    if (pid < (pid_t) 0) {		/* fork failed.  */
	goto out;
    } else if (pid == (pid_t) 0) {	/* Child. */

	/* Restore the signals.  */
	(void) sigaction (SIGINT, &rpmsigTbl_sigint->oact, NULL);
	(void) sigaction (SIGQUIT, &rpmsigTbl_sigquit->oact, NULL);
	(void) sigprocmask (SIG_SETMASK, &oldMask, NULL);

	/* Reset rpmsigTbl lock and refcnt. */
	INIT_LOCK ();

	(void) execve (argv[0], (char *const *) argv, environ);
	_exit (127);
    } else {				/* Parent. */
	do {
	    result = waitpid(pid, &status, 0);
	} while (result == (pid_t)-1 && errno == EINTR);
	if (result != pid)
	    status = -1;
    }

    CLEANUP_RESET(0, oldtype);

    xx = DO_LOCK ();
    if ((SUB_REF (rpmsigTbl_sigchld) == 0 &&
        (rpmsqEnable(-SIGINT, NULL) < 0 || rpmsqEnable (-SIGQUIT, NULL) < 0))
      || sigprocmask (SIG_SETMASK, &oldMask, NULL) != 0)
    {
	status = -1;
    }
    goto out;

out_restore_sigquit_and_sigint:
    xx = rpmsqEnable(-SIGQUIT, NULL);
out_restore_sigint:
    xx = rpmsqEnable(-SIGINT, NULL);
out:
    xx = DO_UNLOCK ();
    return status;
}
