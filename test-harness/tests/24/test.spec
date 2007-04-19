Summary:	The rpm that simply works.
Name: 		test	
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%{_tmppath}/%NVR

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
echo $1 > /tmp/%{NVR}_pre_icount
exit 0
                                                                                
%post
echo $1 > /tmp/%{NVR}_post_icount

#
# Now lets remove our delivered file so that on rollback we will
# see if rollback handles re-installing the repackaged package
# that is missing a file:
rm /tmp/mytest_file

exit 0
                                                                                
%preun
echo $1 > /tmp/%{NVR}_preun_icount
exit 0
                                                                                
%postun
echo $1 > /tmp/%{NVR}_postun_icount
exit 0

%files
/tmp/mytest_file
