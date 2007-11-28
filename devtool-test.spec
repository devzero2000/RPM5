Summary: devtool test
Name: devtool-test
Version: 1.0
Release: 1
License: Public Domain
Group: Development/Tools
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildArch: noarch

%description
Trivial standalone test package

%prep

%build

%install
rm -rf $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)

%changelog

