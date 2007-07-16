##
##  devtool.bashrc -- Development Tool GNU Bash convenience function
##  Copyright (c) 2001-2007 Ralf S. Engelschall <rse@engelschall.com>
##

devtool () {
    workdir=`pwd`
    basedir=$workdir
    while [ ".$basedir" != . ]; do
        if [ -f "$basedir/devtool" ]; then
            break
        else
            basedir=`echo "${basedir}" | sed -e 's;[^/]*$;;' -e 's;/*$;;'`
        fi
    done
    if [ ".$basedir" = . ]; then
        echo "devtool: sorry, you are staying outside any \"devtool\" controlled area"
    else
        platform=`(shtool platform -n -L -S "" -C "+" -F '%<ap>-%<sp>' || uname -s) 2>/dev/null`
        if [ ".$1" = ".setup-platform" ]; then
            if [ ! -d "${basedir}/.devtool/${platform}" ]; then
                shtool mkdir -f -p -m 755 "${basedir}/.devtool/${platform}"
            fi
        else
            subdir=`echo "$workdir" | sed -e "s;^$basedir/*;;"`
            (   if [ -d "${basedir}/.devtool/${platform}/${subdir}" ]; then
                    cd "${basedir}/.devtool/${platform}/${subdir}"
                    $basedir/devtool "$@" | tee "${basedir}/.devtool/${platform}/devtool.log"
                else
                    $basedir/devtool "$@" | tee "${basedir}/devtool.log"
                fi
            ) || return $?
        fi
    fi
}

