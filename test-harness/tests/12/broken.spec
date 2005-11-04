Summary:	The rpm that simply works.
Name: 		broken
Version:	1
Release:	2
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
exit 0

%post
i=$1
echo "%{name}-%{version}-%{release}($i): Running post..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_post_icount
echo "Running %{name}-%{version}-%{release} post..."
exit 0

%preun
i=$1
echo "%{name}-%{version}-%{release}($i): Running preun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_preun_icount
rm -f /tmp/%{name}_ran_preun_in_rollback
touch /tmp/%{name}_ran_preun_in_rollback
exit 0

%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running postun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_postun_icount
rm -f /tmp/%{name}_ran_postun_in_rollback
touch /tmp/%{name}_ran_postun_in_rollback
exit 0

%files
