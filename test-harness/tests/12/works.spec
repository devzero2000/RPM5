Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	2
Group: 		System Environment/Base
Copyright:	GPL
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
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_pre_icount
rm -f /tmp/works-1-1_ran_pre_in_rollback
rm -f /tmp/works-1-1_ran_post_in_rollback
exit 0

%post
i=$1
echo "%{name}-%{version}-%{release}($i): Running post..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_post_icount
exit 0

%preun
i=$1
echo "%{name}-%{version}-%{release}($i): Running preun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_preun_icount
rm -f /tmp/%{name}_ran_preun_in_rollback
touch /tmp/%{name}_ran_post_in_rollback
exit 0

%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running postun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_postun_icount
rm -f /tmp/%{name}_ran_postun_in_rollback
touch /tmp/%{name}_ran_postun_in_rollback
exit 0

%files
/tmp/mytest_file
