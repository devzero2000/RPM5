/** \ingroup rpmio
 * \file rpmio/rpmzlog.c
 * Trace logging (originally swiped from PIGZ).
 */

/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007, 2008 Mark Adler
 * Version 2.1.4  9 Nov 2008  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu

  Mark accepts donations for providing this software.  Donations are not
  required or expected.  Any amount that you feel is appropriate would be
  appreciated.  You can use this link:

  https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=536055

 */

#include "system.h"
#include <stdarg.h>

#include <rpmiotypes.h>
#include "yarn.h"

#define	_RPMZLOG_INTERNAL
#include "rpmzlog.h"

#include "debug.h"

/*@access rpmzMsg @*/
/*@access rpmzLog @*/

/*==============================================================*/

/* maximum log entry length */
#define _RPMZLOG_MAXMSG 256

rpmzLog rpmzLogInit(struct timeval *tv)
{
    rpmzLog zlog = xcalloc(1, sizeof(*zlog));

    if (zlog->msg_tail == NULL) {
/*@-mustfreeonly@*/
	zlog->msg_lock = yarnNewLock(0);
	zlog->msg_head = NULL;
	zlog->msg_tail = &zlog->msg_head;
/*@=mustfreeonly@*/
	/* starting time for log entries */
	if (tv != NULL) {
/*@-assignexpose@*/
	    zlog->start = *tv;	/* structure assignment */
/*@=assignexpose@*/
	} else
	    (void) gettimeofday(&zlog->start, NULL);
    }
    return zlog;
}

void rpmzLogAdd(rpmzLog zlog, char *fmt, ...)
{
    rpmzMsg me;
    struct timeval now;
    va_list ap;
    char msg[_RPMZLOG_MAXMSG];
    int xx;

    if (zlog == NULL) return;

    xx = gettimeofday(&now, NULL);
    me = xmalloc(sizeof(*me));
    me->when = now;
    va_start(ap, fmt);
    xx = vsnprintf(msg, sizeof(msg)-1, fmt, ap);
    va_end(ap);
    msg[sizeof(msg)-1] = '\0';

/*@-mustfreeonly@*/
    me->msg = xmalloc(strlen(msg) + 1);
/*@=mustfreeonly@*/
    strcpy(me->msg, msg);
/*@-mustfreeonly@*/
    me->next = NULL;
/*@=mustfreeonly@*/
assert(zlog->msg_lock != NULL);
    yarnPossess(zlog->msg_lock);
    *zlog->msg_tail = me;
    zlog->msg_tail = &me->next;
    yarnTwist(zlog->msg_lock, BY, 1);
}

int rpmzMsgShow(rpmzLog zlog, FILE * fp)
{
    rpmzMsg me;
    struct timeval diff;
    int xx;

    if (zlog == NULL || zlog->msg_tail == NULL)
	return 0;

    if (fp == NULL)
	fp = stderr;
    yarnPossess(zlog->msg_lock);
    me = zlog->msg_head;
    if (me == NULL) {
	yarnRelease(zlog->msg_lock);
	return 0;
    }
    zlog->msg_head = me->next;
    if (me->next == NULL)
	zlog->msg_tail = &zlog->msg_head;
    yarnTwist(zlog->msg_lock, BY, -1);
    diff.tv_usec = me->when.tv_usec - zlog->start.tv_usec;
    diff.tv_sec = me->when.tv_sec - zlog->start.tv_sec;
    if (diff.tv_usec < 0) {
	diff.tv_usec += 1000000L;
	diff.tv_sec--;
    }
    fprintf(fp, "trace %ld.%06ld %s\n",
	    (long)diff.tv_sec, (long)diff.tv_usec, me->msg);
    xx = fflush(fp);
    me->msg = _free(me->msg);
    me = _free(me);
    return 1;
}

rpmzLog rpmzLogFree(rpmzLog zlog)
{
    rpmzMsg me;

    if (zlog == NULL)
	return NULL;

    if (zlog->msg_tail != NULL) {
	yarnPossess(zlog->msg_lock);
	while ((me = zlog->msg_head) != NULL) {
	    zlog->msg_head = me->next;
	    me->msg = _free(me->msg);
/*@-compdestroy@*/
	    me = _free(me);
/*@=compdestroy@*/
	}
	yarnTwist(zlog->msg_lock, TO, 0);
	zlog->msg_lock = yarnFreeLock(zlog->msg_lock);
	zlog->msg_tail = NULL;
    }
    zlog = _free(zlog);
    return NULL;
}

rpmzLog rpmzLogDump(rpmzLog zlog, FILE * fp)
{
    while (rpmzMsgShow(zlog, fp))
	{;}

    return rpmzLogFree(zlog);
}
