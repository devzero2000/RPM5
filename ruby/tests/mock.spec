Name: Foo
Version: 1.0
Release: 1
Group: Test/Mocks
License: BSD
Summary: A mock spec file
Source0: foosource1
Source1: %{name}-%{version}
Source3: barsource1
%define foo Barbazl

%description
This is a mock spec file for testing.


%prep
echo 'prepared'


%build
echo 'built'


%install
echo 'installed'


%files
