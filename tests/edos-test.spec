Summary:	EDOS test case packages.
Name:		edos-test
Version:	1
Release:	0
License:	Public Domain		# <== NUKE
Group:		Public Domain		# <== NUKE
URL:		http://www.mancoosi.org/edos/manager.html
BuildArch:	noarch

%description
NUKE

%package -n car
Summary:	car		# <== NUKE
Group:		car		# <== NUKE
%description -n	car
NUKE
Requires:	engine
Requires:	wheel
Requires:	door

%package -n	engine
Summary:	engine		# <== NUKE
Group:		engine		# <== NUKE
%description -n	engine
NUKE
Requires:	turbo

%package -n	wheel
Version:	3
Summary:	wheel		# <== NUKE
Group:		wheel		# <== NUKE
%description -n	wheel
NUKE
Requires:	tyre

%package -n	door
Version:	2
Summary:	door		# <== NUKE
Group:		door		# <== NUKE
%description -n	door
NUKE
Requires:	window

%package -n	turbo
Summary:	turbo		# <== NUKE
Group:		turbo		# <== NUKE
%description -n	turbo
NUKE

%package -n	tyre
Summary:	tyre		# <== NUKE
Group:		tyre		# <== NUKE
%description -n	tyre
NUKE

%package -n	window
Summary:	window		# <== NUKE
Group:		window		# <== NUKE
%description -n	window
NUKE
Requires:	glass

%package -n	glass
Version:	2
Summary:	glass		# <== NUKE
Group:		glass		# <== NUKE
Conflicts:	tyre = 2
%description -n	glass
NUKE

%files -n	car
%files -n	engine
%files -n	wheel
%files -n	door
%files -n	turbo
%files -n	tyre
%files -n	window
%files -n	glass

%changelog
* Sat Jun  7 2008 Jeff Johnson <jbj@rpm5.org> - 1-0
- create.
