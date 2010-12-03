# --- Permit RHSA: CVEID: and RHBZ: arbitrary tags in headers.
# XXX This cannot be done in a *.spec, must be configured before building.
# XXX Let's pretend that's a featlet, not a bugture.
%{expand:%%global _arbitrary_tags %{_arbitrary_tags}:RHSA:CVEID:RHBZ:ATAS}

Summary:	Demonstrate how to use arbitrary tags.
Name:		arbitrarytag
Version:	1.0
Release:	1
License:	Public Domain
Group:		Development/Examples
URL:		http://rpm5.org/
BuildArch:	noarch

# --- Parse the tags from the *.spec file:
CVEID:		CVE-2008-5964 
RHSA:		RHSA-2007:1128-6
RHSA:		RHSA-2008:1129-6
RHBZ:		410031
RHBZ:		410032

# XXX Hmmm ... the text block ends up in the *.src.rpm, others in binary.
# XXX Let's pretend that's a featlet, not a bugture.
%atas
This is a test of the ATAS Arbitrary Tag Alert System ... 1 ... 2 ... 3 ...

%description
A *.spec file to demonstrate how to use arbitrary tags.

Permit CVEID/RHSA/RHBZ arbitrary tags by configuring a macro:
  echo "%%_arbitrary_tags CVEID:RHSA:RHBZ:ATAS" >> /etc/rpm/macros

Build and install the package from this *.spec:
  rpmbuild -ba %{name}.spec
  rpm -Uvh %{name}-%{version}-%{release}.noarch.rpm

Query the CVEID/RHSA/RHBZ tags:
  rpm -q --qf '[CVEID: %%{CVEID}\n][RHSA: %%{RHSA}\n][RHBZ: %%{RHBZ}\n]%%{ATAS}\n' %{name}

%posttrans
rpm -q --qf '[CVEID: %{CVEID}\n][RHSA: %{RHSA}\n][RHBZ: %{RHBZ}\n]%{ATAS}\n' %{name}

%files

%changelog 
* Wed Feb 24 2010 Elia Pinto <devzero2000@rpm5.org> - 1.0-1
- First Built but already fix a bunch of vulnerability.
  Yeap, in fact do nothing.
