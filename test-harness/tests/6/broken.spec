Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
Prefix:		/tmp
BuildRoot:	%_tmppath/%NVR

Requires:  works


%description
You wanted something more...fine!!!

%install
rm -rf $RPM_BUILD_ROOT/tmp
mkdir -p $RPM_BUILD_ROOT/tmp
touch $RPM_BUILD_ROOT/tmp/%{NVR}-%{arch}
exit 0

%pre
exit 1

%files
/tmp/%{NVR}-%{arch}
