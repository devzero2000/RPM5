%option	8bit yylineno
%option	stack

%option	noyyget_lineno
%option	noyyget_in
%option	noyyget_out
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

    #include "tqf.h"
    #include "Tgrammar.h"

    #include "debug.h"
%}

L	[a-zA-Z_]
A	[a-zA-Z_0-9]

%x	Q
%x	QF
%x	QB
%x	QC

%%

\"	{ yy_push_state(Q, yyscanner); }
<Q>\"	{ yy_pop_state(yyscanner); }

	/* DELETE
	<Q>(?s:({A}|[ :]|\\.)+)	{ yylval->S = yytext;	return TEXT; }
	<Q>(?s:({A}|[ :]|\\"%{"|\\"%|"|\\.)+) { yylval->S = yytext;	return TEXT; }
<Q>[\}\|]
	{   if (poppa) {
		yy_pop_state(yyscanner);
		poppa = 0;
		yy_pop_state(yyscanner);
		return (*yytext == '}' ? TCTF_END : TC_END);
	    }
	}
	<Q>(?s:({A}|[ :]|\\.)+)[^%\[\]]? { yylval->S = yytext;	return TEXT; }
	*/

<Q>"["	{ return TL_BGN; }
<Q>"]"	{ return TL_END;; }
<Q>"%{"	{ yy_push_state(QF, yyscanner); 	return TF_BGN; }
<Q>"%|"	{ yy_push_state(QB, yyscanner); 	return TC_BGN; }
<Q>(?s:(\\.|[^%\[\]"])+) { yylval->S = yytext;	return TEXT; }

<QF>"}"		{ yy_pop_state(yyscanner); 	return TF_END; }
<QF>":"{A}+	{ yylval->S = yytext;		return TF_MOD; }
<QF>{A}+	{ yylval->S = yytext;		return TF_TAGN; }

<QB>(\\.|[^?])+	{ BEGIN(QC); yylval->S = yytext; return TC_TAGN; }

<QC>"|"		{ yy_pop_state(yyscanner); 	return TC_END; }
<QC>"}"		{ return TCTF_END; }
<QC>"?{"	{ return TCT_BGN; }
<QC>":{"	{ return TCF_BGN; }
<QC>"%{"	{ yy_push_state(QF, yyscanner); return TF_BGN; }
<QC>(?s:(\\.|[^|}?:%])+) { yylval->S = yytext;	return TEXT; }

.	{ fprintf(stderr, "--eating '%c': (%s)\n", *yytext, yytext ); }

%%

#if !defined(TSCANNER_MAIN)

void Tparse_flex_init(Tparse_t *x)
{
    yylex_init(&x->flex_scanner);
#ifdef	NOTYET
    x->flex_extra = calloc(1, sizeof(struct Tcontext_t));
    Tyy_scan_string(x->text, x->flex_scanner);
    Tyyset_extra(x->flex_extra, x->flex_scanner);
#endif
}

void Tparse_flex_destroy(Tparse_t *x)
{
#ifdef	NOTYET
    free(x->flex_extra);
    x->flex_extra = NULL;
#endif
    yylex_destroy(x->flex_scanner);
}

#else

RPM_GNUC_NORETURN
int yywrap(yyscan_t scanner)
{
    yylex_destroy(scanner);
    exit(0);
}

int main(int argc, char **argv)
{
    YYSTYPE llval;
    yyscan_t scanner;
    int rc;

    yylex_init(&scanner);

    yyset_debug(0, scanner);

    while (1) {
	rc = yylex(&llval, scanner);
	switch (rc) {
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
	default:
	    fprintf(stderr, "==>%c<===]", (char)rc);
	    break;
	}
    }
}

#endif
