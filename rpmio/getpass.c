/** \ingroup rpmio
 * \file rpmio/getpass.c
 */

#include "system.h"
#include "rpmio.h"
#if defined(HAVE_KEYUTILS_H)
#include <argv.h>
#include <keyutils.h>
#endif
#include "debug.h"

char * _GetPass(const char * prompt)
{
    char * pw;

/*@-unrecog@*/
    pw = getpass( prompt ? prompt : "" );
/*@=unrecog@*/

#if defined(HAVE_KEYUTILS_H)
    if (pw && *pw) {
	size_t npw = strlen(pw);
	key_serial_t keyring = KEY_SPEC_PROCESS_KEYRING;
	(void) add_key("user", "rpm:passwd", pw, npw, keyring);
	(void) memset(pw, 0, npw);	/* burn the password */
	pw = "@u user rpm:passwd";
    }
#endif

assert(pw != NULL);
/*@-observertrans -statictrans@*/
    return pw;
/*@=observertrans =statictrans@*/
}

char * _RequestPass(/*@unused@*/ const char * prompt)
{
/*@only@*/ /*@relnull@*/
    static char * password = NULL;
#if defined(HAVE_KEYUTILS_H)
    const char * foo = "user rpm:yyyy spoon";
    ARGV_t av = NULL;
    int xx = argvSplit(&av, foo, NULL);
    key_serial_t dest = 0;
    key_serial_t key = 0;

    if (password != NULL) {
	free(password);
	password = NULL;
    }
assert(av != NULL);
assert(av[0] != NULL);
assert(av[1] != NULL);
assert(av[2] != NULL);
    key = request_key(av[0], av[1], av[2], dest);

/*@-nullstate@*/	/* XXX *password may be null. */
    xx = keyctl_read_alloc(key, (void *)&password);
/*@=nullstate@*/
assert(password != NULL);
#endif

/*@-statictrans@*/
    return password;
/*@=statictrans@*/
}

/*@-redecl@*/
char * (*Getpass) (const char * prompt) = _GetPass;
/*@=redecl@*/
