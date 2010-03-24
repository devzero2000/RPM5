#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmlog.h>
#include <rpmmacro.h>
#include <argv.h>
#include <poptIO.h>

#include "debug.h"


#define _KFB(n) (1U << (n))
#define _DFB(n) (_KFB(n) | 0x40000000)

#define F_ISSET(_nix, _FLAG) ((_nix)->flags & ((RPMNIX_FLAGS_##_FLAG) & ~0x40000000))

/**
 * Bit field enum for rpmdigest CLI options.
 */
enum nixFlags_e {
    RPMNIX_FLAGS_NONE		= 0,
    RPMNIX_FLAGS_ADDDRVLINK	= _DFB(0),	/*    --add-drv-link */
    RPMNIX_FLAGS_NOOUTLINK	= _DFB(1),	/* -o,--no-out-link */
    RPMNIX_FLAGS_DRYRUN		= _DFB(2),	/*    --dry-run */

    RPMNIX_FLAGS_EVALONLY	= _DFB(16),	/*    --eval-only */
    RPMNIX_FLAGS_PARSEONLY	= _DFB(17),	/*    --parse-only */
    RPMNIX_FLAGS_ADDROOT	= _DFB(18),	/*    --add-root */
    RPMNIX_FLAGS_XML		= _DFB(19),	/*    --xml */
    RPMNIX_FLAGS_STRICT		= _DFB(20),	/*    --strict */
    RPMNIX_FLAGS_SHOWTRACE	= _DFB(21),	/*    --show-trace */

    RPMNIX_FLAGS_SKIPWRONGSTORE	= _DFB(30)	/*    --skip-wrong-store */
};

/**
 */
typedef struct rpmnix_s * rpmnix;

/**
 */
struct rpmnix_s {
    enum nixFlags_e flags;	/*!< rpmnix control bits. */

    const char * outLink;
    const char * drvLink;

    const char ** instArgs;
    const char ** buildArgs;
    const char ** exprs;

    const char * attr;

    const char * url;
    const char * expHash;
    const char * hashType;
    const char * hashFormat;
    const char * hash;
    int quiet;
    int print;
    const char * finalPath;
};

/**
 */
static struct rpmnix_s _nix = {
	.hashType = "sha256"
};

/*==============================================================*/
#ifdef	REFERENCE
/*
mkTempDir() {
    if test -n "$tmpPath"; then return; fi
    local i=0
    while true; do
        if test -z "$TMPDIR"; then TMPDIR=/tmp; fi
        tmpPath=$TMPDIR/nix-prefetch-url-$$-$i
        if mkdir "$tmpPath"; then break; fi
        # !!! to bad we can't check for ENOENT in mkdir, so this check
        # is slightly racy (it bombs out if somebody just removed
        # $tmpPath...).
        if ! test -e "$tmpPath"; then exit 1; fi
        i=$((i + 1))
    done
    trap removeTempDir EXIT SIGINT SIGQUIT
}

removeTempDir() {
    if test -n "$tmpPath"; then
        rm -rf "$tmpPath" || true
    fi
}


doDownload() {
    /usr/bin/curl $cacheFlags --fail --location --max-redirs 20 --disable-epsv \
        --cookie-jar $tmpPath/cookies "$url" -o $tmpFile
}
*/
#endif

/*==============================================================*/

#ifdef	UNUSED
static int verbose = 0;
#endif

static void nixInstantiateArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
#ifdef	UNUSED
    rpmnix nix = &_nix;
#endif

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixInstantiateOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixInstantiateArgCallback, 0, NULL, NULL },
/*@=type@*/

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
syntax: nix-prefetch-url URL [EXPECTED-HASH]\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = &_nix;
    poptContext optCon = rpmioInit(argc, argv, nixInstantiateOptions);
    int ec = 1;		/* assume failure */
    const char * s;
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
#ifdef	UNUSED
    int xx;
#endif

#ifdef	REFERENCE
/*
#! /bin/bash -e

url=$1
expHash=$2

# needed to make it work on NixOS
export PATH=$PATH:/bin

hashType=$NIX_HASH_ALGO
if test -z "$hashType"; then
    hashType=sha256
fi

hashFormat=
if test "$hashType" != "md5"; then
    hashFormat=--base32
fi

if test -z "$url"; then
    echo "syntax: nix-prefetch-url URL [EXPECTED-HASH]" >&2
    exit 1
fi
*/
#endif

    if ((s = getenv("QUIET")) && *s) nix->quiet = 1;
    if ((s = getenv("PRINT_PATH")) && *s) nix->print = 1;

    switch (ac) {
    default:	poptPrintUsage(optCon, stderr, 0);	goto exit;
	/*@notreached@*/
    case 2:	nix->expHash = av[1];	/*@fallthrough@*/
    case 1:	nix->url = av[0];	break;
    }
    if ((s = getenv("NIX_HASH_ALGO")))	nix->hashType = s;
    nix->hashFormat = (strcmp(nix->hashType, "md5") ? "--base32" : "");

#ifdef	REFERENCE
/*
# Handle escaped characters in the URI.  `+', `=' and `?' are the only
# characters that are valid in Nix store path names but have a special
# meaning in URIs.
name=$(basename "$url" | /bin/sed -e 's/%2b/+/g' -e 's/%3d/=/g' -e 's/%3f/\?/g')
if test -z "$name"; then echo "invalid url"; exit 1; fi
*/
#endif


    /* If expected hash specified, check if it already exists in the store. */
    if (nix->expHash != NULL) {
#ifdef	REFERENCE
/*
    finalPath=$(/usr/bin/nix-store --print-fixed-path "$hashType" "$expHash" "$name")
    if ! /usr/bin/nix-store --check-validity "$finalPath" 2> /dev/null; then
        finalPath=
    fi
    hash=$expHash
*/
#endif
	nix->hash = nix->expHash;
    }

#ifdef	REFERENCE
/*
# Hack to support the mirror:// scheme from Nixpkgs.
if test "${url:0:9}" = "mirror://"; then
    if test -z "$NIXPKGS_ALL"; then
        echo "Resolving mirror:// URLs requires Nixpkgs.  Please point \$NIXPKGS_ALL at a Nixpkgs tree." >&2
        exit 1
    fi

    mkTempDir
    nix-build "$NIXPKGS_ALL" -A resolveMirrorURLs --argstr url "$url" -o $tmpPath/urls > /dev/null

    expanded=($(cat $tmpPath/urls))
    if test "${#expanded[*]}" = 0; then
        echo "$0: cannot resolve $url." >&2
        exit 1
    fi

    echo "$url expands to ${expanded[*]} (using ${expanded[0]})" >&2
    url="${expanded[0]}"
fi
*/
#endif


#ifdef	REFERENCE
/*
# If we don't know the hash or a file with that hash doesn't exist,
# download the file and add it to the store.
if test -z "$finalPath"; then

    mkTempDir
    tmpFile=$tmpPath/$name

    # Optionally do timestamp-based caching of the download.
    # Actually, the only thing that we cache in $NIX_DOWNLOAD_CACHE is
    # the hash and the timestamp of the file at $url.  The caching of
    # the file *contents* is done in Nix store, where it can be
    # garbage-collected independently.
    if test -n "$NIX_DOWNLOAD_CACHE"; then
        echo -n "$url" > $tmpPath/url
        urlHash=$(/usr/bin/nix-hash --type sha256 --base32 --flat $tmpPath/url)
        echo "$url" > "$NIX_DOWNLOAD_CACHE/$urlHash.url"
        cachedHashFN="$NIX_DOWNLOAD_CACHE/$urlHash.$hashType"
        cachedTimestampFN="$NIX_DOWNLOAD_CACHE/$urlHash.stamp"
        cacheFlags="--remote-time"
        if test -e "$cachedTimestampFN" -a -e "$cachedHashFN"; then
            # Only download the file if it is newer than the cached version.
            cacheFlags="$cacheFlags --time-cond $cachedTimestampFN"
        fi
    fi

    # Perform the download.
    doDownload

    if test -n "$NIX_DOWNLOAD_CACHE" -a ! -e $tmpFile; then
        # Curl didn't create $tmpFile, so apparently there's no newer
        # file on the server.
        hash=$(cat $cachedHashFN)
        finalPath=$(/usr/bin/nix-store --print-fixed-path "$hashType" "$hash" "$name") 
        if ! /usr/bin/nix-store --check-validity "$finalPath" 2> /dev/null; then
            echo "cached contents of \`$url' disappeared, redownloading..." >&2
            finalPath=
            cacheFlags="--remote-time"
            doDownload
        fi
    fi

    if test -z "$finalPath"; then

        # Compute the hash.
        hash=$(/usr/bin/nix-hash --type "$hashType" $hashFormat --flat $tmpFile)
        if ! test -n "$QUIET"; then echo "hash is $hash" >&2; fi

        if test -n "$NIX_DOWNLOAD_CACHE"; then
            echo $hash > $cachedHashFN
            touch -r $tmpFile $cachedTimestampFN
        fi

        # Add the downloaded file to the Nix store.
        finalPath=$(/usr/bin/nix-store --add-fixed "$hashType" $tmpFile)

        if test -n "$expHash" -a "$expHash" != "$hash"; then
            echo "hash mismatch for URL \`$url'" >&2
            exit 1
        fi
        
    fi
fi
*/
#endif

#ifdef	REFERENCE
/*
if ! test -n "$QUIET"; then echo "path is $finalPath" >&2; fi

echo $hash

if test -n "$PRINT_PATH"; then
    echo $finalPath
fi
*/
#endif
    if (!nix->quiet)
	fprintf(stderr, _("path is %s\n"), nix->finalPath);
    fprintf(stdout, "%s\n", nix->hash);
    if (nix->print)
	fprintf(stdout, _("%s\n"), nix->finalPath);

    ec = 0;	/* XXX success */

exit:

    optCon = rpmioFini(optCon);

    return ec;
}
