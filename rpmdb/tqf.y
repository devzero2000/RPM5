%lex-param	{void * scanner}
%parse-param	{struct pass_to_bison *x}

%debug

%{
    #include "system.h"

    #include <stdarg.h>
    #include <popt.h>

    #include "rpmutil.h"

    #include "tqf.h"

    #include "debug.h"

#define	yylex	Tyylex
#define scanner  (x->flex_scanner)

    /* prototypes */
    nodeType *opr(int oper, int nops, ...);
    nodeType *con(unsigned long long value);
    nodeType *id(int i);
    nodeType *text(char *text);
    nodeType *tag(char *tag, char *mod);
    void freeNode(nodeType *p);
    long long ex(nodeType *p);
    long long sym[26]; /* symbol table */

extern int Tyylex ();
extern int Tyylex_init ();
extern int Tyylex_destroy ();

RPM_GNUC_PURE int Tyyget_column();
RPM_GNUC_PURE int Tyyget_in();
RPM_GNUC_PURE int Tyyget_out();

    void yyerror(void *x, char *s);

    static int tqfdebug = 0;
    
%}

%union {
    char *S;
    void *v;

    unsigned long long I;
    double F;

    char *text;
    char *tag;

    char sIndex; /* symbol table index */
    nodeType *nPtr; /* node pointer */
};

%token <I>	I_CONSTANT
%token <F>	F_CONSTANT
%token <S>	TC_TAGN
%token <S>	TF_TAGN
%token <S>	TF_MOD
%token <S>	TEXT

%token	TL_BGN
%token	TL_END

%token	TF_BGN
%token	TF_END

%token	TC_BGN
%token	TCT_BGN
%token	TCT_END
%token	TCF_BGN
%token	TCF_END
%token	TCTF_END
%token	TC_END

%token <sIndex> VARIABLE
%token WHILE IF PRINT

%nonassoc IFX
%nonassoc ELSE
%left '|'
%left '^'
%left '&'
%left EQ NE
%left GE LE '>' '<'
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'
%nonassoc UMINUS
%right POW
%left ','

%token	EXISTS
%token	GET
%token	SIZEOF

// %type <nPtr> stmt expr stmt_list
%type <nPtr> foo foo_list

%%

//program:
//      function { exit(0); }
//    ;
//
//function:
//      function stmt { ex($2); freeNode($2); }
//    | /* NULL */
//    ;
//
//stmt:
//      ';'			{ $$ = opr(';', 2, NULL, NULL); }
//    | expr ';'			{ $$ = $1; }
//    | PRINT expr ';'		{ $$ = opr(PRINT, 1, $2); }
//    | VARIABLE '=' expr ';'	{ $$ = opr('=', 2, id($1), $3); }
//    | WHILE '(' expr ')' stmt	{ $$ = opr(WHILE, 2, $3, $5); }
//    | IF '(' expr ')' stmt %prec IFX { $$ = opr(IF, 2, $3, $5); }
//    | IF '(' expr ')' stmt ELSE stmt { $$ = opr(IF, 3, $3, $5, $7); }
//    | '{' stmt_list '}'		{ $$ = $2; }
//    ;
//
//stmt_list:
//      stmt			{ $$ = $1; }
//    | stmt_list stmt		{ $$ = opr(';', 2, $1, $2); }
//    ;
//
//expr:
//      I_CONSTANT		{ $$ = con($1); }
//    | VARIABLE			{ $$ = id($1); }
//    | '-' expr %prec UMINUS	{ $$ = opr(UMINUS, 1, $2); }
//    | expr '+' expr		{ $$ = opr('+', 2, $1, $3); }
//    | expr '-' expr		{ $$ = opr('-', 2, $1, $3); }
//    | expr '*' expr		{ $$ = opr('*', 2, $1, $3); }
//    | expr '/' expr		{ $$ = opr('/', 2, $1, $3); }
//    | expr '%' expr		{ $$ = opr('%', 2, $1, $3); }
//    | expr '&' expr		{ $$ = opr('&', 2, $1, $3); }
//    | expr '|' expr		{ $$ = opr('|', 2, $1, $3); }
//    | expr '^' expr		{ $$ = opr('^', 2, $1, $3); }
//    | expr LSHIFT expr		{ $$ = opr(LSHIFT, 2, $1, $3); }
//    | expr RSHIFT expr		{ $$ = opr(RSHIFT, 2, $1, $3); }
//    | expr POW expr		{ $$ = opr(POW, 2, $1, $3); }
//    | expr '<' expr		{ $$ = opr('<', 2, $1, $3); }
//    | expr '>' expr		{ $$ = opr('>', 2, $1, $3); }
//    | expr GE expr		{ $$ = opr(GE, 2, $1, $3); }
//    | expr LE expr		{ $$ = opr(LE, 2, $1, $3); }
//    | expr NE expr		{ $$ = opr(NE, 2, $1, $3); }
//    | expr EQ expr		{ $$ = opr(EQ, 2, $1, $3); }
//    | '(' expr '?' expr ':' expr ')'	{ $$ = opr(IF, 3, $2, $4, $6); }
//    | '(' expr ')'		{ $$ = $2; }
//    ;

program:
      function { return 0; }
    ;

function:
      function foo_list
	{	nodeType *p = opr(PRINT, 1, $2); ex(p); freeNode(p); }
    | /* NULL */
    ;

foo:
      TEXT
	{	if (tqfdebug) fprintf(stderr, "-- TEXT(%u)\n", (unsigned)strlen($1));
		$$ = text($1);
	}
    | TF_BGN TF_TAGN TF_END
	{	if (tqfdebug) fprintf(stderr, "-- TFORMAT(%s)\n", $2);
		$$ = opr(GET, 1, tag($2, NULL));
	}
    | TF_BGN TF_TAGN TF_MOD TF_END
	{	if (tqfdebug) fprintf(stderr, "-- TFORMAT(%s%s)\n", $2, $3);
		$$ = opr(GET, 1, tag($2, $3));
	}
    | TC_BGN TC_TAGN TCT_BGN foo_list TCTF_END TC_END
	{	if (tqfdebug) fprintf(stderr, "-- TCOND IF(%s) THEN\n", $2);
		$$ = opr(IF, 2,
			opr(EXISTS, 1, tag($2, NULL)),
			opr(PRINT, 1, $4));
		if (tqfdebug) ex($$);
	}
    | TC_BGN TC_TAGN TCT_BGN foo_list TCTF_END TCF_BGN foo_list TCTF_END TC_END
	{	if (tqfdebug) fprintf(stderr, "-- TCOND IF(%s) THEN ELSE\n", $2);
		$$ = opr(IF, 3,
			opr(EXISTS, 1, tag($2, NULL)),
			opr(PRINT, 1, $4),
			opr(PRINT, 1, $7));
		if (tqfdebug) ex($$);
	}
    | TL_BGN foo_list TL_END
	{	if (tqfdebug) fprintf(stderr, "-- TLOOP\n");
		/* XXX Walk the AST and promote all the GET's, assign to scalars. */
		$$ = opr(',', 2,
			opr('=', 2, id('Z'-'A'), con(10)),
			opr(WHILE, 3,
				opr('<', 2, id('I'-'A'), id('Z'-'A')),
				opr(PRINT, 1, $2),
				opr('+', 2, id('I'-'A'), con(1)))
			);
		if (tqfdebug) ex($$);
	}
    ;

foo_list:
      foo
	{
		$$ = $1;
	}
    | foo_list foo
	{
		$$ = opr(',', 2, $1, $2);
	}
    ;

%%

#define SIZEOF_NODETYPE ((char *)&p->con - (char *)p)

static char *_TYPES[] = {
    "con",
    "id",
    "opr",
    "text",
    "tag",
    "?5?",
    "?6?",
    "?7?",
};

nodeType *con(unsigned long long value)
{
    nodeType * p = malloc(sizeof(*p));
assert(p != NULL);
    /* copy information */
    p->type = typeCon;
    p->con.u.I = value;
if (tqfdebug)
fprintf(stderr, "<== %s(%u) %p[%s:%u]\n", __FUNCTION__, (unsigned)value, p, _TYPES[p->type&0x7], (unsigned)p->con.u.I);
    return p;
}

nodeType *id(int i)
{
    nodeType * p = malloc(sizeof(*p));
assert(p != NULL);
    /* copy information */
    p->type = typeId;
    p->id.i = i;
if (tqfdebug)
fprintf(stderr, "<== %s(%d) %p[%s:%c]\n", __FUNCTION__, i, p, _TYPES[p->type&0x7], (p->id.i+'A'));
    return p;
} 

nodeType *text(char * text)
{
    nodeType * p = malloc(sizeof(*p));
assert(p != NULL);
    /* copy information */
    p->type = typeText;
    p->text.S = strdup(text);
if (tqfdebug)
fprintf(stderr, "<== %s(%s) %p[%s:%p]\n", __FUNCTION__, text, p, _TYPES[p->type&0x7], p->text.S);
    return p;
} 

nodeType *tag(char * tag, char * mod)
{
    nodeType * p = malloc(sizeof(*p));
assert(p != NULL);
    /* copy information */
    p->type = typeTag;

    /* HACK: tag needs to be truncated because of yy_{push,pop}_state() */
    if (tag) {
	char * S;
	/* HACK: filter leading non-printables */
	while (*tag && !isprint(*tag)) tag++;
	S = strdup(tag);
	for (char *se = S; *se; se++) {
	    if (strchr(":?}", *se) == NULL)
		continue;
	    if (isprint(*se))
		continue;
	    *se = '\0';
	    break;
	}
	p->tag.S = S;
    } else
	p->tag.S = strdup("NULL");


    /* W2DO? mod includes leading ':' */
    /* HACK: mod needs to be truncated because of yy_{push,pop}_state() */
    if (mod) {
	char * M = strdup((mod ? mod : ""));
	/* HACK: filter leading non-printables */
	while (*mod && !isprint(*mod)) mod++;
    	M = strdup(mod);
	for (char *se = M; *se; se++) {
	    if (strchr("}", *se) == NULL)
		continue;
	    if (isprint(*se))
		continue;
	    *se = '\0';
	    break;
	}
	p->tag.M = M;
    } else
	p->tag.M = strdup("");

if (tqfdebug)
fprintf(stderr, "<== %s(%s,%s) %p[%s:%s%s]\n", __FUNCTION__, tag, mod, p, _TYPES[p->type&0x7], p->tag.S, p->tag.M);

    return p;
} 

nodeType *opr(int oper, int nops, ...)
{
    nodeType * p = malloc(sizeof(*p));
    int i;
assert(p != NULL);
    p->opr.op = malloc(nops * sizeof(nodeType));
assert(p->opr.op != NULL);
    /* copy information */
    p->type = typeOpr;
    p->opr.oper = oper;
    p->opr.nops = nops;
    {	va_list ap;
	va_start(ap, nops);
	for (i = 0; i < nops; i++)
	    p->opr.op[i] = va_arg(ap, nodeType*);
	va_end(ap);
    }

if (tqfdebug) {
fprintf(stderr, "<== %s(0x%x,%d) %p [%s:", __FUNCTION__, oper, nops, p, _TYPES[p->type&0x7]);
    switch (p->opr.oper) {
    case PRINT:
fprintf(stderr, "PRINT:%d]", p->opr.nops);
	break;
    case WHILE:
fprintf(stderr, "WHILE:%d]", p->opr.nops);
	break;
    case IF:
fprintf(stderr, "IF:%d]", p->opr.nops);
	break;
    default:
fprintf(stderr, "%d:%d]", p->opr.oper, p->opr.nops);
	break;
    }

    for (i = 0; i < nops; i++) {
	nodeType * q = p->opr.op[i];
	fprintf(stderr, " [%d]=%p", i,  q);
	if (q) fprintf(stderr, "[%d]=%s", i,  _TYPES[q->type&0x7]);
    }
fprintf(stderr, "\n");
}

    return p;
}

void freeNode(nodeType *p)
{
    if (!p) return;
    if (p->type == typeOpr) {
	int i;
	for (i = 0; i < p->opr.nops; i++)
	    freeNode(p->opr.op[i]);
	free(p->opr.op);
    }
    free(p);
}

void yyerror(void * _x, char *s)
{
    fflush(stdout);
    fprintf(stderr, "*** %s\n", s);
}

RPM_GNUC_CONST
int Tyywrap(void * _scanner)
{
    return 1;
}

#if !defined(TSCANNER_MAIN)

int yydebug = 0;

int main(int argc, const char ** argv)
{
    Tparse_t x = {};
    void * _ctx = NULL;
    char * _ifn = NULL;
    char * _ofn = NULL;
    int _verbose = 0;
    struct poptOption _opts[] = {
     { "debug", 'd', POPT_BIT_SET,		&yydebug,	1,
        N_("debug"), NULL },
     { "verbose", 'v', POPT_ARG_VAL,		&_verbose,	1,
        N_("debug"), NULL },
     { "input", 'i', POPT_ARG_STRING,		&x.flex_ifn,	0,
        N_("input <fn>"), N_("<fn>") },
     { "output", 'o', POPT_ARG_STRING,		&x.flex_ofn,	0,
        N_("output <fn>"), N_("<fn>") },
      POPT_TABLEEND
    };
    poptContext con = poptGetContext(argv[0], argc, argv, _opts, 0);
    const char ** av;
    int ec = 0;

    while ((ec = poptGetNextOpt(con)) > 0) {
	char * arg = poptGetOptArg(con);
	if (arg) free(arg);
    }

    av = poptGetArgs(con);

    x.flex_debug = yydebug;

    if (av == NULL || av[0] == NULL) {
	Tparse_flex_init(&x);
	ec = yyparse(&x);
	Tparse_flex_destroy(&x);
    } else {
	int i;
	for (i = 0; av[i]; i++) {
fprintf(stderr, "==> %s\n", av[i]);
	    x.flex_ifn = av[i];
	    Tparse_flex_init(&x);
	    ec = yyparse(&x);
	    Tparse_flex_destroy(&x);
	    x.flex_ifn = NULL;
fprintf(stderr, "<== %s ec %d\n", av[i], ec);
	}
    }

    if (_ifn) free(_ifn);
    if (_ofn) free(_ofn);
    con = poptFreeContext(con);

    return ec;
}

#endif
