Summary:	I am broken
Name: 		broken
Version:	1
Release:	1
Group: 		BROKEN/STUFF
Copyright:	GPL

Requires:	t1
BuildRoot:	/tmp/%{name}-%{verison}-%{release}

%description
So very broken.

%post 
exit 1

%files 
