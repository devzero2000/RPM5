/** \ingroup rpmio
 * \file rpmio/getpass.c
 */

#include "system.h"
#if defined(HAVE_KEYUTILS_H)
#include <argv.h>
#include <keyutils.h>
#endif
#include "debug.h"

char * _GetPass(const char * prompt)
{
/*@-unrecog@*/
    return getpass( prompt ? prompt : "" );
/*@=unrecog@*/
}

char * _RequestPass(const char * prompt)
{
    static char * password = NULL;
#if defined(HAVE_KEYUTILS_H)
    const char * foo = "user rpm:yyyy spoon";
    ARGV_t av = NULL;
    int xx = argvSplit(&av, foo, NULL);
    key_serial_t dest = 0;
    key_serial_t key = 0;

    if (password) {
	free(password);
	password = NULL;
    }
    key = request_key(av[0], av[1], av[2], dest);

    xx = keyctl_read_alloc(key, (void *)&password);
#endif

    return password;
}

char * (*Getpass) (const char * prompt) = _GetPass;
