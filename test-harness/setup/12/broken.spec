Summary:	The rpm that simply works.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:	/tmp/%{name}-%{release}-%{version}

%description
It just works.  What more do you want?

%install
rm -rf "${RPM_BUILD_ROOT}"
mkdir -p "${RPM_BUILD_ROOT}"
exit 0

%pre
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_pre_icount
rm -f /tmp/%{name}-%{version}-%{release}_ran_pre_in_rollback
touch /tmp/%{name}-%{version}-%{release}_ran_pre_in_rollback
exit 0

%post
i=$1
echo "%{name}-%{version}-%{release}($i): Running post..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_post_icount
rm -f /tmp/%{name}-%{version}-%{release}_ran_post_in_rollback
touch /tmp/%{name}-%{version}-%{release}_ran_post_in_rollback
exit 0

#
# We will remove the semaphores when we are uninstalled, so that
# if are install scriptlets don't get ran in the autorollback
# we'll know, and the harness will catch the error.
%preun
i=$1
echo "%{name}-%{version}-%{release}($i): Running preun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_preun_icount
rm -f /tmp/%{name}-%{version}-%{release}_ran_pre_in_rollback
rm -f /tmp/%{name}-%{version}-%{release}_ran_post_in_rollback
exit 0

%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running postun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_postun_icount
exit 1

%files
