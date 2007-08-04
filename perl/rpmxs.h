/* common function for perl module */

#ifndef _HAVE_RPMXS_H_
#define _HAVE_RPMXS_H_

void _rpm2header(rpmts ts, char * filename, int checkmode);

int sv2constant(SV * svconstant, const char * context);

#endif
