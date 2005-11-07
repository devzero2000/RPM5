#!/bin/sh

[ $# -ge 1 ] || {
    cat > /dev/null
    exit 0
}

case $1 in
-P|--provides)
    cat > /dev/null
    exit 0
    ;;
-R|--requires)
    while read f ; do
	/bin/bash --rpm-requires $f
    done | sort -u
    ;;
esac
exit 0
