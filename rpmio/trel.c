#include "system.h"

#include <rpmio.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <poptIO.h>

#include "debug.h"

struct T_s {
    char * prefix;
    char * suffix;
} Tvals[] = {
  { "",				"", },
  { "",				".%{date +%Y%m%d}", },
  { "%{date +%Y%m%d}.",		"", },
  { NULL,			NULL, },
};

const char * Rvals[] = {
    "1%{?dist}",
    NULL
};

const char * Rexpand(const char *prefix, const char *R, const char *suffix)
{
    const char * _R_ = rpmExpand(prefix, R, suffix, NULL);
fprintf(stderr, "|%s|%s|%s|\t==> %s\n", prefix, R, suffix, _R_);
    return _R_;
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
    poptContext con = rpmioInit(argc, argv, optionsTable);
    struct T_s * T;
    const char ** R;
    int rc = 0;

    for (R = Rvals; *R; R++) {
	for (T = Tvals; (T->prefix && T->suffix); T++) {
	    const char * _R_ = Rexpand(T->prefix, *R, T->suffix);
	    _R_ = _free(_R_);
	}
    }

    con = rpmioFini(con);

    return rc;
}
