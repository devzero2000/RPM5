#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PARM_BLOCK_SIZE   10

typedef struct pass_to_bison Jparse_t;

struct pass_to_bison {
    void * flex_extra;
    void * flex_scanner;
    char * text;
    void * tree;
};

void Jparse_flex_init(Jparse_t *x);
int  Jparse_parse(Jparse_t *x, char *text);
void Jparse_flex_destroy(Jparse_t *x);

typedef enum Jtype_enum 
{
    JError   = 0,
    JNull    = 1,
    JString  = 2,
    JNumber  = 3,
    JObject  = 4,
    JArray   = 5,
    JTrue    = 6,
    JFalse   = 7
} Jtype_t;

typedef struct Jpair_struct   Jpair_t;
typedef struct Jvalue_struct  Jvalue_t;
typedef struct Jarray_struct  Jarray_t;
typedef struct Jobject_struct Jobject_t;

struct Jvalue_struct 
{
    Jtype_t   type;
    void       *value;
};

struct Jpair_struct
{
    char       *name;
    Jvalue_t   *value;
};

struct Jobject_struct {
    Jpair_t   **pair;
    size_t      count;
    size_t      capacity;
};

struct Jarray_struct {
    Jvalue_t  **items;
    size_t      count;
    size_t      capacity;
};

//new functions
