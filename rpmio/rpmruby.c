#include "system.h"
#include <argv.h>

#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif


#if defined(WITH_RUBYEMBED)

/* Make sure Ruby's fun stuff has its own xmalloc & Co functions available */

#undef xmalloc
#undef xcalloc
#undef xrealloc

#include <ruby.h>

#endif


/**
 * Allows access to the gory details of rpmruby pool items.
 */
#define _RPMRUBY_INTERNAL 1
#include "rpmruby.h"

#include "debug.h"

/*@unchecked@*/
int _rpmruby_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmruby _rpmrubyI = NULL;



/**
 * Finalizes a Ruby interpreter instance/pool item
 */
static void rpmrubyFini(void *_ruby)
        /*@globals fileSystem @*/
        /*@modifies *_ruby, fileSystem @*/
{
    rpmruby ruby = _ruby;

#   if defined(WITH_RUBYEMBED)
        ruby_cleanup(0); 
#   endif
    ruby->I = NULL;
}


/**
* The pool of Ruby interpreter instances
* @see rpmioPool
*/
/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmrubyPool;


/**
 * Returns the current rpmio pool responsible for Ruby interpreter instances
 *
 * This is a wrapper function that returns the current rpmio pool responsible
 * for embedded Ruby interpreters. It creates and initializes a new pool when
 * there is no current pool.
 *
 * @return The current pool
 * @see rpmioNewPool
 */
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

/** Initializes Ruby's StringIO for storing output in a string. */
static const char * rpmrubyInitStringIO = "\
require 'stringio'\n\
$stdout = StringIO.new($result, \"w+\")\n\
";

#endif


static rpmruby rpmrubyI()
        /*@globals _rpmrubyI @*/
        /*@modifies _rpmrubyI @*/
{
    if (NULL == _rpmrubyI) {
        _rpmrubyI = rpmrubyNew(NULL, 0);
    }
    return _rpmrubyI;
}


rpmruby rpmrubyNew(char **av, uint32_t flags)
{
    /* TODO: Once Ruby allows several interpreter instances, distinguish
     * between the global interpreter (1<<31 flag) and a new one.
     */
    
    if (NULL != _rpmrubyI) {
        return _rpmrubyI;
    }

    rpmruby ruby = rpmrubyGetPool(_rpmrubyPool);

    static char *_av[] = { "rpmruby", NULL };
    if (NULL == av) {
        av = _av;
    }


    /* Set up embedded Ruby interpreter if embedded Ruby is enabled. */

#   if defined(WITH_RUBYEMBED)
        RUBY_INIT_STACK;
        ruby_init();
        ruby_init_loadpath();

        ruby_script((char *)av[0]);
        rb_gv_set("$result", rb_str_new2(""));
        (void) rpmrubyRun(ruby, rpmrubyInitStringIO, NULL);
#   endif

    return rpmrubyLink(ruby);
}


rpmRC rpmrubyRun(rpmruby ruby, const char *str, const char **resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (_rpmruby_debug) {
        fprintf(stderr, "==> %s(%p,%s,%p)\n",
            __FUNCTION__, ruby, str, resultp);
    }

    if (NULL == ruby) {
        ruby = rpmrubyI();
    }

    if (NULL != str) {
#       if defined(WITH_RUBYEMBED)
            int state = -1;
            ruby->state = rb_eval_string_protect(str, &state);

            /* Check whether the evaluation was successful or not */

            if(0 == state) {
                rc = RPMRC_OK;
                if (resultp != NULL) {
                    *resultp = RSTRING_PTR(rb_gv_get("$result"));
                }
            }
#       endif
    }

    return rc;
}

