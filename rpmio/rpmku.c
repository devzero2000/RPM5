/** \ingroup rpmio
 * \file rpmio/rpmku.c
 */

#include "system.h"

#include <rpmio.h>
#if defined(HAVE_KEYUTILS_H)
#include <rpmmacro.h>
#include <argv.h>
#include <keyutils.h>
#define _RPMPGP_INTERNAL
#include <rpmpgp.h>
#endif
#include <rpmku.h>

#include "debug.h"

/*@access pgpDigParams@ */

#if defined(HAVE_KEYUTILS_H)
/*@unchecked@*/
int32_t _kuKeyring;

/*@unchecked@*/
static int _kuCache = 1;

typedef struct _kuItem_s {
/*@observer@*/
    const char *name;
    key_serial_t val;
} * _kuItem;

/* NB: the following table must be sorted lexically for bsearch(3). */
/*@unchecked@*/ /*@observer@*/
static struct _kuItem_s kuTable[] = {
    { "group",		KEY_SPEC_GROUP_KEYRING },
    { "process",	KEY_SPEC_PROCESS_KEYRING },
    { "session",	KEY_SPEC_SESSION_KEYRING },
    { "thread",		KEY_SPEC_THREAD_KEYRING },
    { "user",		KEY_SPEC_USER_KEYRING },
    { "user_session",	KEY_SPEC_USER_SESSION_KEYRING },
#ifdef	NOTYET	/* XXX is this useful? */
  { "???",		KEY_SPEC_REQKEY_AUTH_KEY },
#endif
};

/*@unchecked@*/
static size_t nkuTable = sizeof(kuTable) / sizeof(kuTable[0]);

static int
kuCmp(const void * a, const void * b)
	/*@*/
{
    return strcmp(((_kuItem)a)->name, ((_kuItem)b)->name);
}

static key_serial_t
kuValue(const char * name)
	/*@*/
{
    _kuItem k = NULL;

    if (name != NULL && *name != '\0') {
	_kuItem tmp = memset(alloca(sizeof(*tmp)), 0, sizeof(*tmp));
/*@-temptrans@*/
	tmp->name = name;
/*@=temptrans@*/
	k = (_kuItem)bsearch(tmp, kuTable, nkuTable, sizeof(kuTable[0]), kuCmp);
    }
    return (k != NULL ? k->val :  0);
}
#endif

/*@-globs -internalglobs -mods @*/
char * _GetPass(const char * prompt)
{
    char * pw;

/*@-unrecog@*/
    pw = getpass( prompt ? prompt : "" );
/*@=unrecog@*/

#if defined(HAVE_KEYUTILS_H)
    if (_kuKeyring == 0) {
	const char * _keyutils_keyring
		= rpmExpand("%{?_keyutils_keyring}", NULL);
	_kuKeyring = (uint32_t) kuValue(_keyutils_keyring);
	if (_kuKeyring == 0)
	    _kuKeyring = KEY_SPEC_PROCESS_KEYRING;
	_keyutils_keyring = _free(_keyutils_keyring);
    }

    if (pw && *pw) {
	key_serial_t keyring = (key_serial_t) _kuKeyring;
	size_t npw = strlen(pw);
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
/*@=globs =internalglobs =mods @*/

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

rpmRC rpmkuFindPubkey(pgpDigParams sigp, /*@out@*/ uint8_t ** bp, size_t * blenp)
{
    if (bp != NULL)
	*bp = NULL;
    if (blenp != NULL)
	*blenp = 0;

#if defined(HAVE_KEYUTILS_H)
    if (_kuCache) {
/*@observer@*/
	static const char krprefix[] = "rpm:gpg:pubkey:";
	key_serial_t keyring = (key_serial_t) _kuKeyring;
	char krfp[32];
	char * krn = alloca(strlen(krprefix) + sizeof("12345678"));
	long key;
	int xx;

	(void) snprintf(krfp, sizeof(krfp), "%08X", pgpGrab(sigp->signid+4, 4));
	krfp[sizeof(krfp)-1] = '\0';
	*krn = '\0';
	(void) stpcpy( stpcpy(krn, krprefix), krfp);

	key = keyctl_search(keyring, "user", krn, 0);
	xx = keyctl_read(key, NULL, 0);
	if (xx > 0) {
	    uint8_t * b = NULL;
	    ssize_t blen = xx;
	    xx = keyctl_read_alloc(key, (void **)&b);
	    if (xx > 0) {
#ifdef	NOTYET
		pubkeysource = xstrdup(krn);
		_kuCache = 0;	/* XXX don't bother caching. */
#endif
	    } else
		b = _free(b);

	    if (b != NULL) {
		if (bp)
		    *bp = b;
		if (blenp)
		    *blenp = blen;
		return RPMRC_OK;
	    } else
		return RPMRC_NOTFOUND;
	} else
	    return RPMRC_NOTFOUND;
    } else
#endif
    return RPMRC_NOTFOUND;
}

rpmRC rpmkuStorePubkey(pgpDigParams sigp, /*@only@*/ uint8_t * b, size_t blen)
{
#if defined(HAVE_KEYUTILS_H)
    if (_kuCache) {
/*@observer@*/
	static const char krprefix[] = "rpm:gpg:pubkey:";
	key_serial_t keyring = (key_serial_t) _kuKeyring;
	char krfp[32];
	char * krn = alloca(strlen(krprefix) + sizeof("12345678"));

	(void) snprintf(krfp, sizeof(krfp), "%08X", pgpGrab(sigp->signid+4, 4));
	krfp[sizeof(krfp)-1] = '\0';
	*krn = '\0';
	(void) stpcpy( stpcpy(krn, krprefix), krfp);
/*@-moduncon -noeffectuncon @*/
	(void) add_key("user", krn, b, blen, keyring);
/*@=moduncon =noeffectuncon @*/
    }
#endif
    b = _free(b);
    return RPMRC_OK;
}

const char * rpmkuPassPhrase(const char * passPhrase)
{
    const char * pw;

#if defined(HAVE_KEYUTILS_H)
    if (passPhrase && !strcmp(passPhrase, "@u user rpm:passwd")) {
	key_serial_t keyring = (key_serial_t) _kuKeyring;
	long key;
	int xx;

/*@-moduncon@*/
	key = keyctl_search(keyring, "user", "rpm:passwd", 0);
	pw = NULL;
	xx = keyctl_read_alloc(key, (void **)&pw);
/*@=moduncon@*/
	if (xx < 0)
	    pw = NULL;
    } else
#endif
	pw = xstrdup(passPhrase);
    return pw;
}
