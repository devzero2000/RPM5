#include "system.h"

#include <pthread.h>
static pthread_t ruby_coroutine;

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

static struct rpmruby_s __ruby = {
	.nstack = (4 * 1024 * 1024)
};
static rpmruby ruby = &__ruby;

/* puts the Ruby coroutine in control */
static void relay_from_main_to_ruby(void)
{
RUBYDBG((stderr, "--> %s(void) main => ruby\n", __FUNCTION__));
    yarnRelease(ruby->ruby_coroutine_lock);
    yarnPossess(ruby->main_coroutine_lock);
RUBYDBG((stderr, "<-- %s(void) main <= ruby\n", __FUNCTION__));
}

/* puts the main C program in control */
static VALUE relay_from_ruby_to_main(VALUE self)
{
RUBYDBG((stderr, "--> %s(%p) ruby => main\n", __FUNCTION__, (void *)self));
    yarnRelease(ruby->main_coroutine_lock);
    yarnPossess(ruby->ruby_coroutine_lock);
RUBYDBG((stderr, "<-- %s(%p) ruby <= main\n", __FUNCTION__, (void *)self));
    return Qnil;
}

static VALUE ruby_coroutine_body_require(const char * file)
{
    int error;
    VALUE result;

RUBYDBG((stderr, "--> %s(%s)\n", __FUNCTION__, file));

    result = rb_protect((VALUE (*)(VALUE))rb_require, (VALUE)file, &error);
    if (error) {
        fprintf(stderr, "rb_require('%s') failed with status=%d\n",
               file, error);

        VALUE exception = rb_gv_get("$!");
        if (RTEST(exception)) {
            fprintf(stderr, "... because an exception was raised:\n");

            VALUE inspect = rb_inspect(exception);
            rb_io_puts(1, &inspect, rb_stderr);

            VALUE backtrace = rb_funcall(
                exception, rb_intern("backtrace"), 0);
            rb_io_puts(1, &backtrace, rb_stderr);
        }
    }

RUBYDBG((stderr, "<-- %s(%s) rc 0x%lx\n", __FUNCTION__, file, (long)result));
    return result;
}

static void * ruby_coroutine_body(void * _ruby)
{
    static char * _av[] = { "rpmruby", NULL };
    char ** av = _av;
    int ac = 1;
    int i;

RUBYDBG((stderr, "--> %s(%p)\n", __FUNCTION__, _ruby));

    relay_from_ruby_to_main(Qnil);

    fprintf(stderr, "Coroutine: begin\n");

    for (i = 0; i < 2; i++) {
        fprintf(stderr, "Coroutine: relay %d\n", i);
        relay_from_ruby_to_main(Qnil);
    }

    fprintf(stderr, "Coroutine: Ruby begin\n");

    ruby_sysinit(&ac, (char ***) &av);

    {
	uint8_t * b = ruby->stack;
	uint8_t * e = b + ruby->nstack;
	
        ruby_bind_stack((VALUE *)b, (VALUE *)e);

        RUBY_INIT_STACK;
        ruby_init();
        ruby_init_loadpath();

        /* allow Ruby script to relay */
        rb_define_module_function(rb_mKernel, "relay_from_ruby_to_main",
                                  relay_from_ruby_to_main, 0);

        /* run the "hello world" Ruby script */
        fprintf(stderr, "Ruby: require 'hello' begin\n");
        ruby_coroutine_body_require(ruby->fn);
        fprintf(stderr, "Ruby: require 'hello' end\n");

        ruby_cleanup(0);
    }

    fprintf(stderr, "Coroutine: Ruby end\n");

    fprintf(stderr, "Coroutine: end\n");

    ruby->ruby_coroutine_finished = 1;
    relay_from_ruby_to_main(Qnil);

    pthread_exit(NULL);

RUBYDBG((stderr, "<-- %s(%p)\n", __FUNCTION__, _ruby));

    return NULL;
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main(int argc, char *argv[])
{
    pthread_attr_t attr;
    int error;
    int ec = 1;

_rpmruby_debug = -1;
    /* allocate the coroutine stack */
    if ((ruby->stack = malloc(ruby->nstack)) == NULL) {
        fprintf(stderr, "Could not allocate %lu bytes!\n", (unsigned long)ruby->nstack);
	goto exit;
    }
    ruby->fn = xstrdup("../ruby/hello.rb");

    /* initialize the relay mechanism */
    ruby->ruby_coroutine_lock = yarnNewLock(0);
    yarnPossess(ruby->ruby_coroutine_lock);
    ruby->main_coroutine_lock = yarnNewLock(0);
    yarnPossess(ruby->main_coroutine_lock);

    /* create pthread to house Ruby */
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, ruby->stack, ruby->nstack);

    error = pthread_create(&ruby_coroutine, &attr, &ruby_coroutine_body, NULL);
    if (error) {
        fprintf(stderr, "ERROR: pthread_create() returned %d\n", error);
	goto exit;
    }

    /* relay control to Ruby until it is finished */
    ruby->ruby_coroutine_finished = 0;
    while (!ruby->ruby_coroutine_finished)
        relay_from_main_to_ruby();
    ec = 0;

exit:
    fprintf(stderr, "Main: Goodbye!\n");
#ifdef	NOTYET	/* XXX FIXME */
    yarnRelease(ruby->main_coroutine_lock);
#endif
    ruby->main_coroutine_lock = yarnFreeLock(ruby->main_coroutine_lock);
    yarnRelease(ruby->ruby_coroutine_lock);
    ruby->ruby_coroutine_lock = yarnFreeLock(ruby->ruby_coroutine_lock);
    ruby->fn = _free(ruby->fn);
    return ec;
}
