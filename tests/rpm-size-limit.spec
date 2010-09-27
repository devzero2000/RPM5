%{?!nofile:%global nofile 1 }
%{?!totalfilesizeMB:%global totalfilesizeMB 1 } 
%{?!filesizeMB:%global filesizeMB 1 } 
%{?!compresstgz:%global compresstgz 0 } 

%if 0%{?totalfilesizeMB}
%{expand: %%global filesizeMB $(echo $((%{totalfilesizeMB}/%{nofile})))}
%endif

Summary:        This is a toy rpm to test some rpm size limit
Name:           rpm-size-limit
Version:        1.0
Release:        1
Group:          Development/System
License:        LGPL
BuildArch:      noarch
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root

%description

This toy rpm contains %{nofile} files of 
%{filesizeMB} MB in /tmp/. i
If you build with --define 'compresstgz 1'
then the rpm contains twice as much %{nofile} files: 
the compressed tarball version and the non-compressed.


Example :

1) as in rhbz#462539 

rpmbuild -ba --define 'nofile 1' --define 'totalfilesizeMB 110' --define 'compresstgz 1' %{name}.spec 

2) The  2Gb Limit rhbz#498236

rpmbuild -ba --define 'nofile 1' --define 'totalfilesizeMB 2000'  %{name}.spec 


%prep
#empty
%build
#empty
%install

mkdir -p %{buildroot}/tmp
_c=1
while [ ${_c} -le %{nofile} ]
do
 dd if=/dev/zero of=%{buildroot}/tmp/file%{name}${_c} count=%{filesizeMB} bs=1M 
 [ "%{?compresstgz}" -ne 0 ] && tar -zcvf %{buildroot}/tmp/file%{name}${_c}.tgz %{buildroot}/tmp/file%{name}${_c} 
 let _c=_c+1
done


%clean
rm -rf %{buildroot}

%files
/tmp/file%{name}*

%changelog
* Mon Sep 27 2010 Elia Pinto <devzero2000@rpm5.org> - 1.0-1
- First Build
