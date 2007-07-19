##
##  .dmalloc.bashrc -- DMalloc GNU bash glue
##

__dmallocrc="`pwd`/.dmallocrc"

dmalloc () { 
    eval `command dmalloc -b -f ${__dmallocrc} ${1+"$@"}`
}

