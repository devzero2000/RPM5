#define	DEBUG

#include "system.h"

#define	_RPMRUBY_INTERNAL
#include <rpmruby.h>

#undef	xmalloc
#undef	xcalloc
#undef	xrealloc
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ruby.h>
#pragma GCC diagnostic warning "-Wstrict-prototypes"

#include "debug.h"

static struct rpmruby_s __ruby;
static rpmruby Xruby = &__ruby;

static VALUE relay_from_ruby_to_main(VALUE self)
{
    return _rpmruby_ruby_to_main(Xruby, self);
}

rpmRC rpmrubyRunFile(rpmruby ruby, const char * fn, const char **resultp)
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
	    rpmrubyRunFile(ruby, ruby->av[i], NULL);
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

static int runOnce(rpmruby ruby)
{
    int ec = 0;
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

    return ec;
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main(int argc, char *argv[])
{
rpmruby ruby = Xruby;
static char * _av[] = { "rpmruby", "../ruby/hello.rb", NULL };
ARGV_t av = (ARGV_t)_av;
    int ec = 1;
int xx;

_rpmruby_debug = 1;
ruby->nstack = 4 * 1024 * 1024;
ruby->stack = malloc(ruby->nstack);
assert(ruby->stack != NULL);

xx = argvAppend(&ruby->av, av);
ruby->ac = argvCount(ruby->av);

    if (_rpmruby_debug)
	ruby->zlog = rpmzLogNew(&ruby->start);	/* initialize logging */

    /* initialize the relay mechanism */
    ruby->ruby_coroutine_lock = yarnNewLock(0);
    ruby->main_coroutine_lock = yarnNewLock(0);

    ec = runOnce(ruby);
    if (ec) goto exit;

#ifdef	NOWORKIE	/* XXX can't restart a ruby interpreter. */
    ec = runOnce(ruby);
    if (ec) goto exit;
#endif	/* NOWORKIE */

exit:
    fprintf(stderr, "Main: Goodbye!\n");

ruby->zlog = rpmzLogDump(ruby->zlog, NULL);

    ruby->main_coroutine_lock = yarnFreeLock(ruby->main_coroutine_lock);
    ruby->ruby_coroutine_lock = yarnFreeLock(ruby->ruby_coroutine_lock);

ruby->av = argvFree(ruby->av);
ruby->ac = 0;

    return ec;
}
