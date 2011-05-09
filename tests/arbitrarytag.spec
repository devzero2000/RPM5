Summary:	Demostrate how to use arbitrary tags.
Name:		arbitrarytag
Version:	1.0
Release:	2
License:	LGPL2
Group:		Development/Examples
URL:		http://rpm5.org/
BuildArch:	noarch

Cveid:		CVE-2008-5964 
Rhbz:		410031
Rhbz:		410032
Cvssv2:         7.2 (AV:L/AC:L/Au:N/C:C/I:C/A:C)
Susesa:		SUSE-SA:2010:052

%description
A *.spec file to demonstrate how to use arbitrary tags.
In this specific example we are using the new
arbitrary security tags.

%sanitycheck
# execute rpm -Vv %{name} for having the result on stdout 
rpm -q --qf '[CVEID: %%{CVEID}\n][RHSA: %%{RHSA}\n][RHBZ: %%{RHBZ}\n][CVSSv2: %%{CVSSv2}\n][SUSESA: %%{SUSESA}]\n' %{name} 

%files

%changelog 
* Mon May  9 2011 Elia Pinto <devzero2000@rpm5.org> - 1.0-2
- Changed to new License, use the new arbitrary_tag_security 
  arbitrary tag
* Wed Feb 24 2010 Elia Pinto <devzero2000@rpm5.org> - 1.0-1
- First Built but already fix a bunch of vulnerability.
  Yeap, in fact do nothing.
