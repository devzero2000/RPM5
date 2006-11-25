Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL

Requires:  works = 0:1-1

%description
You wanted something more...fine!!!

%pre
echo $1 > /tmp/%{NVR}_pre_icount
rm -f /tmp/ran_in_autorollback
exit 0

%post
echo $1 > /tmp/%{NVR}_post_icount
exit 1

%preun
echo $1 > /tmp/%{NVR}_preun_icount
touch /tmp/ran_in_autorollback
ls -l /tmp/ran_in_autorollback
exit 0

%postun
echo $1 > /tmp/%{NVR}_postun_icount
exit 0

%files
