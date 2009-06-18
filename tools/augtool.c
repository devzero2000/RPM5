/*
 * augtool.c:
 *
 * Copyright (C) 2007, 2008 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: David Lutterkort <dlutter@redhat.com>
 */

#include "system.h"

#include <readline/readline.h>
#include <readline/history.h>

#define	_RPMIOB_INTERNAL
#include <rpmiotypes.h>
#include <poptIO.h>
#include <argv.h>

#define	_RPMAUG_INTERNAL
#include <rpmaug.h>

#include "debug.h"

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

#if defined(REFERENCE)
/* Define: AUGEAS_ROOT_ENV
 * The env var that points to the chroot holding files we may modify.
 * Mostly useful for testing */
#define AUGEAS_ROOT_ENV "AUGEAS_ROOT"

/* Define: AUGEAS_FILES_TREE
 * The root for actual file contents */
#define AUGEAS_FILES_TREE "/files"

/* Define: AUGEAS_META_TREE
 * Augeas reports some information in this subtree */
#define AUGEAS_META_TREE "/augeas"

/* Define: AUGEAS_META_FILES
 * Information about files */
#define AUGEAS_META_FILES AUGEAS_META_TREE AUGEAS_FILES_TREE

/* Define: AUGEAS_META_ROOT
 * The root directory */
#define AUGEAS_META_ROOT AUGEAS_META_TREE "/root"

/* Define: AUGEAS_META_SAVE_MODE
 * How we save files. One of 'backup', 'overwrite' or 'newfile' */
#define AUGEAS_META_SAVE_MODE AUGEAS_META_TREE "/save"

/* Define: AUGEAS_CLONE_IF_RENAME_FAILS
 * Control what save does when renaming the temporary file to its final
 * destination fails with EXDEV or EBUSY: when this tree node exists, copy
 * the file contents. If it is not present, simply give up and report an
 * error.  */
#define AUGEAS_COPY_IF_RENAME_FAILS \
    AUGEAS_META_SAVE_MODE "/copy_if_rename_fails"

/* A hierarchy where we record certain 'events', e.g. which tree
 * nodes actually gotsaved into files */
#define AUGEAS_EVENTS AUGEAS_META_TREE "/events"

#define AUGEAS_EVENTS_SAVED AUGEAS_EVENTS "/saved"

/* Where to put information about parsing of path expressions */
#define AUGEAS_META_PATHX AUGEAS_META_TREE "/pathx"

/* Define: AUGEAS_LENS_ENV
 * Name of env var that contains list of paths to search for additional
   spec files */
#define AUGEAS_LENS_ENV "AUGEAS_LENS_LIB"

/* Define: MAX_ENV_SIZE
 * Fairly arbitrary bound on the length of the path we
 *  accept from AUGEAS_SPEC_ENV */
#define MAX_ENV_SIZE 4096

/* Constants for setting the save mode via the augeas path at
 * AUGEAS_META_SAVE_MODE */
#define AUG_SAVE_BACKUP_TEXT "backup"
#define AUG_SAVE_NEWFILE_TEXT "newfile"
#define AUG_SAVE_NOOP_TEXT "noop"
#define AUG_SAVE_OVERWRITE_TEXT "overwrite"
#endif	/* REFERENCE */

/* Define: PATH_SEP_CHAR
 * Character separating paths in a list of paths */
#define PATH_SEP_CHAR ':'

/* ===== */

struct command {
    const char *name;
    int minargs;
    int maxargs;
    int(*handler) (int ac, char *av[]);
    const char *synopsis;
    const char *help;
};

static const struct command const commands[];

static const char *const progname = "augtool";

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
    const struct command *c;

    rpmaugFprintf(NULL, "Commands:\n\n");
    for (c=commands; c->name != NULL; c++) {
        rpmaugFprintf(NULL, "    %s\n        %s\n\n", c->synopsis, c->help);
    }
    rpmaugFprintf(NULL, "\nEnvironment:\n\n");
    rpmaugFprintf(NULL, "    AUGEAS_ROOT\n        the file system root, defaults to '/'\n\n");
    rpmaugFprintf(NULL, "    AUGEAS_LENS_LIB\n        colon separated list of directories with lenses,\n\
        defaults to " AUGEAS_LENS_DIR "\n\n");
    return 0;
}

static const struct command const commands[] = {
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

typedef struct rpmaup_s {
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

static rpmRC rpmaugRun(rpmaug aug, const char * str, const char ** resultp)
{
    rpmaugP P = NULL;
    rpmRC rc = RPMRC_OK;	/* assume success */
    int xx;

    if (aug == NULL)	aug = _rpmaugI;

    if (resultp)
	*resultp = NULL;

    while (rpmaugCommand(&P, str) != RPMRC_NOTFOUND) {	/* XXX exit on EOS */
	const struct command *c;
	str = NULL;

	if (P->av && P->ac > 0 && P->av[0] != NULL && strlen(P->av[0]) > 0) {

	    for (c = commands; c->name; c++) {
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
    {	rpmiob iob = aug->iob;
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

static char *readline_path_generator(const char *text, int state)
{
    static int current = 0;
    static char **children = NULL;
    static int nchildren = 0;

    if (state == 0) {
        char *end = strrchr(text, SEP);
        char *path;
        if (end == NULL) {
            if ((path = strdup("/*")) == NULL)
                return NULL;
        } else {
            end += 1;
	    path = xcalloc(1, end - text + 2);
            if (path == NULL)
                return NULL;
            strncpy(path, text, end - text);
            strcat(path, "*");
        }

        for (;current < nchildren; current++)
            free((void *) children[current]);
        free((void *) children);
        nchildren = rpmaugMatch(NULL, path, &children);
        current = 0;
        free(path);
    }

    while (current < nchildren) {
        char *child = children[current];
        current += 1;
        if (!strncmp(child, text, strlen(text))) {
            if (child_count(child) > 0) {
                char *c = realloc(child, strlen(child)+2);
                if (c == NULL)
                    return NULL;
                child = c;
                strcat(child, "/");
            }
            rl_filename_completion_desired = 1;
            rl_completion_append_character = '\0';
            return child;
        } else {
            free(child);
        }
    }
    return NULL;
}

static char *readline_command_generator(const char *text, int state)
{
    static int current = 0;
    const char *name;

    if (state == 0)
        current = 0;

    rl_completion_append_character = ' ';
    while ((name = commands[current].name) != NULL) {
        current += 1;
        if (!strncmp(text, name, strlen(text)))
            return strdup(name);
    }
    return NULL;
}

#define	HAVE_RL_COMPLETION_MATCHES	/* XXX no AutoFu yet */
#ifndef HAVE_RL_COMPLETION_MATCHES
typedef char *rl_compentry_func_t(const char *, int);
static char **rl_completion_matches(/*@unused@*/ const char *text,
                           /*@unused@*/ rl_compentry_func_t *func)
{
    return NULL;
}
#endif

static char **readline_completion(const char *text, int start,
		/*@unused@*/ int end)
{
    if (start == 0)
        return rl_completion_matches(text, readline_command_generator);
    else
        return rl_completion_matches(text, readline_path_generator);

    return NULL;
}

static void readline_init(void)
{
    rl_readline_name = "augtool";
    rl_attempted_completion_function = readline_completion;
    rl_completion_entry_function = readline_path_generator;
}

static int main_loop(void)
{
    char *line = NULL;
    size_t len = 0;
    int ret = 0;	/* assume success */

    while (1) {
	const char *buf;

        if (isatty(fileno(stdin))) {
            line = readline("augtool> ");
        } else if (getline(&line, &len, stdin) == -1) {
	    break;
        }
        cleanstr(line, '\n');
        if (line == NULL) {
            printf("\n");
	    break;
        }
        if (line[0] == '#')
            continue;

	buf = NULL;
	if (rpmaugRun(NULL, line, &buf) == RPMRC_OK) {
	    if (isatty(fileno(stdin)))
		add_history(line);
	}
	if (buf && *buf)
	    fprintf(stdout, "%s", buf);
    }
    line = _free(line);
    return ret;
}

/*@unchecked@*/ /*@null@*/
extern const char ** _rpmaugLoadargv;

static struct poptOption _optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmaugPoptTable, 0,
        N_("Options:"), NULL },

#ifdef	NOTYET	/* XXX dueling --root options */
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
        N_("Common options for all rpmio executables:"),
        NULL },

    POPT_AUTOALIAS
#endif

    POPT_AUTOHELP
    POPT_TABLEEND
};

static struct poptOption *optionsTable = &_optionsTable[0];

int main(int argc, char **argv)
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    const char ** av = NULL;
    int ac;
    int r;

    if (_rpmaugLoadargv != NULL)
	_rpmaugLoadpath = argvJoin(_rpmaugLoadargv, PATH_SEP_CHAR);

    _rpmaugI = rpmaugNew(_rpmaugRoot, _rpmaugLoadpath, _rpmaugFlags);
    if (_rpmaugI == NULL) {
        fprintf(stderr, "Failed to initialize Augeas\n");
        exit(EXIT_FAILURE);
    }
    readline_init();

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac > 0) {	// Accept one command from the command line
	const char * cmd = argvJoin((const char **)av, ' ');
	const char *buf;

        r = rpmaugRun(NULL, cmd, &buf);
	cmd = _free(cmd);
	if (buf && *buf)
	    fprintf(stdout, "%s", buf);
    } else {
        r = main_loop();
    }

    if (_rpmaugLoadargv)
	_rpmaugLoadpath = _free(_rpmaugLoadpath);
    _rpmaugLoadargv = argvFree(_rpmaugLoadargv);
    _rpmaugI = rpmaugFree(_rpmaugI);

    optCon = rpmioFini(optCon);

    return r == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
