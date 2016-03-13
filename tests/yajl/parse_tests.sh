#!/bin/sh

ECHO=`which echo`

DIFF_FLAGS="-u"
case "$(uname)" in
  *W32*)
    DIFF_FLAGS="-wu"
    ;;
esac

if [ -z "$testBin" ]; then
    testBin="$1"
fi

testBin=$(pwd)/yajl_test

srcdir=`dirname $0`

${ECHO} "using test binary: $testBin"
${ECHO} "using test cases: $srcdir/cases"

testBinShort=`basename $testBin`

testsSucceeded=0
testsTotal=0

for file in ${srcdir}/cases/*.json ; do
  allowComments=""
  allowGarbage=""
  allowMultiple=""
  allowPartials=""

  # if the filename starts with dc_, we disallow comments for this test
  case $(basename $file) in
    ac_*)
      allowComments="-c "
    ;;
    ag_*)
      allowGarbage="-g "
     ;;
    am_*)
     allowMultiple="-m ";
     ;;
    ap_*)
     allowPartials="-p ";
    ;;
  esac
  fileShort=`basename $file`
  testName=`echo $fileShort | sed -e 's/\.json$//'`

  ${ECHO} -n " test ($testName): "
  iter=1
  success="SUCCESS"

  # ${ECHO} -n "$testBinShort $allowPartials$allowComments$allowGarbage$allowMultiple-b $iter < $fileShort > ${fileShort}.test : "
  # parse with a read buffer size ranging from 1-31 to stress stream parsing
  while [ $iter -lt 32  ] && [ $success = "SUCCESS" ] ; do
    $testBin $allowPartials $allowComments $allowGarbage $allowMultiple -b $iter < $file > ${fileShort}.test  2>&1
    diff ${DIFF_FLAGS} ${file}.gold ${fileShort}.test > ${fileShort}.out
    if [ $? -eq 0 ] ; then
      if [ $iter -eq 31 ] ; then testsSucceeded=$(( $testsSucceeded + 1 )) ; fi
    else
      success="FAILURE"
      iter=32
      ${ECHO}
      cat ${fileShort}.out
    fi
    iter=$(( iter + 1 ))
    rm ${fileShort}.test ${fileShort}.out
  done

  ${ECHO} $success
  testsTotal=$(( testsTotal + 1 ))
done

${ECHO} $testsSucceeded/$testsTotal tests successful

if [ $testsSucceeded != $testsTotal ] ; then
  exit 1
fi

exit 0
