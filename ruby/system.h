#ifndef	H_SYSTEM_PYTHON
#define	H_SYSTEM_PYTHON

#include "../system.h"

/* XXX grrr, ruby.h includes its own config.h too. */
#undef  PACKAGE_NAME
#undef  PACKAGE_TARNAME
#undef  PACKAGE_VERSION
#undef  PACKAGE_STRING
#undef  PACKAGE_BUGREPORT

#define	_save		_
#undef	_
#define	_xmalloc	xmalloc
#undef	xmalloc
#define	_xcalloc	xcalloc
#undef	xcalloc
#define	_xrealloc	xrealloc
#undef	xrealloc
#define	_xfree		xfree
#undef	xfree

#include <ruby.h>

#undef	_
#define	_		_save
#undef	_save
#undef	xmalloc
#define	xmalloc		_xmalloc
#undef	_xmalloc
#undef	xrealloc
#define	xrealloc	_xrealloc
#undef	_xrealloc
#undef	xfree
#define	xfree		_xfree
#undef	xfree

#endif	/* H_SYSTEM_PYTHON */
