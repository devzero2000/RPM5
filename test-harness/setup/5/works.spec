# This spec file's pre and post scripts create semaphore files if 
# its see the backout semaphore.
# The first time I am installed in setup I won't create the files,
# bug if all works as planned I will in the autorollback.
Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	1
Group: 		System Environment/Base
Copyright:	GPL

%description
It just works.  What more do you want?

%pre
rm -f /tmp/rollback_pre_sema_exist
echo "pre:  %{name}-%{version}-%{release}:"
echo "	Looking for %{semaphore_backout}..."
if [ -f "%{semaphore_backout}" ]   
then 
	echo "		Found it."
	touch /tmp/rollback_pre_sema_exist
fi
[ ! -f "%{semaphore_backout}" ] && echo "		Not there!"
exit 0

%post
rm -f /tmp/rollback_post_sema_exist
echo "post:  %{name}-%{version}-%{release}:"
echo "	Looking for %{semaphore_backout}..."
if [ -f "%{semaphore_backout}" ]
then
	echo "		Found it."
	touch /tmp/rollback_post_sema_exist
fi
[ ! -f "%{semaphore_backout}" ] && echo "		Not there!"
exit 0

%files
