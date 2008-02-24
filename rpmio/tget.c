#include "system.h"

#include <rpmio_internal.h>
#include <poptIO.h>

#include "debug.h"

static char * rpmPermsString(mode_t st_mode)
{
    char *perms = xstrdup("----------");
   
    if (S_ISREG(st_mode)) 
	perms[0] = '-';
    else if (S_ISDIR(st_mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(st_mode))
	perms[0] = 'l';
    else if (S_ISFIFO(st_mode)) 
	perms[0] = 'p';
    /*@-unrecog@*/
    else if (S_ISSOCK(st_mode)) 
	perms[0] = 's';
    /*@=unrecog@*/
    else if (S_ISCHR(st_mode))
	perms[0] = 'c';
    else if (S_ISBLK(st_mode))
	perms[0] = 'b';
    else
	perms[0] = '?';

    if (st_mode & S_IRUSR) perms[1] = 'r';
    if (st_mode & S_IWUSR) perms[2] = 'w';
    if (st_mode & S_IXUSR) perms[3] = 'x';
 
    if (st_mode & S_IRGRP) perms[4] = 'r';
    if (st_mode & S_IWGRP) perms[5] = 'w';
    if (st_mode & S_IXGRP) perms[6] = 'x';

    if (st_mode & S_IROTH) perms[7] = 'r';
    if (st_mode & S_IWOTH) perms[8] = 'w';
    if (st_mode & S_IXOTH) perms[9] = 'x';

    if (st_mode & S_ISUID)
	perms[3] = ((st_mode & S_IXUSR) ? 's' : 'S'); 

    if (st_mode & S_ISGID)
	perms[6] = ((st_mode & S_IXGRP) ? 's' : 'S'); 

    if (st_mode & S_ISVTX)
	perms[9] = ((st_mode & S_IXOTH) ? 't' : 'T');

    return perms;
}

static void printStat(const char * path, struct stat * st)
{
    size_t nt = 100;
    char * t = alloca(nt);
    time_t when = st->st_mtime;
    struct tm * tm = localtime(&when);
    size_t nb = strftime(t, nt, "%F %T", tm);
    const char * perms = rpmPermsString(st->st_mode);

    t[nb] = '\0';
    /* XXX Note st->st_blocks to convert to linux "ls -island" spew. */
    fprintf(stderr, "%u %u %s %u %u %u %u %s %s",
	(unsigned) st->st_ino, (unsigned)st->st_blocks/2,
	perms, (unsigned)st->st_nlink,
	(unsigned)st->st_uid, (unsigned)st->st_gid,
	(unsigned)st->st_size, t, path);
    fprintf(stderr, "\n");
    perms = _free(perms);
}

static int readFile(const char * path)
{
    rpmop sop = memset(alloca(sizeof(*sop)), 0, sizeof(*sop));
    rpmop gop = memset(alloca(sizeof(*gop)), 0, sizeof(*gop));
    FD_t fd;
    struct stat sb;
    size_t len = 0;
    int rc;
    int xx;

fprintf(stderr, "===== %s\n", path);
    xx = rpmswEnter(sop, 0);
    rc = Stat(path, &sb);
    xx = rpmswExit(sop, 1);
    if (rc < 0)
	goto exit;

    printStat(path, &sb);
    if (!S_ISREG(sb.st_mode))
	goto exit;

    xx = rpmswEnter(gop, 0);
    fd = Fopen(path, "r.ufdio");
    if (fd != NULL) {
	size_t nb = 8 * BUFSIZ;
	char * buf = alloca(nb);
	*buf = '\0';
	len = Fread(buf, 1, nb-2, fd);
	buf[BUFSIZ-1] = '\0';
        xx = Fclose(fd);
	if (rpmIsVerbose() && len >= 0) {
	    buf[len] = '\n';
	    buf[len+1] = '\0';
	    fwrite(buf, 1, len+1, stderr);
	}
    }
    xx = rpmswExit(gop, len);

exit:
    if (_rpmsw_stats) {
	rpmswPrint("stat:", sop);
	rpmswPrint(" get:", gop);
    }
    return rc;
}

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    ARGV_t av = NULL;
    int ac;
    const char * fn;
    int rc;

    if (__debug) {
_av_debug = -1;
_dav_debug = -1;
_ftp_debug = -1;
_url_debug = -1;
_rpmio_debug = -1;
    }

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    rc = 0;
    while (rc == 0 && (fn = *av++) != NULL)
	rc = readFile(fn);

exit:

    optCon = rpmioFini(optCon);

    return rc;
}
