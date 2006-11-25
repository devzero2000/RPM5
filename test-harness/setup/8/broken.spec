Summary:	The rpm that simply works.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%_tmppath/%NVR

%description
It just works.  What more do you want?

%install
rm -rf "${RPM_BUILD_ROOT}"
mkdir -p "${RPM_BUILD_ROOT}"
exit 0

%pre
echo $1 > /tmp/%{NVR}_pre_icount
rm -f /tmp/%{name}_ran_pre_in_rollback
touch /tmp/%{name}_ran_pre_in_rollback
exit 0

%post
echo $1 > /tmp/%{NVR}_post_icount
rm -f /tmp/%{name}_ran_post_in_rollback
touch /tmp/%{name}_ran_post_in_rollback
exit 0

%preun
echo $1 > /tmp/%{NVR}_preun_icount
exit 1

%postun
exit 0

%files
