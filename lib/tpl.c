#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

#ifdef	DYING
#define	_isspace(_c)	\
	((_c) == ' ' || (_c) == '\t' || (_c) == '\r' || (_c) == '\n')

/*@unchecked@*/ /*@observer@*/
static struct cmpop {
/*@observer@*/ /*@null@*/
    const char * operator;
    rpmsenseFlags sense;
} cops[] = {
    { "<=", RPMSENSE_LESS | RPMSENSE_EQUAL},
    { "=<", RPMSENSE_LESS | RPMSENSE_EQUAL},

    { "==", RPMSENSE_EQUAL},
    
    { ">=", RPMSENSE_GREATER | RPMSENSE_EQUAL},
    { "=>", RPMSENSE_GREATER | RPMSENSE_EQUAL},

    { "<", RPMSENSE_LESS},
    { "=", RPMSENSE_EQUAL},
    { ">", RPMSENSE_GREATER},

    { NULL, 0 },
};

#define	_PERL_PROVIDES	"find /usr/lib/perl5 | /usr/lib/rpm/perl.prov"
/*@unchecked@*/ /*@observer@*/
static const char * _perl_provides = _PERL_PROVIDES;

int rpmdsPerl(rpmds * dsp, const char * cmd)
{
    char buf[BUFSIZ];
    const char *N, *EVR;
    int_32 Flags = 0;
    rpmds ds;
    char * f, * fe;
    char * g, * ge;
    FILE * fp = NULL;
    int rc = -1;
    int cmdprinted;
    int ln;
    int xx;

    if (cmd == NULL)
	cmd = _perl_provides;

    fp = popen(cmd, "r");
    if (fp == NULL)
	goto exit;

    ln = 0;
    cmdprinted = 0;
    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	ln++;

	/* insure a terminator. */
	buf[sizeof(buf)-1] = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* skip empty lines and comments */
	if (*f == '\0' || *f == '#')
	    continue;

	/* rtrim on line. */
	fe = f + strlen(f);
	while (--fe > f && _isspace(*fe))
	    *fe = '\0';

	/* split on ' '  or comparison operator. */
	fe = f;
	while (*fe && !_isspace(*fe) && strchr("<=>", *fe) == NULL)
	    fe++;
	while (*fe && _isspace(*fe))
	    *fe++ = '\0';

	if (!(xisalnum(f[0]) || f[0] == '_' || f[0] == '/')) {
	    if (!cmdprinted++)
		fprintf(stderr, _("running \"%s\" pipe command\n"), cmd);
	    /* XXX N must begin with alphanumeric, _, or /. */
	    fprintf(stderr,
		_("\tline %d: \"%s\"  must begin with alphanumeric, _, or /. Skipping ...\n"),
		    ln, f);
            continue;
	}

	N = f;
	EVR = NULL;
	Flags = 0;

	/* parse for non-path, versioned dependency. */
/*@-branchstate@*/
	if (*f != '/' && *fe != '\0') {
	    struct cmpop *cop;

	    /* parse comparison operator */
	    g = fe;
	    for (cop = cops; cop->operator != NULL; cop++) {
		if (strncmp(g, cop->operator, strlen(cop->operator)))
		    /*@innercontinue@*/ continue;
		*g = '\0';
		g += strlen(cop->operator);
		Flags = cop->sense;
		/*@innerbreak@*/ break;
	    }

	    if (Flags == 0) {
		if (!cmdprinted++)
		    fprintf(stderr, _("running \"%s\" pipe command\n"), cmd),
		/* XXX No comparison operator found. */
		fprintf(stderr, _("\tline %d: \"%s\" has not a comparison operator found. Skipping ...\n"),
			ln, g);
		continue;
	    }

	    /* ltrim on field 2. */
	    while (*g && _isspace(*g))
		g++;
	    if (*g == '\0') {
		if (!cmdprinted++)
		    fprintf(stderr, _("running \"%s\" pipe command\n"), cmd),
		/* XXX No EVR comparison value found. */
		fprintf(stderr, _("\tline %d: No EVR comparison value found.\n Skipping ..."),
			ln);
		continue;
	    }

	    ge = g + 1;
	    while (*ge && !_isspace(*ge))
		ge++;

	    if (*ge != '\0')
		*ge = '\0';	/* XXX can't happen, line rtrim'ed already. */

	    EVR = g;
	}

	if (EVR == NULL)
	    EVR = "";
	Flags |= RPMSENSE_PROBE;
/*@=branchstate@*/
	ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR , Flags);
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }

exit:
    if (fp != NULL) (void) pclose(fp);
    return rc;
}
#endif

int main(int argc, char *argv[])
{
    rpmds ds = NULL;
    int rc;

    rc = rpmdsPipe(&ds, 0, NULL);
    
    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0)
	fprintf(stderr, "%d %s\n", rpmdsIx(ds), rpmdsDNEVR(ds)+2);
    ds = rpmdsFree(ds);

    return 0;
}
