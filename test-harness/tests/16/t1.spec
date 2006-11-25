%define my_file /tmp/blue
Summary:	A test
Name: 		t1
Version:	1
Release:	2
Group: 		Does/It/Matter
License:	GPL

BuildRoot:	%_tmppath/%NVR

%description
A test

%install 
rm -rf ${RPM_BUILD_ROOT}
mkdir -p "${RPM_BUILD_ROOT}/tmp/"
cat <<EOF > "${RPM_BUILD_ROOT}%{my_file}"
Bugs don't manifest from nothing.  Code must be written.
EOF

%post 
echo "Horrors" >> %{my_file}
echo 0

%files 
%attr(0700, root, root) %{my_file}
