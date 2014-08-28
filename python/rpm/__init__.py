r"""RPM Module

This module enables you to manipulate rpms and the rpm database.

"""

import warnings
import os
from rpm._rpm import *

import rpm._rpm as _rpm
_RPMVSF_NODIGESTS = \
	_rpm.RPMVSF_NOSHA1HEADER | \
	_rpm.RPMVSF_NOMD5HEADER | \
	_rpm.RPMVSF_NOSHA1 | \
	_rpm.RPMVSF_NOMD5
_RPMVSF_NOHEADER = \
	_rpm.RPMVSF_NODSAHEADER | \
	_rpm.RPMVSF_NORSAHEADER | \
	_rpm.RPMVSF_NODSA | \
	_rpm.RPMVSF_NORSA
_RPMVSF_NOPAYLOAD = \
	_rpm.RPMVSF_NOSHA1HEADER | \
	_rpm.RPMVSF_NOMD5HEADER | \
	_rpm.RPMVSF_NODSAHEADER | \
	_rpm.RPMVSF_NORSAHEADER
_RPMVSF_NOSIGNATURES = \
	_rpm.RPMVSF_NOSHA1 | \
	_rpm.RPMVSF_NOMD5 | \
	_rpm.RPMVSF_NODSA | \
	_rpm.RPMVSF_NORSA

__version__ = _rpm.__version__
__version_info__ = tuple(__version__.split('.'))

## try to import build bits but dont require it
#try:
#    from rpm._rpmb import *
#except ImportError:
#    pass

## try to import signing bits but dont require it
#try:
#    from rpm._rpms import *
#except ImportError:
#    pass

# backwards compatibility + give the same class both ways
ts = TransactionSet

