Summary:   Triggers on D pattern functional tests.
Name:      triggers-DP
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

%triggerprein  a -- /tmp/%{name}-[bq]/

%triggerin     a -- /tmp/%{name}-[bq]/

%triggerun     a -- /tmp/%{name}-[bq]/

%triggerpostun a -- /tmp/%{name}-[bq]/

%triggerprein  b -- /tmp/%{name}-[aq]/

%triggerin     b -- /tmp/%{name}-[aq]/

%triggerun     b -- /tmp/%{name}-[aq]/

%triggerpostun b -- /tmp/%{name}-[aq]/

%triggerprein  a -- /tmp/%{name}-[cq]/

%triggerin     a -- /tmp/%{name}-[cq]/

%triggerun     a -- /tmp/%{name}-[cq]/

%triggerpostun a -- /tmp/%{name}-[cq]/

%triggerprein  b -- /tmp/%{name}-[cq]/

%triggerin     b -- /tmp/%{name}-[cq]/

%triggerun     b -- /tmp/%{name}-[cq]/

%triggerpostun b -- /tmp/%{name}-[cq]/

%files a
%dir /tmp/%{name}-a
/tmp/%{name}-a/a

%files b
%dir /tmp/%{name}-b
/tmp/%{name}-b/b

%changelog
* Sat Sep 20 2008 Jeff Johnson <jbj@rpm5.org> - 1.0-1
- Triggers on D pattern functional tests.
