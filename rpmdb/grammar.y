%lex-param{void *scanner} 
%parse-param{struct pass_to_bison *x}

%{

    #include <stdio.h>
    #include <rpmutil.h>
    #include "json.h"
    #define yylex Jyylex

    /* 
    http://www.lemoda.net/c/reentrant-parser/index.html

    Note, for a valid JSON parser, 
    %start object
    For use in seismic, we need
    %start members
    */
    #define scanner  (x->flex_scanner)

    static Jpair_t   * Jpair_new(char *id, void *val);
    static Jvalue_t  * Jvalue_new(void *any, Jtype_t type);
    static Jarray_t  * Jarray_new(Jvalue_t *value);
    static Jarray_t  * Jarray_add(Jvalue_t *value, Jarray_t *array);
    static Jobject_t * Jobject_new(Jpair_t *pair);
    static Jobject_t * Jobject_add(Jpair_t *pair, Jobject_t *obj);

    //%start members

    extern int Jyylex ();

RPM_GNUC_PURE int Jyyget_column();
RPM_GNUC_PURE int Jyyget_in();
RPM_GNUC_PURE int Jyyget_out();

    void yyerror(void *x, char *s);

%}

%union{
    char        *s;
    void        *v;
}

%start object
%token <s> TOK_NUM TOK_STR TOK_ID TOK_NULL TOK_TRUE TOK_FALSE
%type <v> value pair elements array object members
%left TOK_OBEG TOK_OEND 
%left TOK_ABEG TOK_AEND 
%left TOK_COMMA
%left TOK_EQUAL
%%

object   : TOK_OBEG TOK_OEND { $$=x->tree=NULL; } 
         | TOK_OBEG members TOK_OEND { $$=x->tree=$2; }
;

members  : pair { $$=Jobject_new($1); }
         | pair TOK_COMMA members { $$=Jobject_add($1, $3); }
;

pair     : TOK_STR TOK_EQUAL value {$$=Jpair_new($1, $3); }
;

array    : TOK_ABEG TOK_AEND {$$=NULL;}
         | TOK_ABEG elements TOK_AEND { $$=$2; }
;

elements : value { $$=Jarray_new($1); }
         | value TOK_COMMA elements { $$=Jarray_add($1, $3); }
;
value    : array  {$$=Jvalue_new($1, JArray);  }
         | object {$$=Jvalue_new($1, JObject); }
         | TOK_STR  {$$=Jvalue_new($1, JString); }
         | TOK_NUM  {$$=Jvalue_new($1, JNumber); }
         | TOK_NULL {$$=Jvalue_new($1, JNull); }
         | TOK_TRUE {$$=Jvalue_new($1, JTrue); }
         | TOK_FALSE{$$=Jvalue_new($1, JFalse); } 
;

%%

static Jvalue_t * Jvalue_new(void *any, Jtype_t type)
{
    Jvalue_t *value = calloc(1, sizeof(Jvalue_t));
    value->type  = type;
    value->value = any;
    return value;
}

static Jpair_t * Jpair_new(char *id, void *val)
{
    Jpair_t *pair = calloc(1, sizeof(Jpair_t));
    pair->name  = id;
    pair->value = val;
    return pair;
}

static Jarray_t * Jarray_add(Jvalue_t *value, Jarray_t *array)
{
    size_t i = array->count;
    if(i+1>=array->capacity) {
        array->capacity += PARM_BLOCK_SIZE;
        array->items = realloc(array->items, array->capacity*sizeof(Jvalue_t*));
    }
    array->items[i] = value;
    array->count++;
    return array;
}

static Jarray_t * Jarray_new(Jvalue_t *value)
{
    Jarray_t *array = calloc(1, sizeof(Jarray_t));
    if(value!=NULL) 
      array = Jarray_add(value, array);
    return array;
}

//add pair
static Jobject_t * Jobject_add(Jpair_t *pair, Jobject_t *obj)
{
    size_t i = obj->count;
    if(i+1>=obj->capacity) {
        obj->capacity += PARM_BLOCK_SIZE;
        obj->pair = realloc(obj->pair, obj->capacity*sizeof(Jpair_t*));
    }
    obj->pair[i] = pair;
    obj->count++;
    return obj;
}

static Jobject_t * Jobject_new(Jpair_t *pair)
{
    Jobject_t *obj = calloc(1, sizeof(Jobject_t));
    if(pair!=NULL)
      obj = Jobject_add(pair, obj);
    return obj;
}


