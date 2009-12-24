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

#define	WITH_GLIB
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

#endif	/* WITH_GLIB_H */

#include <cudf.h>

#include "debug.h"

const char *__progname;

/*@unchecked@*/
int _rpmcudf_debug = 0;

typedef	struct rpmcudf_s * rpmcudf;

/*==============================================================*/
/* XXX Extend cudf_value_t to include additional types. */
typedef struct rpmcudv_s * rpmcudv;

#define	RPMCUDV_EBASE	256
enum rpmcudv_e {
    RPMCUDV_NOTYPE	= TYPE_NOTYPE,		/* dummy type */
    RPMCUDV_INT		= TYPE_INT,		/* type "int" */
    RPMCUDV_POSINT	= TYPE_POSINT,		/* type "posint" */
    RPMCUDV_NAT		= TYPE_NAT,		/* type "nat" */
    RPMCUDV_BOOL	= TYPE_BOOL,		/* type "bool" */
    RPMCUDV_STRING	= TYPE_STRING,		/* type "string" */
    RPMCUDV_ENUM	= TYPE_ENUM,		/* type "enum" (enum list) */
    RPMCUDV_PKGNAME	= TYPE_PKGNAME,		/* type "pkgname" */
    RPMCUDV_IDENT	= TYPE_IDENT,		/* type "ident" */
    RPMCUDV_VPKG	= TYPE_VPKG,		/* type "vpkg" */
    RPMCUDV_VPKGFORMULA	= TYPE_VPKGFORMULA,	/* type "vpkgformula" */
    RPMCUDV_VPKGLIST	= TYPE_VPKGLIST,	/* type "vpkglist" */
    RPMCUDV_VEQPKG	= TYPE_VEQPKG,		/* type "veqpkg" */
    RPMCUDV_VEQPKGLIST	= TYPE_VEQPKGLIST,	/* type "veqpkglist" */
    RPMCUDV_TYPEDECL	= TYPE_TYPEDECL,	/* type "typedecl" */

	/* XXX extensions */
    RPMCUDV_PREAMBLE	= RPMCUDV_EBASE+0,	/* cudf_preamble_t */
    RPMCUDV_REQUEST	= RPMCUDV_EBASE+1,	/* cudf_request_t */
    RPMCUDV_UNIVERSE	= RPMCUDV_EBASE+2,	/* cudf_universe_t */

    RPMCUDV_PACKAGE	= RPMCUDV_EBASE+3,	/* cudf_package_t */

    RPMCUDV_CUDFDOC	= RPMCUDV_EBASE+4,	/* cudf_doc_t */
    RPMCUDV_CUDF	= RPMCUDV_EBASE+5,	/* cudf_t */

    RPMCUDV_EXTRA	= RPMCUDV_EBASE+6,	/* cudf_extra_t */

};

union rpmcudv_u {
    int i;
    char *s;
    cudf_vpkg_t *vpkg;
    cudf_vpkgformula_t f;
    cudf_vpkglist_t vpkgs;

    void * ptr;
    cudf_preamble_t preamble;
    cudf_request_t request;
    cudf_universe_t univ;

    cudf_package_t *pkg;

    cudf_doc_t *doc;
    cudf_t *cudf;

    cudf_extra_t extra;
};

struct rpmcudv_s {
    enum rpmcudv_e typ;
    union rpmcudv_u val;
};

static rpmcudv rpmcudvFree(rpmcudv v)
{
    if (v == NULL)
	return NULL;
    switch (v->typ) {
    default :
	fprintf(stderr, "Internal error: unexpected type: %d\n", v->typ);
assert(0);
	break;
    case RPMCUDV_INT:
    case RPMCUDV_POSINT:
    case RPMCUDV_NAT:
    case RPMCUDV_BOOL:
	break;
    case RPMCUDV_STRING:
    case RPMCUDV_PKGNAME:
    case RPMCUDV_IDENT:
    case RPMCUDV_ENUM:
	free(v->val.s);
	break;
    case RPMCUDV_VPKGFORMULA:
	cudf_free_vpkgformula(v->val.f);
	break;
    case RPMCUDV_VPKG:
    case RPMCUDV_VEQPKG:
	cudf_free_vpkg(v->val.vpkg);
	break;
    case RPMCUDV_VPKGLIST:
    case RPMCUDV_VEQPKGLIST:
	cudf_free_vpkglist(v->val.vpkgs);
	break;
    case RPMCUDV_TYPEDECL:
assert(0);	/* XXX unimplemented */
	break;

    case RPMCUDV_PREAMBLE:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_REQUEST:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_UNIVERSE:
	cudf_free_universe(v->val.univ);
	break;
    case RPMCUDV_PACKAGE:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_CUDFDOC:
	cudf_free_doc(v->val.doc);
	break;
    case RPMCUDV_CUDF:
	cudf_free_cudf(v->val.cudf);
	break;
    case RPMCUDV_EXTRA:
	g_hash_table_destroy(v->val.extra);
	break;
    }

    /* XXX memset(v, 0, sizeof(*v)); */
    v->typ = RPMCUDV_NOTYPE;
    v->val.ptr = NULL;

    return NULL;
}

/*==============================================================*/

/*==============================================================*/

struct rpmcudf_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */

    struct rpmcudv_s V;

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
void print_vpkg(rpmcudf cudf, rpmcudv v)
{
    FILE * fp = cudf->fp;

assert(v->typ == RPMCUDV_VPKG || v->typ == RPMCUDV_VEQPKG);
    if (v->val.vpkg == NULL)
	return;
    fprintf(fp, "%s", v->val.vpkg->name);
    if (v->val.vpkg->relop) {
	fprintf(fp, " ");
	fprintf(fp, "%s", relops[v->val.vpkg->relop & 0x7]);
	fprintf(fp, " %d", v->val.vpkg->version);
    }
}

/* Print a list of package predicates, separated by a given separator */
static
void print_vpkglist(rpmcudf cudf, rpmcudv v, const char * sep)
{
    FILE * fp = cudf->fp;
cudf_vpkglist_t l;
GList * last;

assert(v->typ == RPMCUDV_VPKGLIST || v->typ == RPMCUDV_VEQPKGLIST);

    for (l = v->val.vpkgs, last=g_list_last(l); l != NULL; l = g_list_next(l)) {
struct rpmcudv_s x;
x.typ = RPMCUDV_VPKG;
x.val.vpkg = g_list_nth_data(l, 0);
	print_vpkg(cudf, &x);
	if (l != last)
	    fprintf(fp, "%s", sep);
    }
}

/* Print a package formula */
static
void print_vpkgformula(rpmcudf cudf, rpmcudv v)
{
    FILE * fp = cudf->fp;
cudf_vpkgformula_t l;
GList * last;

assert(v->typ == RPMCUDV_VPKGFORMULA);

    for (l = v->val.f, last=g_list_last(l); l != NULL; l = g_list_next(l)) {
struct rpmcudv_s x;
x.typ = RPMCUDV_VPKGLIST;
x.val.vpkg = g_list_nth_data(l, 0);
	print_vpkglist(cudf, &x, " | ");
	if (l != last)
	    fprintf(fp, ", ");
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
    default :
	fprintf(stderr, "%s: unexpected keep value: %d\n", __FUNCTION__, keep);
assert(0);
	break;
    case KEEP_NONE:	fprintf(fp, "  keep: version\n");	break;
    case KEEP_VERSION:	fprintf(fp, "  keep: version\n");	break;
    case KEEP_PACKAGE:	fprintf(fp, "  keep: package\n");	break;
    case KEEP_FEATURE:	fprintf(fp, "  keep: feature\n");	break;
    }
}

/* forward ref */
static void rpmcudvPrint(rpmcudf cudf, rpmcudv v);

/* Print a generic property, i.e. a pair <name, typed value> */
static
void print_property(void * k, void * v, void * _cudf)
{
    rpmcudf cudf = _cudf;
    FILE * fp = cudf->fp;
    fprintf(fp, "  %s: ", (char *) k);
    rpmcudvPrint(cudf, v);
    fprintf(fp, "\n");
}

/* Print to stdout a set of extra properties */
#define print_extra(_cudf, e)	g_hash_table_foreach(e, print_property, _cudf)

static void rpmcudvPrint(rpmcudf cudf, rpmcudv v)
{
    FILE * fp = cudf->fp;

    if (v == NULL)
	return;

assert(fp);
    switch (v->typ) {
    default :
	fprintf(stderr, "%s: unexpected type: %d\n", __FUNCTION__, v->typ);
assert(0);
	break;
    case RPMCUDV_INT:
    case RPMCUDV_POSINT:
    case RPMCUDV_NAT:
	fprintf(fp, "%d", v->val.i);
	break;
    case RPMCUDV_BOOL:
	fprintf(fp, "%s", v->val.i ? "true" : "false");
	break;
    case RPMCUDV_STRING:
    case RPMCUDV_PKGNAME:
    case RPMCUDV_IDENT:
    case RPMCUDV_ENUM:
	fprintf(fp, "%s", v->val.s);
	break;
    case RPMCUDV_VPKGFORMULA:
	print_vpkgformula(cudf, v);
	break;
    case RPMCUDV_VPKG:
    case RPMCUDV_VEQPKG:
	print_vpkg(cudf, v);
	break;
    case RPMCUDV_VPKGLIST:
    case RPMCUDV_VEQPKGLIST:
	print_vpkglist(cudf, v, ", ");
	break;
    case RPMCUDV_TYPEDECL:
assert(0);	/* XXX unimplemented */
	break;

    case RPMCUDV_PREAMBLE:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_REQUEST:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_UNIVERSE:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_PACKAGE:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_CUDFDOC:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_CUDF:
assert(0);	/* XXX unimplemented */
	break;
    case RPMCUDV_EXTRA:
	print_extra(cudf, v->val.extra);
	break;
    }
}

typedef	struct rpmcudp_s * rpmcudp;
struct rpmcudp_s {
    GList * l;
    struct rpmcudv_s V;
    struct rpmcudv_s W;
};

static rpmcudp rpmcudpFree(rpmcudp cudp)
{
    if (cudp) {
	memset(cudp, 0, sizeof(*cudp));		/* trash & burn */
	free(cudp);
    }
    return NULL;
}

static rpmcudp rpmcudpNew(rpmcudf cudf)
{
    rpmcudp cudp = xcalloc(1, sizeof(*cudp));
    cudp->l = cudf->V.val.doc->packages;
    return cudp;
}

static rpmcudv rpmcudpNext(rpmcudp cudp)
{
    if (cudp == NULL || cudp->l == NULL)
	return NULL;
    cudp->V.typ = RPMCUDV_PACKAGE;
    cudp->V.val.pkg = (cudf_package_t) g_list_nth_data(cudp->l, 0);
    cudp->l = g_list_next(cudp->l);
    return &cudp->V;
}

static const char * rpmcudpName(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return cudf_pkg_name(v->val.pkg);
}

/* XXX const char * return? */
static int rpmcudpVersion(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return cudf_pkg_version(v->val.pkg);
}

static const char * rpmcudpInstalled(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return (cudf_pkg_installed(v->val.pkg) ? "true" : "false");
}

static const char * rpmcudpWasInstalled(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return (cudf_pkg_was_installed(v->val.pkg) ? "true" : "false");
}

static rpmcudv rpmcudpW(rpmcudp cudp, int typ, void * ptr)
{
rpmcudv w = &cudp->W;
    w->typ = typ;
    w->val.ptr = ptr;
    return w;
}
static rpmcudv rpmcudpDepends(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return rpmcudpW(cudp, RPMCUDV_VPKGFORMULA, cudf_pkg_depends(v->val.pkg));
}
static rpmcudv rpmcudpConflicts(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return rpmcudpW(cudp, RPMCUDV_VPKGLIST, cudf_pkg_conflicts(v->val.pkg));
}
static rpmcudv rpmcudpProvides(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return rpmcudpW(cudp, RPMCUDV_VPKGLIST, cudf_pkg_provides(v->val.pkg));
}
static rpmcudv rpmcudpKeep(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
rpmcudv w = &cudp->W;
assert(v->typ == RPMCUDV_PACKAGE);
    w->typ = RPMCUDV_INT;
    w->val.i = cudf_pkg_keep(v->val.pkg);
    return w;
}
static rpmcudv rpmcudpExtra(rpmcudp cudp)
{
rpmcudv v = &cudp->V;
assert(v->typ == RPMCUDV_PACKAGE);
    return rpmcudpW(cudp, RPMCUDV_EXTRA, cudf_pkg_extra(v->val.pkg));
}

/* Print a CUDF package */
static
void print_package(rpmcudf cudf, rpmcudp cudp)
{
FILE * fp = cudf->fp;
rpmcudv w;

assert(fp != NULL);
w = &cudp->V;
assert(w->typ == RPMCUDV_PACKAGE);

    fprintf(fp, "  package: %s\n", rpmcudpName(cudp));
    fprintf(fp, "  version: %d\n", rpmcudpVersion(cudp));
    fprintf(fp, "  installed: %s\n", rpmcudpInstalled(cudp));
    fprintf(fp, "  was-installed: %s\n", rpmcudpWasInstalled(cudp));

    fprintf(fp, "  depends: ");
w = rpmcudpDepends(cudp);
    print_vpkgformula(cudf, w);
rpmcudvFree(w);
    fprintf(fp, "\n");

    fprintf(fp, "  conflicts: ");
w = rpmcudpConflicts(cudp);
    print_vpkglist(cudf, w, ", ");
rpmcudvFree(w);
    fprintf(fp, "\n");

    fprintf(fp, "  provides: ");
w = rpmcudpProvides(cudp);
    print_vpkglist(cudf, w, ", ");
rpmcudvFree(w);
    fprintf(fp, "\n");

w = rpmcudpKeep(cudp);
    print_keep(cudf, w->val.i);
rpmcudvFree(w);

w = rpmcudpExtra(cudp);
    print_extra(cudf, w->val.extra);
rpmcudvFree(w);

    fprintf(fp, "\n");

}

/* Print a CUDF universe */
static
void print_universe(rpmcudf cudf, cudf_doc_t *doc)
{
    FILE * fp = cudf->fp;

    {   rpmcudp cudp = rpmcudpNew(cudf);
	fprintf(fp, "Universe:\n");
	while (rpmcudpNext(cudp) != NULL)
	    print_package(cudf, cudp);
	cudp = rpmcudpFree(cudp);
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
typedef long intnat;
typedef unsigned long uintnat;
typedef intnat value;

typedef struct {
  void *block;           /* address of the malloced block this chunk live in */
  size_t alloc;         /* in bytes, used for compaction */
  size_t size;          /* in bytes */
  char *next;
} heap_chunk_head;
  
#define Chunk_size(c) (((heap_chunk_head *) (c)) [-1]).size
#define Chunk_alloc(c) (((heap_chunk_head *) (c)) [-1]).alloc
#define Chunk_next(c) (((heap_chunk_head *) (c)) [-1]).next
#define Chunk_block(c) (((heap_chunk_head *) (c)) [-1]).block
extern char * caml_heap_start;
extern intnat caml_stat_heap_size;

struct caml_ref_table {
  void **base;
  void **end;
  void **threshold;
  void **ptr; 
  void **limit;
  size_t size;
  size_t reserve;
};
extern struct caml_ref_table caml_ref_table;
extern struct caml_ref_table caml_weak_ref_table;

#define Page_log 12             /* A page is 4 kilobytes. */
#define Pagetable2_log 11
#define Pagetable2_size (1 << Pagetable2_log)
#define Pagetable1_log (Page_log + Pagetable2_log) 
#define Pagetable1_size (1 << (32 - Pagetable1_log))
extern unsigned char * caml_page_table[Pagetable1_size];

struct channel {
  int fd;                       /* Unix file descriptor */
  off_t offset;                 /* Absolute position of fd in the file */
  char * end;                   /* Physical end of the buffer */
  char * curr;                  /* Current position in the buffer */
  char * max;                   /* Logical end of the buffer (for input) */
  void * mutex;                 /* Placeholder for mutex (for systhreads) */
  struct channel * next, * prev;/* Double chaining of channels (flush_all) */
  int revealed;                 /* For Cash only */
  int old_revealed;             /* For Cash only */
  int refcount;                 /* For flush_all and for Cash */
  int flags;                    /* Bitfield */
  char buff[1];                 /* The buffer itself */
};
extern struct channel * caml_all_opened_channels;

struct global_root {
  value * root;                    /* the address of the root */
  struct global_root * forward[1]; /* variable-length array */
};
#define NUM_LEVELS 17
struct global_root_list {
  value * root;                 /* dummy value for layout compatibility */
  struct global_root * forward[NUM_LEVELS]; /* forward chaining */
  int level;                    /* max used level */
};
struct global_root_list caml_global_roots;
struct global_root_list caml_global_roots_young;
struct global_root_list caml_global_roots_old;

static void caml_empty_global_roots(struct global_root_list * rootlist)
{
  struct global_root * gr, * next;
  int i;

  for (gr = rootlist->forward[0]; gr != NULL; /**/) {
    next = gr->forward[0];
    caml_stat_free(gr);
    gr = next;
  }
  for (i = 0; i <= rootlist->level; i++) rootlist->forward[i] = NULL;
  rootlist->level = 0;
}

#ifdef	NOTYET		/* XXX byterun/custom.c static custom_ops_table */
struct custom_operations {
  char *identifier;
  void (*finalize)(value v);
  int (*compare)(value v1, value v2);
  intnat (*hash)(value v);
  void (*serialize)(value v,
                    /*out*/ uintnat * wsize_32 /*size in bytes*/,
                    /*out*/ uintnat * wsize_64 /*size in bytes*/);
  uintnat (*deserialize)(void * dst);
};
extern struct custom_operations caml_int32_ops,
                                caml_nativeint_ops,
                                caml_int64_ops;
#endif

struct ext_table {
  int size;
  int capacity;
  void ** contents;
};
extern struct ext_table caml_prim_table;

extern void * caml_stack_low;

#define In_heap 1                         
#define In_young 2 
extern char * caml_young_start;
extern char * caml_young_end;

static void
cudf_fini(void)
{
    void * _base;
    void * _empty;
    uintnat *_p;
    struct channel * _c;

    int i;

    caml_gc_full_major(0);

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_all_opened_channels\n");
    i = 0;
    while ((_c = caml_all_opened_channels) != NULL) {
	if (_c->fd >= 0 && _c->fd <= 2)	/* XXX avoid closing std{in,out,err} */
	    _c->fd = dup(_c->fd);
	_c->refcount = 0;
	caml_close_channel(_c);
if (_rpmcudf_debug)
fprintf(stderr, "--- channel: %d %p %p\n", i, _c, caml_all_opened_channels);
	i++;
    }

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_prim_table\n");
    caml_ext_table_free(&caml_prim_table, 0);

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_stack_low\n");
    _base = caml_stack_low;
    if (_base) caml_stat_free(_base);	/* XXX free(_base) instead? */

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_global_roots\n");
    caml_empty_global_roots(&caml_global_roots);

    /* XXX the OCAML major heap has chained allocation chunks. */
if (_rpmcudf_debug)
fprintf(stderr, "--- caml_heap_start\n");
    caml_page_table_remove(In_heap, caml_heap_start, caml_heap_start+caml_stat_heap_size);
    _base = Chunk_block(caml_heap_start);
    free(_base);

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_weak_ref_table\n");
    _base = caml_weak_ref_table.base;
    if (_base) caml_stat_free(_base);	/* XXX free(_base) instead? */
if (_rpmcudf_debug)
fprintf(stderr, "--- caml_ref_table\n");
    _base = caml_ref_table.base;
    if (_base) caml_stat_free(_base);	/* XXX free(_base) instead? */

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_young_start\n");
    caml_page_table_remove(In_young, caml_young_start, caml_young_end);
    _p = (void *)caml_young_start;

    /* XXX the OCAML minor heap is page aligned and caml_young_base is static */
#ifdef	BRUTAL_HACK
    _p -= (2624/sizeof(*_p));
    free(_p);
#else
#ifdef	DYING
    /* XXX OCAML compiled with DEBUG adds red zones = Debug_filler_align. */
    for (i = 0; i < (int)(2640/sizeof(*_p)); i++) {
fprintf(stderr, "\tp[%d]\t%u\n", -i, (unsigned)*_p);
	_p--;
    }
#endif
#endif

if (_rpmcudf_debug)
fprintf(stderr, "--- caml_page_table\n");
#ifdef	BUGGY
    _empty = caml_page_table[0];
    for (i = 0; i < Pagetable1_size; i++) {
	_base = caml_page_table[i];
	if (_base == _empty)
	    continue;
if (_rpmcudf_debug)
fprintf(stderr, "\t%3d %p\n", i, _base);
	free(_base);
    }
#endif

}

/*==============================================================*/

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
    (void) rpmcudvFree(&cudf->V);
#endif
    memset(&cudf->V, 0, sizeof(cudf->V));
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

static rpmcudf rpmcudfNew(const char * fn, int typ)
{
    rpmcudf cudf = rpmcudfGetPool(_rpmcudfPool);
    static int oneshot = 0;

    if (!oneshot) {
	cudf_init();
	oneshot++;
    }

    if (fn != NULL)
    switch (typ) {
    default:
assert(0);
	break;
    case RPMCUDV_CUDFDOC:
	cudf->V.typ = typ;
	cudf->V.val.ptr = cudf_parse_from_file((char *)fn);
	break;
    case RPMCUDV_CUDF:
	cudf->V.typ = typ;
	cudf->V.val.ptr = cudf_load_from_file((char *)fn);
	break;
    }

    cudf->fp = stdout;
    cudf->iob = rpmiobNew(0);
    return rpmcudfLink(cudf);
}

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

    optCon = rpmioInit(argc, argv, optionsTable);
    av = poptGetArgs(optCon);
    ac = argvCount(av);

    if (!(ac == 1 || ac == 2))
	goto exit;

    X = rpmcudfNew(av[0], RPMCUDV_CUDFDOC);
    fp = X->fp;
    fprintf(fp, "Has preamble: %s\n", X->V.val.doc->has_preamble
		? "yes" : "no");
    if (X->V.val.doc->has_preamble) {
	fprintf(fp, "Preamble: \n");
	print_preamble(X, X->V.val.doc);
	fprintf(fp, "\n");
    }
    fprintf(fp, "Has request: %s\n", X->V.val.doc->has_request
		? "yes" : "no");
    if (X->V.val.doc->has_request) {
	fprintf(fp, "Request: \n");
	print_request(X, X->V.val.doc);
	fprintf(fp, "\n");
    }

    print_universe(X, X->V.val.doc);

    fflush(X->fp);
    X = rpmcudfFree(X);

    X = rpmcudfNew(av[0], RPMCUDV_CUDF);
    fp = X->fp;
    fprintf(fp, "Universe size: %d/%d (installed/total)\n",
	       cudf_installed_size(X->V.val.cudf->universe),
	       cudf_universe_size(X->V.val.cudf->universe));
    fprintf(fp, "Universe consistent: %s\n",
	       cudf_is_consistent(X->V.val.cudf->universe) ? "yes" : "no");
    if (ac >= 2) {
	rpmcudf Y = rpmcudfNew(av[1], RPMCUDV_CUDF);
	fprintf(fp, "Is solution: %s\n",
	       cudf_is_solution(X->V.val.cudf, Y->V.val.cudf->universe) ? "yes" : "no");
	Y = rpmcudfFree(Y);
    }
    fflush(X->fp);
    X = rpmcudfFree(X);
    ec = 0;

exit:
if (_rpmcudf_debug)
fprintf(stderr, "=== EXITING\n");
cudf_fini();
    _rpmcudfPool = rpmioFreePool(_rpmcudfPool);
    optCon = rpmioFini(optCon);

    return ec;
}
