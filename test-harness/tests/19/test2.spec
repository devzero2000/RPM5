Summary:	The rpm that simply works.
Name: 		test2
Version:	1
Release:	2
Group: 		System Environment/Base
License:	GPL
BuildRoot:	/tmp/%{name}-%{version}-%{release}

%description
It just works.  What more do you want?

%define my_file /tmp/test_file-%{name}-%{version}-%{release}

%install

rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT%{my_file} <<EOF
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
exit 0
                                                                                
%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running postun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_postun_icount
exit 0

%files
%{my_file}
