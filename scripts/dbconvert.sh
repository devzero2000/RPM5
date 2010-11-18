#!/bin/sh

TIMESTAMP="$(LC_TIME=C date -u +%F_%R)"
TMPDIR="/var/tmp"
BACKUP="${BACKUP:-$TMPDIR/rpmdb-$TIMESTAMP}"
NEWDB="${NEWDB:-`mktemp -d -t newdb-XXXXXXXXXX`}"
DBHOME="${DBHOME:-/var/lib/rpm}"
DBFORCE=${DBFORCE:-0}

export DBHOME

DB_STAT="$(which db51_stat 2> /dev/null)"
if [ -z "$DB_STAT" -a ! -x "$DB_STAT" ]; then
    DB_STAT="$(which db_stat 2> /dev/null)"
fi
if [ ! -x "$DB_STAT" ]; then
    echo "Unable to locate db_stat"
    exit 1
fi

# Database is assumed to be converted, so let's ditch it
if [ "$($DB_STAT -f -d "$DBHOME/Packages" |grep -e 'Btree magic number' -e 'Big-endian' -c)" == 2 ] && \
     rpm --dbpath "$DBHOME" -qa &> /dev/null && rpm --dbpath "$DBHOME" -q rpm &> /dev/null; then
    if [ "$DBFORCE" == 0 ]; then
    	#echo "rpmdb already converted, set variable DBFORCE=1 to force"
	exit 0
    fi
fi

DB_DUMP="$(which db51_dump 2> /dev/null)"
if [ -z "$DB_DUMP" -a ! -x "$DB_DUMP" ]; then
    DB_DUMP="$(which db_dump 2> /dev/null)"
fi
if [ -x "$DB_DUMP" ]; then
    echo "Found $DB_DUMP"
else
    echo "Unable to locate db_dump"
    exit 1
fi

DB_DUMP_VERSION="$($DB_DUMP -V |sed 's/^Berkeley DB \([0-9]\+\.[0-9]\+\).*/\1/')"
if [ "$DB_DUMP_VERSION" == 5.1 ]; then
    echo "$DB_DUMP version $DB_DUMP_VERSION ok"
else
    echo "Incompatible db_dump version ($DB_DUMP_VERSION) found, 5.1.* required"
    exit 1
fi

DB_LOAD="$(which db51_load 2> /dev/null)"
if [ -z "$DB_LOAD" -a ! -x "$DB_LOAD" ]; then
    DB_LOAD="$(which db_load 2> /dev/null)"
fi
if [ -x "$DB_LOAD" ]; then
    echo "Found $DB_LOAD"
else
    echo "Unable to locate db_load"
    exit 1
fi

DB_LOAD_VERSION="$($DB_LOAD -V |sed 's/^Berkeley DB \([0-9]\+\.[0-9]\+\).*/\1/')"
if [ "$DB_LOAD_VERSION" == 5.1 ]; then
    echo "$DB_LOAD version $DB_LOAD_VERSION ok"
else
    echo "Incompatible db_load version ($DB_LOAD_VERSION) found, 5.1.* required"
    exit 1
fi

DB_UPGRADE="$(which db51_upgrade 2> /dev/null)"
if [ -z "$DB_UPGRADE" -a ! -x "$DB_UPGRADE" ]; then
    DB_UPGRADE="$(which db_upgrade 2> /dev/null)"
fi
if [ -x "$DB_UPGRADE" ]; then
    echo "Found $DB_UPGRADE"
else
    echo "Unable to locate db_load"
    exit 1
fi

DB_UPGRADE_VERSION="$($DB_UPGRADE -V |sed 's/^Berkeley DB \([0-9]\+\.[0-9]\+\).*/\1/')"
if [ "$DB_LOAD_VERSION" == 5.1 ]; then
    echo "$DB_UPGRADE version $DB_UPGRADE_VERSION ok"
else
    echo "Incompatible db_load version ($DB_UPGRADE_VERSION) found, 5.1.* required"
    exit 1
fi

echo "Converting system database."
rm -rf "$NEWDB"
mkdir -p "$NEWDB"/{log,tmp}
if [ "$DBHOME" != "/var/lib/rpm" ]; then
    if [ -f /var/lib/rpm/DB_CONFIG ]; then
	cp /var/lib/rpm/DB_CONFIG "$NEWDB/DB_CONFIG"
    else
	tee "$NEWDB/DB_CONFIG" << EOH
# ================ Environment
#add_data_dir		.
set_data_dir		.
set_create_dir		.
set_lg_dir		./log
set_tmp_dir		./tmp

# -- thread_count must be >= 8
set_thread_count	64

#set_verbose		DB_VERB_DEADLOCK
#set_verbose		DB_VERB_FILEOPS
#set_verbose		DB_VERB_FILEOPS_ALL
#set_verbose		DB_VERB_RECOVERY
#set_verbose		DB_VERB_REGISTER
#set_verbose		DB_VERB_REPLICATION
#set_verbose		DB_VERB_REP_ELECT
#set_verbose		DB_VERB_REP_LEASE
#set_verbose		DB_VERB_REP_MISC
#set_verbose		DB_VERB_REP_MSGS
#set_verbose		DB_VERB_REP_SYNC
#set_verbose		DB_VERB_REP_TEST
#set_verbose		DB_VERB_REPMGR_CONNFAIL
#set_verbose		DB_VERB_REPMGR_MISC
#set_verbose		DB_VERB_WAITSFOR

# ================ Logging

# ================ Memory Pool
#XXX initializing dbenv with set_cachesize has unimplemented prerequsites
#set_cachesize		0 1048576 0 
set_mp_mmapsize		16777216

# ================ Locking
set_lk_max_locks	16384
set_lk_max_lockers	16384
set_lk_max_objects	16384
mutex_set_max		163840

# ================ Replication
EOH
    fi
fi

echo "--> convert header instances to network order"
$DB_DUMP "$DBHOME/Packages" \
    | sed \
    -e 's/^type=hash$/type=btree/' \
    -e '/^h_nelem=/d' \
    -e 's/^ \(..\)\(..\)\(..\)\(..\)$/ \4\3\2\1/' \
    | $DB_LOAD -c db_lorder=4321 -h "$NEWDB" Packages
#mv "$NEWDB/Packages" "$DBHOME/Packages"
echo "--> regenerate the indices"
rpm \
    --dbpath "$NEWDB" \
    --rebuilddb -vv
echo "--> test the conversion"
rpm --dbpath "$NEWDB" -qa > /dev/null && \
rpm --dbpath "$NEWDB" -q rpm > /dev/null
if [ $? -eq 0 ]; then
    echo "Conversion successful "
    echo "--> move old rpmdb files to $BACKUP"
    mkdir -p "$BACKUP"
    for db in \
	Arch Filepaths Name Os Providename Release Seqno Sourcepkgid \
	Basenames Dirnames Group Nvra Packagecolor Provideversion Requirename Sha1header \
	Triggername Conflictname Filedigests Installtid Obsoletename Packages Pubkeys \
	Requireversion Sigmd5 Version; do
    test -f "$DBHOME/$db" && mv "$DBHOME/$db" "$BACKUP/$db"
    done
    echo "--> move new rpmdb files to $DBHOME"
    rm -rf "$DBHOME"/{log,tmp}
    mv -f "$NEWDB"/* "$DBHOME"
    rmdir "$NEWDB"
    db51_recover -h "$DBHOME"
else
    echo "Conversion failed"
fi
