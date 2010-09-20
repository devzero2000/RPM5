#define	DEBUG

#include "system.h"

#include <pthread.h>

#include <argv.h>

#define	_RPMRUBY_INTERNAL
#include <rpmruby.h>

#undef	xmalloc
#undef	xcalloc
#undef	xrealloc
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ruby.h>
#pragma GCC diagnostic warning "-Wstrict-prototypes"

#include "debug.h"

#define RUBYDBG(_l) if (_rpmruby_debug) fprintf _l
#define Trace(_x) do { rpmzLogAdd _x; } while (0)

struct yarnThread_s {
    pthread_t id;
    int done;                   /* true if this thread has exited */
/*@dependent@*/ /*@null@*/
    yarnThread next;            /* for list of all launched threads */
};

static struct rpmruby_s __ruby = {
	.nstack = (4 * 1024 * 1024)
};
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

RUBYDBG((stderr, "--> %s(%p,%s)\n", __FUNCTION__, ruby, fn));

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

RUBYDBG((stderr, "<-- %s(%p,%s) rc %d\n", __FUNCTION__, ruby, fn, rc));
    return rc;
}

static void * rpmrubyThread(void * _ruby)
{
    rpmruby ruby = _ruby;
    rpmzLog zlog = ruby->zlog;
    int i;

RUBYDBG((stderr, "--> %s(%p)\n", __FUNCTION__, _ruby));

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

    ruby->more = 0;
    _rpmruby_ruby_to_main(ruby, Qnil);

    pthread_exit(NULL);

    Trace((zlog, "-- %s: ended", __FUNCTION__));

RUBYDBG((stderr, "<-- %s(%p)\n", __FUNCTION__, _ruby));

    return NULL;
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
    yarnPossess(ruby->ruby_coroutine_lock);
    ruby->main_coroutine_lock = yarnNewLock(0);
    yarnPossess(ruby->main_coroutine_lock);

    /* create a thread to house Ruby */
    ruby->thread = yarnLaunchStack((void (*)(void *))rpmrubyThread, ruby,
				ruby->stack, ruby->nstack);
assert(ruby->thread != NULL);

    /* relay control to Ruby until it is finished */
    ruby->more = (ruby->ac > 1);
    while (ruby->more)
        _rpmruby_main_to_ruby(ruby);

    ec = 0;

exit:
    fprintf(stderr, "Main: Goodbye!\n");
ruby->zlog = rpmzLogDump(ruby->zlog, NULL);

#ifdef	NOTYET
    if (ruby->thread)
	ruby->thread = yarnJoin(ruby->thread);
#endif
ruby->thread = _free(ruby->thread);

#ifdef	NOTYET	/* XXX FIXME */
    yarnRelease(ruby->main_coroutine_lock);
#endif
    ruby->main_coroutine_lock = yarnFreeLock(ruby->main_coroutine_lock);
    yarnRelease(ruby->ruby_coroutine_lock);
    ruby->ruby_coroutine_lock = yarnFreeLock(ruby->ruby_coroutine_lock);

ruby->av = argvFree(ruby->av);
ruby->ac = 0;

    return ec;
}
