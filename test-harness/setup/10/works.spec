Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%_tmppath/%NVR

%description
It just works.  What more do you want?

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT/tmp/mytest_file <<EOF
We need test packages with files, as if things are not
done right in the rollback transaction this little file
can/has caused a segfault.

%NVR
EOF
exit 0


%pre
rm -f /tmp/%{name}_ran_pre_in_rollback
touch /tmp/%{name}_ran_pre_in_rollback
exit 0

%post
rm -f /tmp/%{name}_ran_post_in_rollback
touch /tmp/%{name}_ran_post_in_rollback
exit 0

%preun
exit 0

%postun
exit 0

%files
/tmp/mytest_file
