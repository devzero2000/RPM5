#ifndef H_RPMDB
#define H_RPMDB

#include "rpmlib.h"

/* for RPM's internal use only */

int openDatabase(const char * prefix, const char * dbpath, rpmdb *rpmdbp,
		int mode, int perms, int justcheck);
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);
int rpmdbAdd(rpmdb db, Header dbentry);
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);
void rpmdbRemoveDatabase(const char * rootdir, const char * dbpath);
int rpmdbMoveDatabase(const char * rootdir, const char * olddbpath,
	const char * newdbpath);

#endif
