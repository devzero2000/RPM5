Summary:	Test Package for testing changing directory to symlink on upgrade
Name:		test-change-dir-to-symlink
Version:	1
Release:	1
License:	LGPL
Group:		Amusements/Games
URL:		http://rpm5.org
BuildRoot:	%{_tmppath}/%{name}-%{version}-root-%(id -u -n)
BuildArch:      noarch

%description
This is the first package release. It install
a /tmp/%{name} directory. It will
be replaced in upgrade by
a symbolic link to /tmp/%{name}-directory

%prep
#empty
%build
#empty

%install
rm -rf $RPM_BUILD_ROOT

install -d $RPM_BUILD_ROOT/tmp/%{name}
echo "hello" > $RPM_BUILD_ROOT/tmp/%{name}/%{name}-file
echo "I can fix this issue" > %{buildroot}/tmp/%{name}/%{name}-file-1
mkdir -p %{buildroot}/tmp/%{name}/%{name}-directory-1
touch %{buildroot}/tmp/%{name}/%{name}-directory-1/%{name}-file-2

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
/tmp/%{name}

%changelog
* Mon Aug 4 2010 Elia Pinto <devzero2000@rpm5.org> 1-1
- First Build
