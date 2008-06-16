Name:         probes-test
Summary:      RPM dependency probes test
Version:      1
Release:      0
License:      Any
Group:        Any
BuildArch:    noarch

Source1:      key1.pgp
Source2:      key2.pgp
Source3:      test1.txt
Source4:      test1.sig
Source5:      test1.asc
Source6:      test2.txt
Source7:      test2.sig
Source8:      test2.asc
Source9:      test3.txt
Source10:     test3.sig
Source11:     test3.asc

BuildPreReq:  gnupg(%{SOURCE5})                 =            4E23E878D41A0A88EDFCFA5A6E744ACBA9C09E30
BuildPreReq:  signature(%{SOURCE5})             = %{SOURCE1}:4E23E878D41A0A88EDFCFA5A6E744ACBA9C09E30
BuildPreReq:  signature(%{SOURCE3}:%{SOURCE4})  = %{SOURCE1}:4E23E878D41A0A88EDFCFA5A6E744ACBA9C09E30

BuildPreReq:  gnupg(%{SOURCE8})                 =            7D121A8FC05DC18A4329E9EF67042EC961B7AE34
BuildPreReq:  signature(%{SOURCE8})             = %{SOURCE2}:7D121A8FC05DC18A4329E9EF67042EC961B7AE34
BuildPreReq:  signature(%{SOURCE6}:%{SOURCE7})  = %{SOURCE2}:7D121A8FC05DC18A4329E9EF67042EC961B7AE34

BuildPreReq:  gnupg(%{SOURCE11})                =            7D121A8FC05DC18A4329E9EF67042EC961B7AE34
BuildPreReq:  signature(%{SOURCE11})            = %{SOURCE2}:7D121A8FC05DC18A4329E9EF67042EC961B7AE34
BuildPreReq:  signature(%{SOURCE9}:%{SOURCE10}) = %{SOURCE2}:7D121A8FC05DC18A4329E9EF67042EC961B7AE34

BuildPreReq:  user(root)
BuildPreReq: !user(ToOr)
BuildPreReq:  group(root)
BuildPreReq: !group(ToOr)
BuildPreReq:  mounted(/)
BuildPreReq: !mounted(/PaCkAgEs)
BuildPreReq:  diskspace(/tmp) > 1
BuildPreReq:  diskspace(/tmp) >= 1Kb
BuildPreReq: !diskspace(/tmp) < 1Mb
BuildPreReq:  executable(/bin/sh)
BuildPreReq:  executable(cat)

%description
This is the parent package.

%package -n  probes
Version:      1
Requires:     user(root)
Requires:    !user(ToOr)
Requires:     group(root)
Requires:    !group(ToOr)
Requires:     mounted(/)
Requires:    !mounted(/PaCkAgEs)
Requires:     diskspace(/tmp) > 1
Requires:     diskspace(/tmp) >= 1Kb
Requires:    !diskspace(/tmp) < 1Mb
Requires:     executable(/bin/sh)
Requires:     executable(cat)

%description -n probes-1
This is the arch-specific package.

%package -n  probes
Version:      2
BuildArch:    noarch
Requires:     user(root)
Requires:    !user(ToOr)
Requires:     group(root)
Requires:    !group(ToOr)
Requires:     mounted(/)
Requires:    !mounted(/PaCkAgEs)
Requires:     diskspace(/tmp) > 1
Requires:     diskspace(/tmp) >= 1Kb
Requires:    !diskspace(/tmp) < 1Mb
Requires:     executable(/bin/sh)
Requires:     executable(cat)


%description -n probes-2
This is the noarch package.

%prep

%build

%install

%clean

%files -n probes-1

%files -n probes-2

