Summary:	The rpm that simply works.
Name: 		broken
Version:	1
Release:	2
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
exit 0

%post
exit 0

%preun
rm -f /tmp/%{name}_ran_preun_in_rollback
touch /tmp/%{name}_ran_preun_in_rollback
exit 0

%postun
rm -f /tmp/%{name}_ran_postun_in_rollback
touch /tmp/%{name}_ran_postun_in_rollback
exit 0

%files
