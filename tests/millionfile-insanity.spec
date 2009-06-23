Summary:   1 million file insanity test
Name:      millionfile-insanity
Version:   1.0
Release:   1
License:   Public Domain
Group:     Development/Tools
URL:       http://rpm5.org/
Prefix:    /tmp
Source0:   JBJ-GPG-KEY
BuildArch: noarch

%description
Trivial standalone test of a 1 million file package

%install
for ((i=0;i<1000;i+=1)); do
    mkdir -p %{buildroot}/tmp/${i}
    for ((j=0;j<1000;j+=1)); do
        echo "%{buildroot}/tmp/${i}/${j}" > %{buildroot}/tmp/${i}/${j}
    done
done

%files
%defattr(-,root,root,-)
/tmp

%changelog
* Tue Jun 23 2009 Jason Corley <jasonc@rpm5.org> - 1.0-1
- basically entirety of spec trimmed/stolen from devtool-sanity.spec

