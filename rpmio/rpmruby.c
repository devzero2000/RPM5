#include "system.h"

#if defined(WITH_RUBYEMBED)
/* XXX ruby-1.8.6 grrr, ruby.h includes its own config.h too. */
#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif
#undef	PACKAGE_NAME
#undef	PACKAGE_TARNAME
#undef	PACKAGE_VERSION
#undef	PACKAGE_STRING
#undef	PACKAGE_BUGREPORT

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
#if defined(HAVE_RUBY_DEFINES_H)	/* XXX ruby-1.9.2p0 */
/* puts the Ruby coroutine in control */
static void _rpmruby_main_to_ruby(rpmruby ruby)
{
    rpmzLog zlog = ruby->zlog;

    yarnRelease(ruby->ruby_coroutine_lock);
    yarnPossess(ruby->main_coroutine_lock);
if (_rpmruby_debug < 0) Trace((zlog, "-> %s", __FUNCTION__));
}

/* puts the main C program in control */
static unsigned long _rpmruby_ruby_to_main(rpmruby ruby, unsigned long self)
{
    rpmzLog zlog = ruby->zlog;

    yarnRelease(ruby->main_coroutine_lock);
    yarnPossess(ruby->ruby_coroutine_lock);
if (_rpmruby_debug < 0) Trace((zlog, "<- %s", __FUNCTION__));
    return Qnil;
}

/* Using _rpmrubyI, puts the main C program in control */
static VALUE relay_from_ruby_to_main(VALUE self)
{
    /* XXX FIXME: _rpmrubyI is global */
    return _rpmruby_ruby_to_main(_rpmrubyI, self);
}

static rpmRC rpmrubyRunThreadFile(rpmruby ruby, const char * fn,
		const char **resultp)
{
    int error;
    VALUE result;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */

    result = rb_protect((VALUE (*)(VALUE))rb_require, (VALUE)fn, &error);
    if (error) {
        fprintf(stderr, "rb_require('%s') failed with status=%d\n",
               fn, error);

        VALUE exception = rb_gv_get("$!");
        if (RTEST(exception)) {
            fprintf(stderr, "... because an exception was raised:\n");

            VALUE inspect = rb_inspect(exception);
            rb_io_puts(1, &inspect, rb_stderr);

            VALUE backtrace = rb_funcall(
                exception, rb_intern("backtrace"), 0);
            rb_io_puts(1, &backtrace, rb_stderr);
        }
    } else {
	/* XXX FIXME: check result */
	rc = RPMRC_OK;
    }

    return rc;
}

static void * rpmrubyThread(void * _ruby)
{
    rpmruby ruby = _ruby;
    rpmzLog zlog = ruby->zlog;
    int i;

    Trace((zlog, "-- %s: running", __FUNCTION__));

    _rpmruby_ruby_to_main(ruby, Qnil);

    for (i = 0; i < 2; i++)
        _rpmruby_ruby_to_main(ruby, Qnil);

    {
	VALUE variable_in_this_stack_frame;
	uint8_t * b = ruby->stack;
	uint8_t * e = b + ruby->nstack;

	/* Start up the ruby interpreter. */
	Trace((zlog, "-- %s: interpreter starting", __FUNCTION__));
	ruby_sysinit(&ruby->ac, (char ***) &ruby->av);

        ruby_bind_stack((VALUE *)b, (VALUE *)e);

	ruby_init_stack(&variable_in_this_stack_frame);
        ruby_init();
	ruby_init_loadpath();

        /* allow Ruby script to relay */
        rb_define_module_function(rb_mKernel, "relay_from_ruby_to_main",
                                  relay_from_ruby_to_main, 0);
	Trace((zlog, "-- %s: interpreter started", __FUNCTION__));

	/* Run file.rb arguments. */
	for (i = 1; i < ruby->ac; i++) {
	    if (*ruby->av[i] == '-')	/* XXX FIXME: skip options. */
		continue;
	    Trace((zlog, "-- %s: require '%s' begin", __FUNCTION__, ruby->av[i]));
	    rpmrubyRunThreadFile(ruby, ruby->av[i], NULL);
	    Trace((zlog, "-- %s: require '%s' end", __FUNCTION__, ruby->av[i]));
	}

	/* Terminate the ruby interpreter. */
	Trace((zlog, "-- %s: interpreter terminating", __FUNCTION__));
	ruby_finalize();
        ruby_cleanup(0);
	Trace((zlog, "-- %s: interpreter terminated", __FUNCTION__));
    }

    /* Report interpreter end to main. */
    ruby->more = 0;
    /* Permit main thread to run without blocking. */
    yarnRelease(ruby->main_coroutine_lock);

    Trace((zlog, "-- %s: ended", __FUNCTION__));
    return NULL;
}
#endif	/* HAVE_RUBY_DEFINES_H */

int rpmrubyRunThread(rpmruby ruby)
{
    int ec = 0;

#if defined(HAVE_RUBY_DEFINES_H)	/* XXX ruby-1.9.2p0 */
    yarnPossess(ruby->ruby_coroutine_lock);
    yarnPossess(ruby->main_coroutine_lock);

    /* create a thread to house Ruby */
    ruby->thread = yarnLaunchStack((void (*)(void *))rpmrubyThread, ruby,
				ruby->stack, ruby->nstack);
assert(ruby->thread != NULL);

    /* Relay control to ruby until nothing more to do. */
    ruby->more = (ruby->ac > 1);
    while (ruby->more)
        _rpmruby_main_to_ruby(ruby);

    /* Permit ruby thread to run without blocking. */
    yarnRelease(ruby->ruby_coroutine_lock);
    /* Reap the ruby thread. */
    ruby->thread = yarnJoin(ruby->thread);
    ec = 0;
#endif	/* HAVE_RUBY_DEFINES_H */

    return ec;
}

/*==============================================================*/

static void rpmrubyFini(void * _ruby)
        /*@globals fileSystem @*/
        /*@modifies *_ruby, fileSystem @*/
{
    rpmruby ruby = _ruby;

    /* XXX FIXME: 0x40000000 => xruby.c wrapper without interpreter. */
    if (ruby->flags & 0x40000000) {
	ruby->main_coroutine_lock = yarnFreeLock(ruby->main_coroutine_lock);
	ruby->ruby_coroutine_lock = yarnFreeLock(ruby->ruby_coroutine_lock);
	ruby->zlog = rpmzLogDump(ruby->zlog, NULL);
	ruby->stack = _free(ruby->stack);
	ruby->nstack = 0;
	_rpmrubyI = NULL;
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
#if defined(WITH_RUBYEMBED) && !defined(HAVE_RUBY_DEFINES_H)/* XXX ruby-1.8.6 */
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

	/* XXX save as global interpreter. */
	_rpmrubyI = ruby;

	ruby->nstack = _rpmrubyStackSize;
	ruby->stack = malloc(ruby->nstack);
assert(ruby->stack != NULL);

	gettimeofday(&ruby->start, NULL);  /* starting time for log entries */
	if (_rpmruby_debug)
	    ruby->zlog = rpmzLogNew(&ruby->start);  /* initialize logging */

	/* initialize the relay mechanism */
	ruby->ruby_coroutine_lock = yarnNewLock(0);
	ruby->main_coroutine_lock = yarnNewLock(0);

    } else {

#if defined(WITH_RUBYEMBED)
	VALUE variable_in_this_stack_frame;		/* RUBY_INIT_STSCK */

#if defined(HAVE_RUBY_DEFINES_H)	/* XXX ruby-1.9.2 */
	ruby_sysinit(&ruby->ac, (char ***) &ruby->av);
	/* XXX ruby-1.9.2p0 ruby_bind_stack() patch needed */
	{
	    uint8_t * b = ruby->stack;
	    uint8_t * e = b + ruby->nstack;
	    ruby_bind_stack((VALUE *)b, (VALUE *) e);
	}
#endif	/* NOTYET */

	ruby_init_stack(&variable_in_this_stack_frame);	/* RUBY_INIT_STACK */

	ruby_init();
	ruby_init_loadpath();

	ruby_script((char *)av[0]);
	if (av[1])
	    ruby_set_argv(argvCount((ARGV_t)av)-1, av+1);

	rb_gv_set("$result", rb_str_new2(""));
#if !defined(HAVE_RUBY_DEFINES_H)	/* XXX ruby-1.8.6 */
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
#if !defined(HAVE_RUBY_DEFINES_H)	/* XXX ruby-1.8.6 */
    rb_load_file(fn);
    ruby->state = ruby_exec();
#else
    ruby->state = ruby_exec_node(rb_load_file(fn));
#endif
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
