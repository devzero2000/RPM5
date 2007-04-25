Summary:	The rpm that simply works.
Name: 		t2
Epoch:		0
Version:	1
Release:	2
Group: 		System Environment/Base
License:	GPL
BuildRoot:	%_tmppath/%NVR

#
# I'm making this requirement so that I can ensure the order 
# that the triggers will fire.  t1->t2 produces different instance
# counts than t2->t1.
Requires: t1

%description
It just works.  What more do you want?

%install

rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT/tmp/testfile-%{name} <<EOF
We need test packages with files, as if things are not
done right in the rollback transaction this little file
can/has caused a segfault.

%NVR
EOF
exit 0

%files
/tmp/testfile-%{name}
