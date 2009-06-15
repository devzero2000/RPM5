/** \ingroup rpmio
 * \file rpmio/rpmaug.c
 */

#include "system.h"

#if defined(WITH_AUGEAS) && defined(HAVE_AUGEAS_H)
#include "augeas.h"
#endif

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#define	_RPMAUG_INTERNAL
#include <rpmaug.h>

#include "debug.h"

/*@unchecked@*/
int _rpmaug_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmaug _rpmaugI = NULL;

/*@-mustmod@*/	/* XXX splint on crack */
static void rpmaugFini(void * _aug)
	/*@globals fileSystem @*/
	/*@modifies *_aug, fileSystem @*/
{
    rpmaug aug = _aug;

#if defined(WITH_AUGEAS)
    (void) aug_close(aug->I);
#endif
    aug->I = NULL;
    (void)rpmiobFree(aug->iob);
    aug->iob = NULL;
    aug->root = _free(aug->root);
    aug->loadpath = _free(aug->loadpath);
}
/*@=mustmod@*/

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmaugPool = NULL;

static rpmaug rpmaugGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmaugPool, fileSystem @*/
	/*@modifies pool, _rpmaugPool, fileSystem @*/
{
    rpmaug aug;

    if (_rpmaugPool == NULL) {
	_rpmaugPool = rpmioNewPool("aug", sizeof(*aug), -1, _rpmaug_debug,
			NULL, NULL, rpmaugFini);
	pool = _rpmaugPool;
    }
    return (rpmaug) rpmioGetPool(pool, sizeof(*aug));
}

/*@unchecked@*/
static const char _root[] = "/";
const char * _rpmaugRoot = _root;
/*@unchecked@*/
static const char _loadpath[] = "";	/* XXX /usr/lib/rpm/lib ? */
const char * _rpmaugLoadpath = _loadpath;
unsigned int _rpmaugFlags = 0;		/* AUG_NONE */

#if defined(REFERENCE)
__attribute__((noreturn))
static void usage(void)
{
    fprintf(stderr, "Usage: %s [OPTIONS] [COMMAND]\n", progname);
    fprintf(stderr, "Load the Augeas tree and modify it. If no COMMAND is given, run interactively\n");
    fprintf(stderr, "Run '%s help' to get a list of possible commands.\n",
            progname);
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  -c, --typecheck    typecheck lenses\n");
    fprintf(stderr, "  -b, --backup       preserve originals of modified files with\n"
                    "                     extension '.augsave'\n");
    fprintf(stderr, "  -n, --new          save changes in files with extension '.augnew',\n"
                    "                     leave original unchanged\n");
    fprintf(stderr, "  -r, --root ROOT    use ROOT as the root of the filesystem\n");
    fprintf(stderr, "  -I, --include DIR  search DIR for modules; can be given mutiple times\n");
    fprintf(stderr, "  --nostdinc         do not search the builtin default directories for modules\n");
    fprintf(stderr, "  --noload           do not load any files into the tree on startup\n");
    fprintf(stderr, "  --noautoload       do not autoload modules from the search path\n");

    exit(EXIT_FAILURE);
}
#endif

/*@unchecked@*/ /*@null@*/
const char ** _rpmaugLoadargv;

struct poptOption rpmaugPoptTable[] = {
#if defined(WITH_AUGEAS)
    /* XXX POPT_ARGFLAG_TOGGLE? */
    { "typecheck",'c', POPT_BIT_SET,		&_rpmaugFlags, AUG_TYPE_CHECK,
	N_("Type check lenses"), NULL },
    { "backup", 'b',POPT_BIT_SET,		&_rpmaugFlags, AUG_SAVE_BACKUP,
	N_("Backup modified files with suffix '.augsave'"), NULL },
    { "new", 'n', POPT_BIT_SET,			&_rpmaugFlags, AUG_SAVE_NEWFILE,
	N_("Save modified files with suffix '.augnew'"), NULL },
    { "root", 'r', POPT_ARG_STRING,		&_rpmaugRoot, 0,
	N_("Use ROOT as the root of the filesystem"), N_("ROOT") },
    { "include", 'I', POPT_ARG_ARGV,		&_rpmaugLoadargv, 0,
	N_("Search DIR for modules (may be used more than once)"), N_("DIR") },
    { "nostdinc", '\0', POPT_BIT_SET,		&_rpmaugFlags, AUG_NO_STDINC,
	N_("Do not search default modules path"), NULL },
    { "noload", '\0', POPT_BIT_SET,		&_rpmaugFlags, AUG_NO_LOAD,
	N_("Do not load files into tree on startup"), NULL },
    { "noautoload", '\0', POPT_BIT_SET,	 &_rpmaugFlags, AUG_NO_MODL_AUTOLOAD,
	N_("Do not autoload modules from the search path"), NULL },
#endif
    POPT_TABLEEND
};

rpmaug rpmaugNew(const char * root, const char * loadpath, unsigned int flags)
{
    rpmaug aug = rpmaugGetPool(_rpmaugPool);

    if (root == NULL)
	root = _rpmaugRoot;
    if (loadpath == NULL)
	loadpath = _rpmaugLoadpath;
    aug->root = xstrdup(root);
    aug->loadpath = xstrdup(loadpath);
    aug->flags = flags;
#if defined(WITH_AUGEAS)
    aug->I = (void *) aug_init(aug->root, aug->loadpath, aug->flags);
assert(aug->I != NULL);
#endif
    aug->iob = rpmiobNew(0);

    return rpmaugLink(aug);
}

#ifdef	WITH_AUGEAS
static rpmaug rpmaugI(void)
        /*@globals _rpmaugI @*/
        /*@modifies _rpmaugI @*/
{
    if (_rpmaugI == NULL)
        _rpmaugI = rpmaugNew(_rpmaugRoot, _rpmaugLoadpath, _rpmaugFlags);
    return _rpmaugI;
}
#endif

int rpmaugDefvar(rpmaug aug, const char * name, const char * expr)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_defvar(aug->I, name, expr);
#endif
    return rc;
}

int rpmaugDefnode(rpmaug aug, const char * name, const char * expr,
		const char * value, int * created)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_defnode(aug->I, name, expr, value, created);
#endif
    return rc;
}

int rpmaugGet(rpmaug aug, const char * path, const char ** value)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_get(aug->I, path, value);
#endif
    return rc;
}

int rpmaugSet(rpmaug aug, const char * path, const char * value)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_set(aug->I, path, value);
#endif
    return rc;
}

int rpmaugInsert(rpmaug aug, const char * path, const char * label, int before)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_insert(aug->I, path, label, before);
#endif
    return rc;
}

int rpmaugRm(rpmaug aug, const char * path)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_rm(aug->I, path);
#endif
    return rc;
}

int rpmaugMv(rpmaug aug, const char * src, const char * dst)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_mv(aug->I, src, dst);
#endif
    return rc;
}

int rpmaugMatch(rpmaug aug, const char * path, char *** matches)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_match(aug->I, path, matches);
#endif
    return rc;
}

int rpmaugSave(rpmaug aug)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_save(aug->I);
#endif
    return rc;
}

int rpmaugLoad(rpmaug aug)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_load(aug->I);
#endif
    return rc;
}

int rpmaugPrint(rpmaug aug, FILE *out, const char * path)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    if (aug == NULL) aug = rpmaugI();
    rc = aug_print(aug->I, out, path);
#endif
    return rc;
}

void rpmaugFprintf(rpmaug aug, const char *fmt, ...)
{
#if defined(WITH_AUGEAS)
    size_t nb = 1024;
    char * b = xmalloc(nb);
    va_list va;

    va_start(va, fmt);
    while(1) {
	int nw = vsnprintf(b, nb, fmt, va);
	if (nw > -1 && (size_t)nw < nb)
	    break;
	if (nb > -1)		/* glibc 2.1 (and later) */
	    nb = nw+1;
	else			/* glibc 2.0 */
	    nb *= 2;
	b = xrealloc(b, nb);
    }
    va_end(va);

    if (aug == NULL) aug = rpmaugI();
    (void) rpmiobAppend(aug->iob, b, 0);
    b = _free(b);
#endif
}
