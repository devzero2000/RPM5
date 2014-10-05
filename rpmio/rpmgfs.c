#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmmacro.h>
#include <rpmlog.h>
#include <rpmurl.h>

#include <rpmmg.h>
#include <magic.h>

#define	_RPMPGP_INTERNAL
#include <rpmbc.h>
#include <rpmpgp.h>

#define	_RPMGFS_INTERNAL
#include <rpmgfs.h>

#include "debug.h"

int _rpmgfs_debug;
#define	SPEW(_list)	if (_rpmgfs_debug) fprintf _list

rpmgfs _rpmgfsI;

static int gfs_nopens;

/*==============================================================*/
static void rpmgfsSpew(rpmgfs gfs)
{
    if (_rpmgfs_debug == 0)
	return;

    fprintf(stderr, "\t    fn: %s\n",	gfs->fn);
    fprintf(stderr, "\t flags: 0x%x\n",	gfs->flags);
    fprintf(stderr, "\t  mode: 0%06o\n",	gfs->mode);
    fprintf(stderr, "\tscheme: %s\n",	gfs->scheme);
    fprintf(stderr, "\t     u: %s\n",	gfs->u);
    fprintf(stderr, "\t    pw: %s\n",	gfs->pw);
    fprintf(stderr, "\t  user: %s\n",	gfs->user);
    fprintf(stderr, "\t     h: %s\n",	gfs->h);
    fprintf(stderr, "\t     p: %s\n",	gfs->p);
    fprintf(stderr, "\t  host: %s\n",	gfs->host);
    fprintf(stderr, "\t    db: %s\n",	gfs->db);
    fprintf(stderr, "\t  coll: %s\n",	gfs->coll);
    fprintf(stderr, "\t  opts: %s\n",	gfs->opts);
    fprintf(stderr, "\t   uri: %s\n",	gfs->uri);

    fprintf(stderr, "\t     C: %p\n",	gfs->C);
    fprintf(stderr, "\t     S: %p\n",	gfs->S);
    fprintf(stderr, "\t     G: %p\n",	gfs->G);
    fprintf(stderr, "\t     F: %p\n",	gfs->F);
    fprintf(stderr, "\t     D: %p\n",	gfs->D);
    fprintf(stderr, "\t   iov: %p[%u]\n",gfs->iov.iov_base, (unsigned)gfs->iov.iov_len);
}

static int m2rlevel[] = {
    RPMLOG_ERR,		/* 0 MONGOC_LOG_LEVEL_ERROR */
    RPMLOG_CRIT,	/* 1 MONGOC_LOG_LEVEL_CRITICAL */
    RPMLOG_WARNING,	/* 2 MONGOC_LOG_LEVEL_WARNING */
    RPMLOG_NOTICE,	/* 3 MONGOC_LOG_LEVEL_MESSAGE */
    RPMLOG_INFO,	/* 4 MONGOC_LOG_LEVEL_INFO */
    RPMLOG_DEBUG,	/* 5 MONGOC_LOG_LEVEL_DEBUG */
    RPMLOG_DEBUG,	/* 6 MONGOC_LOG_LEVEL_TRACE */
    RPMLOG_DEBUG,	/* 7 unused */
};

static void rpmgfsLog(mongoc_log_level_t _level, const char * _domain,
		const char * _message, void * _gfs)
{
    rpmgfs gfs = (rpmgfs) _gfs;

SPEW((stderr, "--> %s(%s(%d), %s, %p) %s\n", __FUNCTION__, mongoc_log_level_str(_level), _level, _domain, gfs, _message));

    rpmlog(m2rlevel[_level & 0x7], "gfs %p %s: %s\n", gfs, _domain, _message);

}

static void * rpmgfsChk(rpmgfs gfs, const char * msg,
		bson_error_t * bep, void * p)
{
    if (p == NULL) {
fprintf(stderr, "*** %s: (%u.%u) %s\n", msg, bep->domain, bep->code, bep->message);
    }
    return p;
}

static int rpmgfsSetFileOpt(rpmgfs gfs, const char * ifn,
		 mongoc_gridfs_file_opt_t *op)
{
    struct stat sb;
    FD_t ifd = NULL;
    size_t nb = 0;
    void * b = (void *)-1;

    struct MD_s {
	const char *	name;
	uint32_t	algo;
	const char *	digest;
	size_t		digestlen;
    } MD[] = {
	{ "md5",	PGPHASHALGO_MD5,	NULL, 0 },
	{ "sha1",	PGPHASHALGO_SHA1,	NULL, 0 },
	{ "sha224",	PGPHASHALGO_SHA224,	NULL, 0 },
	{ "sha256",	PGPHASHALGO_SHA256,	NULL, 0 },
	{ "sha384",	PGPHASHALGO_SHA384,	NULL, 0 },
	{ "sha512",	PGPHASHALGO_SHA512,	NULL, 0 },
	{ "ripemd128",	PGPHASHALGO_RIPEMD128,	NULL, 0 },
	{ "ripemd160",	PGPHASHALGO_RIPEMD160,	NULL, 0 },
	{ "ripemd256",	PGPHASHALGO_RIPEMD256,	NULL, 0 },
	{ "ripemd320",	PGPHASHALGO_RIPEMD320,	NULL, 0 },
	{ "blake2b",	PGPHASHALGO_BLAKE2B,	NULL, 0 },
	{ "blake2bp",	PGPHASHALGO_BLAKE2BP,	NULL, 0 },
	{ "blake2s",	PGPHASHALGO_BLAKE2S,	NULL, 0 },
	{ "blake2sp",	PGPHASHALGO_BLAKE2SP,	NULL, 0 },
	{ "rg32",	PGPHASHALGO_RG32_256,	NULL, 0 },
	{ "rg64",	PGPHASHALGO_RG64_256,	NULL, 0 },
	{ "adler32",	PGPHASHALGO_ADLER32,	NULL, 0 },
	{ "crc32",	PGPHASHALGO_CRC32,	NULL, 0 },
	{ "crc64",	PGPHASHALGO_CRC64,	NULL, 0 },
	{ "jlu32",	PGPHASHALGO_JLU32,	NULL, 0 },
	{ NULL,		0,			NULL, 0 },
    };

    struct MC_s {
	const char *	name;
	unsigned	flags;
	const char *	value;
    } MC[] = {
	{ "content_type",	MAGIC_MIME_TYPE	,	NULL },
	{ "content_encoding",	MAGIC_MIME_ENCODING,	NULL },
	{ "content",		0,			NULL },
	{ NULL,			0,			NULL },
    };

    struct MS_s {
	const char *	name;
	const char *	algo;
	const char *	pub;
	const char *	sig;
    } MS[] = {
	{ "DSA",	"DSA/SHA1",	NULL, 0 },
	{ "RSA",	"RSA/SHA256",	NULL, 0 },
	{ "ECDSA",	"ECDSA/SHA256",	NULL, 0 },
	{ NULL,		NULL,		NULL, 0 },
    };
int save_cpl;

    char * t;
    char * te;
    size_t nt = 0;

    bson_error_t berr;
    bson_t * metadata = NULL;

    int rc = 0;
    int i;
    int xx;

    ifd = Fopen(ifn, "r");
    if (ifd == NULL || Ferror(ifd))
	goto exit;
    xx = Fstat(ifd, &sb);
    if (xx < 0)
	goto exit;

    nb = sb.st_size;
assert(nb);
    b = mmap(NULL, nb, PROT_READ, MAP_SHARED, Fileno(ifd), 0);
assert(b != (void *)-1);

    nt += sizeof("    {");
    for (i = 0; MD[i].name != NULL; i++) {
	DIGEST_CTX ctx = NULL;
	ctx = rpmDigestInit(MD[i].algo, RPMDIGEST_NONE);
assert(ctx);
	xx = rpmDigestUpdate(ctx, b, nb);
	xx = rpmDigestFinal(ctx, &MD[i].digest, &MD[i].digestlen, 1);
	nt += sizeof("\n\t\"\":\t\"\",");
	nt += strlen(MD[i].name);
	nt += strlen(MD[i].digest);
    }
    nt += sizeof("    }");

    {	const char * magicfile = rpmExpand("%{?_rpmfc_magic_path}", NULL);
	nt += sizeof("    {");
	for (i = 0; MC[i].name != NULL; i++) {
	    rpmmg mg = NULL;

	    mg = rpmmgNew(magicfile, MC[i].flags);
assert(mg);
	    MC[i].value = rpmmgBuffer(mg, b, nb);
	    mg = rpmmgFree(mg);
	    nt += sizeof("\n\t\"\":\t\"\",");
	    nt += strlen(MC[i].name);
	    nt += strlen(MC[i].value);
	}
	nt += sizeof("    }");

	magicfile = _free(magicfile);
    }

save_cpl = b64encode_chars_per_line;
b64encode_chars_per_line = 0;
    nt += sizeof("    {");
    for (i = 0; MS[i].name != NULL; i++) {
	pgpDig dig = pgpDigNew(RPMVSF_DEFAULT, PGPPUBKEYALGO_UNKNOWN);
	pgpDigParams sigp = pgpGetSignature(dig);
	pgpDigParams pubp = pgpGetPubkey(dig);
	DIGEST_CTX ctx = NULL;

	(void)pubp;
	addMacro(NULL, "_build_sign",       NULL, MS[i].algo, -1);

	xx = pgpDigSetAlgos(dig);
	xx = pgpImplGenerate(dig);
	xx = pgpImplExportPubkey(dig);
	MS[i].pub = b64encode(dig->pub, dig->publen);

	ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);
assert(ctx);
	xx = rpmDigestUpdate(ctx, b, nb);

	/* Finalize the signature digest. */
	sigp->signhash16[0] = 0;	/* XXX FIXME */
	sigp->signhash16[1] = 0;	/* XXX FIXME */

	xx = pgpImplExportSignature(dig, ctx);
	MS[i].sig = b64encode(dig->sig, dig->siglen);

	nt += sizeof("\n\t\"\":\t\"\",");
	nt += strlen(MS[i].name) + sizeof("pub");
	nt += strlen(MS[i].pub);
	nt += sizeof("\n\t\"\":\t\"\",");
	nt += strlen(MS[i].name) + sizeof("sig");
	nt += strlen(MS[i].sig);

	delMacro(NULL, "_build_sign");

	dig = pgpDigFree(dig);
    }
    nt += sizeof("    }");
b64encode_chars_per_line = save_cpl;

    t = xmalloc(nt+1);
    te = t;
    
    te = stpcpy(te, "    {");
    for (i = 0; MD[i].name != NULL; i++) {
	if (i != 0)
	    te = stpcpy(te, ",");
	te = stpcpy(te, "\n\t\"");
	te = stpcpy(te, MD[i].name);
	te = stpcpy(te, "\":\t\"");
        te = stpcpy(te, MD[i].digest);
	te = stpcpy(te, "\"");
    }

    for (i = 0; MC[i].name != NULL; i++) {
	te = stpcpy(te, ",");
	te = stpcpy(te, "\n\t\"");
	te = stpcpy(te, MC[i].name);
	te = stpcpy(te, "\":\t\"");
        te = stpcpy(te, MC[i].value);
	te = stpcpy(te, "\"");
    }

    for (i = 0; MS[i].name != NULL; i++) {
	te = stpcpy(te, ",");
	te = stpcpy(te, "\n\t\"");
	te = stpcpy(te, MS[i].name);
	te = stpcpy(te, "pub");
	te = stpcpy(te, "\":\t\"");
        te = stpcpy(te, MS[i].pub);
	te = stpcpy(te, "\"");

	te = stpcpy(te, ",");
	te = stpcpy(te, "\n\t\"");
	te = stpcpy(te, MS[i].name);
	te = stpcpy(te, "sig");
	te = stpcpy(te, "\":\t\"");
        te = stpcpy(te, MS[i].sig);
	te = stpcpy(te, "\"");
    }

    te = stpcpy(te, "\n    }");
    *te = '\0';

    fprintf(stderr, "%s\n", t);

    metadata = rpmgfsChk(gfs, __FUNCTION__, &berr,
			bson_new_from_json((uint8_t *)t, strlen(t), &berr));

    t = _free(t);

exit:
SPEW((stderr, "<-- %s(%p,%s,%p) rc %d\n", __FUNCTION__, gfs, ifn, op, rc));
    if (b != (void *)-1)
	xx = munmap(b, nb);
    if (ifd)
	xx = Fclose(ifd);

    op->md5 = MD[0].digest;			MD[0].digest = NULL;
    op->filename = rpmCleanPath(xstrdup(ifn));
    op->content_type = MC[0].value;		MC[0].value = NULL;
    op->aliases = NULL;
    op->metadata = metadata;
    op->chunk_size = 0;

    for (i = 0; MD[i].name != NULL; i++)
	MD[i].digest = _free(MD[i].digest);
    for (i = 0; MC[i].name != NULL; i++)
	MC[i].value = _free(MC[i].value);
    for (i = 0; MS[i].name != NULL; i++) {
	MS[i].pub = _free(MS[i].pub);
	MS[i].sig = _free(MS[i].sig);
    }

    return rc;
}

/*==============================================================*/
int rpmgfsGet(rpmgfs gfs, const char * dfn, const char * sfn)
{
    mongoc_stream_t *S = NULL;
    mongoc_gridfs_file_t *F = NULL;
    char buf[BUFSIZ];
    bson_error_t berr;
    FD_t fd = NULL;
    ssize_t nr;
    int rc = 1;		/* assume failure */

    if (dfn == NULL)
	dfn = "-";
    fd = Fopen(dfn, "w");
    if (fd == NULL || Ferror(fd))
	goto exit;

    gfs->iov.iov_base = (void *)buf;
    gfs->iov.iov_len = sizeof buf;

    F = rpmgfsChk(gfs, __FUNCTION__, &berr,
		mongoc_gridfs_find_one_by_filename(gfs->G, sfn, &berr));
    if (F == NULL)
	goto exit;

    S = mongoc_stream_gridfs_new(F);
    if (S == NULL)
	goto exit;

    for (;;) {
	nr = mongoc_stream_readv(S, &gfs->iov, 1, -1, 0);
	if (nr < 0)
	    goto exit;
	if (nr == 0)
            break;

	if (Fwrite(gfs->iov.iov_base, nr, 1, fd) != (size_t)nr)
	    goto exit;
    }
    rc = 0;

exit:
SPEW((stderr, "<-- %s(%p,%s) rc %d\n", __FUNCTION__, gfs, sfn, rc));
    if (fd)
	(void) Fclose(fd);
    if (S)
	mongoc_stream_destroy(S);
    if (F)
	mongoc_gridfs_file_destroy(F);
    return rc;
}

int rpmgfsPut(rpmgfs gfs, const char * dfn, const char * sfn)
{
    mongoc_stream_t *S = NULL;
    mongoc_gridfs_file_t *F = NULL;
    mongoc_gridfs_file_opt_t opt = { 0 };
    int rc = 1;		/* assume failure */

assert(dfn);
assert(sfn);

    if (rpmgfsSetFileOpt(gfs, sfn, &opt))
	goto exit;

    S = mongoc_stream_file_new_for_path(sfn, O_RDONLY, 0);
    if (S == NULL)
	goto exit;

    F = mongoc_gridfs_create_file_from_stream(gfs->G, S, &opt);
    if (F == NULL)
	goto exit;

    mongoc_gridfs_file_save(F);
    rc = 0;

exit:
SPEW((stderr, "<-- %s(%p,%s,%s) rc %d\n", __FUNCTION__, gfs, dfn, sfn, rc));

    opt.md5 = _free(opt.md5);
    opt.filename = _free(opt.filename);
    opt.content_type = _free(opt.content_type);
    if (opt.metadata)
	bson_free((void *)opt.metadata);

#ifdef	SEGFAULT
    if (S)
	mongoc_stream_destroy(S);
#endif
    if (F)
	mongoc_gridfs_file_destroy(F);
    return rc;
}

int rpmgfsDel(rpmgfs gfs, const char * sfn)
{
    bson_error_t berr;
    mongoc_gridfs_file_t *F = NULL;
    int rc = 1;		/* assume failure */

    F = rpmgfsChk(gfs, __FUNCTION__, &berr,
		mongoc_gridfs_find_one_by_filename(gfs->G, sfn, &berr));
    if (F == NULL)
	goto exit;

    if (!mongoc_gridfs_file_remove(F, &berr)) {
	(void) rpmgfsChk(gfs, __FUNCTION__, &berr, NULL);
	goto exit;
    }

    rc = 0;

exit:
SPEW((stderr, "<-- %s(%p,%s) rc %d\n", __FUNCTION__, gfs, sfn, rc));
    if (F)
	mongoc_gridfs_file_destroy(F);
    return rc;
}

int rpmgfsList(rpmgfs gfs)
{
    mongoc_gridfs_file_t *F = NULL;
    mongoc_gridfs_file_list_t *D = NULL;
    bson_t query;
    bson_t child;
    int rc = 0;

    bson_init(&query);
    bson_append_document_begin(&query, "$orderby", -1, &child);
    bson_append_int32(&child, "filename", -1, 1);
    bson_append_document_end(&query, &child);
    bson_append_document_begin(&query, "$query", -1, &child);
    bson_append_document_end(&query, &child);

    D = mongoc_gridfs_find(gfs->G, &query);

    bson_destroy (&query);

    while ((F = mongoc_gridfs_file_list_next(D))) {
	const char * md5 = mongoc_gridfs_file_get_md5(F);
	const char * fn = mongoc_gridfs_file_get_filename(F);
	const char * content_type = mongoc_gridfs_file_get_content_type(F);
	const bson_t * aliases =  mongoc_gridfs_file_get_aliases(F);
	const bson_t * metadata =  mongoc_gridfs_file_get_metadata(F);
	uint64_t length = mongoc_gridfs_file_get_length(F);
	uint32_t chunk_size = mongoc_gridfs_file_get_chunk_size(F);
	int64_t upload_msecs = mongoc_gridfs_file_get_upload_date(F);
	time_t secs = (time_t) (upload_msecs/1000);
	struct tm _tm;
	struct tm *tm = gmtime_r(&secs, &_tm);
	char _iso[64];

	(void)aliases;
	(void)metadata;

	(void) strftime(_iso, sizeof(_iso), "%FT%T", tm);

	printf("%s %s\t%8lu(%uk) %s\t%s\n",
		(md5 ? md5 : ""), (content_type ? content_type : ""),
		(unsigned long)length, (unsigned)((chunk_size+1023)/1024),
		_iso, fn);

	mongoc_gridfs_file_destroy(F);
	F = NULL;
    }

SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, gfs, rc));
    if (F)
	mongoc_gridfs_file_destroy(F);
    if (D)
	mongoc_gridfs_file_list_destroy(D);
    return rc;
}

static bool
mongoc_dump_mkdir_p(const char * dn, mode_t mode)
{
   if (access(dn, F_OK) && mkdir(dn, mode))
	return false;
   return true;
}

static int
mongoc_dump_collection(rpmgfs gfs,
		const char * database, const char * collection)
{
    mongoc_collection_t * col = NULL;
    mongoc_cursor_t * cursor = NULL;
    const bson_t * doc = NULL;
    bson_error_t berr;
    bson_t query = BSON_INITIALIZER;
    FD_t fd = NULL;
    char * fn = NULL;
    int rc = EXIT_SUCCESS;

    fn = bson_strdup_printf("dump/%s/%s.bson", database, collection);
    if (access(fn, F_OK))
	unlink(fn);

    fd = Fopen(fn, "w");
    if (fd == NULL || Ferror(fd)) {
	fprintf (stderr, "Failed to open \"%s\", aborting.\n", fn);
	goto exit;
    }

    col = mongoc_client_get_collection(gfs->C, database, collection);
    if (col == NULL)
	goto exit;

    cursor = mongoc_collection_find(col, MONGOC_QUERY_NONE, 0, 0, 0,
                                    &query, NULL, NULL);

    while (mongoc_cursor_next(cursor, &doc)) {
	size_t nw = Fwrite(bson_get_data(doc), 1, doc->len, fd);
	if (BSON_UNLIKELY(doc->len != nw)) {
	    fprintf(stderr, "Failed to write %u bytes to %s\n", doc->len, fn);
	    goto exit;
	}
    }

    if (mongoc_cursor_error(cursor, &berr)) {
fprintf(stderr, "*** %s: (%u.%u) %s\n", __FUNCTION__, berr.domain, berr.code, berr.message);
	goto exit;
    }
    rc = EXIT_SUCCESS;

exit:
SPEW((stderr, "<-- %s(%p,%s,%s) rc %d\n", __FUNCTION__, gfs, database, collection, rc));
    if (fn)
	bson_free(fn);
    if (fd)
	(void) Fclose(fd);
    if (cursor)
	mongoc_cursor_destroy(cursor);
    if (col)
	mongoc_collection_destroy(col);

   return rc;
}

static int
mongoc_dump_database (rpmgfs gfs,
		const char * database, const char * collection)
{
    mongoc_database_t * db = NULL;
    bson_error_t berr;
    char * dn = NULL;
    char ** str = NULL;
    int rc = EXIT_FAILURE;
    int i;

    BSON_ASSERT (database);

    dn = bson_strdup_printf ("dump/%s", database);
    if (!mongoc_dump_mkdir_p(dn, 0750)) {
	fprintf (stderr, "%s: failed to create directory \"%s\"", __FUNCTION__, dn);
	goto exit;
    }

    if (collection && *collection) {
	rc = mongoc_dump_collection(gfs, database, collection);
	goto exit;
    }

    db = mongoc_client_get_database(gfs->C, database);
    if (db == NULL)
	goto exit;

    str = mongoc_database_get_collection_names(db, &berr);
    if (str == NULL) {
fprintf(stderr, "*** %s: (%u.%u) %s\n", __FUNCTION__, berr.domain, berr.code, berr.message);
	goto exit;
    }

    for (i = 0; str[i]; i++) {
	if (mongoc_dump_collection(gfs, database, str[i]) != EXIT_SUCCESS)
	    goto exit;
    }
    rc = EXIT_SUCCESS;

exit:
SPEW((stderr, "<-- %s(%p,%s,%s) rc %d\n", __FUNCTION__, gfs, database, collection, rc));
    if (dn)
	bson_free (dn);
    if (db)
	mongoc_database_destroy(db);
    if (str)
	bson_strfreev(str);

   return rc;
}

static int
mongoc_dump (rpmgfs gfs)
{
    bson_error_t berr;
    char ** str = NULL;
    int rc = EXIT_FAILURE;
    int i;

    if (!mongoc_dump_mkdir_p("dump", 0750)) {
	perror("Failed to create directory \"dump\"");
	goto exit;
    }

    if (gfs->db && *gfs->db) {
	rc = mongoc_dump_database(gfs, gfs->db, gfs->coll);
	goto exit;
    }

    str = rpmgfsChk(gfs, __FUNCTION__, &berr,
		mongoc_client_get_database_names(gfs->C, &berr));
    if (str == NULL)
	goto exit;

    for (i = 0; str[i]; i++) {
	if (mongoc_dump_database(gfs, str[i], NULL) != EXIT_SUCCESS)
	    goto exit;
    }
    rc = EXIT_SUCCESS;

exit:
SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, gfs, rc));
    if (str)
	bson_strfreev(str);

    return rc;
}

int rpmgfsDump(rpmgfs gfs)
{
    int rc = mongoc_dump(gfs);

SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, gfs, rc));
    return rc;
}

/*==============================================================*/

static void rpmgfsFini(void * _gfs)
{
    rpmgfs gfs = (rpmgfs) _gfs;

    if (gfs->S)
	mongoc_stream_destroy(gfs->S);
    gfs->S = NULL;

    if (gfs->G)
	mongoc_gridfs_destroy(gfs->G);
    gfs->G = NULL;

    if (gfs->C)
	mongoc_client_destroy(gfs->C);
    gfs->C = NULL;

    if (--gfs_nopens <= 0)
	mongoc_cleanup();
    gfs_nopens = 0;

    gfs->scheme	= _free(gfs->scheme);
    gfs->u	= _free(gfs->u);
    gfs->pw	= _free(gfs->pw);
    gfs->user	= _free(gfs->user);
    gfs->h	= _free(gfs->h);
    gfs->p	= _free(gfs->p);
    gfs->host	= _free(gfs->host);
    gfs->db	= _free(gfs->db);
    gfs->coll	= _free(gfs->coll);
    gfs->opts	= _free(gfs->opts);
    gfs->uri	= _free(gfs->uri);

    gfs->fn = _free(gfs->fn);
    gfs->flags = 0;
    gfs->mode = 0;
}

rpmioPool _rpmgfsPool = NULL;

static rpmgfs rpmgfsGetPool(/*@null@*/ rpmioPool pool)
{
    rpmgfs gfs;

    if (_rpmgfsPool == NULL) {
	_rpmgfsPool = rpmioNewPool("gfs", sizeof(*gfs), -1, _rpmgfs_debug,
			NULL, NULL, rpmgfsFini);
	pool = _rpmgfsPool;
    }
    gfs = (rpmgfs) rpmioGetPool(pool, sizeof(*gfs));
    memset(((char *)gfs)+sizeof(gfs->_item), 0, sizeof(*gfs)-sizeof(gfs->_item));
    return gfs;
}

rpmgfs rpmgfsNew(const char * fn, int flags)
{
    rpmgfs gfs = rpmgfsGetPool(_rpmgfsPool);
    char * _Curi = NULL;
    char * _Gdb = NULL;
    char * _Gprefix = NULL;
    bson_error_t berr;

SPEW((stderr, "--> %s(%s,0x%x)\n", __FUNCTION__, fn, flags));
    if (fn)
	gfs->fn = xstrdup(fn);

    gfs->scheme	= rpmExpand("%{?_gfs_scheme}", NULL);
    gfs->u	= rpmExpand("%{?__gfs_u}", NULL);
    gfs->pw	= rpmExpand("%{?__gfs_pw}", NULL);
    gfs->user	= rpmExpand("%{?_gfs_user}", NULL);
    gfs->h	= rpmExpand("%{?__gfs_h}", NULL);
    gfs->p	= rpmExpand("%{?__gfs_p}", NULL);
    gfs->host	= rpmExpand("%{?_gfs_host}", NULL);
    gfs->db	= rpmExpand("%{?_gfs_db}", NULL);
#ifdef	NOTYET
    gfs->coll	= rpmExpand("%{?_gfs_coll}", NULL);
#else
    gfs->coll	= NULL;		/* XXX rpmgfsDump() needs NULL here. */
#endif
    gfs->opts	= rpmExpand("%{?_gfs_opts}", NULL);
    gfs->uri	= rpmExpand("%{?_gfs_uri}", NULL);

rpmgfsSpew(gfs);

    if (gfs_nopens++ == 0) {
	mongoc_log_set_handler(rpmgfsLog, (void *)gfs);
	mongoc_init();
    }

    _Curi = xstrdup(gfs->uri);
    _Gdb = strrchr(_Curi, '/');
    if (_Gdb) {
	*_Gdb++ = '\0';
	_Gprefix = strchr(_Gdb, '.');
	if (_Gprefix) {
	    char * t;
	    *_Gprefix++ = '\0';
	    t = strchr(_Gprefix, '.');
	    if (t)
		*t++ = '\0';
	}
    } else {
	_Gdb = (char *) (gfs->db ? gfs->db : "gfs");
	_Gprefix = "fs";
    }
    gfs->C = mongoc_client_new(_Curi);
assert(gfs->C);

    gfs->G = mongoc_client_get_gridfs(gfs->C, _Gdb, _Gprefix, &berr);
    if (gfs->G == NULL) {
	fprintf(stderr, "*** %s: (%u.%u) %s\n", __FUNCTION__, berr.domain, berr.code, berr.message);
    }
assert(gfs->G);

    _Curi = _free(_Curi);

    return rpmgfsLink(gfs);
}
