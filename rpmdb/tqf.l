%option header-file="tqf.h"
%option outfile="tqf.c"

%option	8bit yylineno noyywrap
%option	stack

%option	noyyget_lineno
%option	noyyget_in
%option	noyyget_out
%option	noyyget_text
%option	noyyget_debug
%option noyyget_leng

%option noyy_top_state
%option noyy_scan_buffer
%option noyy_scan_bytes
%option noyy_scan_string

%{
    #include "system.h"

    #include "debug.h"
%}

%x	QF
%x	QI
%x	QJ
%x	QK
%x	QT
%%

\"\\\n		{ yy_push_state(QF); }
<QF>"%|"	{ printf("|"); yy_push_state(QI); }
<QF>\\n\"	{ yy_pop_state(); }

<QI>"?{"	{ printf("?"); yy_push_state(QJ); }
<QJ>"}"		{ yy_pop_state(); }
<QI>":{"	{ printf(":"); yy_push_state(QK); }
<QK>"}"		{ yy_pop_state(); }
<QI>"|"		{ printf("|"); yy_pop_state(); }

<QF,QJ,QK>"%{"	{ yy_push_state(QT); }
<QT>[A-Za-z0-9_:(,)]+"}"	{ printf("<QT>"); yy_pop_state(); }

\n		{ printf("%c", yytext[0] ); }

.		{ printf("%c", yytext[0] ); }

%%

int main(int argc, char **argv)	{ return yylex(); }
