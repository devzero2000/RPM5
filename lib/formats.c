/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for %_i18ndomains */

#define	_RPMEVR_INTERNAL
#include <rpmds.h>
#include <rpmfi.h>

#include "legacy.h"
#include "manifest.h"
#include "argv.h"
#include "misc.h"
#include "fs.h"

#include "debug.h"


/**
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fsnamesTag( /*@unused@*/ Header h, /*@out@*/ int_32 * type,
		/*@out@*/ void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals fileSystem, internalState @*/
	/*@modifies *type, *data, *count, *freeData,
		fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    const char ** list;

/*@-boundswrite@*/
    if (rpmGetFilesystemList(&list, count))
	return 1;
/*@=boundswrite@*/

    if (type) *type = RPM_STRING_ARRAY_TYPE;
    if (data) *((const char ***) data) = list;
    if (freeData) *freeData = 0;

    return 0;
}

/**
 * Retrieve mounted file system space.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fssizesTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char ** filenames;
    uint_32 * filesizes;
    uint_64 * usages;
    int numFiles;

    if (!hge(h, RPMTAG_FILESIZES, NULL, &filesizes, &numFiles)) {
	filesizes = NULL;
	numFiles = 0;
	filenames = NULL;
    } else {
	headerGetExtension(h, RPMTAG_FILEPATHS, NULL, &filenames, &numFiles);
    }

/*@-boundswrite@*/
    if (rpmGetFilesystemList(NULL, count))
	return 1;
/*@=boundswrite@*/

    *type = RPM_INT64_TYPE;
    *freeData = 1;

    if (filenames == NULL) {
	usages = xcalloc((*count), sizeof(*usages));
	*data = usages;

	return 0;
    }

/*@-boundswrite@*/
    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 1;
/*@=boundswrite@*/

    *data = usages;

    filenames = _free(filenames);

    return 0;
}

/**
 * Retrieve file classes.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fileclassTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFClasses(h, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

/**
 * Retrieve file contexts from header.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filecontextsTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFContexts(h, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

/**
 * Retrieve file contexts from file system.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fscontextsTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFSContexts(h, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

/**
 * Retrieve file contexts from policy RE's.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int recontextsTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildREContexts(h, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

/**
 * Retrieve file provides.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fileprovideTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_PROVIDENAME, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filerequireTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_REQUIRENAME, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

/**
 * Retrieve Requires(missingok): array for Suggests: or Enhances:.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int missingokTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, *type, *data, *count, *freeData,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    rpmds ds = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
    ARGV_t av = NULL;
    ARGV_t argv;
    int argc = 0;
    char * t;
    size_t nb = 0;
    int i;

assert(ds != NULL);
    /* Collect dependencies marked as hints. */
    ds = rpmdsInit(ds);
    if (ds != NULL)
    while (rpmdsNext(ds) >= 0) {
	int Flags = rpmdsFlags(ds);
	const char * DNEVR;
	if (!(Flags & RPMSENSE_MISSINGOK))
	    continue;
	DNEVR = rpmdsDNEVR(ds);
	if (DNEVR == NULL)
	    continue;
	nb += sizeof(*argv) + strlen(DNEVR+2) + 1;
	(void) argvAdd(&av, DNEVR+2);
	argc++;
    }
    nb += sizeof(*argv);	/* final argv NULL */

    /* Create contiguous header string array. */
    argv = (ARGV_t) xcalloc(nb, 1);
    t = (char *)(argv + argc);
    for (i = 0; i < argc; i++) {
	argv[i] = t;
	t = stpcpy(t, av[i]);
	*t++ = '\0';
    }
    av = argvFree(av);
    ds = rpmdsFree(ds);

    /* XXX perhaps return "(none)" inband if no suggests/enhances <shrug>. */

    *type = RPM_STRING_ARRAY_TYPE;
    *data = argv;
    *count = argc;
    *freeData = 1;
    return 0;
}

/*@-type@*/ /* FIX: cast? */
const struct headerSprintfExtension_s rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_ENHANCES",	{ missingokTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILECLASS",	{ fileclassTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILECONTEXTS",	{ filecontextsTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEPROVIDE",	{ fileprovideTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEREQUIRE",	{ filerequireTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSCONTEXTS",	{ fscontextsTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES",		{ fsnamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES",		{ fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_RECONTEXTS",	{ recontextsTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUGGESTS",	{ missingokTag } },
    { HEADER_EXT_MORE, NULL,		{ (void *) headerCompoundFormats } }
} ;
/*@=type@*/
