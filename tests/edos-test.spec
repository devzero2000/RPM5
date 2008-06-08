Summary:	EDOS test case packages.
Name:		edos-test
Version:	1		# <== permit E:V-R
Release:	0		# <== NUKE
License:	Public Domain	# <== NUKE
Group:		Public Domain	# <== NUKE
URL:		http://www.mancoosi.org/edos/manager.html
BuildArch:	noarch

%description			# <== NUKE

%package -n car
#Summary:	car		# <== NUKE
#Group:		car		# <== NUKE
#%description -n	car		# <== NUKE
Requires:	engine
Requires:	wheel
Requires:	door

%package -n	engine
Version:	1		# <== permit E:V-R
#Summary:	engine		# <== NUKE
#Group:		engine		# <== NUKE
#%description -n	engine		# <== NUKE
Requires:	turbo

%package -n	engine		# <== multi-version
Version:	2		# <== permit E:V-R

%package -n	wheel		# <== multi-version
Version:	2		# <== permit E:V-R

%package -n	wheel
Version:	3		# <== permit E:V-R
#Summary:	wheel		# <== NUKE
#Group:		wheel		# <== NUKE
#%description -n	wheel		# <== NUKE
Requires:	tyre

%package -n	door		# <== multi-version
Version:	1		# <== permit E:V-R

%package -n	door
Version:	2		# <== permit E:V-R
#Summary:	door		# <== NUKE
#Group:		door		# <== NUKE
#%description -n	door		# <== NUKE
Requires:	window

%package -n	turbo
#Summary:	turbo		# <== NUKE
#Group:		turbo		# <== NUKE
#%description -n	turbo

%package -n	tyre
Version:	1		# <== permit E:V-R
#Summary:	tyre		# <== NUKE
#Group:		tyre		# <== NUKE
#%description -n	tyre		# <== NUKE

%package -n	tyre
Version:	2		# <== permit E:V-R

%package -n	window		# <== multi-version
Version:	0		# <== permit E:V-R

%package -n	window		# <== multi-version
Version:	1		# <== permit E:V-R
Requires:	glass

%package -n	window
Version:	2		# <== permit E:V-R
#Summary:	window		# <== NUKE
#Group:		window		# <== NUKE
#%description -n	window		# <== NUKE
Requires:	glass

%package -n	glass		# <== multi-version
Version:	1		# <== permit E:V-R

%package -n	glass
Version:	2		# <== permit E:V-R
#Summary:	glass		# <== NUKE
#Group:		glass		# <== NUKE
#%description -n	glass		# <== NUKE
Conflicts:	tyre = 2

%files -n	car
%files -n	engine-2
%files -n	engine-1
%files -n	wheel-2
%files -n	wheel-3
%files -n	door-1
%files -n	door-2
%files -n	turbo
%files -n	tyre-1
%files -n	tyre-2
%files -n	window-0
%files -n	window-1
%files -n	window-2
%files -n	glass-1
%files -n	glass-2

%changelog
* Sat Jun  7 2008 Jeff Johnson <jbj@rpm5.org> - 1-0
- create.
