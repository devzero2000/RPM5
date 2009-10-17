#!/bin/sh

[ $# -ge 2 ] || {
    cat > /dev/null
    exit 0
}

pkgname="$3"

case $1 in
-P|--provides)
    shift
    RPM_BUILD_ROOT="`readlink -f "$1"`"
    while read possible
    do
	case "$possible" in
	*.la)
	    possible="`readlink -f "$possible" 2> /dev/null || echo "$possible"`"
	    if file "$possible" | grep -iq 'libtool library file' 2> /dev/null ; then
		possible="`echo ${possible} | sed -e s,${RPM_BUILD_ROOT}/,/,`"
		echo "libtool($possible)"
	    fi
	    ;;
	esac
    done
    ;;
-R|--requires)
    case $pkgname in
    *-devel)
	    while read possible ; do
		case "$possible" in
		*.la)
		    for dep in `grep '^dependency_libs=' "$possible" 2> /dev/null | \
				sed -e "s,^dependency_libs='\(.*\)',\1,g"`
		    do
			case "$dep" in
			/*.la)
			    dep="`readlink -f "$dep" 2> /dev/null || echo "$dep"`"
			    echo "libtool($dep)"
			    ;;
			esac
		    done
		    ;;
		esac
	    done
     ;;
     *)
            cat > /dev/null
     ;;
     esac
esac
exit 0
