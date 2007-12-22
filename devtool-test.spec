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

%track
prog rpm = {
    version   = 5.0b2
    url       = http://rpm5.org/files/rpm/rpm-5.0/
    regex     = rpm-(__VER__)\.tar\.gz
}

%prep

%build

%install
rm -rf $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)

%changelog

