Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:      /tmp/%{name}-%{version}-%{release}

Requires: broken = 0:1-1
                                                                                
%description
It just works.  What more do you want?
                                                                                
%install
rm -rf "${RPM_BUILD_ROOT}"
mkdir -p "${RPM_BUILD_ROOT}/tmp"
touch "${RPM_BUILD_ROOT}/tmp/%{name}-%{version}-%{release}.test"


%files
/tmp/%{name}-%{version}-%{release}.test
