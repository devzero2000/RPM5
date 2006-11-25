Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	2
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%_tmppath/%NVR

%description
It just works.  What more do you want?

%install

mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT/tmp/mytest_file <<EOF
We need test packages with files, as if things are not
done right in the rollback transaction this little file
can/has caused a segfault.

%NVR
EOF
exit 0


%pre
echo $1 > /tmp/%{NVR}_pre_icount
rm -f /tmp/%{NVR}_ran_pre_in_rollback
rm -f /tmp/%{NVR}_ran_post_in_rollback
exit 0

%post
echo $1 > /tmp/%{NVR}_post_icount
exit 0

%preun
echo $1 > /tmp/%{name}-%{version}-%{release}_preun_icount
rm -f /tmp/%{name}_ran_preun_in_rollback
touch /tmp/%{name}_ran_post_in_rollback
exit 0

%postun
echo $1 > /tmp/%{NVR}_postun_icount
rm -f /tmp/%{name}_ran_postun_in_rollback
touch /tmp/%{name}_ran_postun_in_rollback
exit 0

%files
/tmp/mytest_file
