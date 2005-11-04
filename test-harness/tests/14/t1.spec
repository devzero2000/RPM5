Summary:	The rpm that simply works.
Name: 		t1
Epoch:		0
Version:	1
Release:	2
Group: 		System Environment/Base
License:	GPL
BuildRoot:	/tmp/%{name}-%{version}-%{release}

Requires: t2 

%description
It just works.  What more do you want?

%install

rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/tmp

cat >$RPM_BUILD_ROOT/tmp/testfile-%{name} <<EOF
We need test packages with files, as if things are not
done right in the rollback transaction this little file
can/has caused a segfault.

%{name}-%{version}-%{release}
EOF
exit 0

%triggerin -- t2
myi=$1
ti=$2
echo "%{name}-%{version}-%{release}($myi, $ti): Running trigger against t2..."
echo ${myi} > /tmp/%{name}-%{version}-%{release}_trigger_myicount
echo ${ti} > /tmp/%{name}-%{version}-%{release}_trigger_ticount
exit 0

%files
/tmp/testfile-%{name}
