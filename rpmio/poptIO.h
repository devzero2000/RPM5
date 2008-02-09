#ifndef H_POPTIO
#define	H_POPTIO

/** \ingroup rpmio
 * \file rpmio/poptIO.h
 */

#include <rpmio.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <rpmmg.h>
#include <rpmurl.h>
#include <argv.h>
#include <fts.h>
#include <mire.h>
#include <popt.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 * Popt option table for options shared by all modes and executables.
 */
/*@unchecked@*/
extern struct poptOption		rpmioAllPoptTable[];

/*@unchecked@*/
extern int rpmioFtsOpts;

/*@unchecked@*/
extern struct poptOption		rpmioFtsPoptTable[];

/*@unchecked@*/ /*@observer@*/ /*@null@*/
extern const char * rpmioPipeOutput;

/*@unchecked@*/ /*@observer@*/ /*@null@*/
extern const char * rpmioRootDir;

/**
 * Initialize most everything needed by an rpmio executable context.
 * @param argc			no. of args
 * @param argv			arg array
 * @param optionsTable		popt option table
 * @return			popt context (or NULL)
 */
/*@null@*/
poptContext
rpmioInit(int argc, char *const argv[], struct poptOption * optionsTable)
	/*@globals rpmCLIMacroContext, rpmGlobalMacroContext, h_errno, stderr, 
		fileSystem, internalState @*/
	/*@modifies rpmCLIMacroContext, rpmGlobalMacroContext, stderr, 
		fileSystem, internalState @*/;

/**
 * Make sure that rpm configuration has been read.
 * @warning Options like --rcfile and --verbose must precede callers option.
 */
/*@mayexit@*/
void rpmioConfigured(void)
	/*@globals rpmCLIMacroContext,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmCLIMacroContext, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/**
 * Destroy most everything needed by an rpm CLI executable context.
 * @param optCon		popt context
 * @return			NULL always
 */
poptContext
rpmioFini(/*@only@*/ /*@null@*/ poptContext optCon)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies optCon, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_POPTIO */
