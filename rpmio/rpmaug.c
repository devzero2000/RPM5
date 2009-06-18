/** \ingroup rpmio
 * \file rpmio/rpmaug.c
 */

#include "system.h"

#if defined(WITH_AUGEAS) && defined(HAVE_AUGEAS_H)
#include "augeas.h"
#endif

#define _RPMIOB_INTERNAL
#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <argv.h>

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
	if (nw > -1)		/* glibc 2.1 (and later) */
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

/* ===== internal.h */

#if !defined(SEP)
#define	SEP	'/'
#endif

#define DATADIR "/usr/share"

/* Define: AUGEAS_LENS_DIR
 * The default location for lens definitions */
#define AUGEAS_LENS_DIR DATADIR "/augeas/lenses"

/* The directory where we install lenses distribute with Augeas */
#define AUGEAS_LENS_DIST_DIR DATADIR "/augeas/lenses/dist"

/* Define: PATH_SEP_CHAR
 * Character separating paths in a list of paths
 */
#define PATH_SEP_CHAR ':'

/* ===== */

static char *cleanstr(char *path, const char sep)
{
    if (path && *path) {
	char *e = path + strlen(path) - 1;
	while (e >= path && (*e == sep || xisspace(*e)))
	    *e-- = '\0';
    }
    return path;
}

static char *cleanpath(char *path)
{
    return cleanstr(path, SEP);		/* XXX strip pesky trailing slashes */
}

static char *ls_pattern(const char *path)
{
    char *q;
    int r;

    if (path[strlen(path)-1] == SEP)
        r = asprintf(&q, "%s*", path);
    else
        r = asprintf(&q, "%s/*", path);
    if (r == -1)
        return NULL;
    return q;
}

static int child_count(const char *path)
{
    char *q = ls_pattern(path);
    int cnt;

    if (q == NULL)
        return 0;
    cnt = rpmaugMatch(NULL, q, NULL);
    free(q);
    return cnt;
}

static int cmd_quit(int ac, char *av[])
{
    exit(EXIT_SUCCESS);
}

static int cmd_ls(int ac, char *av[])
{
    char *path = cleanpath(av[0]);
    char **paths;
    int cnt;
    int i;

    path = ls_pattern(path);
    if (path == NULL)
        return -1;
    cnt = rpmaugMatch(NULL, path, &paths);
    for (i=0; i < cnt; i++) {
        const char *basnam = strrchr(paths[i], SEP);
        int dir = child_count(paths[i]);
        const char *val;
        rpmaugGet(NULL, paths[i], &val);
        basnam = (basnam == NULL) ? paths[i] : basnam + 1;
        if (val == NULL)
            val = "(none)";
        rpmaugFprintf(NULL, "%s%s= %s\n", basnam, dir ? "/ " : " ", val);
    }
    (void) argvFree((const char **)paths);
    path = _free(path);
    return 0;
}

static int cmd_match(int ac, char *av[])
{
    const char *pattern = cleanpath(av[0]);
    int filter = (av[1] != NULL) && (strlen(av[1]) > 0);
    int result = 0;
    char **matches = NULL;
    int cnt;
    int i;

    cnt = rpmaugMatch(NULL, pattern, &matches);
    if (cnt < 0) {
        rpmaugFprintf(NULL, "  (error matching %s)\n", pattern);
        result = -1;
        goto done;
    }
    if (cnt == 0) {
        rpmaugFprintf(NULL, "  (no matches)\n");
        goto done;
    }

    for (i=0; i < cnt; i++) {
        const char *val;
        rpmaugGet(NULL, matches[i], &val);
        if (val == NULL)
            val = "(none)";
        if (filter) {
            if (!strcmp(av[1], val))
                rpmaugFprintf(NULL, "%s\n", matches[i]);
        } else {
            rpmaugFprintf(NULL, "%s = %s\n", matches[i], val);
        }
    }
 done:
    (void) argvFree((const char **)matches);
    return result;
}

static int cmd_rm(int ac, char *av[])
{
    const char *path = cleanpath(av[0]);
    int cnt;

    rpmaugFprintf(NULL, "rm : %s", path);
    cnt = rpmaugRm(NULL, path);
    rpmaugFprintf(NULL, " %d\n", cnt);
    return 0;
}

static int cmd_mv(int ac, char *av[])
{
    const char *src = cleanpath(av[0]);
    const char *dst = cleanpath(av[1]);
    int r = rpmaugMv(NULL, src, dst);

    return r;
}

static int cmd_set(int ac, char *av[])
{
    const char *path = cleanpath(av[0]);
    const char *val = av[1];
    int r = rpmaugSet(NULL, path, val);

    return r;
}

static int cmd_defvar(int ac, char *av[])
{
    const char *name = av[0];
    const char *path = cleanpath(av[1]);
    int r = rpmaugDefvar(NULL, name, path);

    return r;
}

static int cmd_defnode(int ac, char *av[])
{
    const char *name = av[0];
    const char *path = cleanpath(av[1]);
    const char *value = av[2];
    int r;

    /* Our simple minded line parser treats non-existant and empty values
     * the same. We choose to take the empty string to mean NULL */
    if (value != NULL && value[0] == '\0')
        value = NULL;

    r = rpmaugDefnode(NULL, name, path, value, NULL);

    return r;
}

static int cmd_clear(int ac, char *av[])
{
    const char *path = cleanpath(av[0]);
    int r = rpmaugSet(NULL, path, NULL);

    return r;
}

static int cmd_get(int ac, char *av[])
{
    const char *path = cleanpath(av[0]);
    const char *val;

    rpmaugFprintf(NULL, "%s", path);
    if (rpmaugGet(NULL, path, &val) != 1)
        rpmaugFprintf(NULL, " (o)\n");
    else if (val == NULL)
        rpmaugFprintf(NULL, " (none)\n");
    else
        rpmaugFprintf(NULL, " = %s\n", val);
    return 0;
}

static int cmd_print(int ac, char *av[])
{
    return rpmaugPrint(NULL, stdout, cleanpath(av[0]));
}

static int cmd_save(int ac, /*@unused@*/ char *av[])
{
    int r = rpmaugSave(NULL);

    if (r != -1) {
        r = rpmaugMatch(NULL, "/augeas/events/saved", NULL);
        if (r > 0)
            rpmaugFprintf(NULL, "Saved %d file(s)\n", r);
        else if (r < 0)
            rpmaugFprintf(NULL, "Error during match: %d\n", r);
    }
    return r;
}

static int cmd_load(int ac, /*@unused@*/ char *av[])
{
    int r = rpmaugLoad(NULL);
    if (r != -1) {
        r = rpmaugMatch(NULL, "/augeas/events/saved", NULL);
        if (r > 0)
            rpmaugFprintf(NULL, "Saved %d file(s)\n", r);
        else if (r < 0)
            rpmaugFprintf(NULL, "Error during match: %d\n", r);
    }
    return r;
}

static int cmd_ins(int ac, char *av[])
{
    const char *label = av[0];
    const char *where = av[1];
    const char *path = cleanpath(av[2]);
    int before;
    int r = -1;	/* assume failure */

    if (!strcmp(where, "after"))
        before = 0;
    else if (!strcmp(where, "before"))
        before = 1;
    else {
        rpmaugFprintf(NULL, "The <WHERE> argument must be either 'before' or 'after'.");
	goto exit;
    }

    r = rpmaugInsert(NULL, path, label, before);

exit:
    return r;
}

static int cmd_help(int ac, /*@unused@*/ char *av[])
{
    rpmaugC c;

    rpmaugFprintf(NULL, "Commands:\n\n");
    for (c=(rpmaugC)_rpmaugCommands; c->name != NULL; c++) {
        rpmaugFprintf(NULL, "    %s\n        %s\n\n", c->synopsis, c->help);
    }
    rpmaugFprintf(NULL, "\nEnvironment:\n\n");
    rpmaugFprintf(NULL, "    AUGEAS_ROOT\n        the file system root, defaults to '/'\n\n");
    rpmaugFprintf(NULL, "    AUGEAS_LENS_LIB\n        colon separated list of directories with lenses,\n\
        defaults to " AUGEAS_LENS_DIR "\n\n");
    return 0;
}

const struct rpmaugC_s const _rpmaugCommands[] = {
    { "exit",  0, 0, cmd_quit, "exit",
      "Exit the program"
    },
    { "quit",  0, 0, cmd_quit, "quit",
      "Exit the program"
    },
    { "ls",  1, 1, cmd_ls, "ls <PATH>",
      "List the direct children of PATH"
    },
    { "match",  1, 2, cmd_match, "match <PATH> [<VALUE>]",
      "Find all paths that match the path expression PATH. If VALUE is given,\n"
      "        only the matching paths whose value equals VALUE are printed"
    },
    { "rm",  1, 1, cmd_rm, "rm <PATH>",
      "Delete PATH and all its children from the tree"
    },
    { "mv", 2, 2, cmd_mv, "mv <SRC> <DST>",
      "Move node SRC to DST. SRC must match exactly one node in the tree.\n"
      "        DST must either match exactly one node in the tree, or may not\n"
      "        exist yet. If DST exists already, it and all its descendants are\n"
      "        deleted. If DST does not exist yet, it and all its missing \n"
      "        ancestors are created." },
    { "set", 1, 2, cmd_set, "set <PATH> <VALUE>",
      "Associate VALUE with PATH. If PATH is not in the tree yet,\n"
      "        it and all its ancestors will be created. These new tree entries\n"
      "        will appear last amongst their siblings"
    },
    { "clear", 1, 1, cmd_clear, "clear <PATH>",
      "Set the value for PATH to NULL. If PATH is not in the tree yet,\n"
      "        it and all its ancestors will be created. These new tree entries\n"
      "        will appear last amongst their siblings"
    },
    { "get", 1, 1, cmd_get, "get <PATH>",
      "Print the value associated with PATH"
    },
    { "print", 0, 1, cmd_print, "print [<PATH>]",
      "Print entries in the tree. If PATH is given, printing starts there,\n"
      "        otherwise the whole tree is printed"
    },
    { "ins", 3, 3, cmd_ins, "ins <LABEL> <WHERE> <PATH>",
      "Insert a new node with label LABEL right before or after PATH into\n"
     "        the tree. WHERE must be either 'before' or 'after'."
    },
    { "save", 0, 0, cmd_save, "save",
      "Save all pending changes to disk. For now, files are not overwritten.\n"
      "        Instead, new files with extension .augnew are created"
    },
    { "load", 0, 0, cmd_load, "load",
      "Load files accordig to the transforms in /augeas/load."
    },
    { "defvar", 2, 2, cmd_defvar, "defvar <NAME> <EXPR>",
      "Define the variable NAME to the result of evalutating EXPR. The\n"
      "        variable can be used in path expressions as $NAME. Note that EXPR\n"
      "        is evaluated when the variable is defined, not when it is used."
    },
    { "defnode", 2, 3, cmd_defnode, "defnode <NAME> <EXPR> [<VALUE>]",
      "Define the variable NAME to the result of evalutating EXPR, which\n"
      "        must be a nodeset. If no node matching EXPR exists yet, one\n"
      "        is created and NAME will refer to it. If VALUE is given, this\n"
      "        is the same as 'set EXPR VALUE'; if VALUE is not given, the\n"
      "        node is created as if with 'clear EXPR' would and NAME refers\n"
      "        to that node."
    },
    { "help", 0, 0, cmd_help, "help",
      "Print this help text"
    },
    { NULL, -1, -1, NULL, NULL, NULL }
};

/**
 * Return text between pl and matching pr characters.
 * @param p		start of text
 * @param pl		left char, i.e. '[', '(', '{', etc.
 * @param pr		right char, i.e. ']', ')', '}', etc.
 * @return		pointer to matching pr char in string (or NULL)
 */
/*@null@*/
static char *
matchchar(char * p, char pl, char pr)
        /*@*/
{
    int lvl = 0;
    char c;

    while ((c = *p++) != '\0') {
        if (c == '\\') {	/* Ignore escaped chars */
            p++;
            continue;
        }
        if (c == pr) {
            if (--lvl <= 0)     return --p;
        } else if (c == pl)
            lvl++;
    }
    return (char *)NULL;
}

typedef struct rpmaugP_s {
    char * str;
    char * next;
    const char ** av;
    int ac;
} * rpmaugP;

static rpmRC rpmaugCommand(rpmaugP *Pptr, const char * str)
{
    static char whitespace[] = " \t\n\r";
    rpmaugP P;
    rpmRC rc;
    char *b;
    char *be;
    int xx;
    int c;

    if ((P = *Pptr) == NULL)
	*Pptr = P = xcalloc(1, sizeof(*P));

    if (str != NULL) {
	P->str = _free(P->str);
	P->next = P->str = xstrdup(str);
    }

    /* Ltrim whitespace. Anything left to parse? */
    if ((b = P->next) != NULL)
    while (*b && strchr(whitespace, *b))
	*b++ = '\0';
    if (b == NULL || *b == '\0')
	return RPMRC_NOTFOUND;

    /* Parse next command into an argv. */
    P->ac = 0;
    P->av = argvFree(P->av);
    if ((be = b) != NULL)
  while (1) {
    c = *be++;
    switch (c) {
    default:
	break;
    case '\\':		/* escaped character. */
	if (*be != '\0')
	    be++;
	break;
    case '\0':		/* end-of-command termination. */
    case '\n':
    case '\r':
    case ';':	
	if (be[-1] != '\0')
	    be[-1] = '\0';
	else
	    be--;			/* XXX one too far */
	if ((be - b) > 1) {
	    xx = argvAdd(&P->av, b);
	    P->ac++;
	}
	goto exit;
	break;
    case '[':		/* XPath construct with '[' balancing. */
	if ((be = matchchar(be, '[', ']')) == NULL) {
	    be += strlen(b);	/* XXX unmatched ']' */
	    goto exit;
	}
	be++;
	break;
    case '"':		/* quoted string */
	while (1) {
	    if ((be = strchr(be, '"')) == NULL) {
		be += strlen(b);	/* XXX unmatched '"' */
		goto exit;
	    }
	    be++;
	    if (be[-2] == '\\')	/* escaped quote */
		continue;
	    break;
	}
	break;
    case ' ':		/* argument separator */
    case '\t':
	be[-1] = '\0';
	if ((be - b) > 1) {
	    xx = argvAdd(&P->av, b);
	    P->ac++;
	}
	b = be;
	while (*b && (*b == ' ' || *b == '\t'))
	    *b++ = '\0';
	be = b;
	break;
    }
  }
    rc = RPMRC_OK;

exit:
    P->next = be;
    return rc;
}

rpmRC rpmaugRun(rpmaug aug, const char * str, const char ** resultp)
{
    rpmaugP P = NULL;
    rpmRC rc = RPMRC_OK;	/* assume success */
    int xx;

    if (aug == NULL) aug = rpmaugI();

    if (resultp)
	*resultp = NULL;

    while (rpmaugCommand(&P, str) != RPMRC_NOTFOUND) {	/* XXX exit on EOS */
	rpmaugC c;
	str = NULL;

	if (P->av && P->ac > 0 && P->av[0] != NULL && strlen(P->av[0]) > 0) {

	    for (c = (rpmaugC) _rpmaugCommands; c->name; c++) {
	        if (!strcmp(P->av[0], c->name))
	            break;
	    }
	    if (c->name == NULL) {
	        rpmaugFprintf(NULL, "Unknown command '%s'\n", P->av[0]);
		rc = RPMRC_FAIL;
	    } else
	    if ((P->ac - 1) < c->minargs) {
		rpmaugFprintf(NULL, "Not enough arguments for %s\n", c->name);
		rc = RPMRC_FAIL;
	    } else
	    if ((P->ac - 1) > c->maxargs) {
		rpmaugFprintf(NULL, "Too many arguments for %s\n", c->name);
		rc = RPMRC_FAIL;
	    } else
	    if ((xx = (*c->handler)(P->ac-1, (char **)P->av+1)) < 0) {
	        rpmaugFprintf(NULL, "Failed(%d): %s\n", xx, P->av[0]);
		rc = RPMRC_FAIL;
	    }
	}
	if (rc != RPMRC_OK)
	    break;
    }
    if (aug != NULL) {
	rpmiob iob = aug->iob;
	if (resultp && iob->blen > 0) /* XXX return result iff bytes appended */
	    *resultp = rpmiobStr(iob);
	iob->blen = 0;			/* XXX reset the print buffer */
    }
    if (P != NULL) {
	P->str = _free(P->str);
	P->av = argvFree(P->av);
	P = _free(P);
    }
    return rc;
}
