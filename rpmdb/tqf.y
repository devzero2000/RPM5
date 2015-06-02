%lex-param	{void * scanner}
%parse-param	{struct pass_to_bison *x}

%debug

%{
    #include "system.h"
    #include <stdarg.h>

    #include <rpmio.h>  /* for *Pool methods */
    #include <rpmlog.h>
    #include <poptIO.h>
    #include <argv.h>

    #include <rpmtypes.h>

#define _RPMTAG_INTERNAL
    #include <rpmtag.h>

#define	_TQF_INTERNAL
    #include "tqf.h"

    #include "debug.h"

#define	yylex	Tyylex
#define scanner	(x->flex_scanner)

#define yyHDR	((Header)x->flex_extra)
static HE_t heGet(Tparse_t *x, const char * tagname);
static nodeType *textTag(Tparse_t * x, char *l, char *t, char *m, char *r);

    /* prototypes */
    nodeType *con(unsigned long long value);
    nodeType *id(int i);
    nodeType *text(char *text);
    nodeType *tag(char *tag, char *mod);
    nodeType *opr(int oper, int nops, ...);
nodeType *appendNode(nodeType *p, nodeType *q);
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
    
#define HASHTYPE symtab
#define HTKEYTYPE const char *
#define HTDATATYPE HE_t
#include "rpmhash.H"
#include "rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

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

%token	TC_BGN
%token	TCT_BGN
%token	TCT_END
%token	TCF_BGN
%token	TCF_END
%token	TCTF_END
%token	TC_END

%token	<S>	TF_BGN
%token	<S>	TF_END

%token	TL_BGN
%token	TL_END

%token	TDQ
%token	TDQ_BGN
%token	TDQ_END
%token	TSQ_BGN
%token	TSQ_END

%token <sIndex> VARIABLE
%token WHILE IF PRINT TRANSLATE

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
%type <nPtr> foo foo_list // tagmod tagmod_list

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
//    | expr ';'		{ $$ = $1; }
//    | PRINT expr ';'		{ $$ = opr(PRINT, 1, $2); }
//    | VARIABLE '=' expr ';'	{ $$ = opr('=', 2, id($1), $3); }
//    | WHILE '(' expr ')' stmt	{ $$ = opr(WHILE, 2, $3, $5); }
//    | IF '(' expr ')' stmt %prec IFX { $$ = opr(IF, 2, $3, $5); }
//    | IF '(' expr ')' stmt ELSE stmt { $$ = opr(IF, 3, $3, $5, $7); }
//    | '{' stmt_list '}'	{ $$ = $2; }
//    ;
//
//stmt_list:
//      stmt			{ $$ = $1; }
//    | stmt_list stmt		{ $$ = opr(';', 2, $1, $2); }
//    ;
//
//expr:
//      I_CONSTANT		{ $$ = con($1); }
//    | VARIABLE		{ $$ = id($1); }
//    | '-' expr %prec UMINUS	{ $$ = opr(UMINUS, 1, $2); }
//    | expr '+' expr		{ $$ = opr('+', 2, $1, $3); }
//    | expr '-' expr		{ $$ = opr('-', 2, $1, $3); }
//    | expr '*' expr		{ $$ = opr('*', 2, $1, $3); }
//    | expr '/' expr		{ $$ = opr('/', 2, $1, $3); }
//    | expr '%' expr		{ $$ = opr('%', 2, $1, $3); }
//    | expr '&' expr		{ $$ = opr('&', 2, $1, $3); }
//    | expr '|' expr		{ $$ = opr('|', 2, $1, $3); }
//    | expr '^' expr		{ $$ = opr('^', 2, $1, $3); }
//    | expr LSHIFT expr	{ $$ = opr(LSHIFT, 2, $1, $3); }
//    | expr RSHIFT expr	{ $$ = opr(RSHIFT, 2, $1, $3); }
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
	{ ex($2); freeNode($2); }
    | /* NULL */
    ;

foo:
      TEXT
	{	if (tqfdebug) fprintf(stderr, "-- TEXT(%u)\n", (unsigned)strlen($1));
		$$ = text($1);
	}
    | TF_BGN TF_TAGN TF_END
	{	if (tqfdebug) fprintf(stderr, "-- TFORMAT(%s|%s(%u)|%s)\n", $1, $2, tagValue($2), $3);
		$$ = textTag( x, $1, $2, NULL, $3);
	}
    | TF_BGN TF_TAGN TF_MOD TF_END
	{	if (tqfdebug) fprintf(stderr, "-- TFORMAT(%s|%s(%u)|%s|%s)\n", $1, $2, tagValue($2), $3, $4);
		$$ = textTag( x, $1, $2, $3, $4);
	}
    | TC_BGN TC_TAGN TCT_BGN foo_list TCTF_END TC_END
	{	if (tqfdebug) fprintf(stderr, "-- TCOND IF(%s(%u)) THEN\n", $2, tagValue($2));
		$$ = opr(IF, 2,
			opr(EXISTS, 1, tag($2, NULL)),
			opr(PRINT, 1, $4));
		if (tqfdebug) ex($$);
	}
    | TC_BGN TC_TAGN TCT_BGN foo_list TCTF_END TCF_BGN foo_list TCTF_END TC_END
	{	if (tqfdebug) fprintf(stderr, "-- TCOND IF(%s(%u)) THEN ELSE\n", $2, tagValue($2));
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
    | TDQ_BGN foo_list TDQ_END
	{ $$ = opr(PRINT, 1, $2); }
    | TDQ foo_list TDQ_END
	{
		if (x->flex_lang)
		    $$ = opr(PRINT, 1, opr(TRANSLATE, 2, text(x->flex_lang), $2));
		else
		    $$ = opr(PRINT, 1, $2);
	}
    ;

foo_list:
      foo
	{
		$$ = $1;
	}
    | foo_list foo
	{
		$$ = appendNode($1, $2);
	}
    ;

%%

static const char * dAV(ARGV_t av)
{
    static char b[256];
    char * te = b;
    int i;

    *te = '\0';
    if (av == NULL) {
	stpcpy(b, "NULL");
	return b;
    }
    *te++ = '[';
    for (i = 0; av[i]; i++) {
	if (i) *te++ = ',';
	te = stpcpy(te, (char *)av[i]);
    }
    *te++ = ']';
    *te = '\0';
    return b;
}

/* XXX http://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers */
#if defined(__GNUC__)
#define __log2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
#else
static inline
unsigned __log2(uint64_t n)
{
    static const int table[64] = {
         0, 58,  1, 59, 47, 53,  2, 60, 39, 48, 27, 54, 33, 42,  3, 61,
        51, 37, 40, 49, 18, 28, 20, 55, 30, 34, 11, 43, 14, 22,  4, 62,
        57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19, 29, 10, 13, 21, 56,
        45, 25, 31, 35, 16,  9, 12, 44, 24, 15,  8, 23,  7,  6,  5, 63 };

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;

    return table[(n * 0x03f6eaf2cd271461) >> 58];
}
#endif

static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabdefghijklmnopqrstuvwxyz";
static char * heCoerce(HE_t he, const char ** av, const char *fmt)
{
unsigned base = 0;
    uint32_t ix = (he->ix > 0 ? he->ix : 0);
    uint64_t ui = 0;
    const char * str = NULL;
    char * b = NULL;
    size_t nb = 0;

    switch (he->t & ~RPM_MASK_RETURN_TYPE) {
    default:
assert(0);
	break;
    case RPM_BIN_TYPE:	/* hex */
    {   const uint8_t * s = he->p.ui8p;
	rpmTagCount c;
	char * t;

	nb = 2 * he->c + 1;
	t = b = xmalloc(nb);
	for (c = 0; c < he->c; c++) {
	    unsigned i = (unsigned) *s++;
	    *t++ = digits[ (i >> 4) & 0xf ];
	    *t++ = digits[ (i     ) & 0xf ];
	}
	*t = '\0';
    }   break;
    case RPM_UINT8_TYPE:	ui = (uint64_t)he->p.ui8p[ix];	break;
    case RPM_UINT16_TYPE:	ui = (uint64_t)he->p.ui16p[ix];	break;
    case RPM_UINT32_TYPE:	ui = (uint64_t)he->p.ui32p[ix];	break;
    case RPM_UINT64_TYPE:	ui = (uint64_t)he->p.ui64p[ix];	break;
    case RPM_STRING_TYPE:	str = he->p.str;		break;
    case RPM_STRING_ARRAY_TYPE:	str = he->p.argv[ix];		break;
    }

    if (str)		/* string */
	b = xstrdup(str);
    else if (nb == 0) {	/* number */
	if (base < 2 || base > sizeof(digits)) base = 10;
	nb = (ui ? (__log2(ui) / __log2(base) + 1) : 1);
	b = alloca(nb+1);
	b[nb] = '\0';
	do {
	    b[--nb] = digits[ ui % base ];
	    ui /= base;
	} while (ui);
	b = xstrdup(b+nb);
    }

#ifdef	NOISY
fprintf(stderr, "<== %s(%p,%s,%s) %s\n", __FUNCTION__, he, dAV(av), fmt, b);
#endif
    return b;
}

static const char *tagtypes[] = {
    "NULL",
    "CHAR",
    " UI8",
    "UI16",
    "UI32",
    "UI64",
    " STR",
    " BIN",
    "ARGV",
    " I18",
    "ASN1",
    " PGP",
    " ?12",
    " ?13",
    " ?14",
    " ?15",
};

static const char * dHE(HE_t he)
{
    static char b[1024];
    const char * str = heCoerce(he, NULL, NULL);

    snprintf(b, sizeof(b), "\t%s %s %p[%u] |%s|",
	tagtypes[he->t & 0xf], tagName(he->tag), he->p.ptr, he->c,
	(str ? str : ""));
    str = _free(str);
    return b;
}

static HE_t heGet(Tparse_t *x, const char * tagname)
{
    static const char allowed[] =
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
    HE_t he = NULL;
    Header h = yyHDR;
    HE_t * Chep = NULL;
    int nChe = 0;

    /* XXX trim trailing '#' */
    char * tag = xstrdup(tagname);
    int tsave = 0;
    char *te;

    /* Split off trailing tag modifiers */
    for (te = tag; *te != '\0' && strchr(allowed, *te); te++)
	;
    if (te && *te) {
	tsave = *te;
	*te++ = '\0';
    }

    /* Find the desired value in the cache or header. */
    if (symtabGetEntry(x->symtab, tagname, &Chep, &nChe, NULL)) {
	he = Chep[0];
	goto exit;
    } else if (tsave && symtabGetEntry(x->symtab, tag, &Chep, &nChe, NULL)) {
	he = Chep[0];
    } else {
	he = (HE_t) xcalloc(1, sizeof(*he));
	he->tag = tagValue(tag);
	if (headerGet(h, he, 0)) {
	    he->t = tagType(he->tag);
	} else			/* XXX Fill in missing epoch. */
	if (he->tag == RPMTAG_EPOCH) {
	    rpmuint32_t * ui32p = xcalloc(1, sizeof(*ui32p));
	    ui32p[0] = 0;
	    he->t = RPM_STRING_TYPE | RPM_SCALAR_RETURN_TYPE;
	    he->p.ui32p = ui32p;
	    he->c = 1;
	} else {
	    he = _free(he);
	    goto exit;
	}
    }

    /* Add new result(s) to cache. */
    if (Chep == NULL) {
if (tqfdebug)
fprintf(stderr, "==> %s: symtabAdd(%s,%p) %s\n", __FUNCTION__, tag, he, dHE(he));
	symtabAddEntry(x->symtab, xstrdup(tag), he);
    }

    /* Process and cache trailing modifiers */
    switch (tsave) {
	rpmuint64_t ui;
    case '#':	ui = he->c;	goto bottom;
    case '.':
		if (!strcmp(te, "tag"))		ui = he->tag;
	else	if (!strcmp(te, "t"))		ui = he->t;
	else	if (!strcmp(te, "p"))		ui = (rpmuint64_t) he->p.ptr;
	else	if (!strcmp(te, "c"))		ui = he->c;
	else	if (!strcmp(te, "ix"))		ui = he->ix;
	else	if (!strcmp(te, "freeData"))	ui = he->freeData;
	else	if (!strcmp(te, "avail"))	ui = he->avail;
	else	if (!strcmp(te, "append"))	ui = he->append;
	else	break;
	goto bottom;
    case '?':	ui = 1;		goto bottom;
    case '=':
    case '[':
    case ':':
if (tqfdebug)
fprintf(stderr, "*** %s unimplemented: |%c%s|\n", __FUNCTION__, tsave, te);
    case 0:
	break;
    default:
assert(0);	/* XXX FIXME */
	break;

    bottom:
      {	rpmuint64_t * ui64p = xcalloc(1, sizeof(*ui64p));
	ui64p[0] = ui;
	he = (HE_t) xcalloc(1, sizeof(*he));
	he->t = RPM_UINT64_TYPE | RPM_SCALAR_RETURN_TYPE;
	he->p.ui64p = ui64p;
	he->c = 1;
if (tqfdebug)
fprintf(stderr, "==> %s: symtabAdd(%s,%p) %s\n", __FUNCTION__, tagname, he, dHE(he));
	symtabAddEntry(x->symtab, xstrdup(tagname), he);
      }	break;

    }

exit:
    tag = _free(tag);

if (tqfdebug)
fprintf(stderr, "<== %s(%s) he %p %s\n", __FUNCTION__, tagname, he, (he ? dHE(he) : ""));
    return he;
}

static HE_t heScalar(const HE_t he, uint32_t ix)
{
    HE_t Nhe = (HE_t) xcalloc(1, sizeof(*Nhe));
    uint64_t ui = 0;
    char * b = NULL;
    size_t nb = 0;

    switch (he->t & ~RPM_MASK_RETURN_TYPE) {
    default:
assert(0);
	break;
    case RPM_BIN_TYPE:	/* hex */
    {   const uint8_t * s = he->p.ui8p;
	rpmTagCount c;
	char * t;

	nb = 2 * he->c + 1;
	t = b = xmalloc(nb);
	for (c = 0; c < he->c; c++) {
	    unsigned i = (unsigned) *s++;
	    *t++ = digits[ (i >> 4) & 0xf ];
	    *t++ = digits[ (i     ) & 0xf ];
	}
	*t = '\0';
    }   break;
    case RPM_UINT8_TYPE:	ui = (uint64_t)he->p.ui8p[ix];	break;
    case RPM_UINT16_TYPE:	ui = (uint64_t)he->p.ui16p[ix];	break;
    case RPM_UINT32_TYPE:	ui = (uint64_t)he->p.ui32p[ix];	break;
    case RPM_UINT64_TYPE:	ui = (uint64_t)he->p.ui64p[ix];	break;
    case RPM_STRING_TYPE:	b = xstrdup(he->p.str);	break;
    case RPM_STRING_ARRAY_TYPE:	b = xstrdup(he->p.argv[ix]);	break;
    }

    Nhe->tag = he->tag;
    if (b) {
	Nhe->t = RPM_STRING_TYPE;
	Nhe->p.str = b;
	Nhe->c = 1;
    } else {
	Nhe->tag = he->tag;
	Nhe->t = RPM_UINT64_TYPE;
	Nhe->p.ui64p = xcalloc(1, sizeof(*Nhe->p.ui64p));
	Nhe->p.ui64p[0] = ui;
	Nhe->c = 1;
    }

if (tqfdebug)
fprintf(stderr, "<== %s(%p,%u) %s\n", __FUNCTION__, he, ix, dHE(Nhe));
    return Nhe;
}

/**
 * Return tag value from name.
 * @param tbl		tag table
 * @param name		tag name to find
 * @return		tag value, 0 on not found
 */
static rpmuint32_t myTagValue(headerTagTableEntry tbl, const char * name)
	/*@*/
{
    rpmuint32_t val = 0;

    /* XXX Use bsearch on the "normal" rpmTagTable lookup. */
    if (tbl == NULL || tbl == rpmTagTable)
	val = tagValue(name);
    else
    for (; tbl->name != NULL; tbl++) {
	if (xstrcasecmp(tbl->name, name))
	    continue;
	val = tbl->val;
	break;
    }
    return val;
}

/** \ingroup header
 */
typedef /*@abstract@*/ struct sprintfTag_s * sprintfTag;

/** \ingroup header
 */
struct sprintfTag_s {
    HE_s he;
/*@null@*/
    headerTagFormatFunction * fmtfuncs;
/*@null@*/
    headerTagTagFunction ext;   /*!< NULL if tag element is invalid */
    int extNum;
/*@only@*/ /*@relnull@*/
    rpmTag * tagno;
    int justOne;
    int arrayCount;
/*@kept@*/
    char * format;
/*@only@*/ /*@relnull@*/
    ARGV_t av;
/*@only@*/ /*@relnull@*/
    ARGV_t params;
    unsigned pad;
};

/** \ingroup header
 */
typedef /*@abstract@*/ struct sprintfToken_s * sprintfToken;

/** \ingroup header
 */
typedef enum {
        PTOK_NONE       = 0,
        PTOK_TAG        = 1,
        PTOK_ARRAY      = 2,
        PTOK_STRING     = 3,
        PTOK_COND       = 4
} sprintfToken_e;

/** \ingroup header
 */
struct sprintfToken_s {
    sprintfToken_e type;
    union {
	struct sprintfTag_s tag;	/*!< PTOK_TAG */
	struct {
	/*@only@*/
	    sprintfToken format;
	    size_t numTokens;
	} array;			/*!< PTOK_ARRAY */
	struct {
	/*@dependent@*/
	    char * string;
	    size_t len;
	} string;			/*!< PTOK_STRING */
	struct {
	/*@only@*/ /*@null@*/
	    sprintfToken ifFormat;
	    size_t numIfTokens;
	/*@only@*/ /*@null@*/
	    sprintfToken elseFormat;
	    size_t numElseTokens;
	    struct sprintfTag_s tag;
	} cond;				/*!< PTOK_COND */
    } u;
};

/**
 * Search extensions and tags for a name.
 * @param hsa		headerSprintf args
 * @param token		parsed fields
 * @param name		name to find
 * @return		0 on success, 1 on not found
 */
static int findTag(Tparse_t * hsa, sprintfToken token, const char * name)
	/*@modifies token @*/
{
    headerSprintfExtension exts = hsa->exts;
    headerSprintfExtension ext;
    sprintfTag stag = (token->type == PTOK_COND
	? &token->u.cond.tag : &token->u.tag);
    int extNum;
    rpmTag tagno = (rpmTag)-1;

    stag->fmtfuncs = NULL;
    stag->ext = NULL;
    stag->extNum = 0;

    if (!strcmp(name, "*")) {
	tagno = (rpmTag)-2;
	goto bingo;
    }

    if (strncmp("RPMTAG_", name, sizeof("RPMTAG_")-1)) {
	char * t = alloca(strlen(name) + sizeof("RPMTAG_"));
	(void) stpcpy( stpcpy(t, "RPMTAG_"), name);
	name = t;
    }

    /* Search extensions for specific tag override. */
    for (ext = exts, extNum = 0; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? *ext->u.more : ext+1), extNum++)
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_TAG)
	    continue;
	if (!xstrcasecmp(ext->name, name)) {
	    stag->ext = ext->u.tagFunction;
	    stag->extNum = extNum;
	    tagno = tagValue(name);
	    goto bingo;
	}
    }

    /* Search tag names. */
    tagno = myTagValue(hsa->tags, name);
    if (tagno != 0)
	goto bingo;

    return 1;

bingo:
    stag->tagno = xcalloc(1, sizeof(*stag->tagno));
    stag->tagno[0] = tagno;
    /* Search extensions for specific format(s). */
    if (stag->av != NULL) {
	int i;
/*@-type@*/
	stag->fmtfuncs = xcalloc(argvCount(stag->av) + 1, sizeof(*stag->fmtfuncs));
/*@=type@*/
	for (i = 0; stag->av[i] != NULL; i++) {
	    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
		 ext = (ext->type == HEADER_EXT_MORE ? *ext->u.more : ext+1))
	    {
		if (ext->name == NULL || ext->type != HEADER_EXT_FORMAT)
		    /*@innercontinue@*/ continue;
		if (strcmp(ext->name, stag->av[i]+1))
		    /*@innercontinue@*/ continue;
		stag->fmtfuncs[i] = ext->u.fmtFunction;
		/*@innerbreak@*/ break;
	    }
	}
    }
    return 0;
}

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
    nodeType * p = xmalloc(sizeof(*p));
    /* copy information */
    p->type = typeCon;
    p->con.u.I = value;
if (tqfdebug)
fprintf(stderr, "<== %s(%u) %p[%s:%u]\n", __FUNCTION__, (unsigned)value, p, _TYPES[p->type&0x7], (unsigned)p->con.u.I);
    return p;
}

nodeType *id(int i)
{
    nodeType * p = xmalloc(sizeof(*p));
    /* copy information */
    p->type = typeId;
    p->id.i = i;
if (tqfdebug)
fprintf(stderr, "<== %s(%d) %p[%s:%c]\n", __FUNCTION__, i, p, _TYPES[p->type&0x7], (p->id.i+'A'));
    return p;
} 

nodeType *text(char * text)
{
    nodeType * p = xmalloc(sizeof(*p));
    /* copy information */
    p->type = typeText;
    p->text.S = xstrdup(text);
if (tqfdebug)
fprintf(stderr, "<== %s(%s) %p[%s:%p]\n", __FUNCTION__, text, p, _TYPES[p->type&0x7], p->text.S);
    return p;
} 

nodeType *tag(char * _tag, char * _mod)
{
    nodeType * p = xmalloc(sizeof(*p));
    /* copy information */
    p->type = typeTag;

    /* HACK: tag needs to be truncated because of yy_{push,pop}_state() */
    if (_tag) {
	char * S;
	/* HACK: filter leading non-printables */
	while (*_tag && !isprint(*_tag)) _tag++;
	S = xstrdup(_tag);
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
	p->tag.S = xstrdup("NULL");


    /* W2DO? mod includes leading ':' */
    /* HACK: mod needs to be truncated because of yy_{push,pop}_state() */
    if (_mod) {
	char * M = xstrdup((_mod ? _mod : ""));
	/* HACK: filter leading non-printables */
	while (*_mod && !isprint(*_mod)) _mod++;
    	M = strdup(_mod);
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
	p->tag.M = NULL;

if (tqfdebug)
fprintf(stderr, "<== %s(%s,%s) %p[%s:%s%s]\n", __FUNCTION__, _tag, _mod, p, _TYPES[p->type&0x7], p->tag.S, p->tag.M);

    return p;
} 

static char * callFormat(Tparse_t *x, HE_t he, ARGV_t av)
{
    headerSprintfExtension exts = x->exts;
    headerSprintfExtension ext;
    char * str = NULL;
    int ix = 0;

if (tqfdebug)
fprintf(stderr, "==> %s(%p,%s) %s\n", __FUNCTION__, he, dAV(av), dHE(he));
	    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
		 ext = (ext->type == HEADER_EXT_MORE ? *ext->u.more : ext+1))
	    {
		if (ext->name == NULL || ext->type != HEADER_EXT_FORMAT)
		    continue;
		if (strcmp(ext->name, av[ix]))
		    continue;
if (tqfdebug)
fprintf(stderr, "==>\t%s(%p,%s)\n", av[0], he, dAV(av+1));
		str = (*ext->u.fmtFunction) (he, av+1);	/* XXX skip av[0]? */
		break;
	    }

if (tqfdebug)
fprintf(stderr, "<== %s(%p,%s)\t|%s|\n", __FUNCTION__, he, dAV(av), str);
    return str;
}

static char * doQFormat(Tparse_t *x, HE_t he, char *qlist, char *fmt)
{
    ARGV_t av = NULL;
    int xx = argvSplit(&av, qlist, "(,)");	
    char * str = callFormat(x, he, av);

    (void)xx;

if (tqfdebug)
fprintf(stderr, "<==\t%s(%s)\t%s\t%s\n", __FUNCTION__, qlist, str, dAV(av));

    av = argvFree(av);
    return str;
}

static char * doPFormat(Tparse_t *x, HE_t he, char *plist, char *fmt)
{
    ARGV_t av = NULL;
    int xx = argvSplit(&av, plist, ":");
    char * str = NULL;
    int i;

    (void)xx;

    for (i = 0; av[i]; i++) {
	char * t = doQFormat(x, he, (char *)av[i], fmt);
	str = _free(str);
	str = t;
    }

if (tqfdebug)
fprintf(stderr, "<==\t%s(%s)\t%s\t%s\n", __FUNCTION__, plist, str, dAV(av));
    av = argvFree(av);
    return str;
}

nodeType *textTag(Tparse_t * x, char *l, char *t, char *m, char *r)
{
    nodeType * p = xmalloc(sizeof(*p));
    char * tm = rpmExpand(t, m, NULL);
    HE_t * Chep = NULL;
    int nChe = 0;
    HE_t Che = (symtabGetEntry(x->symtab, tm, &Chep, &nChe, NULL))
		? Chep[0] : heGet(x, t);
    uint32_t ix = 0;
    HE_t Nhe = NULL;
    char * str = NULL;
    int xx;

if (tqfdebug)
if (Chep)
fprintf(stderr, "==> symtabGet(%s,%p) %p[%d]\n", tm, Che, Chep, nChe);

    /* Post processing for ":qfmt(a,b,c)|pfmt(d,e,f)" */
    if (m) {
	Nhe = heScalar(Che, ix);	/* XXX Convert to he->p.{str,ui64} */
	str = doPFormat(x, Nhe, (*m == ':' ? m+1 : m), NULL);
    }

    /* XXX force a string value. */
    if (str == NULL) {
	ARGV_t av = NULL;
	str = heCoerce((Nhe ? Nhe : Che), av, NULL);
	av = argvFree(av);
    }

    /* Cache new value (if not already). */
    if (Chep == NULL) {
	if (Nhe) {
if (tqfdebug)
fprintf(stderr, "==> %s: symtabAdd(%s,%p) %s\n", __FUNCTION__, tm, Nhe, dHE(Nhe));
	    symtabAddEntry(x->symtab, xstrdup(tm), Nhe);
	    Nhe = NULL;		/* XXX don't free */
	}
    } else if (Nhe) {
	Nhe->p.ptr = _free(Nhe->p.ptr);
	Nhe = _free(Nhe);
    }

    tm = _free(tm);

    /* Padding from prefix(l) */
    if (l) {	
	char *le;
	char * nstr = NULL;
	for (le = l + 1; *le && strchr("-0123456789.", *le); le++)
	    ;
	le[0] = 's';
	le[1] = '\0';
	xx = asprintf(&nstr, l, str);
	str = _free(str);
	str = nstr;
assert(xx);
    }

    p->type = typeText;
    p->text.S = str;

if (tqfdebug)
fprintf(stderr, "<== %s(%s|%s|%s|%s) %p[%s:%p]\n", __FUNCTION__, l, t, m, r, p, _TYPES[p->type&0x7], p->text.S);

    l = _free(l); t = _free(t); m = _free(m); r = _free(r);
    return p;
} 

nodeType *opr(int oper, int nops, ...)
{
    nodeType * p = xmalloc(sizeof(*p));
    int i;
    p->opr.op = xmalloc(nops * sizeof(nodeType));

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

nodeType *appendNode(nodeType *p, nodeType *q)
{
    if (p->type == typeText && q->type == typeText) {
	char * S = rpmExpand(p->text.S, q->text.S, NULL);
	free(p->text.S);	p->text.S = S;
	free(q->text.S);	q->text.S = NULL;
	free(q);
if (tqfdebug)
fprintf(stderr, "**|%s|\n", S);
	return p;
    } else
    if (p->type == typeOpr && p->opr.oper == PRINT
     && q->type == typeOpr && q->opr.oper == PRINT)
    {
	int i;
	p->opr.op = xrealloc(p->opr.op, (p->opr.nops+q->opr.nops) * sizeof(nodeType));
	for (i = 0; i < q->opr.nops; i++)
	    p->opr.op[p->opr.nops++] = q->opr.op[i];
	free(q->opr.op);
	free(q);
	return p;
    } else
	return opr(',', 2, p, q);
}

void freeNode(nodeType *p)
{
    int i;
    if (!p) return;
    switch (p->type) {
    case typeOpr:
	for (i = 0; i < p->opr.nops; i++)
	    freeNode(p->opr.op[i]);
	free(p->opr.op);
	break;
    case typeText:
fprintf(stderr, "==|%s|\n", p->text.S);
	p->text.S = _free(p->text.S);
	break;
    case typeTag:
	p->tag.M = _free(p->tag.M);
	p->tag.S = _free(p->tag.S);
	break;
    case typeCon:
    case typeId:
	break;
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

RPM_GNUC_PURE
static inline
unsigned int _symtabHash(const char *str)
{
    /* Jenkins One-at-a-time hash */ 
    unsigned int hash = 0xe4721b68;
    const char * s = str;

    while (*s != '\0') {
	hash += *s;
	hash += (hash << 10); 
	hash ^= (hash >> 6);
	s++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

static inline
int _symtabCmp(const char *a, const char *b)
{
    return strcmp(a, b);
}

static inline
const char * _symtabFreeKey(const char * s)
{
    if (s) free((void *)s);
    return NULL;
}

static inline
HE_t _symtabFreeData(HE_t he)
{
    if (he->p.ptr) free((void *)he->p.ptr);
    if (he) free((void *)he);
    return NULL;
}

int main(int argc, const char ** argv)
{
    Tparse_t x = {};
    int _verbose = 0;
    struct poptOption _opts[] = {
     { "debug", 'd', POPT_BIT_SET,		&yydebug,	1,
        N_("debug"), NULL },
     { "verbose", 'v', POPT_ARG_VAL,		&_verbose,	1,
        N_("debug"), NULL },
     { "dbpath", 'D', POPT_ARG_STRING,		&x.flex_db,       0,
	N_("dbpath <dir>/tmp/rpmdb"), N_("<dir>") },
     { "input", 'i', POPT_ARG_STRING,		&x.flex_ifn,	0,
        N_("input <fn>"), N_("<fn>") },
     { "lang", 'l', POPT_ARG_STRING,		&x.flex_lang,	0,
        N_("input <fn>"), N_("<fn>") },
     { "output", 'o', POPT_ARG_STRING,		&x.flex_ofn,	0,
        N_("output <fn>"), N_("<fn>") },
     { "rpm", 'r', POPT_ARG_STRING,		&x.flex_rpm,	0,
        N_("rpm package <rpm>"), N_("<rpm>") },
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

    x.symtab = symtabCreate(257,
		_symtabHash,
		_symtabCmp,
		_symtabFreeKey,
		_symtabFreeData);

    if (av == NULL || av[0] == NULL) {
	if (yydebug)
	    fprintf(stderr, "==> %s\n", "<stdin>");
	Tparse_flex_init(&x);
	ec = yyparse(&x);
	Tparse_flex_destroy(&x);
	if (yydebug)
	    fprintf(stderr, "<== %s ec %d\n", "<stdin>", ec);
    } else {
	int i;
	for (i = 0; av[i]; i++) {
	    if (yydebug)
		fprintf(stderr, "==> %s\n", av[i]);
	    x.flex_ifn = (char *) av[i];
	    Tparse_flex_init(&x);
	    ec = yyparse(&x);
	    Tparse_flex_destroy(&x);
	    x.flex_ifn = NULL;
	    if (yydebug)
		fprintf(stderr, "<== %s ec %d\n", av[i], ec);
	}
    }

    if (yydebug) {
	fprintf(stderr, "==> symtab stats:\n");
	symtabPrintStats(x.symtab);
    }

    if (x.flex_extra) {
	Header h = (Header) x.flex_extra;
	headerFree(h);
    }

    if (x.symtab) symtabFree(x.symtab);

    if (x.flex_rpm) free(x.flex_rpm);
    if (x.flex_ofn) free(x.flex_ofn);
    if (x.flex_lang) free(x.flex_lang);
    if (x.flex_ifn) free(x.flex_ifn);
    if (x.flex_db)  free(x.flex_db);

    con = poptFreeContext(con);

    return ec;
}

#endif
