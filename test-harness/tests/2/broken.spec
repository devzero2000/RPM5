Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%_tmppath/%NVR

%description
You wanted something more...fine!!!

%install
rm -rf "${RPM_BUILD_ROOT}"
mkdir -p "${RPM_BUILD_ROOT}/tmp"
touch "${RPM_BUILD_ROOT}/tmp/%{NVR}.test"

%pre
echo $1 > /tmp/%{NVR}_pre_icount
exit 1

%preun
echo $1 > /tmp/%{NVR}_preun_icount
exit 0

%postun
echo $1 > /tmp/%{NVR}_postun_icount
exit 0

%files
/tmp/%{NVR}.test
