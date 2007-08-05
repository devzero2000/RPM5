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

extern void _populate_header_tags(HV *href);

static void
_populate_constant(HV *href, char *name, int val)
{
    hv_store(href, name, strlen(name), newSViv(val), 0);
}

#define REGISTER_CONSTANT(name) _populate_constant(constants, #name, name)

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
#if DYING
    {
	HV *header_tags, *constants; */
#endif
	rpmReadConfigFiles(NULL, NULL);
#if DYING
	header_tags = perl_get_hv("RPM::header_tag_map", TRUE);
	_populate_header_tags(header_tags);

	constants = perl_get_hv("RPM::constants", TRUE);

	/* not the 'standard' way of doing perl constants, but a lot easier to maintain */
	REGISTER_CONSTANT(RPMVSF_DEFAULT);
	REGISTER_CONSTANT(RPMVSF_NOHDRCHK);
	REGISTER_CONSTANT(RPMVSF_NEEDPAYLOAD);
	REGISTER_CONSTANT(RPMVSF_NOSHA1HEADER);
	REGISTER_CONSTANT(RPMVSF_NOMD5HEADER);
	REGISTER_CONSTANT(RPMVSF_NODSAHEADER);
	REGISTER_CONSTANT(RPMVSF_NORSAHEADER);
	REGISTER_CONSTANT(RPMVSF_NOSHA1);
	REGISTER_CONSTANT(RPMVSF_NOMD5);
	REGISTER_CONSTANT(RPMVSF_NODSA);
	REGISTER_CONSTANT(RPMVSF_NORSA);
	REGISTER_CONSTANT(_RPMVSF_NODIGESTS);
	REGISTER_CONSTANT(_RPMVSF_NOSIGNATURES);
	REGISTER_CONSTANT(_RPMVSF_NOHEADER);
	REGISTER_CONSTANT(_RPMVSF_NOPAYLOAD);
	REGISTER_CONSTANT(TR_ADDED);
	REGISTER_CONSTANT(TR_REMOVED);
    }
#endif

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
# #
#

void
_read_package_info(fp, vsflags)
	FILE *fp
	int vsflags
    PREINIT:
	rpmts ts;
	Header ret;
	rpmRC rc;
	FD_t fd;
    PPCODE:
	ts = rpmtsCreate();

        /* XXX Determine type of signature verification when reading
	vsflags |= _RPMTS_VSF_NOLEGACY;
	vsflags |= _RPMTS_VSF_NODIGESTS;
	vsflags |= _RPMTS_VSF_NOSIGNATURES;
	xx = rpmtsSetVerifySigFlags(ts, vsflags);
        */ 

	fd = fdDup(fileno(fp));
	rpmtsSetVSFlags(ts, vsflags);
	rc = rpmReadPackageFile(ts, fd, "filename or other identifier", &ret);

	Fclose(fd);

	if (rc == RPMRC_OK) {
	    SV *h_sv;

	    EXTEND(SP, 1);

	    h_sv = sv_newmortal();
            sv_setref_pv(h_sv, "RPM::C::Header", (void *)ret);

	    PUSHs(h_sv);
	}
	else {
	    croak("error reading package");
	}
	ts = rpmtsFree(ts);

void
_create_transaction(vsflags)
	int vsflags
    PREINIT:
	rpmts ret;
	SV *h_sv;
    PPCODE:
	/* Looking at librpm, it does not look like this ever
	   returns error (though maybe it should).
	*/
	ret = rpmtsCreate();

	/* Should I save the old vsflags aside? */
	rpmtsSetVSFlags(ret, vsflags);

	/* Convert and throw the results on the stack */	
	EXTEND(SP, 1);

	h_sv = sv_newmortal();
	sv_setref_pv(h_sv, "RPM::C::Transaction", (void *)ret);

	PUSHs(h_sv);

void
_read_from_file(fp)
	FILE *fp
PREINIT:
	SV *h_sv;
	FD_t fd;
	Header h;
PPCODE:
	fd = fdDup(fileno(fp));
	h = headerRead(fd);

	if (h) {
	    EXTEND(SP, 1);

	    h_sv = sv_newmortal();
	    sv_setref_pv(h_sv, "RPM::C::Header", (void *)h);

	    PUSHs(h_sv);
	}
	Fclose(fd);


rpmdb
_open_rpm_db(for_write)
	int   for_write
    PREINIT:
	 rpmdb db;
    CODE:
	if (rpmdbOpen(NULL, &db, for_write ? O_RDWR | O_CREAT : O_RDONLY, 0644)) {
		croak("rpmdbOpen failed");
		RETVAL = NULL;
	}
	RETVAL = db;		
    OUTPUT:
	RETVAL

