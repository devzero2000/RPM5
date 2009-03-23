# $Revision$, $Date$
%bcond_without	pkg1	# build the "first" package (%{name}-1-1)
%bcond_with	pkg2	# build the "second" package (%{name}-4-1)

%if %{with pkg2}
# disable pkg1 if pkg2 is built
%undefine	with_pkg1
%endif
Summary:	Multiple triggers fire test from upgrade
Name:		triggers-multiple
Version:	%{?with_pkg1:1}%{?with_pkg2:4}
Release:	1
License:	Public Domain
Group:		Applications/System
BuildArch:	noarch
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%description
Summary:	Multiple triggers fire test from upgrade

%prep
%setup -qcT

%clean
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT

%if %{with pkg2}
%triggerpostun -- %{name} < 2
echo "%{name}-%{version}-%{release} postun on %{name} < 2"

%triggerpostun -- %{name} < 3
echo "%{name}-%{version}-%{release} postun on %{name} < 3"

%triggerpostun -- %{name} < 4
echo "%{name}-%{version}-%{release} postun on %{name} < 4"
%endif

%files
%defattr(644,root,root,755)

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* %{date} PLD Team <feedback@pld-linux.org>
All persons listed below can be reached at <cvs_login>@pld-linux.org

$Log$
Revision 1.1  2009/03/23 19:52:09  glen
- spec to build pkgs for multiple triggers fire

Revision 1.16.4.69.2.4.2.1  2009/03/23 19:37:24  glen
- spec for triggers
