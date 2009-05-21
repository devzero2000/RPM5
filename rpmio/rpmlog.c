/** \ingroup rpmio
 * \file rpmio/rpmlog.c
 */

#include "system.h"
#include <stdarg.h>
#define	_RPMLOG_INTERNAL
#include <rpmlog.h>
#include "debug.h"

/*@access rpmlogRec @*/

/*@unchecked@*/
static int nrecs = 0;
/*@unchecked@*/
static /*@only@*/ /*@null@*/ rpmlogRec recs = NULL;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p) /*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

int rpmlogGetNrecs(void)
{
    return nrecs;
}

int rpmlogCode(void)
{
    if (recs != NULL && nrecs > 0)
	return recs[nrecs-1].code;
    return -1;
}

const char * rpmlogMessage(void)
{
    if (recs != NULL && nrecs > 0)
	return recs[nrecs-1].message;
    return _("(no error)");
}

const char * rpmlogRecMessage(rpmlogRec rec)
{
    assert(rec != NULL);
    return (rec->message);
}

rpmlogLvl rpmlogRecPriority(rpmlogRec rec)
{
    assert(rec != NULL);
    return (rec->pri);
}

/*@-modfilesys@*/
void rpmlogPrint(FILE *f)
{
    int i;

    if (f == NULL)
	f = stderr;

    if (recs)
    for (i = 0; i < nrecs; i++) {
	rpmlogRec rec = recs + i;
	if (rec->message && *rec->message)
	    fprintf(f, "    %s", rec->message);
    }
}
/*@=modfilesys@*/

void rpmlogClose (void)
	/*@globals recs, nrecs @*/
	/*@modifies recs, nrecs @*/
{
    int i;

    if (recs)
    for (i = 0; i < nrecs; i++) {
	rpmlogRec rec = recs + i;
	rec->message = _free(rec->message);
    }
    recs = _free(recs);
    nrecs = 0;
}

void rpmlogOpen (/*@unused@*/ const char *ident,
		/*@unused@*/ int option,
		/*@unused@*/ int facility)
{
}

/*@unchecked@*/
static unsigned rpmlogMask = RPMLOG_UPTO( RPMLOG_NOTICE );

#if 0
/*@unchecked@*/
static /*@unused@*/ unsigned rpmlogFacility = RPMLOG_USER;
#endif

int rpmlogSetMask (int mask)
	/*@globals rpmlogMask @*/
	/*@modifies rpmlogMask @*/
{
    int omask = rpmlogMask;
    if (mask)
        rpmlogMask = mask;
    return omask;
}

/*@unchecked@*/ /*@null@*/
static rpmlogCallback _rpmlogCallback;

/*@unchecked@*/ /*@null@*/
static rpmlogCallbackData _rpmlogCallbackData;

rpmlogCallback rpmlogSetCallback(rpmlogCallback cb, rpmlogCallbackData data)
	/*@globals _rpmlogCallback, _rpmlogCallbackData @*/
	/*@modifies _rpmlogCallback, _rpmlogCallbackData @*/
{
    rpmlogCallback ocb = _rpmlogCallback;
    _rpmlogCallback = cb;
    _rpmlogCallbackData = data;
    return ocb;
}

void rpmlogGetCallback(rpmlogCallback *cb, rpmlogCallbackData *data)
	/*@globals _rpmlogCallback, _rpmlogCallbackData @*/
{
    *cb = _rpmlogCallback;
    *data = _rpmlogCallbackData;
    return;
}

/*@unchecked@*/ /*@null@*/
static FILE * _stdlog = NULL;

static int rpmlogDefault(rpmlogRec rec)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    FILE *msgout = (_stdlog ? _stdlog : stderr);

    switch (rec->pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
	msgout = (_stdlog ? _stdlog : stdout);
	break;
    case RPMLOG_EMERG:
    case RPMLOG_ALERT:
    case RPMLOG_CRIT:
    case RPMLOG_ERR:
    case RPMLOG_WARNING:
    case RPMLOG_DEBUG:
    default:
	break;
    }

    (void) fputs(rpmlogLevelPrefix(rec->pri), msgout);

    if (rec->message)
	(void) fputs(rec->message, msgout);
    (void) fflush(msgout);

    return (rec->pri <= RPMLOG_CRIT ? RPMLOG_EXIT : 0);
}

FILE * rpmlogSetFile(FILE * fp)
	/*@globals _stdlog @*/
	/*@modifies _stdlog @*/
{
    FILE * ofp = _stdlog;
    _stdlog = fp;
    return ofp;
}

/*@-readonlytrans@*/	/* FIX: double indirection. */
/*@observer@*/ /*@unchecked@*/
static const char *rpmlogMsgPrefix[] = {
    N_("fatal error: "),/*!< RPMLOG_EMERG */
    N_("fatal error: "),/*!< RPMLOG_ALERT */
    N_("fatal error: "),/*!< RPMLOG_CRIT */
    N_("error: "),	/*!< RPMLOG_ERR */
    N_("warning: "),	/*!< RPMLOG_WARNING */
    "",			/*!< RPMLOG_NOTICE */
    "",			/*!< RPMLOG_INFO */
    "D: ",		/*!< RPMLOG_DEBUG */
};
/*@=readonlytrans@*/

const char * rpmlogLevelPrefix(rpmlogLvl pri)
{
    return rpmlogMsgPrefix[pri&0x7];
}

#if !defined(HAVE_VSNPRINTF)
static inline int vsnprintf(char * buf, /*@unused@*/ int nb,
	const char * fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}
#endif

/*@-modfilesys@*/
/*@-compmempass@*/ /* FIX: rpmlogMsgPrefix[] dependent, not unqualified */
/*@-nullstate@*/ /* FIX: rpmlogMsgPrefix[] may be NULL */
void vrpmlog (unsigned code, const char *fmt, va_list ap)
	/*@globals nrecs, recs, internalState @*/
	/*@modifies nrecs, recs, internalState @*/
{
    unsigned pri = RPMLOG_PRI(code);
    unsigned mask = RPMLOG_MASK(pri);
#if 0
    /*@unused@*/ unsigned fac = RPMLOG_FAC(code);
#endif
    char *msgbuf, *msg;
    size_t msgnb = BUFSIZ;
    int nb;
    int cbrc = RPMLOG_DEFAULT;
    int needexit = 0;
    struct rpmlogRec_s rec;

    if ((mask & rpmlogMask) == 0)
	return;

    msgbuf = xmalloc(msgnb);
    *msgbuf = '\0';

    /* Allocate a sufficently large buffer for output. */
    while (1) {
	va_list apc;
	/*@-unrecog -usedef@*/ va_copy(apc, ap); /*@=unrecog =usedef@*/
	nb = vsnprintf(msgbuf, msgnb, fmt, apc);
	if (nb > -1 && (size_t)nb < msgnb)
	    break;
	if (nb > -1)		/* glibc 2.1 (and later) */
	    msgnb = nb+1;
	else			/* glibc 2.0 */
	    msgnb *= 2;
	msgbuf = xrealloc(msgbuf, msgnb);
/*@-mods@*/
	va_end(apc);
/*@=mods@*/
    }
    msgbuf[msgnb - 1] = '\0';
    msg = msgbuf;

    rec.code = code;
    rec.message = msg;
    rec.pri = pri;

    /* Save copy of all messages at warning (or below == "more important"). */
    if (pri <= RPMLOG_WARNING) {
	if (recs == NULL)
	    recs = xmalloc((nrecs+2) * sizeof(*recs));
	else
	    recs = xrealloc(recs, (nrecs+2) * sizeof(*recs));
	recs[nrecs].code = rec.code;
	recs[nrecs].pri = rec.pri;
	recs[nrecs].message = xstrdup(msgbuf);
	++nrecs;
	recs[nrecs].code = 0;
	recs[nrecs].pri = 0;
	recs[nrecs].message = NULL;
    }

    if (_rpmlogCallback) {
	cbrc = _rpmlogCallback(&rec, _rpmlogCallbackData);
	needexit += cbrc & RPMLOG_EXIT;
    }

    if (cbrc & RPMLOG_DEFAULT) {
/*@-usereleased@*/
	cbrc = rpmlogDefault(&rec);
/*@=usereleased@*/
	needexit += cbrc & RPMLOG_EXIT;
    }

/*@-usereleased@*/	/* msgbuf is NULL or needs free'ing */
    msgbuf = _free(msgbuf);
/*@=usereleased@*/
    if (needexit)
	exit(EXIT_FAILURE);
}
/*@=compmempass =nullstate@*/
/*@=modfilesys@*/

void _rpmlog (int code, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    /*@-internalglobs@*/ /* FIX: shrug */
    vrpmlog(code, fmt, ap);
    /*@=internalglobs@*/
    va_end(ap);
}
