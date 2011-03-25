#!/bin/sh
##########################################################
#  Output a rpm scriplets classification
#
#  Perhaps useful for understending what of
#  the scriplet can be trasformed in lua
#  or use augeas for some task (FILEOPERATION
#  for example). 
#
#  Copyright (C) 2011 Elia Pinto (devzero2000@rpm5.org)
#
#  Credit : Jeff Johnson, who wrote the initial version
#  Todo   : Add an option for quering binary rpm 
#           present in a directory and not only
#           the actual rpmdb. Probably it is necessary
#           to extend the classification in some area
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published by
#  The Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
##########################################################
#
readonly _PROGNAME=${0##*/}
#
readonly _RPM_PACKAGES="rpm -qa --pipe=\"sort\""
readonly _POPT_QUERY="%|PREIN?{%{PREIN}}:{}|\n%|POSTIN?{%{POSTIN}}:{}|\n%|PREUN?{%{PREUN}}:{}|\n%|POSTUN?{%{POSTUN}}:{}|\n%|VERIFYSCRIPT?{%{VERIFYSCRIPT}}:{}|\n"
readonly _RPM_SCRIPTLETS="rpm -q --qf ${_POPT_QUERY}"


_RPM_PKGS="0"

for _i_pkg in `$_RPM_PACKAGES`
do
    _RPM_PKGS="`expr $_RPM_PKGS + 1`"
    if [ ! -z "`$_RPM_SCRIPTLETS ${_i_pkg}`" ]; then
        echo "==== ${_i_pkg}"
        $_RPM_SCRIPTLETS ${_i_pkg} | sed \
            -e '/^[ \t]*$/d' \
            -e '/^[ ]*fi/d' \
            -e 's,#.*$,,' \
            -e 's,^.*(/sbin/ldconfig): (none)$,LDCONFIG_INTERP,' \
            -e 's,^.*cat .*$,FILEOPERATION,' \
            -e 's,^.*chmod .*$,FILEOPERATION,' \
            -e 's,^.*chown .*$,FILEOPERATION,' \
            -e 's,^.*cp .*$,FILEOPERATION,' \
            -e 's,^.*echo .*$,FILEOPERATION,' \
            -e 's,^echo .*$,FILEOPERATION,' \
            -e 's,^.*grep .*$,FILEOPERATION,' \
            -e 's,^.*head .*$,FILEOPERATION,' \
            -e 's,^.*install .*$,FILEOPERATION,' \
            -e 's,^.*mktemp .*$,FILEOPERATION,' \
            -e 's,^.*mv .*$,FILEOPERATION,' \
            -e 's,^.*rm .*$,FILEOPERATION,' \
            -e 's,^.*sed .*$,FILEOPERATION,' \
            -e 's,^.*awk .*$,FILEOPERATION,' \
            -e 's,^.*tail .*$,FILEOPERATION,' \
            -e 's,^.*chksession .*$,CHKSESSION,' \
            -e 's,^.*touch .*$,FILEOPERATION,' \
            -e 's,^.*/sbin/ldconfig.*$,LDCONFIG,' \
            -e 's,^.*/bin/true$,TRUE,' \
            -e 's,^[ ]*/bin/true[ ]*.*$,TRUE,' \
            -e 's,^.*/usr/bin/dbus.* .*$,DBUS_OPERATION,' \
            -e 's,^.*/usr/bin/ssh-.* .*$,SSH_OPERATION,' \
            -e 's,^.*/usr/sbin/set_tcb .*$,SET_TCB,' \
            -e 's,^.*/var/lib/mandriva/kde4-profiles/.* .*$,KDE_MANDRIVA_PROFILE,' \
            -e 's,^.*xguest-add-helper.*$,XGUESTADDHELPER,' \
            -e 's,^.*alternatives .*$,ALTERNATIVES,' \
            -e 's,^.*/bin/updmap-sys .*$,UPDMAPSYS,' \
            -e 's,^.*/sbin/.*kernel.* .*$,KERNELSTUFF,' \
            -e 's,^.*/usr/bin/locale.* .*$,LOCALESTUFF,' \
            -e 's,^.*mount .*,MOUNT,' \
            -e 's,^[\b\t]*mknod .*$,MKNOD,' \
            -e 's,^[ ]*/usr/sbin/plymouth-.* .*$,PLYMOUTHSTAFF,' \
            -e 's,^[ ]*/usr/sbin/grub.* .*$,GRUBMENU,' \
            -e 's,^[ ]*/sbin/netprofile .*$,NETPROFILE,' \
            -e 's,^[ ]*/sbin/set-netprofile .*$,NETPROFILE,' \
            -e 's,^[ ]*/sbin/makedev .*$,MAKEDEV,' \
            -e 's,[ ]*/sbin/makedev .*$,MAKEDEV,' \
            -e 's,[ ]*/usr/bin/dot .*$,DOT,' \
            -e 's,^.*/chkconfig .*$,CHKCONFIG,' \
            -e 's,^.*/telinit .*$,TELINIT,' \
            -e 's,^.*fc-cache .*$,FCCACHE,' \
            -e 's,^.*gconftool-2 .*$,GCONFTOOL2,' \
            -e 's,^.*groupadd .*$,GROUPADD,' \
            -e 's,^.*groupdel .*$,GROUPDEL,' \
            -e 's,^.*rebuild-gcj-db .*$,REBUILDGCJDB,' \
            -e 's,^.*/sbin/bootloader-config .*$,DRAKXTOOLSTAFF,' \
            -e 's,^.*gtk-update-icon-cache .*$,GTKUPDATEICONCACHE,' \
            -e 's,^.*glib-compile-schemas .*$,GLIBCOMPILESCHEMAS,' \
            -e 's,^.*gdk-pixbuf-query-loaders .*$,GDKPIXBUFINSTALL,' \
            -e 's,^.*/init\.d/.*$,INITSCRIPT,' \
            -e 's,^.*/install-info .*$,INSTALLINFO,' \
            -e 's,^.*/install-catalog .*$,INSTALLCATALOG,' \
            -e 's,^.*mkfontdir.*$,MKFONTDIR,' \
            -e 's,^.*mkfontscale.*$,MKFONTSCALE,' \
            -e 's,^.*restorecon .*$,RESTORECON,' \
            -e 's,^.*rebuild-gcj-db.*$,REBUILDGCJDB,' \
            -e 's,^.*scrollkeeper-update .*$,SCROLLKEEPER,' \
            -e 's,^.*service .*$,SERVICE,' \
            -e 's,^.*systemctl .*$,SYSTEMCTL,' \
            -e 's,^.*texconfig-sys .*$,TEXCONFIGSYS,' \
            -e 's,^.*update-desktop-database.*$,UPDATEDESKTOPDATABASE,' \
            -e 's,^.*update-mime-database.*$,UPDATEDMIMEDATABASE,' \
            -e 's,^.*desktop-file-install.*$,DESKTOPFILEINSTALL,' \
            -e 's,^.*desktop-file-validate.*$,DESKTOPFILEVALIDATE,' \
            -e 's,^.*firefox.*$,FIREFOXSTUFF,' \
            -e 's,^.*iconvconfig.*$,ICONVCONFIG,' \
            -e 's,^.*kdm4-background-config.*$,KDM4DESKTOPCONFIG,' \
            -e 's,^.*kdm/backgroundrc.*$,KDM4DESKTOPCONFIG,' \
            -e 's,^.*chsh .*$,MOREUSERSTUFF,' \
            -e 's,^.*usermod .*$,USERMOD,' \
            -e 's,^.*useradd .*$,USERADD,' \
            -e 's,^.*userdel .*$,USERDEL,' \
            -e 's,^.*adduser .*$,USERADD,' \
            -e 's,^.*deluser .*$,USERDEL,' \
            -e 's,^.*xmlcatalog .*$,XMLCATALOG,' \
            -e 's,^.*convertsession .*$,CONVERTSESSION,' \
            -e 's,^.*/usr/share/rpm-helper/add-group .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/add-service .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/add-shell .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/add-syslog .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/add-user .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/add-webapp .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/create-file .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/create-ssl-certificate .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/del-group .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/del-service .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/del-shell .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/del-syslog .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/del-user .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/del-webapp .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/get-password .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*/usr/share/rpm-helper/verify-shell .*$,RPM-MANDRIVA-HELPER,' \
            -e 's,^.*depmod .*$,UPDATELKM,' \
            -e 's,^.*insmod .*$,UPDATELKM,' \
            -e 's,^.*modutil .*$,UPDATELKM,' \
            -e 's,^.*rmmod .*$,UPDATELKM,' \
            -e 's,^.*chkconfig .*$,CHKCONFIG,' \
            -e 's,^.*ln .*$,FILEOPERATION,' \
            -e 's,^.*bin/perl .*$,FILEOPERATION,' \
            -e '/^.*n(\/bin\/sh): /d' \
            -e '/^.*n(\/bin\/.+): /d' \
            -e '/^[ \t]*:/d' \
            -e '/^[ ]*else/d' \
            -e '/[ ]*else/d' \
            -e '/^[ ]*exit/d' \
            -e '/[ ]*exit/d' \
            -e '/^[ ]*export/d' \
            -e '/^[ ]*esac/d' \
            -e '/^[ ]*case/d' \
            -e '/^[ ]*if/d' \
            -e '/^.*if.*\[/d' \
            -e '/^[ ]*then/d' \
            -e '/^[ ]*popd/d' \
            -e '/^[ ]*for/d' \
            -e '/[ ]*for.\+do/d' \
            -e '/[ ]*for.\+in/d' \
            -e '/[ ]*for.\+in/d' \
            -e '/[ ]*from.\+import/d' \
            -e '/^[ ]*while/d' \
            -e '/^[ ]*until/d' \
            -e '/^[ ]*done/d' \
            -e '/[ ]*done/d' \
            -e '/^[ ]*do$/d' \
            -e '/[\t\b]\+do/d' \
            -e '/^do$/d' \
            -e '/^do[ ]*$/d' \
            -e '/^[ ]*.\+;[ ]*do/d' \
            -e '/^break[ ]*$/d' \
            -e '/^break$/d' \
            -e '/[ ]*break[ ]*$/d' \
            -e '/[ ]*done/d' \
            -e '/\/usr\/share.*/d' \
            -e '/^[ ]*.*DTD.*DocBook/d' \
            -e '/^[ ]*oasis.*/d' \
            -e '/^.*"http:.*oasis.*"/d' \
            -e '/^.*"http:.*"/d' \
            -e '/^[ ]*docbook.*XML.*/d' \
            -e '/[ ]*--slave/d' \
            -e '/-[a|b|c|d|e|f|g|h|L|k|p|r|s|S|t|u|w|x|O|G|N|z|n|o|v] .*/d' \
            -e '/.\+=.\+/d'    \
            -e '/.\+!=.\+/d'   \
            -e '/.\+<.\+/d'    \
            -e '/.\+>.\+/d'    \
            -e '/.\+>.\+/d'    \
            -e '/![ ]*.\+/d'   \
            -e '/.\+[ ]+-a[ ]+.\+/d'   \
            -e '/.\+[ ]+-o[ ]+.\+/d'   \
            -e '/.\+[ ]+-eq[ ]+.\+/d'  \
            -e '/.\+[ ]+-ne[ ]+.\+/d'  \
            -e '/.\+[ ]+-lt[ ]+.\+/d'  \
            -e '/.\+[ ]+-le[ ]+.\+/d'  \
            -e '/.\+[ ]+-gt[ ]+.\+/d'  \
            -e '/.\+[ ]+-ge[ ]+.\+/d'  \
            -e '/^[ ]*pushd/d'  \
            -e '/^[ ]*unset/d'  \
            -e '/^EOF/d'  \
            -e '/^[ ]*__eof/d'  \
            -e '/cd[ ]\+.\+/d'  \
            -e '/".*xml-dtd.*"/d'  \
            -e '/[ ]*fi$/d' \
            -e '/\/etc\/.*$/d' \
            -e '/\/bin\/.*$/d' \
            -e '/\/usr\/lib.*$/d' \
            -e '/.*keygen.*$/d' \
            -e '/.*nfs-.*$/d' \
            -e '/[ ]*db\.set_profile/d' \
            -e '/[ ]*echo/d' \
            -e '/[\b\t]*echo/d' \
            -e '/[\b\t]*sleep/d' \
            -e '/.\+[\b\t]*;;/d' \
            -e '/*)/d' \
            -e '/.*\/d' \
            | sed -e '/^[ ]*$/d'
    fi
done

