Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
Copyright:	GPL

Requires:  works = 0:1-1

%description
You wanted something more...fine!!!

%pre
i=$1
echo "%{name}-%{version}-%{release}($i): Running pre..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_pre_icount
rm -f /tmp/ran_in_autorollback
exit 0

%post
i=$1
echo "%{name}-%{version}-%{release}($i): Running post..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_post_icount
exit 1

%preun
i=$1
echo "%{name}-%{version}-%{release}($i): Running preun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_preun_icount
touch /tmp/ran_in_autorollback
ls -l /tmp/ran_in_autorollback
exit 0

%postun
i=$1
echo "%{name}-%{version}-%{release}($i): Running postun..."
echo ${i} > /tmp/%{name}-%{version}-%{release}_postun_icount
exit 0

%files
