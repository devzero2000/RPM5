Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	2
Group: 		System Environment/Base
Copyright:	GPL
BuildRoot:	/tmp/works-1-1
Prefix:		/tmp

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
echo "Running works-1-1 pre..."
exit 0

%post
echo "Running works-1-1 post..."
exit 0

%files
/tmp/mytest_file
