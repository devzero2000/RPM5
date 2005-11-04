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
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_pre_icount
rm -f /tmp/%{name}-%{version}-%{release}_ran_pre_in_rollback
touch /tmp/%{name}-%{version}-%{release}_ran_pre_in_rollback
exit 0

%post
i=$1
echo "%{name}-%{version}-%{release}($i): Running post..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_post_icount
rm -f /tmp/%{name}-%{version}-%{release}_ran_post_in_rollback
touch /tmp/%{name}-%{version}-%{release}_ran_post_in_rollback
exit 0

#
# Can't guarantee that these will run due to erasures not
# being sorted:
%preun
i=$1
echo "%{name}-%{version}-%{release}($i): Running preun..."
exit 0

%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running postun..."
exit 0

%files
/tmp/mytest_file
