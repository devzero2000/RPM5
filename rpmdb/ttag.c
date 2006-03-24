
#include "system.h"
#include <rpmlib.h>
#include "debug.h"

int main(int argc, char *argv[])
{
    const struct headerTagTableEntry_s * t;
    int i;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	const char * name;
	const char * tname;
	if ((tname = t->name) == NULL)
	    continue;
	tname += sizeof("RPMTAG_")-1;
	name = tagName(t->val);
	/* XXX Values aren't unique, there may be multiple names for each val */
	if (!xstrcasecmp(name, tname))
	    fprintf(stderr, "%s\n", tname);
	else
	    fprintf(stderr, "#define %s %s\n", tname, name);
fprintf(stderr, "%d\n", tagValue(tname));
fprintf(stderr, "%d\n", (tagType(t->val) & RPM_MASK_TYPE));
    }
    return 0;
}
