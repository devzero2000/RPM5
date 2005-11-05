
#include "system.h"
#include <rpmlib.h>
#include "debug.h"

int main(int argc, char *argv[])
{
    const struct headerTagTableEntry_s * t;
    int i;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	if (t->name == NULL)
	    continue;
fprintf(stderr, "%s\n", tagName(t->val));
fprintf(stderr, "%d\n", tagValue(t->name+sizeof("RPMTAG_")-1));
fprintf(stderr, "%d\n", tagType(t->val));
    }
    return 0;
}
