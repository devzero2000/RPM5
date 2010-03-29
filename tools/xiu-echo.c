#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>

#include "debug.h"

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, _rpmnixEchoOptions);
    int ec = rpmnixEcho(nix);

    nix = rpmnixFree(nix);

    return ec;
}
