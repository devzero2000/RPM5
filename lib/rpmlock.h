#ifndef RPMLOCK_H
#define RPMLOCK_H 

void *rpmtsAcquireLock(rpmts ts)
	/*@*/;
void rpmtsFreeLock(/*@only@*/ void *lock)
	/*@modifies lock @*/;

#endif
