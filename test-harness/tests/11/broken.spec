Summary:	The rpm that simply works.
Name: 		broken
Version:	1
Release:	2
Group: 		System Environment/Base
Copyright:	GPL
BuildRoot:	/tmp/%{name}-%{release}-%{version}

%description
It just works.  What more do you want?

%install
rm -rf "${RPM_BUILD_ROOT}"
mkdir -p "${RPM_BUILD_ROOT}"
exit 0

%pre
echo "Running %{name}-%{version}-%{release} post..."
exit 0

%post
echo "Running %{name}-%{version}-%{release} post..."
exit 0

%preun
rm -f /tmp/%{name}_ran_preun_in_rollback
echo "Running %{name}-%{version}-%{release} preun..."
touch /tmp/%{name}_ran_preun_in_rollback
exit 0

%postun
rm -f /tmp/%{name}_ran_postun_in_rollback
echo "Running %{name}-%{version}-%{release} postun..."
touch /tmp/%{name}_ran_postun_in_rollback
exit 0

%files
