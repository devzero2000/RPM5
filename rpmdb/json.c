#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "json.h"

void yyerror (char const *s) {
    fprintf (stderr, "%s\n", s);
}

void Jindent(int depth)
{
    int i;
    for(i=0; i<4*depth; i++)
       putchar(' ');
}

void Jarray_print(Jarray_t *array, int depth);

void Jobject_print(Jobject_t *obj, int depth)
{
    int i;
    Jpair_t *pair;
    for(i=obj->count-1; i>=0; i--) {
        Jindent(depth); 
        pair = obj->pair[i];
        printf("%s=", pair->name);
        switch(pair->value->type) {
          case JObject:
            printf("{\n");
            Jobject_print(pair->value->value, depth+1);
            Jindent(depth); printf("}\n");
            break;
          case JArray:
            printf("[");
            Jarray_print(pair->value->value, depth+1);
            printf("]\n");
            break;
          default:
            printf("%s", pair->value->value);
            (i!=0)? printf(",\n"): putchar('\n');
        }
    }
}

void Jarray_print(Jarray_t *array, int depth)
{
    int i;
    Jvalue_t *item;
    for(i=array->count-1; i>=0; i--) {
        item = array->items[i];
        switch(item->type) {
          case JObject:
            printf("{\n");
            Jobject_print(item->value, depth+1);
            Jindent(depth); printf("}\n");
            break;
          case JArray:
            printf("[");
            Jarray_print(item->value, depth+1);
            printf("]\n");
            break;
          default:
            printf("%s", item->value);
            if(i!=0) putchar(',');
        }
    }
}

void Jarray_free(Jarray_t *array);
void Jobject_free(Jobject_t *obj)
{
    int i;
    Jpair_t *pair;
    for(i=obj->count-1; i>=0; i--) {
        pair = obj->pair[i];
        switch(pair->value->type) {
          case JObject:
            Jobject_free(pair->value->value);
            break;
          case JArray:
            Jarray_free(pair->value->value);
            break;
          default:
            free(pair->value->value);
        }
        free(pair->value);
        free(pair->name);
        free(pair);
    }
    free(obj->pair);
    free(obj);
}
void Jarray_free(Jarray_t *array)
{
    int i;
    Jvalue_t *item;
    for(i=array->count-1; i>=0; i--) {
        item = array->items[i];
        switch(item->type) {
          case JObject:
            Jobject_free(item->value);
            break;
          case JArray:
            Jarray_free(item->value);
            break;
          default:
            free(item->value);
        }
        free(item);
    }
    free(array->items);
    free(array);
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Usage: %s file_name\n", argv[0]);
        exit(0);
    }

    size_t size;
    struct stat myfile;
    stat(argv[1], &myfile);
    size = myfile.st_size;
    char *script = calloc(size+1, 1);

    FILE *json = fopen(argv[1], "r");
    assert(json != NULL);
    fread(script, size, 1, json);
    fclose(json);

    Jparse_t x;
    memset(&x, 0, sizeof(Jparse_t));
    x.text = script;

    Jparse_flex_init(&x);
    yyparse(&x);
    Jparse_flex_destroy(&x);

    printf("{\n");
    Jobject_print(x.tree, 1);
    printf("}\n");

    Jobject_free(x.tree);

    free(script);

    return 0;
}
