#! /bin/sh
# Identify run path, and forward to ../lib/$0

PROGNAME=$(basename $0)
DIR=$(cd $(dirname $0) ; pwd)

export LD_LIBRARY_PATH=$DIR/../lib
export RPM_USRLIBRPM=$DIR/../lib/rpm
export RPM_ETCRPM=$DIR/../etc/rpm
export RPM_LOCALEDIR=$DIR/../share/locale

exec $DIR/../lib/rpm/$PROGNAME "$@"
