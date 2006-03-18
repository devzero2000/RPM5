#!/bin/bash

ulimit -c 0

#
# --- Grab the file manifest and classify files.
#filelist=`sed "s/['\"]/\\\&/g"`
filelist=`sed "s/[]['\"*?{}]/\\\\\&/g"`
exelist=`echo $filelist | xargs -r file | egrep -v ":.* (commands|script) " | \
	grep ":.*executable" | cut -d: -f1`
liblist=`echo $filelist | xargs -r file | \
	grep ":.*shared object" | cut -d : -f1`

{
#
# --- Executable dependency sonames.
  for f in $exelist; do
    [ -r $f -a -x $f ] || continue
    echo "$f:"
    ldd -u -r $f | sed -e '/^[ 	]*$/d' -e '/Unused direct dependencies:/d'
  done | grep -v 'not a dynamic executable'

#
# --- Library dependency sonames.
  for f in $liblist; do
    [ -r $f ] || continue
    echo "$f:"
    ldd -u -r $f | sed -e '/^[ 	]*$/d' -e '/Unused direct dependencies:/d'
  done

}

exit 0
