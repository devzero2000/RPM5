/* Copyright (C) 1991, 92, 1995-2004, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include "system.h"
#include <rpmlib.h>
#include <rpmds.h>
#include "debug.h"

int
main (int argc, char *argv[])
{
     rpmds ds = NULL;
     int xx;

    /* Set locale.  Do not set LC_ALL because the other categories must
       not be affected (according to POSIX.2).  */
    setlocale (LC_CTYPE, "");
    setlocale (LC_MESSAGES, "");

    /* Initialize the message catalog.  */
    textdomain (PACKAGE);

    xx = rpmdsGetconf (&ds, NULL);

    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0)
	fprintf(stderr, "%d\tProvides: %s\n", rpmdsIx(ds), rpmdsDNEVR(ds)+2);

    ds = rpmdsFree(ds);

    return 0;
}
