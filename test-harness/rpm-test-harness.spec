%define my_prefix /usr/local

Summary:	Test Harness For RPM
Name: 		rpm-test-harness
Version:	%{version}
Release:	1
Group: 		System Environment/Base
Source:         http://lee.k12.nc.us/~joden/misc/patches/rpm/rpm-test-harness/rpm-test-harness-%{version}.tgz
Prefix:         %{my_prefix}
Copyright:	GPL

BuildRoot:      /tmp/%(echo $USER)/rpm-test-harness-%{version}-%{release}

%description
This is a test harness for rpm.  Presently, most of the distributed 
test cases are for autorollback, but feel free to make some of your own.
If you submit them back, I may use them.

%prep
echo "Extracting sources..."
%setup

%build

%install
echo "Running make..."
make DEBUG=1 rpmbuild BUILD_ROOT=${RPM_BUILD_ROOT} \
	PREFIX=%{my_prefix}

%files
%{my_prefix}/%{name}-%{version}

