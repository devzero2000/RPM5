%option bison-bridge
%option reentrant stack noyywrap
%option extra-type="struct Jcontext_t *"
%option prefix="Jyy"

%option	noyyget_extra
%option	noyyget_lineno
%option	noyyget_in
%option	noyyget_out
%option	noyyget_leng
%option	noyyget_text
%option	noyyget_debug
%option	noyyget_lval

%{
    #include <rpmutil.h>

    #include "json.h"
    #include "Jgrammar.h"

    #define MAX(a, b)   ((a>b)?(a):(b))
    #define TEXT_BLOCK_SIZE  1024
    struct Jcontext_t {
	char   *value;
	size_t  used;
	size_t  size;
    };

RPM_GNUC_PURE   int Jyyget_column();
RPM_GNUC_PURE   int Jyyget_debug();
RPM_GNUC_PURE   FILE * Jyyget_in();
RPM_GNUC_PURE   yy_size_t Jyyget_leng();
RPM_GNUC_PURE   int Jyyget_lineno();
RPM_GNUC_PURE   YYSTYPE * Jyyget_lval();
RPM_GNUC_PURE   FILE * Jyyget_out();
RPM_GNUC_PURE   char * Jyyget_text();

static char * Jdonate(struct Jcontext_t *tok);
static void   Jappend(struct Jcontext_t *tok, char *text);

%}

true              [Tt][Rr][Uu][Ee]
false             [Ff][Aa][Ll][Ss][Ee]
null              [Nn][Uu][Ll][Ll]
integer           [-+]?[0-9]+
float             ([-+]?([0-9]*)?\.[0-9]*([eE][-+]?[0-9]+)?)|([-+]?([0-9]+)(\.[0-9]*)?[eE][-+]?[0-9]+)
hexchar           [0-9a-fA-F]{4}
ident             [a-zA-Z_]+[0-9a-zA-Z_]*

%x STRING
%%

\"                      { yy_push_state(STRING, yyscanner); }
<STRING>[^\"\\]+        { Jappend(yyextra, yytext); }
<STRING>\\\"            { Jappend(yyextra, "\""); }
<STRING>\\\\            { Jappend(yyextra, "\\"); }
<STRING>\\\/            { Jappend(yyextra, "/"); }
<STRING>\\b             { Jappend(yyextra, "\b"); }
<STRING>\\f             { Jappend(yyextra, "\f"); }
<STRING>\\n             { Jappend(yyextra, "\n"); }
<STRING>\\r             { Jappend(yyextra, "\r"); }
<STRING>\\t             { Jappend(yyextra, "\t"); }
<STRING>\\u{hexchar}    { Jappend(yyextra, yytext+2); }
<STRING>\"              { yy_pop_state(yyscanner);  yylval->s = Jdonate(yyextra); return TOK_STR; }

{true}                  { Jappend(yyextra, yytext); yylval->s = Jdonate(yyextra); return TOK_TRUE; }
{false}                 { Jappend(yyextra, yytext); yylval->s = Jdonate(yyextra); return TOK_FALSE; }
{null}                  { Jappend(yyextra, yytext); yylval->s = Jdonate(yyextra); return TOK_NULL; }
{integer}|{float}       { Jappend(yyextra, yytext); yylval->s = Jdonate(yyextra); return TOK_NUM; }
\[                      { return TOK_ABEG;  }
\]                      { return TOK_AEND;  }
\{                      { return TOK_OBEG;  }
\}                      { return TOK_OEND;  }
,                       { return TOK_COMMA; }
:                       { return TOK_EQUAL; }
\n                      { /* eat */ }
\r                      { /* eat */ }
.                       { /* eat */ }

%%

static void Jappend(struct Jcontext_t *tok, char *text)
{
    size_t len = strlen(text);
    if((len+tok->used+1)>(tok->size))
    {
        tok->size += MAX(len+1, TEXT_BLOCK_SIZE);
        tok->value = realloc(tok->value, tok->size);
        tok->value[tok->used] = '\0';
    }
    strcat(tok->value, text);
    tok->used += len;
}

static char * Jdonate(struct Jcontext_t *tok)
{ 
    char *str = tok->value;     //transfer to parser
    memset(tok, 0, sizeof(struct Jcontext_t));
    return str;
}

void Jparse_flex_init(Jparse_t *x)
{
    x->flex_extra = calloc(1, sizeof(struct Jcontext_t));
    Jyylex_init(&x->flex_scanner);
    Jyy_scan_string(x->text, x->flex_scanner);
    Jyyset_extra(x->flex_extra, x->flex_scanner);
}

void Jparse_flex_destroy(Jparse_t *x)
{
    Jyylex_destroy(x->flex_scanner);
    free(x->flex_extra);
    x->flex_extra = NULL;
}
