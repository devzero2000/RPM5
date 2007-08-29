
#define	RPM2XAR
#include "system.h"
#include <xar/xar.h>
#include <rpmio.h>
#include <rpmcli.h>
#include "debug.h"

static int _debug = 0;

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
#if 0
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
#else
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
#endif
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

static int wrXARbuffer(rpmmap map, const char * fn, char * b, size_t nb)
{
    if (b && nb > 0) {
	char val[32];
	map->f = xar_add_frombuffer(map->x, NULL, fn, b, nb);
	if (map->f == NULL)
	    return 1;
	xar_prop_set(map->f, "uid", "0");
	xar_prop_set(map->f, "gid", "0");
	xar_prop_set(map->f, "mode", "00644");
	xar_prop_set(map->f, "user", "root");
	xar_prop_set(map->f, "group", "root");
	snprintf(val, sizeof(val), "%llu", (unsigned long long)nb);
	xar_prop_set(map->f, "size", val);
	xar_prop_set(map->f, "data/size", val);
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
    const char * rpmfn = "time-1.7-22.i386.rpm";
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
