Name: mainpkg
Version: 1.0
Release: 1.0
Summary: Main package of multi-package mock spec
Group: Test/Mocks
License: LGPL
URL: http://www.rpm5.org
Source: mocksource-%{version}.tar.gz

# Simple macro for MacroContext tests:
%define spec_name %name

%description
This is a mock spec file. It is constructued to create more than one RPM.


%package subpkg
Summary: Sub-package of multi-package mock spec
Group: Test/Mocks
License: LGPL

%description subpkg
This is the sub-package of the main package of the multi-package mock spec
file.


%package -n mainpkg2
Summary: Independent, second package of the multi-package mock spec file
Group: Test/Mocks
License: LGPL

%description -n mainpkg2
This is another package that is created when building the multi-package mock
spec file. It is, however, in it's name independent from the actual package.


%prep
%setup -q


%build
touch %{name}


%install
true


%files


%files subpkg


%files -n mainpkg2
