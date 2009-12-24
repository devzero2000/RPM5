#ifndef RPMCUDF_H
#define RPMCUDF_H

/** \ingroup rpmio
 * \file rpmio/rpmcudf.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmcudf_s * rpmcudf;

/*@unchecked@*/
extern int _rpmcudf_debug;

/*@unchecked@*/ /*@relnull@*/
extern rpmcudf _rpmcudfI;

#if defined(_RPMCUDF_INTERNAL)
#include <cudf.h>

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
    RPMCUDV_UNIVERSE	= RPMCUDV_EBASE+0,	/* cudf_universe_t */

    RPMCUDV_PACKAGE	= RPMCUDV_EBASE+1,	/* cudf_package_t */

    RPMCUDV_CUDFDOC	= RPMCUDV_EBASE+2,	/* cudf_doc_t */
    RPMCUDV_CUDF	= RPMCUDV_EBASE+3,	/* cudf_t */

    RPMCUDV_EXTRA	= RPMCUDV_EBASE+4,	/* cudf_extra_t */

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

struct rpmcudf_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    struct rpmcudv_s V;		/*!< union of cudf_doc_t and cudf_t */
    rpmiob iob;			/*!< output collector */
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif /* _RPMCUDF_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a cudf interpreter instance.
 * @param cudf	cudf interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmcudf rpmcudfUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmcudf cudf)
	/*@modifies cudf @*/;
#define	rpmcudfUnlink(_ds)	\
    ((rpmcudf)rpmioUnlinkPoolItem((rpmioItem)(_cudf), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a cudf interpreter instance.
 * @param cudf	cudf interpreter
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

/**
 * Create and load a cudf interpreter.
 * @param *av		cudf interpreter args (or NULL)
 * @param flags		cudf interpreter flags (1 == use global interpreter)
 * @return		new cudf interpreter
 */
/*@newref@*/ /*@null@*/
rpmcudf rpmcudfNew(/*@null@*/ const char ** av, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

int rpmcudfHasPreamble(rpmcudf cudf)
	/*@*/;
int rpmcudfHasRequest(rpmcudf cudf)
	/*@*/;
int rpmcudfIsConsistent(rpmcudf cudf)
	/*@*/;
int rpmcudfInstalledSize(rpmcudf cudf)
	/*@*/;
int rpmcudfUniverseSize(rpmcudf cudf)
	/*@*/;
void rpmcudfPrintPreamble(rpmcudf cudf)
	/*@*/;
void rpmcudfPrintRequest(rpmcudf cudf)
	/*@*/;
void rpmcudfPrintUniverse(rpmcudf cudf)
	/*@*/;
int rpmcudfIsSolution(rpmcudf X, rpmcudf Y)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMCUDF_H */
