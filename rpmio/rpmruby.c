#include "system.h"
#include <argv.h>

/* XXX grrr, ruby.h includes its own config.h too. */
#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif
#undef	PACKAGE_NAME
#undef	PACKAGE_TARNAME
#undef	PACKAGE_VERSION
#undef	PACKAGE_STRING
#undef	PACKAGE_BUGREPORT

#if defined(WITH_RUBYEMBED)
#undef	xmalloc
#undef	xcalloc
#undef	xrealloc
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ruby.h>
#pragma GCC diagnostic warning "-Wstrict-prototypes"

#if !defined(RSTRING_PTR)
/* XXX retrofit for ruby-1.8.5 in CentOS5 */
#define RSTRING_PTR(s) (RSTRING(s)->ptr)
#define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

#endif

#define _RPMRUBY_INTERNAL
#include "rpmruby.h"

#include "debug.h"

/*@unchecked@*/
int _rpmruby_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmruby _rpmrubyI = NULL;

/*==============================================================*/
/* puts the Ruby coroutine in control */
void _rpmruby_main_to_ruby(rpmruby ruby)
{
    rpmzLog zlog = ruby->zlog;

    yarnRelease(ruby->ruby_coroutine_lock);
    yarnPossess(ruby->main_coroutine_lock);
if (_rpmruby_debug < 0) Trace((zlog, "-> %s", __FUNCTION__));
}

/* puts the main C program in control */
unsigned long _rpmruby_ruby_to_main(rpmruby ruby, unsigned long self)
{
    rpmzLog zlog = ruby->zlog;

    yarnRelease(ruby->main_coroutine_lock);
    yarnPossess(ruby->ruby_coroutine_lock);
if (_rpmruby_debug < 0) Trace((zlog, "<- %s", __FUNCTION__));
    return Qnil;
}

#ifdef	NOTYET
/* Using _rpmrubyI, puts the main C program in control */
static VALUE relay_from_ruby_to_main(VALUE self)
{
    /* XXX FIXME: _rpmrubyI is global */
    return _rpmruby_ruby_to_main(_rpmrubyI, self);
}
#endif

/*==============================================================*/

static void rpmrubyFini(void * _ruby)
        /*@globals fileSystem @*/
        /*@modifies *_ruby, fileSystem @*/
{
    rpmruby ruby = _ruby;

    /* XXX FIXME: 0x40000000 => xruby.c wrapper without interpreter. */
    if (ruby->flags & 0x40000000) {
	ruby->zlog = rpmzLogDump(ruby->zlog, NULL);
	ruby->main_coroutine_lock = yarnFreeLock(ruby->main_coroutine_lock);
	ruby->ruby_coroutine_lock = yarnFreeLock(ruby->ruby_coroutine_lock);
    } else {
#if defined(WITH_RUBYEMBED)
	ruby_finalize();
	ruby_cleanup(0);
#endif
    }
    ruby->I = NULL;
    ruby->flags = 0;
    ruby->av = argvFree(ruby->av);
    ruby->ac = 0;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmrubyPool;

static rpmruby rpmrubyGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmrubyPool, fileSystem @*/
        /*@modifies pool, _rpmrubyPool, fileSystem @*/
{
    rpmruby ruby;

    if (_rpmrubyPool == NULL) {
        _rpmrubyPool = rpmioNewPool("ruby", sizeof(*ruby), -1, _rpmruby_debug,
                        NULL, NULL, rpmrubyFini);
        pool = _rpmrubyPool;
    }
    return (rpmruby) rpmioGetPool(pool, sizeof(*ruby));
}

/*@unchecked@*/
#if defined(WITH_RUBYEMBED)
static const char * rpmrubyInitStringIO = "\
require 'stringio'\n\
$stdout = StringIO.new($result, \"w+\")\n\
";
#endif

static rpmruby rpmrubyI(void)
	/*@globals _rpmrubyI @*/
	/*@modifies _rpmrubyI @*/
{
    /* XXX FIXME: pass 0x40000000 for wrapper without interpreter? */
    if (_rpmrubyI == NULL)
	_rpmrubyI = rpmrubyNew(NULL, 0);
RUBYDBG((stderr, "<-- %s() I %p\n", __FUNCTION__, _rpmrubyI));
    return _rpmrubyI;
}

rpmruby rpmrubyNew(char ** av, uint32_t flags)
{
    static char * _av[] = { "rpmruby", NULL };
    rpmruby ruby = (flags & 0x80000000)
		? rpmrubyI() : rpmrubyGetPool(_rpmrubyPool);
int xx;

RUBYDBG((stderr, "--> %s(%p,0x%x) ruby %p\n", __FUNCTION__, av, flags, ruby));

    /* If failure, or retrieving already initialized _rpmrubyI, just exit. */
    if (ruby == NULL || ruby == _rpmrubyI)
	goto exit;

    if (av == NULL) av = _av;

    ruby->flags = flags;
    xx = argvAppend(&ruby->av, (ARGV_t)av);
    ruby->ac = argvCount(ruby->av);

    /* XXX FIXME: 0x40000000 => xruby.c wrapper without interpreter. */
    if (ruby->flags & 0x40000000) {
	static size_t _rpmrubyStackSize = 4 * 1024 * 1024;

	ruby->nstack = _rpmrubyStackSize;
	ruby->stack = malloc(ruby->nstack);
assert(ruby->stack != NULL);

	gettimeofday(&ruby->start, NULL);  /* starting time for log entries */
	if (_rpmruby_debug)
	    ruby->zlog = rpmzLogNew(&ruby->start);  /* initialize logging */

	/* initialize the relay mechanism */
	ruby->ruby_coroutine_lock = yarnNewLock(0);
	ruby->main_coroutine_lock = yarnNewLock(0);

	/* XXX save as global interpreter. */
	_rpmrubyI = ruby;

    } else {

#if defined(WITH_RUBYEMBED)
	VALUE variable_in_this_stack_frame;		/* RUBY_INIT_STSCK */

#ifdef	NOTYET	/* XXX ruby-1.9.2p0 ruby_bind_stack patch needed */
	{
	    uint8_t * b = ruby->stack;
	    uint8_t * e = b + ruby->nstack;
	    ruby_sysinit(&ruby->ac, (char ***) &ruby->av);
	}
#endif	/* NOTYET */

	ruby_init_stack(&variable_in_this_stack_frame);	/* RUBY_INIT_STACK */

	ruby_init();
	ruby_init_loadpath();

	ruby_script((char *)av[0]);
	if (av[1])
	    ruby_set_argv(argvCount((ARGV_t)av)-1, av+1);

	rb_gv_set("$result", rb_str_new2(""));
#ifdef	NOTYET	/* XXX avoid ruby-1.9.2p0 segfaults */
	(void) rpmrubyRun(ruby, rpmrubyInitStringIO, NULL);
#endif
#endif	/* WITH_RUBYEMBED */
    }

exit:
    return rpmrubyLink(ruby);
}

rpmRC rpmrubyRunFile(rpmruby ruby, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

RUBYDBG((stderr, "--> %s(%p,%s,%p)\n", __FUNCTION__, ruby, fn, resultp));

    if (ruby == NULL) ruby = rpmrubyI();

    if (fn == NULL)
	goto exit;

#if defined(WITH_RUBYEMBED)
#ifdef	DYING
    rb_load_file(fn);
    ruby->state = ruby_exec();
#else	/* DYING */
    ruby->state = ruby_exec_node(rb_load_file(fn));
#endif	/* DYING */
    if (resultp != NULL)
	*resultp = RSTRING_PTR(rb_gv_get("$result"));
    rc = RPMRC_OK;
#endif	/* WITH_RUBYEMBED */

exit:
RUBYDBG((stderr, "<-- %s(%p,%s,%p) rc %d\n", __FUNCTION__, ruby, fn, resultp, rc));
    return rc;
}

rpmRC rpmrubyRun(rpmruby ruby, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

RUBYDBG((stderr, "--> %s(%p,%s,%p)\n", __FUNCTION__, ruby, str, resultp));

    if (ruby == NULL) ruby = rpmrubyI();

    if (str == NULL)
	goto exit;

#if defined(WITH_RUBYEMBED)
    ruby->state = rb_eval_string(str);
    if (resultp != NULL)
	*resultp = RSTRING_PTR(rb_gv_get("$result"));
    rc = RPMRC_OK;
#endif

exit:
RUBYDBG((stderr, "<-- %s(%p,%s,%p) rc %d\n", __FUNCTION__, ruby, str, resultp, rc));
    return rc;
}
