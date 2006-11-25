Summary:	The rpm that simply works.
Name: 		test2
Version:	1
Release:	2
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%_tmppath/%NVR

%description
It just works.  What more do you want?

%define my_file /tmp/test_file-%NVR

%install

rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT%{my_file} <<EOF
We need test packages with files, as if things are not
done right in the rollback transaction this little file
can/has caused a segfault.

%NVR
EOF
exit 0

%pre
echo $1 > /tmp/%{NVR}_pre_icount
exit 0
                                                                                
%post
echo $1 > /tmp/%{NVR}_post_icount
exit 0
                                                                                
%preun
echo $1 > /tmp/%{NVR}_preun_icount
exit 0
                                                                                
%postun
echo $1 > /tmp/%{NVR}_postun_icount
exit 0

%files
%{my_file}
