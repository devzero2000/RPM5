%define my_file /tmp/blue

Summary:	Test package
Name: 		t1
Version:	1
Release:	1
Group: 		Who/Cares
Copyright:	GPL

BuildRoot:  /tmp/%{name}-%{version}-%{release}

%description
Test of modifiying a config file.

%install 
rm -rf ${RPM_BUILD_ROOT}
mkdir -p "${RPM_BUILD_ROOT}/tmp/"
cat <<EOF > "${RPM_BUILD_ROOT}%{my_file}"
Random thoughts meaning nothing, but from the chaos forms rpm...
EOF

%post 
echo "Horrors" >> "%{my_file}"

%files 
%attr(0700, root, root) %{my_file}
