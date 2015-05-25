#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PARM_BLOCK_SIZE   10

typedef struct pass_to_bison Tparse_t;

struct pass_to_bison {
    void * flex_extra;
    void * flex_scanner;
    char * text;
    void * tree;
};

void Tparse_flex_init(Tparse_t *x);
int  Tparse_parse(Tparse_t *x, char *text);
void Tparse_flex_destroy(Tparse_t *x);

typedef enum Ttype_enum 
{
    TError   = 0,
    TNull    = 1,
    TString  = 2,
    TNumber  = 3,
    TObject  = 4,
    TArray   = 5,
    TTrue    = 6,
    TFalse   = 7
} Ttype_t;

typedef struct Tpair_struct   Tpair_t;
typedef struct Tvalue_struct  Tvalue_t;
typedef struct Tarray_struct  Tarray_t;
typedef struct Tobject_struct Tobject_t;

struct Tvalue_struct 
{
    Ttype_t   type;
    void       *value;
};

struct Tpair_struct
{
    char       *name;
    Tvalue_t   *value;
};

struct Tobject_struct {
    Tpair_t   **pair;
    size_t      count;
    size_t      capacity;
};

struct Tarray_struct {
    Tvalue_t  **items;
    size_t      count;
    size_t      capacity;
};

//new functions

typedef enum {
    typeCon,
    typeId,
    typeOpr,
    typeText,
    typeTag,
} nodeEnum;

/* constants */
typedef struct {
    union {	/* value of constant */
	unsigned long long I;
	double F;
	char * S;
    } u;
} conNodeType;

/* identifiers */
typedef struct {
    int i;			/* subscript to sym array */
} idNodeType;

/* operators */
typedef struct {
    int oper;			/* operator */
    int nops;			/* number of operands */
    struct nodeTypeTag **op;	/* operands */
} oprNodeType;

typedef struct {
    char *S;
} textNodeType;

typedef struct {
    char *S;
    char *M;
} tagNodeType;

typedef struct nodeTypeTag {
    nodeEnum type;		/* type of node */
    union {
	conNodeType con;	/* constants */
	idNodeType id;		/* identifiers */
	oprNodeType opr;	/* operators */
	textNodeType text;
	tagNodeType tag;
    };
} nodeType;

extern long long sym[26];
