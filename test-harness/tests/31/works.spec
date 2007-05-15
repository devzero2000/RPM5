Summary:	The rpm that simply works.
Name: 		works
Version:	2
Release:	0
Group: 		System Environment/Base
License:	GPL

%description
It just works.  What more do you want?

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
