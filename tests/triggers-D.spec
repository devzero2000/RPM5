Summary:   Triggers on D functional tests.
Name:      triggers-D
Version:   1.0
Release:   1
License:   Public Domain
Group:     Development/Tools
URL:       http://rpm5.org/
Prefix:    /tmp

%description			# <== NUKE

%package a
BuildArch: noarch
Provides: A = 1.0
Provides: AA = 2.0

%package b
Provides: B = 1.0
Provides: BB = 2.0

%install
mkdir -p %{buildroot}/tmp/%{name}-a
echo "a" > %{buildroot}/tmp/%{name}-a/a
mkdir -p %{buildroot}/tmp/%{name}-b
echo "b" > %{buildroot}/tmp/%{name}-b/b

%triggerprein  a -- /tmp/%{name}-b/

%triggerin     a -- /tmp/%{name}-b/

%triggerun     a -- /tmp/%{name}-b/

%triggerpostun a -- /tmp/%{name}-b/

%triggerprein  b -- /tmp/%{name}-a/

%triggerin     b -- /tmp/%{name}-a/

%triggerun     b -- /tmp/%{name}-a/

%triggerpostun b -- /tmp/%{name}-a/

%triggerprein  a -- /tmp/%{name}-FAILURE/

%triggerin     a -- /tmp/%{name}-FAILURE/

%triggerun     a -- /tmp/%{name}-FAILURE/

%triggerpostun a -- /tmp/%{name}-FAILURE/

%triggerprein  b -- /tmp/%{name}-FAILURE/

%triggerin     b -- /tmp/%{name}-FAILURE/

%triggerun     b -- /tmp/%{name}-FAILURE/

%triggerpostun b -- /tmp/%{name}-FAILURE/

%files a
%dir /tmp/%{name}-a
/tmp/%{name}-a/a

%files b
%dir /tmp/%{name}-b
/tmp/%{name}-b/b

%changelog
* Sat Sep 20 2008 Jeff Johnson <jbj@rpm5.org> - 1.0-1
- Triggers on D functional tests.
