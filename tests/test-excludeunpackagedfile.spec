Name:           test-excludeunpackagefile
Version:        1.0
Release:        1%{?dist}
Summary:        Reproducer for %exclude issue
Group:          Security
License:        LGPL
BuildRoot:      %{_tmppath}/%{name}-root
BuildArch:      noarch

%description
Reproducer for %%exclude issue.

%%exclude will not only exclude files from being packaged
in that specific package,but also exclude it from the check 
for unpackackaged files as well,so that it could be done 
using %%exclude as well in addition to
removing the files during %%install.

%prep
%setup -q -T -c

%build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_sharedstatedir}/%{name}/%{name}-dir1
touch $RPM_BUILD_ROOT%{_sharedstatedir}/%{name}-file1
touch $RPM_BUILD_ROOT%{_sharedstatedir}/%{name}/%{name}-dir1/%{name}-file1
touch $RPM_BUILD_ROOT%{_sharedstatedir}/%{name}-file2

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_sharedstatedir}/%{name}-file2
%exclude %{_sharedstatedir}/%{name}-file1
%exclude %{_sharedstatedir}/%{name}/%{name}-dir1/%{name}-file1

%changelog
* Wed Mar  9 2011 Elia Pinto <devzero2000@rpm5.org> 1.0-1
- First Build

