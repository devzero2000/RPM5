#######################
# Configuration Items #
#######################
#
# Path to tests
TESTDIR=$(pwd)
#
# Path to test rpm database
DB="${TESTDIR}/db"
RPDIR="${TESTDIR}/rpdir"
TMPDIR="${TESTDIR}/tmp"
PKG_BUILD_DIR="${TESTDIR}/build"
TEST_CACHE_DIR="${TESTDIR}/cache"

#
# Set this to 1 if you want to use an uninstalled just built
# rpm. 
USE_BUILD_RPM=0

#
# Path to rpm build directory
BUILD_DIR=$(cd ..; pwd)

#
# Where built rpms go...
RPM_DIR="${TESTDIR}/build/rpms"
 
RPM_MACROS="
%_topdir         ${PKG_BUILD_DIR}
%_builddir       %{_topdir}
%_rpmdir         ${RPM_DIR}
%_srcrpmdir      %{_topdir}/rpms
%_tmppath	${TESTDIR}/tmp
%NVR		%{name}-%{version}-%{release}
%NVRA		%{name}-%{version}-%{release}.%{_target_cpu}
%buildroot	%{_tmppath}/%{NVR}

%pre	%%pre\\
  echo \"--- pre(%NVRA)	arg \$1 ...\"\\
%nil
%post	%%post\\
  echo \"--- post(%NVRA)	arg \$1 ...\"\\
%nil
%preun	%%preun\\
  echo \"--- preun(%NVRA)	arg \$1 ...\"\\
%nil
%postun	%%postun\\
  echo \"--- postun(%NVRA)	arg \$1 ...\"\\
%nil

%semaphore_backout	/tmp/semaphore_backout
%_rollback_transaction_on_failure	1
"
RPM_MACRO_FILE="${TESTDIR}/test.macros"
RPM_RC_FILE="${TESTDIR}/test.rpmrc"
RPM_MACRO_PATH="/usr/lib/rpm/macros:${RPM_MACRO_FILE}"

#
# Electric Fence Config
export EF_ALIGNMENT=0
export EF_PROTECT_BELOW=0
export EF_PROTECT_FREE=0

#
# Valgrind support
VALGRIND=0
VALGRIND_CMD=/usr/bin/valgrind
VALGRIND_OPTS='-v --leak-check=full'

 
#####################
# Dont Change These #
#####################
#
# Where the installed rpm is located (probably don't need to 
# change this).
REALRPM=/bin/rpm

#
# How to query our private db
MYQUERY="${TESTDIR}/myquery"

#
# What to build rpms with
RPMBUILD="/usr/bin/rpmbuild --rcfile ${RPM_RC_FILE}"

#
# Where is the rpmdb_stat command.
# XXX: May want to use the one that is built when testing with
#      with build rpm.
RPMDB_STAT=/usr/lib/rpm/rpmdb_stat

#
# Option to set database path (we can't use popt with a just
# built rpm).
DBPATH_OPT="_dbpath ${DB}"

#
# Option to set path to tmp dir so we can clean it up after each
# test run (cause with failing rpms we end up with lots of tmp files).
TMPPATH_OPT="_tmppath ${TMPDIR}"

########################
# End of configuration #
########################
#
# Get absolute paths:
BUILD_DIR=$(cd $BUILD_DIR; pwd)

#
# Change things based on us using a build rpm or a production one:
if [ "${USE_BUILD_RPM}" = "1" ]
then
	RPM="${BUILD_DIR}/rpm --rcfile ${RPM_RC_FILE}"
	RPMDB="${BUILD_DIR}/rpmd"
	RPMDB_STAT="${BUILD_DIR}/rpmdb/rpmdb_stat"
	RPM_BINARY=${BUILD_DIR}/.libs/lt-rpm
else
	RPM="${REALRPM} --rcfile ${RPM_RC_FILE}"
	RPM_BINARY="${REALRPM}"
	RPMDB="${REALRPM}"
fi

#
# Change rpm path to use valgrind if configured to do so:
if [ "${VALGRIND}" = 1 ]
then
	RPM="${VALGRIND_CMD} ${VALGRIND_OPTS} ${RPM}"
	RPMDB="${VALGRIND_CMD} ${VALGRIND_OPTS} ${RPMDB}"
fi
	
#
# Make sure we have DB dir
if [ -z "${DB}" ]
then
        echo "No database defined!"
	exit 1
fi

#
# Make sure it not set to some stupid place
for d in / /var/lib/rpm /usr /var /home
do
	if [ "${DB}" = $d ]
	then
		echo "What? Are you nuts!  I am not going to set the rpm database to $d!"
		exit 1
	fi
done	
[ $? != 0 ] && exit 1

###############
# SUBROUTINES #
###############
#
# We use a little awk state machine to strip out of the rpmdb stat 
# output only the locks.  On rpm's previous to the HEAD as of Jan 30,2005
# output looked like:
#
#	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#	Locks grouped by object:
#	Locker   Mode      Count Status  ----------------- Object ---------------
# 
# Now they look like:
#	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#	Lock REGINFO information:
#	Lock    Region type
#	3       Region ID
#	__db.003        Region name
#	0x406af000      Original region address
#	0x406af000      Region address
#	0x4071cf40      Region primary address
#	0       Region maximum allocation
#	0       Region allocated
#	REGION_JOIN_OK  Region flags
#	=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#	Locks grouped by object:
#	Locker   Mode      Count Status  ----------------- Object ---------------
#
# In either case if I have lines pass the last line in the output above 
# I have locks and things are not right in the universe.
check_locks() {
	local locks
	[ -f ${DB}/__db.001 ] || return 0
	pushd ${DB} > /dev/null 2>&1


	locks=$(${RPMDB_STAT} -Co | awk '
	BEGIN                          { state = 0 }
	/^=-=-=/                       { state = 1; next }
    state == 1 && /^Locks grouped/ { state = 2; next }
    state == 1                     { state = 0; next }
    state == 2 && /^Locker/        { state = 3; next }
    state == 2                     { state = 0; next }
	state == 3 { print $0 }
' | wc -l)
	if [ ${locks} != 0 ]
	then
		echo "ERROR: RPMDB has active locks!"
		echo "======= rpmdb_stat OUTPUT ========="
		${RPMDB_STAT} -Co 
		return 1
	fi
	popd > /dev/null 2>&1
	return 0
}


