/*****************************************************************************/
/*  libCUDF - CUDF (Common Upgrade Description Format) manipulation library  */
/*  Copyright (C) 2009  Stefano Zacchiroli <zack@pps.jussieu.fr>             */
/*                                                                           */
/*  This library is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Lesser General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.  A special linking       */
/*  exception to the GNU Lesser General Public License applies to this       */
/*  library, see the COPYING file for more information.                      */
/*****************************************************************************/

/* Compile with:

   cc -o c-test c-test.c `pkg-config --cflags cudf` `pkg-config --libs cudf`
*/

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <argv.h>
#include <poptIO.h>

#undef	WITH_GLIB
#if defined(WITH_GLIB)
#include <glib.h>
#else
#define	__G_LIB_H__
#include <assert.h>

typedef struct _GList GList;
struct _GList {
    void * data;
    GList * next;
    GList * prev;
};

static void *
g_list_nth_data (GList *list, unsigned int n)
{
    while ((n-- > 0) && list)
	list = list->next;
    return list ? list->data : NULL;
}

static GList *
g_list_last (GList *list)
{
    if (list)
    while (list->next)
	list = list->next;
  return list;
}

#define g_list_next(_list)	((_list) ? (((GList *)(_list))->next) : NULL)

typedef struct _GHashNode {
    void * key;
    void * value;
    unsigned int key_hash;
} GHashNode;

typedef struct _GHashTable {
    int              size;
    int              mod;
    unsigned int     mask;
    int              nnodes;
    int              noccupied;  /* nnodes + tombstones */
    GHashNode       *nodes;
    unsigned int   (*hash_func)  (const void * key);
    int            (*key_equal_func) (const void * a, const void * b);
    volatile int     ref_count;
    int              version;
    void           (*key_destroy_func) (void * data);
    void           (*value_destroy_func) (void * data);
} GHashTable;

static void
g_hash_table_foreach (GHashTable *hash_table,
		      void (*func) (void *key, void *value, void *user_data),
                      void * user_data)
{
    int i;

    assert(hash_table != NULL);
    assert(func != NULL);

    for (i = 0; i < hash_table->size; i++) {
	GHashNode *node = &hash_table->nodes[i];

	if (node->key_hash > 1)
	    (*func) (node->key, node->value, user_data);
    }
}

#define	g_error(...)	assert(0)

#endif	/* WITH_GLIB_H */

#include <cudf.h>

#include "debug.h"

const char *__progname;

typedef	struct rpmcudf_s * rpmcudf;

struct rpmcudf_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    cudf_t * cudf;
    cudf_doc_t * doc;
    FILE * fp;
    rpmiob iob;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

/*==============================================================*/

static const char * relops[] = {
    "?0?",		/* 0 RELOP_NOP */
    "=",		/* 1 RELOP_EQ */
    "!=",		/* 2 RELOP_NEQ */
    ">=",		/* 3 RELOP_GEQ */
    ">",		/* 4 RELOP_GT */
    "<=",		/* 5 RELOP_LEQ */
    "<",		/* 6 RELOP_LT */
    "?7?",		/* 7 */
};

/* Print a package version predicate */
static
void print_vpkg(rpmcudf cudf, cudf_vpkg_t *vpkg)
{
    FILE * fp = cudf->fp;
    if (vpkg == NULL)
	return;

    fprintf(fp, "%s", vpkg->name);
    if (vpkg->relop) {
	fprintf(fp, " ");
	fprintf(fp, "%s", relops[vpkg->relop & 0x7]);
	fprintf(fp, " %d", vpkg->version);
    }
}

/* Print a list of package predicates, separated by a given separator */
static
void print_vpkglist(rpmcudf cudf, cudf_vpkglist_t l, const char * sep)
{
    FILE * fp = cudf->fp;
    GList * last = g_list_last(l);
    cudf_vpkg_t *vpkg;

    while (l != NULL) {
	vpkg = g_list_nth_data(l, 0);
	print_vpkg(cudf, vpkg);
	if (l != last)
	    fprintf(fp, "%s", sep);
	l = g_list_next(l);
    }
}

/* Print a package formula */
static
void print_vpkgformula(rpmcudf cudf, cudf_vpkgformula_t fmla)
{
    FILE * fp = cudf->fp;
    GList * last = g_list_last(fmla);

    while (fmla != NULL) {
	print_vpkglist(cudf, g_list_nth_data(fmla, 0), " | ");
	if (fmla != last)
	    fprintf(fp, ", ");
	fmla = g_list_next(fmla);
    }
}

/* Print a CUDF preamble */
static
void print_preamble(rpmcudf cudf, cudf_doc_t *doc)
{
    FILE * fp = cudf->fp;
    static char *props[] = { "preamble", "property", "univ-checksum", "status-checksum", "req-checksum" };
    char *s;
    int i;

    if (!doc->has_preamble)
	return;

    for (i = 0 ; i < 5 ; i++) {
	s = cudf_pre_property(doc->preamble, props[i]);
	fprintf(fp, "  %s: %s\n", props[i], s);
	free(s);
    }
}

/* Print a CUDF request */
static
void print_request(rpmcudf cudf, cudf_doc_t *doc)
{
    FILE * fp = cudf->fp;
    static char *props[] = { "request", "install", "remove", "upgrade" };
    char *s;
    int i;

    if (!doc->has_request)
	return;

    for (i = 0 ; i < 4 ; i++) {
	s = cudf_req_property(doc->request, props[i]);
	fprintf(fp, "  %s: %s\n", props[i], s);
	free(s);
    }
}

/* Print a possible value of the "keep" package property */
static
void print_keep(rpmcudf cudf, int keep)
{
    FILE * fp = cudf->fp;
    switch (keep) {
    case KEEP_NONE:	fprintf(fp, "  keep: version\n");	break;
    case KEEP_VERSION:	fprintf(fp, "  keep: version\n");	break;
    case KEEP_PACKAGE:	fprintf(fp, "  keep: package\n");	break;
    case KEEP_FEATURE:	fprintf(fp, "  keep: feature\n");	break;
    default: g_error("Unexpected \"keep\" value: %d\n", keep);
    }
}

static
void print_value(rpmcudf cudf, cudf_value_t *v)
{
    FILE * fp = cudf->fp;
    if (v == NULL)
	return;

    switch (v->typ) {
    case TYPE_INT:
    case TYPE_POSINT:
    case TYPE_NAT:
	fprintf(fp, "%d", v->val.i);
	break;
    case TYPE_BOOL:
	fprintf(fp, "%s", v->val.i ? "true" : "false");
	break;
    case TYPE_STRING:
    case TYPE_PKGNAME:
    case TYPE_IDENT:
    case TYPE_ENUM:
	fprintf(fp, "%s", v->val.s);
	break;
    case TYPE_VPKG:
    case TYPE_VEQPKG:
	print_vpkg(cudf, v->val.vpkg);
	break;
    case TYPE_VPKGLIST:
    case TYPE_VEQPKGLIST:
	print_vpkglist(cudf, v->val.vpkgs, ", ");
	break;
    case TYPE_TYPEDECL:
	break;
    default:
	g_error("Internal error: unexpected variant for type: %d\n", v->typ);
    }
}

/* Print a generic property, i.e. a pair <name, typed value> */
static
void print_property(void * k, void * v, void * _cudf)
{
    rpmcudf cudf = _cudf;
    FILE * fp = cudf->fp;
    fprintf(fp, "  %s: ", (char *) k);
    print_value(cudf, v);
    fprintf(fp, "\n");
}

/* Print to stdout a set of extra properties */
#define print_extra(_cudf, e)	g_hash_table_foreach(e, print_property, _cudf)

/* Print a CUDF package */
static
void print_package(rpmcudf cudf, cudf_package_t pkg)
{
    FILE * fp = cudf->fp;
    cudf_vpkgformula_t fmla;
    cudf_vpkglist_t vpkglist;

assert(fp != NULL);

    fprintf(fp, "  package: %s\n", cudf_pkg_name(pkg));
    fprintf(fp, "  version: %d\n", cudf_pkg_version(pkg));
    fprintf(fp, "  installed: %s\n",
	       cudf_pkg_installed(pkg) ? "true" : "false");
    fprintf(fp, "  was-installed: %s\n",
	       cudf_pkg_was_installed(pkg) ? "true" : "false");

    fmla = cudf_pkg_depends(pkg);
    fprintf(fp, "  depends: ");
    print_vpkgformula(cudf, fmla);
    fprintf(fp, "\n");
    cudf_free_vpkgformula(fmla);

    vpkglist = cudf_pkg_conflicts(pkg);	/* conflicts */
    fprintf(fp, "  conflicts: ");
    print_vpkglist(cudf, vpkglist, ", ");
    fprintf(fp, "\n");
    cudf_free_vpkglist(vpkglist);

    vpkglist = cudf_pkg_provides(pkg);	/* provides */
    fprintf(fp, "  provides: ");
    print_vpkglist(cudf, vpkglist, ", ");
    fprintf(fp, "\n");
    cudf_free_vpkglist(vpkglist);

    print_keep(cudf, cudf_pkg_keep(pkg));	/* keep */

    print_extra(cudf, cudf_pkg_extra(pkg));	/* extra properties */
    fprintf(fp, "\n");
}

/* Print a CUDF universe */
static
void print_universe(rpmcudf cudf, cudf_doc_t *doc)
{
    FILE * fp = cudf->fp;

    {   GList * l = doc->packages;
	fprintf(fp, "Universe:\n");
	while (l != NULL) {
	    cudf_package_t pkg = (cudf_package_t) g_list_nth_data(l, 0);
	    print_package(cudf, pkg);
	    l = g_list_next(l);
	}
    }

    {   cudf_universe_t univ = cudf_load_universe(doc->packages);
	fprintf(fp, "Universe size: %d/%d (installed/total)\n",
			cudf_installed_size(univ), cudf_universe_size(univ));
	fprintf(fp, "Universe consistent: %s\n", cudf_is_consistent(univ) ?
			"yes" : "no");
	cudf_free_universe(univ);
    }
}

/*==============================================================*/

/*@unchecked@*/
int _rpmcudf_debug = 0;

#ifdef	NOTYET
/*@unchecked@*/ /*@relnull@*/
rpmcudf _rpmcudfI = NULL;
#endif

/**
 * Unreference a cudf interpreter instance.
 * @param cudf		cudf interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmcudf rpmcudfUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmcudf cudf)
	/*@modifies cudf @*/;
#define	rpmcudfUnlink(_ds)	\
    ((rpmcudf)rpmioUnlinkPoolItem((rpmioItem)(_cudf), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a cudf interpreter instance.
 * @param cudf		cudf interpreter
 * @return		new cudf interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmcudf rpmcudfLink (/*@null@*/ rpmcudf cudf)
	/*@modifies cudf @*/;
#define	rpmcudfLink(_cudf)	\
    ((rpmcudf)rpmioLinkPoolItem((rpmioItem)(_cudf), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a cudf interpreter.
 * @param cudf		cudf interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmcudf rpmcudfFree(/*@killref@*/ /*@null@*/rpmcudf cudf)
	/*@globals fileSystem @*/
	/*@modifies cudf, fileSystem @*/;
#define	rpmcudfFree(_cudf)	\
    ((rpmcudf)rpmioFreePoolItem((rpmioItem)(_cudf), __FUNCTION__, __FILE__, __LINE__))

static void rpmcudfFini(void * _cudf)
        /*@globals fileSystem @*/
        /*@modifies *_cudf, fileSystem @*/
{
    rpmcudf cudf = _cudf;

#if defined(WITH_CUDF)
    if (cudf->doc)
	cudf_free_doc(cudf->doc);
    if (cudf->cudf)
	cudf_free_cudf(cudf->cudf);
#endif
    cudf->doc = NULL;
    cudf->cudf = NULL;
    (void)rpmiobFree(cudf->iob);
    cudf->iob = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmcudfPool;

static rpmcudf rpmcudfGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmcudfPool, fileSystem @*/
        /*@modifies pool, _rpmcudfPool, fileSystem @*/
{
    rpmcudf cudf;

    if (_rpmcudfPool == NULL) {
        _rpmcudfPool = rpmioNewPool("cudf", sizeof(*cudf), -1, _rpmcudf_debug,
                        NULL, NULL, rpmcudfFini);
        pool = _rpmcudfPool;
    }
    cudf = (rpmcudf) rpmioGetPool(pool, sizeof(*cudf));
    memset(((char *)cudf)+sizeof(cudf->_item), 0, sizeof(*cudf)-sizeof(cudf->_item));
    return cudf;
}

static rpmcudf rpmcudfNew(const char * fn, int flags)
{
    rpmcudf cudf = rpmcudfGetPool(_rpmcudfPool);

    if (fn != NULL) {
	if (flags)
	    cudf->doc = cudf_parse_from_file((char *)fn);
	else
	    cudf->cudf = cudf_load_from_file((char *)fn);
    }
    cudf->fp = stdout;
    cudf->iob = rpmiobNew(0);
    return rpmcudfLink(cudf);
}

/*==============================================================*/

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,			&_rpmcudf_debug, -1,
	NULL, NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        "\
Usage: cudftool CUDF_FILE [ SOLUTION_FILE ]\n\
", NULL },
  POPT_TABLEEND
};

int main(int argc, char **argv)
{
    poptContext optCon;
    ARGV_t av = NULL;
    int ac;
    rpmcudf X = NULL;
    FILE * fp = NULL;
    int ec = 1;		/* assume failure */
    static int oneshot = 0;

    if (!oneshot) {
	cudf_init();
	oneshot++;
    }

    optCon = rpmioInit(argc, argv, optionsTable);
    av = poptGetArgs(optCon);
    ac = argvCount(av);

    if (!(ac == 1 || ac == 2))
	goto exit;

    X = rpmcudfNew(av[0], 1);
    fp = X->fp;
    fprintf(fp, "Has preamble: %s\n", X->doc->has_preamble
		? "yes" : "no");
    if (X->doc->has_preamble) {
	fprintf(fp, "Preamble: \n");
	print_preamble(X, X->doc);
	fprintf(fp, "\n");
    }
    fprintf(fp, "Has request: %s\n", X->doc->has_request
		? "yes" : "no");
    if (X->doc->has_request) {
	fprintf(fp, "Request: \n");
	print_request(X, X->doc);
	fprintf(fp, "\n");
    }

    print_universe(X, X->doc);

    X = rpmcudfFree(X);

    X = rpmcudfNew(av[0], 0);
    fp = X->fp;
    fprintf(fp, "Universe size: %d/%d (installed/total)\n",
	       cudf_installed_size(X->cudf->universe),
	       cudf_universe_size(X->cudf->universe));
    fprintf(fp, "Universe consistent: %s\n",
	       cudf_is_consistent(X->cudf->universe) ? "yes" : "no");
    if (ac >= 2) {
	rpmcudf Y = rpmcudfNew(av[1], 0);
	fprintf(fp, "Is solution: %s\n",
	       cudf_is_solution(X->cudf, Y->cudf->universe) ? "yes" : "no");
	Y = rpmcudfFree(Y);
    }
    X = rpmcudfFree(X);
    ec = 0;

exit:
    _rpmcudfPool = rpmioFreePool(_rpmcudfPool);
    optCon = rpmioFini(optCon);

    return ec;
}
