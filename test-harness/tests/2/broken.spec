Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:      /tmp/%{name}-%{version}-%{release}

%description
You wanted something more...fine!!!

%install
rm -rf "${RPM_BUILD_ROOT}"
mkdir -p "${RPM_BUILD_ROOT}/tmp"
touch "${RPM_BUILD_ROOT}/tmp/%{name}-%{version}-%{release}.test"

%pre
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_pre_icount
exit 1

%preun
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_preun_icount
exit 0

%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_postun_icount
exit 0

%files
/tmp/%{name}-%{version}-%{release}.test
