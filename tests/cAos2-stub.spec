Summary:	cAos-2 install stub.
Name:		cAos2-stub
Version:	1
Release:	0
License:	Public Domain
Group:		Public Domain
URL:		http://mirror.caoslinux.org/cAos-2/core/i386/
BuildArch:	noarch
Provides: nscd = 2.3.3-73

%description
A shim package to provide missing dependencies for cAos2 packages in
order to install bash in a chroot.

%install
DIRS="
/usr/lib/gconv
/usr/X11R6/lib/X11
/usr/share/locale/en@quot/LC_MESSAGES
/usr/share/locale/en@boldquot/LC_MESSAGES
"
for D in $DIRS; do
    mkdir -p %{buildroot}$D
done

%files
%defattr(-,root,root)
/

%changelog
* Tue Jan 26 2010 Jeff Johnson <jbj@rpm5.org> - 1-0
- create.
