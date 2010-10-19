#!/usr/bin/env python
from getopt import getopt
from os.path import basename, dirname, splitext
from sys import argv, stdin, version
from pkg_resources import PathMetadata, Distribution
from distutils.sysconfig import get_python_lib


opts, args = getopt(argv[1:], 'hPRSECO',
        ['help', 'provides', 'requires', 'suggests', 'enhances', 'conflicts', 'obsoletes'])

Provides = False
Requires = False
Suggests = False
Enhances = False
Conflicts = False
Obsoletes = False

for o, a in opts:
    if o in ('-h', '--help'):
        print '-h, --help\tPrint help'
        print '-P, --provides\tPrint Provides'
        print '-R, --requires\tPrint Requires'
        print '-S, --suggests\tPrint Suggests'
        print '-E, --enhances\tPrint Enhances (unused)'
        print '-C, --conflicts\tPrint Conflicts'
        print '-O, --obsoletes\tPrint Obsoletes (unused)'
        exit(1)
    elif o in ('-P', '--provides'):
        Provides = True
    elif o in ('-R', '--requires'):
        Requires = True
    elif o in ('-S', '--suggests'):
        Suggests = True
    elif o in ('-E', '--enhances'):
        Enhances = True
    elif o in ('-C', '--conflicts'):
        Conflicts = True
    elif o in ('-O', '--obsoletes'):
        Obsoletes = True

Version = version[:3]
for f in stdin.readlines():
    f = f.strip()
    # FIXME: get other versions as well...
    if Provides:
        if "/usr/lib/libpython%s.so" % Version in f or \
                "/usr/lib64/libpython%s.so" % Version in f:
                    print "python(abi) == %s" % Version
    if Requires:
        if get_python_lib(plat_specific=1) in f or get_python_lib() in f:
            print "python(abi) >= %s" % Version
    if f.endswith('.egg') or f.endswith('.egg-info') or f.endswith('.egg-link'):
        base_dir = dirname(f)
        metadata = PathMetadata(base_dir, f)
        dist_name = splitext(basename(f))[0]
        dist = Distribution(base_dir,project_name=dist_name,metadata=metadata)
        if Provides:
            print 'pythonegg(%s) = %s' % (dist.project_name, dist.version)
        if Requires or (Suggests and dist.extras):
            deps = dist.requires()
            if Suggests:
                depsextras = dist.requires(extras=dist.extras)
                if not Requires:
                    for dep in reversed(depsextras):
                        if dep in deps:
                            depsextras.remove(dep)
                deps = depsextras
            for dep in deps:
                if not dep.specs:
                    print 'pythonegg(%s)' % dep.project_name
                else:
                    for spec in dep.specs:
                        if spec[0] != '!=':
                            print 'pythonegg(%s) %s' % (dep.project_name, ' '.join(spec))
        if Conflicts:
            # Should we really add conflicts for extras?
            # Creating a meta package per extra with suggests on, which has
            # the requires/conflicts in stead might be a better solution...
            for dep in dist.requires(extras=dist.extras):
                for spec in dep.specs:
                    if spec[0] == '!=':
                        print 'pythonegg(%s) == %s' % (dep.project_name, spec[1])
