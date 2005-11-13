#include "system.h"

#include <rpmlib.h>

#include "debug.h"

int main(int argc, char *argv[])
{
    int rc;
    int i;

    for (i = 1; i < argc; i++) {
	rc = rpmioAccess(argv[i], NULL, 0);
	fprintf(stderr, "%s: %d\n", argv[i], rc);
    }

    return 0;
}
