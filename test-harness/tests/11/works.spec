Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	2
Group: 		System Environment/Base
License:	GPL
BuildRoot:	/tmp/%{name}-%{version}-%{release}

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
rm -f /tmp/works-1-1_ran_pre_in_rollback
rm -f /tmp/works-1-1_ran_post_in_rollback
echo "Running %{name}-%{version}-%{release} post..."
exit 0

%post
echo "Running %{name}-%{version}-%{release} post..."
exit 0

%preun
rm -f /tmp/%{name}_ran_preun_in_rollback
echo "Running %{name}-%{version}-%{release} preun..."
touch /tmp/%{name}_ran_post_in_rollback
exit 0

%postun
rm -f /tmp/%{name}_ran_postun_in_rollback
echo "Running %{name}-%{version}-%{release} postun..."
touch /tmp/%{name}_ran_postun_in_rollback
exit 0

%files
/tmp/mytest_file
