#ifndef H_RPMSYCK
#define H_RPMSYCK

#include <rpmhash.h>

/**
 */
typedef enum {
    T_STR,
    T_SEQ,
    T_MAP,
    T_END
} syck_type_t; 

/**
 */
typedef struct rpmsyck_node_s * rpmsyck_node;

#if defined(_RPMSYCK_INTERNAL)
/**
 */
struct rpmsyck_node_s {
    syck_type_t type;
    char *tag;
    char *key;
    union {
    	rpmsyck_node seq;
	hashTable map;
    } value;
};
#endif

/**
 */
rpmsyck_node rpmSyckLoad(char *yaml)
	/*@*/;

#endif /* H_RPMSYCK */
