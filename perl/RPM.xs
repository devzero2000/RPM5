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
BOOT:
    {
	HV *header_tags, *constants;
	rpmReadConfigFiles(NULL, NULL);

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

double
rpm_api_version(pkg)
	char * pkg
    CODE:
	RETVAL = (double)5.0; /* TODO: kill this */
    OUTPUT:
	RETVAL


void
add_macro(pkg, name, val)
	char * pkg
	char * name
	char * val
    CODE:
	addMacro(NULL, name, NULL, val, RMIL_DEFAULT);

void
delete_macro(pkg, name)
	char * pkg
	char * name
    CODE:
	delMacro(NULL, name);

void
expand_macro(pkg, str)
	char * pkg
	char * str
    PREINIT:
	char *ret;
    PPCODE:
	ret = rpmExpand(str, NULL);
	PUSHs(sv_2mortal(newSVpv(ret, 0)));
	free(ret);

int
rpmvercmp(one, two)
	char* one
	char* two

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
	h = headerRead(fd, HEADER_MAGIC_YES);

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

MODULE = RPM		PACKAGE = RPM::C::DB

void
DESTROY(db)
	rpmdb db
    CODE:
	rpmdbClose(db);

void
_close_rpm_db(self)
	rpmdb self
    CODE:
	rpmdbClose(self);

rpmdbMatchIterator
_init_iterator(db, rpmtag, key, len)
	rpmdb db
	int rpmtag
	char *key
	size_t len
    CODE:
	RETVAL = rpmdbInitIterator(db, rpmtag, key && *key ? key : NULL, len);
    OUTPUT:
	RETVAL

MODULE = RPM		PACKAGE = RPM::C::PackageIterator
Header
_iterator_next(i)
	rpmdbMatchIterator i
    PREINIT:
	Header       ret;
        SV *         h_sv;
	unsigned int offset;
    PPCODE:
	ret = rpmdbNextIterator(i);
	if (ret)
		headerLink(ret);
	if(ret != NULL) 
		offset = rpmdbGetIteratorOffset(i);
	else
		offset = 0;
	
	EXTEND(SP, 2);
	h_sv = sv_newmortal();
	sv_setref_pv(h_sv, "RPM::C::Header", (void *)ret);
	PUSHs(h_sv);
	PUSHs(sv_2mortal(newSViv(offset)));

void
DESTROY(i)
	rpmdbMatchIterator i
    CODE:
	rpmdbFreeIterator(i);


