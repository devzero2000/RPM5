%option	8bit yylineno
%option	stack

%option	noyyget_lineno
%option	noyyget_text
%option	noyyget_debug
%option noyyget_leng

%option noyyget_extra
%option noyyget_lval

%option noyy_top_state

%option reentrant bison-bridge
%option prefix="Tyy"

%option	debug

%{
    #include "system.h"

    #include <rpmutil.h>
    #include <popt.h>

    #include "tqf.h"
    #include "Tgrammar.h"

RPM_GNUC_PURE	FILE * Tyyget_in();
RPM_GNUC_PURE	FILE * Tyyget_out();
RPM_GNUC_PURE	int Tyyget_column();

    #include "debug.h"

    static void llerror(const char *msg);

%}

D	[0-9]
L	[a-zA-Z_]
A	[a-zA-Z_0-9]
WS	[ \t\r]

LINE	.*
SECTION	.*

SINGLE	{LINE}
MULTI	{LINE}

%x	L_IF
%x	L_ELSE

%x	L_ARGS
%x	L_BLOB

%x	L_SQUOTE
%x	L_DQUOTE

%x	L_QF
%x	L_QB
%x	L_QC

%%

	/* -- Comments */
{WS}*"#"{LINE}
{WS}*"//"{LINE}

	/* -- Single quoted strings */
	/**/
\'		{ yy_push_state(L_SQUOTE, yyscanner); }
<L_SQUOTE>\'\'	{ /* -- eat paired quotes */ }
<L_SQUOTE>\'	{ yy_pop_state(yyscanner); }
<L_SQUOTE><<EOF>> { llerror("unterminated single quote");	return EOF; }
	/**/

	/* -- Parsing blocks */
	/*
\%(ifarch|ifnarch){LINE}	{ yy_push_state(L_IF, yyscanner); }
\%(ifos|ifnos){LINE}		{ yy_push_state(L_IF, yyscanner); }
"%if"{LINE}			{ yy_push_state(L_IF, yyscanner); }
<L_IF>"%else"{LINE}		{ BEGIN(L_ELSE); }
<L_IF,L_ELSE>"%endif"{LINE}	{ yy_pop_state(yyscanner); }
<L_IF,L_ELSE>{LINE}
	*/

	/* ^"%include"{LINE} */

	/* -- Section keywords */
	/*
\%(package)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(description)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(prep)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(build)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(install)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(check)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(clean)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(pretrans)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(preun)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(pre)			{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(posttrans)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(postun)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(post)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(triggerpostun)	{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(triggerprein)	{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(triggerun)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(triggerin)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(trigger)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(verifyscript)	{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(sanitycheck)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(files)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
\%(changelog)		{ yy_push_state(L_BLOB, yyscanner); return TF_BGN; }
<L_BLOB>"^"		{ yy_pop_state(yyscanner); return TF_END; }
<L_BLOB>(?s:(\\.|[^^])+) { yylval->S = yytext;	return TF_TAGN; }
	*/

\%{L}+{WS}*.*{WS}*
"%}"

	/* -- Preamble keywords */
	/*
(?i:"Name")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }
(?i:"Epoch")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }
(?i:"Version")\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Release")\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Distepoch")\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Disttag")\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Repotag")\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Bugurl")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }

(?i:"Url")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }

(?i:"Buildroot")\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Docdir")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }
(?i:"Xmajor")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }
(?i:"Xminor")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }

(?i:"Source"){D}*\:{SINGLE}	{ yylval->S = yytext; return TEXT; }
(?i:"Patch"){D}*\:{SINGLE}	{ yylval->S = yytext; return TEXT; }

(?i:"Icon")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }

(?i:"Cvsid")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }
(?i:"Svnid")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }

(?i:"Serial")\:{SINGLE}		{ yylval->S = yytext; return TEXT; }

(?i:"Summary")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"License")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Group")\:{LINE}		{ yylval->S = yytext; return TEXT; }

(?i:"Copyright")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Distribution")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Disturl")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Vendor")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Packager")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Nosource")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Nopatch")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Excludearch")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Exclusivearch")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Excludeos")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Exclusiveos")\:{LINE}	{ yylval->S = yytext; return TEXT; }

(?i:"Provides")\:{MULTI}	{ yylval->S = yytext; return TEXT; }
(?i:"Requires")\:{MULTI}	{ yylval->S = yytext; return TEXT; }
(?i:"Conflicts")\:{MULTI}	{ yylval->S = yytext; return TEXT; }
(?i:"Obsoletes")\:{MULTI}	{ yylval->S = yytext; return TEXT; }

(?i:"Prereq")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Prefixes")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Prefix")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Buildarchitectures")\:{LINE} { yylval->S = yytext; return TEXT; }
(?i:"Buildarch")\:{LINE}	{ yylval->S = yytext; return TEXT; }

(?i:"BuildProvides")\:{MULTI}	{ yylval->S = yytext; return TEXT; }
(?i:"BuildRequires")\:{MULTI}	{ yylval->S = yytext; return TEXT; }
(?i:"BuildConflicts")\:{MULTI}	{ yylval->S = yytext; return TEXT; }
(?i:"BuildObsoletes")\:{MULTI}	{ yylval->S = yytext; return TEXT; }

(?i:"BuildPrereq")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Autoreqprov")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Autoreq")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Autoprov")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Suggests")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Enhances")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"BuildSuggests")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"BuildEnhances")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Variants")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Variant")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Keywords")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"Keyword")\:{LINE}		{ yylval->S = yytext; return TEXT; }
(?i:"BuildPlatforms")\:{LINE}	{ yylval->S = yytext; return TEXT; }
(?i:"Class")\:{LINE}		{ yylval->S = yytext; return TEXT; }
	*/

(?i:{L}+){D}*\:{LINE}	{ }
	
\"			{ yy_push_state(L_DQUOTE, yyscanner); }
<L_DQUOTE>\"\"		{ }
<L_DQUOTE>\"		{ yy_pop_state(yyscanner); }
<L_DQUOTE><<EOF>>	{ llerror("unterminated double quote");	return EOF; }
<L_DQUOTE>"%%"		{ yylval->S = yytext;		return TEXT; }
<L_DQUOTE>"["		{ return TL_BGN; }
<L_DQUOTE>"]"		{ return TL_END; }
<L_DQUOTE>"%"(((-)?{D}*(\.)?{D}*)|(=))?"{" {
			  yy_push_state(L_QF, yyscanner); return TF_BGN; }
<L_DQUOTE>"%|"		{ yy_push_state(L_QB, yyscanner); return TC_BGN; }
<L_DQUOTE>(?s:(\\.|[^"%\[\]])+) {
			  yylval->S = yytext;		return TEXT; }

<L_QF>"}"		{ yy_pop_state(yyscanner); 	return TF_END; }
<L_QF>":"{A}+		{ yylval->S = yytext;		return TF_MOD; }
<L_QF>{A}+		{ yylval->S = yytext;		return TF_TAGN;}

<L_QB>(\\.|[^?])+	{ BEGIN(L_QC); yylval->S = yytext; return TC_TAGN;}

<L_QC>"|"		{ yy_pop_state(yyscanner); 	return TC_END; }

<L_QC>"}"		{ return TCTF_END; }
<L_QC>"?{"		{ return TCT_BGN; }
<L_QC>":{"		{ return TCF_BGN; }
<L_QC>"%{"		{ yy_push_state(L_QF, yyscanner); return TF_BGN; }
<L_QC>(?s:(\\.|[^|}?:%])+) {
			  yylval->S = yytext;		return TEXT; }

<<EOF>>	{ return EOF; }
.	{ fprintf(stderr, "--eating '%c': (%s)\n", *yytext, yytext ); }

%%

static void llerror(const char * msg)
{
    fprintf(stderr, "%s\n", msg);
}

#if !defined(TSCANNER_MAIN)

void Tparse_flex_init(Tparse_t *x)
{
    yylex_init(&x->flex_scanner);
#ifdef	NOTYET
    x->flex_extra = calloc(1, sizeof(struct Tcontext_t));
    Tyy_scan_string(x->text, x->flex_scanner);
    Tyyset_extra(x->flex_extra, x->flex_scanner);
#endif
    yyset_debug(0, x->flex_scanner);
}

void Tparse_flex_destroy(Tparse_t *x)
{
#ifdef	NOTYET
    free(x->flex_extra);
    x->flex_extra = NULL;
#endif
    yylex_destroy(x->flex_scanner);
}

#else	/* TSCANNER_MAIN */

static
void scan_fini(yyscan_t scanner)
{
    if (scanner) {
	FILE * ifp = yyget_in(scanner);
	FILE * ofp = yyget_out(scanner);

	yylex_destroy(scanner);
	if (ifp != stdin)
	    fclose(ifp);
	if (ofp != stdout && ofp != stderr)
	    fclose(ofp);
    }
}

static
yyscan_t scan_init(YY_EXTRA_TYPE ctx, const char * ifn, const char * ofn,
		int debug)
{
    yyscan_t scanner;
    FILE * ifp = stdin;
    FILE * ofp = stdout;
     
    if (ctx)
	yylex_init_extra(ctx, &scanner);
    else
	yylex_init(&scanner);

    if (ifn) {
if (debug)
fprintf(stderr, "--> %s: yyin(%s)\n", __FUNCTION__, ifn);
	if (!strcmp("-", ifn)) {
	    ifp = stdin;
	} else {
	    ifp = fopen(ifn, "r");
assert(ifp);
	}
	yyset_in(ifp, scanner);
    }

    if (ofn) {
if (debug)
fprintf(stderr, "--> %s: yyout(%s)\n", __FUNCTION__, ofn);
	if (!strcmp("-", ofn))
	    ofp = stdout;
	else {
	    ofp = fopen(ofn, "r");
assert(ofp);
	}
	yyset_out(ofp, scanner);
    }

if (debug)
fprintf(stderr, "--> %s: yydebug(%d)\n", __FUNCTION__, debug);
    yyset_debug(debug, scanner);

    return scanner;
}

int scan_loop(yyscan_t scanner)
{
    int more = 1;

    while (more) {
	YYSTYPE llval;
	int rc;
	rc = yylex(&llval, scanner);
	switch(rc) {
	case TEXT:
	    fprintf(stderr, "%s", llval.S);
	    break;
	case TF_TAGN:
	case TC_TAGN:
	    fprintf(stderr, "%s", llval.S );
	    break;
	case TF_MOD:
	    fprintf(stderr, "%s", llval.S );
	    break;
	case TL_BGN:
	    fprintf(stderr, "[");
	    break;
	case TL_END:
	    fprintf(stderr, "]");
	    break;
	case TF_BGN:
	    fprintf(stderr, "%%{");
	    break;
	case TF_END:
	    fprintf(stderr, "}");
	    break;
	case TC_BGN:
	    fprintf(stderr, "%%|");
	    break;
	case TC_END:
	    fprintf(stderr, "|");
	    break;
	case TCT_BGN:
	    fprintf(stderr, "?{");
	    break;
	case TCF_BGN:
	    fprintf(stderr, ":{");
	    break;
	case TCT_END:
	case TCF_END:
	case TCTF_END:
	    fprintf(stderr, "}");
	    break;
	case '"':
	    fprintf(stderr, "\"");
	    break;
	case EOF:
	    more = 0;
	    break;
	default:
	    fprintf(stderr, "==>%c<==]", (char)rc);
	    break;
	}
    }
    scan_fini(scanner);
    return 0;
}

RPM_GNUC_CONST
int yywrap(yyscan_t scanner)
{
    return 1;
}

int main(int argc, const char ** argv)
{
    void * _ctx = NULL;
    char * _ifn = NULL;
    char * _ofn = NULL;
    int _verbose = 0;
    int _debug = 0;
    struct poptOption _opts[] = {
     { "debug", 'd', POPT_BIT_SET,		&_debug,	1,
        N_("debug"), NULL },
     { "verbose", 'v', POPT_ARG_VAL,		&_verbose,	1,
        N_("debug"), NULL },
     { "input", 'i', POPT_ARG_STRING,		&_ifn,		0,
        N_("input <fn>"), N_("<fn>") },
     { "output", 'o', POPT_ARG_STRING,		&_ofn,		0,
        N_("output <fn>"), N_("<fn>") },
      POPT_TABLEEND
    };
    poptContext con = poptGetContext(argv[0], argc, argv, _opts, 0);
    const char ** av;
    int ec;

    while ((ec = poptGetNextOpt(con)) > 0) {
	char * arg = poptGetOptArg(con);
	if (arg) free(arg);
    }

    av = poptGetArgs(con);

    if (av == NULL || av[0] == NULL)
	ec = scan_loop( scan_init(_ctx, _ifn, _ofn, _debug) );
    else {
	int i;
	for (i = 0; av[i]; i++) {
	    ec = scan_loop( scan_init(_ctx, av[i], _ofn, _debug) );
	}
    }

    if (_ifn) free(_ifn);
    if (_ofn) free(_ofn);
    con = poptFreeContext(con);

    return ec;

}

#endif
