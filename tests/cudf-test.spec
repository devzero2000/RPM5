Summary:	CUDF test case packages.
Name:		cudf-test
Version:	1		# <== permit E:V-R
Release:	0		# <== NUKE
License:	Public Domain	# <== NUKE
Group:		Public Domain	# <== NUKE
URL:		http://www.mancoosi.org/edos/manager.html
BuildArch:	noarch

%description
Some remarks about the example follow.

* The example does not rely on any extended properties.
* Intuitively, the example comes from a packaging world where different
  versions of the same package are implicitly conflicting with each other.
  To grasp this, all packages for which multiple versions are available
  declare a non-versioned conflicts with themselves. 
* The engine feature is mutually exclusive, only one (installed) package
  can provide it. This is encoded using conﬂicts with the feature from
  each package providing it. 

%package -n	car
Version:	1		# <== permit E:V-R. inheirit?
Requires:	engine, wheel, door, battery

%package -n	bicycle
Version:	7		# <== permit E:V-R

%package -n	gasoline-engine
Version:	1		# <== permit E:V-R. inheirit?
Requires:	turbo
Provides:	engine
Conflicts:	engine, gasoline-engine

%package -n	gasoline-engine
Version:	2		# <== permit E:V-R
Provides:	engine
Conflicts:	engine, gasoline-engine

%package -n	electric-engine
Version:	1		# <== permit E:V-R. inheirit?
Requires:	solar-collector	# | huge-battery
Provides:	engine
Conflicts:	engine, electric-engine

%package -n	electric-engine
Version:	2		# <== permit E:V-R
Requires:	solar-collector	# | huge-battery
Provides:	engine
Conflicts:	engine, electric-engine

%package -n	solar-collector
Version:	1		# <== permit E:V-R. inheirit?

%package -n	battery
Version:	3		# <== permit E:V-R
Provides:	huge-battery

%package -n	wheel
Version:	2		# <== permit E:V-R
Conflicts:	wheel

%package -n	wheel
Version:	3		# <== permit E:V-R
Conflicts:	wheel

%package -n	door
Version:	1		# <== permit E:V-R. inheirit?
Requires:	window

%package -n	door
Version:	2		# <== permit E:V-R
Requires:	window

%package -n	turbo
Version:	1		# <== permit E:V-R. inheirit?

%package -n	tire
Version:	1		# <== permit E:V-R. inheirit?
Conflicts:	tire

%package -n	tire
Version:	2		# <== permit E:V-R
Conflicts:	tire

%package -n	window
Version:	1		# <== permit E:V-R. inheirit?
Conflicts:	window

%package -n	window
Version:	2		# <== permit E:V-R. inheirit?
Requires:	glass = 1
Conflicts:	window

%package -n	window
Version:	3		# <== permit E:V-R
Requires:	glass = 2
Conflicts:	window

%package -n	glass
Version:	1		# <== permit E:V-R. inheirit?
Conflicts:	glass

%package -n	glass
Version:	2		# <== permit E:V-R
Conflicts:	glass, tire = 2

%files -n	car-1
%files -n	bicycle-7
%files -n	gasoline-engine-1
%files -n	gasoline-engine-2
%files -n	electric-engine-1
%files -n	electric-engine-2
%files -n	solar-collector-1
%files -n	battery-3
%files -n	wheel-2
%files -n	wheel-3
%files -n	door-1
%files -n	door-2
%files -n	turbo-1
%files -n	tire-1
%files -n	tire-2
%files -n	window-1
%files -n	window-2
%files -n	window-3
%files -n	glass-1
%files -n	glass-2

%changelog
* Tue Nov  4 2008 Jeff Johnson <jbj@rpm5.org> - 1-0
- create.
