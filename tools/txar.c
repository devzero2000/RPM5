
#define	RPM2XAR
#include "system.h"
#include <inttypes.h>
#include <xar/xar.h>
#include <rpmio.h>
#include <rpmcli.h>
#include "debug.h"

static int _debug = 0;
static int _display = 0;
static int _explode = 0;

typedef struct rpmmap_s {
    const char * fn;
    FD_t fd;
    void * b;
    size_t nb;
    char * l;
    size_t nl;
    char * s;
    size_t ns;
    char * h;
    size_t nh;
    char * p;
    size_t np;
    xar_t x;
    xar_file_t f;
    xar_iter_t i;
} * rpmmap;

static rpmmap rpmmapFree(rpmmap map)
{
    if (map) {
	int xx;

	if (map->b) {
	    xx = munmap(map->b, map->nb);
	    map->b = NULL;
	    map->nb = 0;
	} else {
	    map->l = _free(map->l);
	    map->s = _free(map->s);
	    map->h = _free(map->h);
	    map->p = _free(map->p);
	}
	if (map->fd) {
	    (void) Fclose(map->fd);
	    map->fd = NULL;
	}
	map->fn = _free(map->fn);
	map = _free(map);
    }
    return NULL;
}

static rpmmap rpmmapNew(const char * fn)
{
    rpmmap map = xcalloc(1, sizeof(*map));
    struct stat sb, *st = &sb;
    int xx;

    map->fn = xstrdup(fn);
    xx = Stat(map->fn, st);
    map->nb = st->st_size;

    return map;
}

static size_t hSize(uint_32 *p)
{
    return (8 + 8 + 16 * ntohl(p[2]) + ntohl(p[3]));
}

static rpmmap rdRPM(const char * rpmfn)
{
    rpmmap map = rpmmapNew(rpmfn);

    map->fd = Fopen(map->fn, "r");

    map->b = mmap(NULL, map->nb, PROT_READ, MAP_SHARED, Fileno(map->fd), 0);

    map->l = map->b;
    map->nl = 96;

    map->s = map->l + map->nl;
    map->ns = hSize((void *)map->s);
    map->ns += (8 - (map->ns % 8));	/* padding */

    map->h = map->s + map->ns;
    map->nh = hSize((void *)map->h);

    map->p = map->h + map->nh;
    map->np = map->nb;
    map->np -= map->nl + map->ns + map->nh;

    return map;
}

static rpmmap rdXAR(const char * xarfn)
{
    rpmmap map = rpmmapNew(xarfn);

    map->x = xar_open(map->fn, READ);

    map->i = xar_iter_new();
    for (map->f = xar_file_first(map->x, map->i);
	 map->f != NULL;
	 map->f = xar_file_next(map->i))
    {
	if (_display) {
	    const char * key;
	    xar_iter_t p;

	    p = xar_iter_new();
	    for (key = xar_prop_first(map->f, p);
		 key != NULL;
		 key = xar_prop_next(p))
	    {
		const char * val = NULL;
		xar_prop_get(map->f, key, &val);
		fprintf(stderr, "key: %s, value: %s\n", key, val);
	    }
	    xar_iter_free(p);
      }
      {
	const char * type;
	const char * name;
	char * b;
	size_t nb;
	int xx;

	b = NULL;
	nb = 0;
	xx = xar_extract_tobuffersz(map->x, map->f, &b, &nb);
if (_debug)
fprintf(stderr, "*** xx %d %p[%lu]\n", xx, b, (unsigned long)nb);
	if (xx || b == NULL || nb == 0)
	    continue;

	type = NULL;
	xx = xar_prop_get(map->f, "type", &type);
if (_debug)
fprintf(stderr, "*** xx %d type %s\n", xx, type);
	if (xx || type == NULL || strcmp(type, "file"))
	    continue;

	name = NULL;
	xx = xar_prop_get(map->f, "name", &name);
if (_debug)
fprintf(stderr, "*** xx %d name %s\n", xx, name);
	if (xx || name == NULL)
	    continue;

if (_debug)
fprintf(stderr, "*** %s %p[%lu]\n", name, b, (unsigned long)nb);
	if (!strcmp(name, "Lead")) {
	    map->l = b;
	    map->nl = nb;
	} else
	if (!strcmp(name, "Signature")) {
	    map->s = b;
	    map->ns = nb;
	} else
	if (!strcmp(name, "Header")) {
	    map->h = b;
	    map->nh = nb;
	} else
	if (!strcmp(name, "Payload")) {
	    map->p = b;
	    map->np = nb;
	} else
	    continue;
      }
    }
    if (map->i) {
	xar_iter_free(map->i);
	map->i = NULL;
    }
    if (map->x) {
	xar_close(map->x);
	map->x = NULL;
    }
    return map;
}

#ifdef	NOTYET
static struct {
	const char *name;
	mode_t type;
} filetypes [] = {
	{ "file", S_IFREG },
	{ "directory", S_IFDIR },
	{ "symlink", S_IFLNK },
	{ "fifo", S_IFIFO },
	{ "character special", S_IFCHR },
	{ "block special", S_IFBLK },
	{ "socket", S_IFSOCK },
#ifdef S_IFWHT
	{ "whiteout", S_IFWHT },
#endif
	{ NULL, 0 }
};

static const char * filetype_name (mode_t mode) {
	unsigned int i;
	for (i = 0; filetypes[i].name; i++)
		if (mode == filetypes[i].type)
			return (filetypes[i].name);
	return ("unknown");
}

static void x_addtime(xar_file_t f, const char * key, const time_t * timep)
{
    char time[128];
    struct tm t;

    gmtime_r(timep, &t);
    memset(time, 0, sizeof(time));
    strftime(time, sizeof(time), "%FT%T", &t);
    strcat(time, "Z");
    xar_prop_set(f, key, time);
}

int32_t _xar_stat_archive(xar_t x, xar_file_t f, const char *file, const char *buffer, size_t len, struct stat *st);
int32_t _xar_stat_archive(xar_t x, xar_file_t f, const char *file, const char *buffer, size_t len, struct stat *st)
{
	char *tmpstr;
	struct passwd *pw;
	struct group *gr;
	const char *type;

#ifdef	IGNORE
	/* no stat attributes for data from a buffer, it is just a file */
	if(len){
		xar_prop_set(f, "type", "file");
		return 0;
	}
	
	if( S_ISREG(st->st_mode) && (st->st_nlink > 1) ) {
		xar_file_t tmpf;
		const char *id = xar_attr_get(f, NULL, "id");
		if( !id ) {
			xar_err_new(x);
			xar_err_set_file(x, f);
			xar_err_set_string(x, "stat: No file id for file");
			xar_err_callback(x, XAR_SEVERITY_NONFATAL, XAR_ERR_ARCHIVE_CREATION);
			return -1;
		}
		tmpf = xar_link_lookup(x, st->st_dev, st->st_ino, f);
		xar_prop_set(f, "type", "hardlink");
		if( tmpf ) {
			const char *id;
			id = xar_attr_get(tmpf, NULL, "id");
			xar_attr_set(f, "type", "link", id);
		} else {
			xar_attr_set(f, "type", "link", "original");
		}
	} else
#endif
	{
		type = filetype_name(st->st_mode & S_IFMT);
		xar_prop_set(f, "type", type);
	}

#ifdef	IGNORE
	/* Record major/minor device node numbers */
	if( S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		uint32_t major, minor;
		char tmpstr[12];
		xar_devmake(st->st_rdev, &major, &minor);
		memset(tmpstr, 0, sizeof(tmpstr));
		snprintf(tmpstr, sizeof(tmpstr)-1, "%u", major);
		xar_prop_set(f, "device/major", tmpstr);
		memset(tmpstr, 0, sizeof(tmpstr));
		snprintf(tmpstr, sizeof(tmpstr)-1, "%u", minor);
		xar_prop_set(f, "device/minor", tmpstr);
	}

	if( S_ISLNK(st->st_mode) ) {
		char link[4096];
		struct stat lsb;

		memset(link, 0, sizeof(link));
		readlink(file, link, sizeof(link)-1);
		xar_prop_set(f, "link", link);
		if( stat(file, &lsb) != 0 ) {
			xar_attr_set(f, "link", "type", "broken");
		} else {
			type = filetype_name(lsb.st_mode & S_IFMT);
			xar_attr_set(f, "link", "type", type);
		}
	}
#endif

	asprintf(&tmpstr, "%04o", st->st_mode & (~S_IFMT));
	xar_prop_set(f, "mode", tmpstr);
	free(tmpstr);

	asprintf(&tmpstr, "%"PRIu64, (uint64_t)st->st_uid);
	xar_prop_set(f, "uid", tmpstr);
	free(tmpstr);

	pw = getpwuid(st->st_uid);
	if( pw )
		xar_prop_set(f, "user", pw->pw_name);

	asprintf(&tmpstr, "%"PRIu64, (uint64_t)st->st_gid);
	xar_prop_set(f, "gid", tmpstr);
	free(tmpstr);

	gr = getgrgid(st->st_gid);
	if( gr )
		xar_prop_set(f, "group", gr->gr_name);

	x_addtime(f, "atime", &st->st_atime);

	x_addtime(f, "mtime", &st->st_mtime);

	x_addtime(f, "ctime", &st->st_ctime);

#ifdef	IGNORE
	flags_archive(f, st);

	aacls(f, file);
#endif

	return 0;
}
#endif

static int wrXARbuffer(rpmmap map, const char * fn, char * b, size_t nb)
{
    if (b && nb > 0) {
	struct stat sb, *st = &sb;
	time_t now = time(NULL);

	if (_explode) {
	    FD_t fd = Fopen(fn, "w");

	    if (fd) {
		(void) Fwrite(b, 1, nb, fd);
		(void) Fclose(fd);
	    }
	}

	memset(st, 0, sizeof(*st));
	st->st_dev = 0;
	st->st_ino = 0;
	st->st_mode = S_IFREG | 0644;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = nb;
	st->st_blksize = 0;
	st->st_blocks = 0;
	st->st_atime = now;
	st->st_mtime = now;
	st->st_ctime = now;

#ifdef	NOTYET	/* xar-1.5.1 patching needed. */
	xar_set_stat(map->x, st);
#endif

	map->f = xar_add_frombuffer(map->x, NULL, fn, b, nb);
	if (map->f == NULL)
	    return 1;

#ifdef	NOTYET
	xar_prop_set(map->f, "mode", "00644");
	xar_prop_set(map->f, "uid", "0");
	xar_prop_set(map->f, "user", "root");
	xar_prop_set(map->f, "gid", "0");
	xar_prop_set(map->f, "group", "root");

	x_addtime(map->f, "atime", &now);
	x_addtime(map->f, "mtime", &now);
	x_addtime(map->f, "ctime", &now);

#if 0
	xar_prop_set(map->f, "data", NULL);
	xar_prop_set(map->f, "data/extracted-checksum", NULL);
	xar_prop_set(map->f, "data/archived-checksum", NULL);
	xar_prop_set(map->f, "data/encoding", NULL);
#endif

	{   char val[32];
	    snprintf(val, sizeof(val), "%llu", (unsigned long long)nb);
	    xar_prop_set(map->f, "data/size", val);
	}

#if 0
	xar_prop_set(map->f, "data/offset", NULL);
	xar_prop_set(map->f, "data/length", NULL);
#endif
#endif
    }
    return 0;
}

static int wrXAR(const char * xarfn, rpmmap map)
{
    int rc = 1;	/* assume error */

    map->x = xar_open(xarfn, WRITE);
    if (map->x == NULL)
	goto exit;

    if ((rc = wrXARbuffer(map, "Lead", map->l, map->nl)) != 0)
	goto exit;
    if ((rc = wrXARbuffer(map, "Signature", map->s, map->ns)) != 0)
	goto exit;
    if ((rc = wrXARbuffer(map, "Header", map->h, map->nh)) != 0)
	goto exit;
    if ((rc = wrXARbuffer(map, "Payload", map->p, map->np)) != 0)
	goto exit;
    rc = 0;

exit:
    if (map->x) {
	xar_close(map->x);
	map->x = NULL;
    }

    return rc;
}

static int wrRPMbuffer(rpmmap map, const char * fn, char * b, size_t nb)
{
fprintf(stderr, "*** wrRPMbuffer(%p, %s, %p[%lu])\n", map, fn, b, (unsigned long)nb);
    if (b && nb > 0) {
	if (Fwrite(b, 1, nb, map->fd) != nb)
	    return 1;
    }
    return 0;
}

static int wrRPM(const char * rpmfn, rpmmap map)
{
    int rc = 1;	/* assume error */

    map->fd = Fopen(rpmfn, "w");
    if (map->fd == NULL)
	goto exit;

    if ((rc = wrRPMbuffer(map, "Lead", map->l, map->nl)) != 0)
	goto exit;
    if ((rc = wrRPMbuffer(map, "Signature", map->s, map->ns)) != 0)
	goto exit;
    if ((rc = wrRPMbuffer(map, "Header", map->h, map->nh)) != 0)
	goto exit;
    if ((rc = wrRPMbuffer(map, "Payload", map->p, map->np)) != 0)
	goto exit;
    rc = 0;

exit:
    if (map->fd) {
	(void) Fclose(map->fd);
	map->fd = NULL;
    }

    return rc;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,		&_debug, 1,
	NULL, NULL },
 { "display", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_display, 1,
	NULL, NULL },
 { "explode", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&_explode, 1,
	NULL, NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options:"),
	NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    int ret = 0;
    rpmmap map;
    const char * rpmfn = "time-1.7-29.i386.rpm";
    const char * xarfn = "time.xar";

    if (optCon == NULL)
        exit(EXIT_FAILURE);

#if defined(RPM2XAR)
    map = rdRPM(rpmfn);
    ret = wrXAR(xarfn, map);
#else
    map = rdXAR(xarfn);
    ret = wrRPM("time.rpm", map);
#endif

    map = rpmmapFree(map);

    optCon = rpmcliFini(optCon);

    return ret;
}
