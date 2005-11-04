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
exit 0

%pre
rm -f /tmp/%{name}_ran_pre_in_rollback
echo "Running %{name}-%{version}-%{release} post..."
touch /tmp/%{name}_ran_pre_in_rollback
exit 0

%post
rm -f /tmp/%{name}_ran_post_in_rollback
echo "Running %{name}-%{version}-%{release} post..."
touch /tmp/%{name}_ran_post_in_rollback
exit 0

%preun
echo "Running %{name}-%{version}-%{release} preun..."
exit 0

%postun
echo "Running %{name}-%{version}-%{release} postun..."
exit 1

%files
