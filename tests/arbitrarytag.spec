############################
# My arbitrary tags
############################
%global my_arbitrary_tags    RHSA:CVEID:RHBZ
############################
# Append my arbitrary tags
############################
%global _arbitrary_tags %(printf %%s '%{_arbitrary_tags}:%{my_arbitrary_tags}')
Name: arbitrarytag
Version: 1.0
Release: 1
License: GPL
Group: Development/Tools
URL: http://rpm5.org/
Summary: %{name}
BuildArch: noarch
###################
# Arbitrary tag
###################
#CVEID: CVE-2008-5964 
#RHSA: RHSA-2007:1128-6 RHSA-2008:1129-6
#RHBZ: 410031 410032
%description
This is an toy use case of arbitrary tags. 
It is not the best example of using tags 
as CVEID because they should be in repo-md
but none care, as many yum-plugin-security 
users know.
Execute the command below
  $rpm -q --qf '%%{CVEID}\n%%{RHSA}\n%%{RHBZ}\n'
to see the vulnerability fixed.


%prep
# empty
%build
# empty
%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/tmp
echo " " > %{buildroot}/tmp/%{name}
%clean
rm -rf %{buildroot}
%files
%defattr(-,root,root,-)
/tmp/%{name}
%changelog 
* Wed Feb 24 2010 Elia Pinto <devzero2000@rpm5.org> - 1.0-1
- First Built but already fix a bunch of vulnerability.
  Yeap, in fact do nothing.
