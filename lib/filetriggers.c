#include "system.h"
#include <rpmio.h>
#include <rpmlib.h>
#include <rpmlog.h>

#define	_MIRE_INTERNAL
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "rpmfi.h"
#include "filetriggers.h" /* mayAddToFilesAwaitingFiletriggers rpmRunFileTriggers */

#include "argv.h"


static const char *files_awaiting_filetriggers = "/var/lib/rpm/files-awaiting-filetriggers";

#define FILTER_EXTENSION ".filter"

static char *_filetriggers_dir = NULL;
static char *filetriggers_dir(void)
{
     if (!_filetriggers_dir) _filetriggers_dir = rpmExpand("%{?_filetriggers_dir}", NULL);
     return _filetriggers_dir && _filetriggers_dir[0] ? _filetriggers_dir : NULL;
}

static char *get_filter_name(const char *filter_filename) {
     char *p = strrchr(filter_filename, '/');
     if (p != NULL) {
#ifdef HAVE_STRNDUP
         p = strndup(p+1, strlen(p+1) - strlen(FILTER_EXTENSION));
#else
         char *p_src;
         size_t p_len;
         p_src = p;
         p_len = strlen(p_src+1) - strlen(FILTER_EXTENSION);
         p = xmalloc(p_len+1);
         strncpy(p, p_src+1, p_len);
#endif
     }
     return p;
}

int mayAddToFilesAwaitingFiletriggers(const char *rootDir, rpmfi fi, int install_or_erase)
{
    int rc = RPMRC_FAIL;

    if (!filetriggers_dir()) return RPMRC_OK;

    fi = rpmfiInit(fi, 0);
    if (fi == NULL) return RPMRC_OK;

    const char *file = rpmGenPath(rootDir, files_awaiting_filetriggers, NULL);
    if (!file) return RPMRC_FAIL;

    FILE *f = fopen(file, "a");

    if (f == NULL) {
        rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), file, strerror(errno));
	goto exit;
    }

    while (rpmfiNext(fi) >= 0)
    {
        fputc(install_or_erase ? '+' : '-', f);
        fputs(rpmfiFN(fi), f);
	fputc('\n', f);
    }
    fclose(f);
    rc = RPMRC_OK;
exit:
    _free(file);
    return rc;
}

struct filetrigger_raw {
     char *regexp;
     char *name;
};
struct filetrigger {
     miRE mire;
     char *name;
     char *filename;
     int command_pipe;
     int command_pid;
};

static int getFiletriggers_raw(const char *rootDir, int *nb, struct filetrigger_raw **list_raw)
{
     const char *globstr = rpmGenPath(rootDir, filetriggers_dir(), "*" FILTER_EXTENSION);
     const char ** filter_files = NULL;
     int i;
     struct stat st;

     rpmGlob(globstr, nb, &filter_files);
     if (*nb == 0) return RPMRC_OK;

     *list_raw = calloc(*nb, sizeof(**list_raw));

     for (i = 0; i < *nb; i++) {
	  int filter = open(filter_files[i], O_RDONLY);
	  if (filter == -1) {
	       rpmlog(RPMLOG_ERR, "opening %s failed: %s\n", filter_files[i], strerror(errno));
	       continue;
	  }
	  if (fstat(filter, &st) == 0 && st.st_size > 0) {
	       char *regexp = malloc(st.st_size+1);
	       regexp[st.st_size] = '\0';
	       if (read(filter, regexp, st.st_size) != st.st_size) {
		    rpmlog(RPMLOG_ERR, "reading %s failed: %s\n", filter_files[i], strerror(errno));
	       } else {
		    char *p = strchr(regexp, '\n');
		    if (p) *p = '\0';
		    (*list_raw)[i].regexp = regexp;
		    (*list_raw)[i].name = get_filter_name(filter_files[i]);
	       }
	  }
	  close(filter);
     }
     _free(globstr);
     argvFree(filter_files);

     return RPMRC_OK;
}

static char *computeMatchesAnyFilter(int nb, struct filetrigger_raw *list_raw)
{
     int i, regexp_str_size = 0;

     for (i = 0; i < nb; i++) regexp_str_size += strlen(list_raw[i].regexp) + 1;

     char *matches_any = malloc(regexp_str_size);
     char *p = stpcpy(matches_any, list_raw[0].regexp);

     for (i = 1; i < nb; i++) {
	  *p++ = '|';
	  p = stpcpy(p, list_raw[i].regexp);
     }
     rpmlog(RPMLOG_DEBUG, "[filetriggers] matches-any regexp is %s\n", matches_any);
     return matches_any;
}

static void compileFiletriggersRegexp(char *raw, miRE mire)
{
     mireSetCOptions(mire, RPMMIRE_DEFAULT, 0, REG_NOSUB | REG_EXTENDED | REG_NEWLINE, NULL);

     if (mireRegcomp(mire, raw) != 0) {
	  rpmlog(RPMLOG_ERR, "failed to compile filetrigger filter: %s\n", raw);
	  free(mire);
     }
     free(raw);
}

static void getFiletriggers(const char *rootDir, miRE matches_any, int *nb, struct filetrigger **list)
{
     struct filetrigger_raw *list_raw;

     getFiletriggers_raw(rootDir, nb, &list_raw);
     if (*nb == 0) return;

     compileFiletriggersRegexp(computeMatchesAnyFilter(*nb, list_raw), matches_any);

     *list = calloc(*nb, sizeof(**list));
     int i;
     for (i = 0; i < *nb; i++) {
	  (*list)[i].name = list_raw[i].name;
	  (*list)[i].mire = mireNew(0, 0);
	  compileFiletriggersRegexp(list_raw[i].regexp, (*list)[i].mire);
     }
     free(list_raw);
}

static void freeFiletriggers(miRE matches_any, int nb, struct filetrigger *list)
{
     mireFree(matches_any);
     int i;
     for (i = 0; i < nb; i++) {
	  mireFree(list[i].mire);
	  free(list[i].name);
     }
     free(list);
}

static int is_regexp_matching(miRE re, const char *s)
{
     return mireRegexec(re, s, (size_t) 0) == 0;
}

static int popen_with_root(const char *rootDir, const char *cmd, const char *filename, int *pid)
{
     int pipes[2];

     if (pipe(pipes) != 0) return 0;

     *pid = fork();
     if (*pid == 0) {
	  close(pipes[1]);
	  dup2(pipes[0], STDIN_FILENO);
	  close(pipes[0]);

	  if (rootDir != NULL && strcmp(rootDir, "/") != 0) {
	       if (chroot(rootDir) != 0) {
		    rpmlog(RPMLOG_ERR, "chroot to %s failed\n", rootDir);
		    _exit(-1);
	       }
	       chdir("/");
	  }
	  const char *argv[3];
	  argv[0] = cmd;
	  argv[1] = filename;
	  argv[2] = NULL;
	  execv(argv[0], (char *const *) argv);
	  _exit(-1);
     }
    
     close(pipes[0]);

     return pipes[1];   
}

static void mayStartFiletrigger(const char *rootDir, struct filetrigger *trigger)
{
     if (!trigger->command_pipe) {
	  char *cmd = NULL;
	  if (asprintf(&cmd, "%s/%s.script", filetriggers_dir(), trigger->name) != -1) {
	    rpmlog(RPMLOG_DEBUG, "[filetriggers] spawning %s %s\n", cmd, trigger->filename);
	    trigger->command_pipe = popen_with_root(rootDir, cmd, trigger->filename, &trigger->command_pid);
	    _free(cmd);
	  }
     }
}

void rpmRunFileTriggers(const char *rootDir)
{
     miRE matches_any = mireNew(RPMMIRE_DEFAULT, 0);
     int nb = 0;
     struct filetrigger *list = NULL;

     if (!filetriggers_dir()) return;
     rpmlog(RPMLOG_DEBUG, _("[filetriggers] starting\n"));
     
     getFiletriggers(rootDir, matches_any, &nb, &list);

     const char *file = rpmGenPath(rootDir, files_awaiting_filetriggers, NULL);

     FILE *awaiting = NULL;
     if (nb > 0)
          awaiting = fopen(file, "r");
     if (awaiting) {
	  char tmp[1024];

	  void *oldhandler = signal(SIGPIPE, SIG_IGN);

	  while (fgets(tmp, sizeof(tmp), awaiting))
	       if (is_regexp_matching(matches_any, tmp)) {
		    rpmlog(RPMLOG_DEBUG, "[filetriggers] matches-any regexp found %s", tmp);
		    int i;
		    for (i = 0; i < nb; i++)
			  if (is_regexp_matching(list[i].mire, tmp)) {
				int tmplen = strlen(tmp);
				list[i].filename = malloc(tmplen - 1);
				int j;
				for (j = 1; j < tmplen; j++) list[i].filename[j-1] = tmp[j];
				mayStartFiletrigger(rootDir, &list[i]);
				write(list[i].command_pipe, tmp, tmplen);
			 }
	       }
	  fclose(awaiting);

	  int i;
	  for (i = 0; i < nb; i++)
	       if (list[i].command_pipe) {
		    close(list[i].command_pipe);
		    int status;
		    rpmlog(RPMLOG_DEBUG, "[filetriggers] waiting for %s to end\n", list[i].name);
		    waitpid(list[i].command_pid, &status, 0);
	       }
	  freeFiletriggers(matches_any, nb, list);

	  signal(SIGPIPE, oldhandler);
     }
     unlink(file);
     _free(file);
}
