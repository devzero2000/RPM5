Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
License:	GPL

Requires:	works

%description
You wanted something more...fine!!!

%post
exit 1

%preun
rm -f /tmp/broken_ran_in_arb
touch /tmp/broken_ran_in_arb

%files
