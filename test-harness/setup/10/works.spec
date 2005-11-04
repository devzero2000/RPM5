Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:	/tmp/works-1-1

%description
It just works.  What more do you want?

%install

mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT/tmp/mytest_file <<EOF
We need test packages with files, as if things are not
done right in the rollback transaction this little file
can/has caused a segfault.

%{name}-%{version}-%{release}
EOF
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
exit 0

%files
/tmp/mytest_file
