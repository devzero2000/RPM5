#include "system.h"

#if defined(WITH_SYCK)

#include <syck.h>
#include <rpmhash.h>
#define	_RPMSYCK_INTERNAL
#include <rpmsyck.h>

static SYMID
rpmsyck_parse_handler(SyckParser *p, SyckNode *n)
{
    int i = 0;
    rpmsyck_node tn = xmalloc(sizeof(struct rpmsyck_node_s));

    switch ( n->kind )
    {
        case syck_str_kind:
            tn->type = T_STR;
            tn->key = syck_strndup( n->data.str->ptr, n->data.str->len );
            tn->value.seq = 0;
        break;

        case syck_seq_kind:
        {
            rpmsyck_node val;
            rpmsyck_node seq = xmalloc(sizeof(struct rpmsyck_node_s) * (n->data.list->idx + 1));
            tn->type = T_SEQ;
            tn->key = 0;

            for (i = 0; i < n->data.list->idx; i++)
            {
                SYMID oid = syck_seq_read( n, i );
                syck_lookup_sym( p, oid, (char **)&val);
                seq[i] = val[0];
            }
            seq[n->data.list->idx].type = T_END;
            tn->value.seq = seq;
        }
        break;

        case syck_map_kind:
	{
	    hashTable ht = htCreate(n->data.pairs->idx * 2, 0, 0, NULL, NULL);
            rpmsyck_node val;
            tn->type = T_MAP;
            tn->key = 0;
            for (i = 0; i < n->data.pairs->idx; i++)
            {
		char *key, *value;
                SYMID oid = syck_map_read( n, map_key, i);
                syck_lookup_sym(p, oid, (char **)&val);
		key = val[0].key;

                oid = syck_map_read(n, map_value, i);
                syck_lookup_sym(p, oid, (char **)&val);
		value = val[0].key;

		htAddEntry(ht, key, val);
            }
            tn->value.map = ht;
        }
        break;
    }

    tn->tag = 0;
    if (n->type_id != NULL) {
        tn->tag = syck_strndup(n->type_id, strlen( n->type_id ));
    }

    return syck_add_sym( p, (char *) tn );
}

rpmsyck_node rpmSyckLoad(char *yaml)
{
    SyckParser *parser;
    SYMID v;
    rpmsyck_node nodes;

    parser = syck_new_parser();

    syck_parser_str_auto(parser, yaml, NULL);
    syck_parser_handler(parser, rpmsyck_parse_handler);
    syck_parser_error_handler(parser, NULL);
    syck_parser_implicit_typing(parser, 1);
    syck_parser_taguri_expansion(parser, 1);

    if((v = syck_parse(parser)))
         syck_lookup_sym( parser, v, (char**)&nodes);

    return nodes;
}

#endif
