#include "system.h"

#include <rpmruby.h>

#include "debug.h"

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main(int argc, char *argv[])
{
    static char * _av[] = { "rpmruby", "../ruby/hello.rb", NULL };
    ARGV_t av = (ARGV_t)(argc > 1 ? argv : _av);
    rpmruby ruby;
    int ec;

_rpmruby_debug = 1;

    /* XXX FIXME: 0x40000000 => xruby.c wrapper without interpreter. */
    ruby = rpmrubyNew((char **)av, 0x40000000);

    ec = rpmrubyRunThread(ruby);
    if (ec) goto exit;

#ifdef	NOWORKIE	/* XXX can't restart a ruby interpreter. */
    ec = rpmrubyRunThread(ruby);
    if (ec) goto exit;
#endif	/* NOWORKIE */

exit:
    ruby = rpmrubyFree(ruby);
    return ec;
}
