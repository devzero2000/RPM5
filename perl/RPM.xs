#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef Fflush
#undef Mkdir
#undef Stat

#include <stdio.h>
#include <string.h>
#include <utime.h>
#include <utime.h>

#include "rpmlib.h"
#include "rpmio.h"
#include "rpmcli.h"
#include "rpmts.h"
#include "rpmte.h"
#include "rpmmacro.h"
#include "rpmevr.h"
#include "header.h"
#include "rpmdb.h"
#include "misc.h"

/* The perl callback placeholder for output err messages */
SV * log_callback_function = NULL;

/* This function is called by rpm if a callback
 * is set for for the logging system.
 * If the callback is set, rpm does not print any message,
 * and let the callback to do it */
void logcallback(void) {
    dSP;
    if (log_callback_function) {
        int logcode = rpmlogCode();
        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVpv("logcode", 0)));
        XPUSHs(sv_2mortal(newSViv(logcode)));
        XPUSHs(sv_2mortal(newSVpv("msg", 0)));
        XPUSHs(sv_2mortal(newSVpv(rpmlogMessage(), 0)));
        XPUSHs(sv_2mortal(newSVpv("priority", 0)));
        XPUSHs(sv_2mortal(newSViv(RPMLOG_PRI(logcode))));
        PUTBACK;
        call_sv(log_callback_function, G_DISCARD | G_SCALAR);
        SPAGAIN;
    }
}

MODULE = RPM		PACKAGE = RPM

PROTOTYPES: ENABLE

#define crutch_stack_wrap(directive) do { \
  PUSHMARK(SP); \
  PUTBACK; \
  directive; \
  SPAGAIN; \
  PUTBACK; \
} while(0)

BOOT:
    crutch_stack_wrap(boot_RPM__Constant(aTHX_ cv));
    crutch_stack_wrap(boot_RPM__Header(aTHX_ cv));
    crutch_stack_wrap(boot_RPM__Transaction(aTHX_ cv));
    crutch_stack_wrap(boot_RPM__PackageIterator(aTHX_ cv));
    crutch_stack_wrap(boot_RPM__Problems(aTHX_ cv));
    crutch_stack_wrap(boot_RPM__Files(aTHX_ cv));
    crutch_stack_wrap(boot_RPM__Dependencies(aTHX_ cv)); 
    crutch_stack_wrap(boot_RPM__Spec(aTHX_ cv));
	rpmReadConfigFiles(NULL, NULL);

#
# Macro functions
#

void
rpmversion()
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(RPMVERSION, 0)));

void
add_macro(macro)
	char * macro
    CODE:
    rpmDefineMacro(NULL, macro, 0);

void
delete_macro(name)
	char * name
    CODE:
	delMacro(NULL, name);

void
expand_macro(string)
	char * string
    PREINIT:
	char *ret = NULL;
    PPCODE:
	ret = rpmExpand(string, NULL);
	XPUSHs(sv_2mortal(newSVpv(ret, 0)));
	free(ret);

int
load_macro_file(filename)
    char * filename
    CODE:
    RETVAL= ! rpmLoadMacroFile(NULL, filename); /* return False on error */
    OUTPUT:
    RETVAL

void
reset_macros()
    PPCODE:
    rpmFreeMacros(NULL);

void
dump_macros(fp = stdout)
    FILE *fp
    CODE:
    rpmDumpMacroTable(NULL, fp);

#
# Scoring functions
#

int
rpmvercmp(one, two)
	char* one
	char* two

int
platformscore(platform)
    const char * platform
    CODE:
    RETVAL=rpmPlatformScore(platform, NULL, 0);
    OUTPUT:
    RETVAL


#
# Verbosity & log functions
#

void
setverbosity(svlevel)
    SV * svlevel
    CODE:
    rpmSetVerbosity(sv2constant(svlevel, "rpmlog"));

void
lastlogmsg()
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmlogCode())));
    XPUSHs(sv_2mortal(newSVpv(rpmlogMessage(), 0)));

int
setlogfile(filename)
    char * filename
    PREINIT:
    FILE * ofp = NULL;
    FILE * fp = NULL;
    CODE:
    if (filename && *filename != 0) {
        if ((fp = fopen(filename, "a+")) == NULL) {
            XSprePUSH; PUSHi((IV)0);
            XSRETURN(1);
        }
    }
    if((ofp = rpmlogSetFile(fp)) != NULL)
        fclose(ofp);
    RETVAL=1;
    OUTPUT:
    RETVAL

void
setlogcallback(function = NULL)
    SV * function
    CODE:
    if (function == NULL || !SvOK(function)) {
        if (log_callback_function) {
            SvREFCNT_dec(log_callback_function);
            log_callback_function = NULL;
        }
        rpmlogSetCallback(NULL);
    } else if (SvTYPE(SvRV(function)) == SVt_PVCV) {
        if (log_callback_function) {
            SvREFCNT_dec(log_callback_function);
            log_callback_function = NULL;
        }
        SvREFCNT_inc(function);
        log_callback_function = newSVsv(function);
        rpmlogSetCallback(logcallback);
    } else
        croak("First arg is not a code reference");

void
rpmlog(svcode, msg)
    SV * svcode
    char * msg
    CODE:
    rpmlog(sv2constant(svcode, "rpmlog"), "%s", msg);

