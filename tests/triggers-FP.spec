Summary:   Triggers on F pattern functional tests.
Name:      triggers-FP
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

%triggerprein  a -- /tmp/%{name}-b/[bq]

%triggerin     a -- /tmp/%{name}-b/[bq]

%triggerun     a -- /tmp/%{name}-b/[bq]

%triggerpostun a -- /tmp/%{name}-b/[bq]

%triggerprein  b -- /tmp/%{name}-a/[aq]

%triggerin     b -- /tmp/%{name}-a/[aq]

%triggerun     b -- /tmp/%{name}-a/[aq]

%triggerpostun b -- /tmp/%{name}-a/[aq]

%triggerprein  a -- /tmp/%{name}-c/[cq]

%triggerin     a -- /tmp/%{name}-c/[cq]

%triggerun     a -- /tmp/%{name}-c/[cq]

%triggerpostun a -- /tmp/%{name}-c/[cq]

%triggerprein  b -- /tmp/%{name}-c/[cq]

%triggerin     b -- /tmp/%{name}-c/[cq]

%triggerun     b -- /tmp/%{name}-c/[cq]

%triggerpostun b -- /tmp/%{name}-c/[cq]

%files a
%dir /tmp/%{name}-a
/tmp/%{name}-a/a

%files b
%dir /tmp/%{name}-b
/tmp/%{name}-b/b

%changelog
* Sat Sep 20 2008 Jeff Johnson <jbj@rpm5.org> - 1.0-1
- Triggers on F pattern functional tests.
