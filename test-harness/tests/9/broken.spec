Summary:	The rpm that simply won't work.
Name: 		broken
Version:	1
Release:	1
Group: 		System Environment/Base
Copyright:	GPL

Requires:	works

%description
You wanted something more...fine!!!

%post
echo "Failing in post!"
exit 1

%preun
[ -f /tmp/broken_ran_in_arb ] && rm /tmp/broken_ran_in_arb
touch /tmp/broken_ran_in_arb

%files
