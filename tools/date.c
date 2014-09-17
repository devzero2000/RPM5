
#include "system.h"

#include <rpmio.h>

#define	_RPMDATE_INTERNAL
#include <rpmdate.h>

#include "debug.h"

int main(int argc, char ** argv)
{
    rpmdate date = NULL;
    int ec = EXIT_FAILURE;

    __progname = "date";
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    date = rpmdateNew(argv, 0);
    if (date) {
	int ac = argvCount(date->results);
	int i;
	for (i = 0; i < ac; i++)
	    fprintf(stdout, "%s\n", date->results[i]);
	ec = EXIT_SUCCESS;
    }
    date = rpmdateFree(date);

    rpmioClean();

    return ec;
}
