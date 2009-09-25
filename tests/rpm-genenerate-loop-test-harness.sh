#!/bin/bash
#
##########################################################
#  Generate an rpm spec file for testing looping issue
#
#  rpm-genenerate-loop-test-harness.sh  -s <namespec> -m <max direct Requires clauses> -p <number of packages>
#
#  Copyright (C) 2006 Elia Pinto (devzero2000@rpm5.org)
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
#
##########################################################
# OK: it is really a toy script, written in JAPH style
# to generate pathological rpm loop dependencies
##########################################################
##############################
# Useful (?) function
##############################
Die() {
 echo "$_PROGNAME: ERROR: $@" >&2
 exit 1
}
Info() {
 echo "$_PROGNAME: INFO: $@" >&2
}
Warn() {
 echo "$_PROGNAME: WARNING: $@" >&2
}
Usage() {
 echo "Usage: $_PROGNAME -s <filespec> -m <max direct Requires clauses> -p <number of packages>" >&2
 exit 1
}
#################################
# Argument Check
#################################
readonly _PROGNAME=${0##*/}
while getopts "m:s:p:" opt; do
	case "$opt" in
	m)	if [ -z "$_MAX_REQUIRES" ]
                then   _MAX_REQUIRES=$OPTARG
                else
                     Usage
                fi;;
	s)	if [ -z "$_SPEC_FILE_NAME" ]
                then  readonly _SPEC_FILE_NAME=$OPTARG
                else
                     Usage
                fi;;
	p)	if [ -z "$_COUNTPKG" ]
                then  readonly _COUNTPKG=$OPTARG
                else
                     Usage
                fi;;
	*)	Usage;;
        \?)     Usage;;
	esac
done
[ -z "${_MAX_REQUIRES}" -o -z "$_SPEC_FILE_NAME" -o -z "$_COUNTPKG" ] && Usage

readonly _PKGNAME=${_SPEC_FILE_NAME%%\.*}
readonly _SPECFILE=${_PKGNAME}.spec

[ -e ${_SPECFILE} ] && Die "${_SPECFILE} exists. Remove it first"


####################
# Hardcoded
###################
readonly _VERSION=1.0
readonly _RELEASE=1

if [ ${_MAX_REQUIRES} -gt ${_COUNTPKG} ]
then
 _MAX_REQUIRES="${_COUNTPKG}"
fi
#readonly _MAX_REQUIRES

cat <<EOF> $_SPECFILE
##############################################
# Created by ${_PROGNAME}
# with 
# - max direct Requires: ${_MAX_REQUIRES}
# - Number of Packages : ${_COUNTPKG}
##############################################
Summary: Loop Ordering Test Harness
Name: ${_PKGNAME}
BuildArch: noarch
Version: ${_VERSION}
Release: ${_RELEASE}
License: LGPLv3
Group: Applications/System 
Buildroot: %{_tmppath}/%{name}-tmp
URL: http://rpm5.org

%description

A toy loop rpm test harness. 

This package(s) generate pathological rpm loop dependencies

For an example see rhbz#437041

EOF

#############################
_c=1
while [ $_c -le $_COUNTPKG ]
do
cat <<EOF >> $_SPECFILE

%package $_c
Summary: $_PKGNAME-$_c
Group: Applications/Security
BuildArch: noarch
EOF
# 
count=1
number=0
oldnumber=0
prec=0
succ=0
while [ "$count" -le ${_MAX_REQUIRES} ]      
do
  number=$RANDOM
  number_in_range=$RANDOM
  range=10
  let "count += 1"  # Increment count.
  let "number_in_range %= range"  # Scales $number down within range.
  let "number %= $_COUNTPKG"      # Scales $number down within $_COUNTPKG.
  let "number += 1"               # Increment number.
  [ $number -lt 1 ] &&  continue
  [ $number -eq $oldnumber ] && continue
  #### XXX
  #### prec 
  let "prec = count - 1"  
  let "succ = count + 1"  
  if [ $prec -gt 1 -a $prec -ne $number -a $prec -lt ${_COUNTPKG} ] 
  then
  [ $prec -ne ${_c} ] && 
    cat <<EOF >> $_SPECFILE
Requires: ${_PKGNAME}-${prec}
EOF
  [ $number_in_range = 0 -a $succ -lt ${_COUNTPKG} -a $succ -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires(pre): ${_PKGNAME}-${succ}
EOF
}
  [ $number_in_range = 1 -a $succ -lt ${_COUNTPKG} -a $succ -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires(post): ${_PKGNAME}-${succ}
EOF
}
  [ $number_in_range = 2 -a $succ -lt ${_COUNTPKG} -a $number -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires(postun): ${_PKGNAME}-${number}
EOF
}
  [ $number_in_range = 3  -a $prec -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires: /tmp/${_PKGNAME}-${prec}
EOF
}
  [ $number_in_range = 4  -a $prec -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires: /tmp/${_PKGNAME}-${prec}_ln
EOF
}
  [ $number_in_range = 5  -a $prec -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires: /tmp/${_PKGNAME}-${prec}_dir
EOF
}
  fi
  
  if [ $succ -gt 1 -a $succ -ne $number -a $succ -lt ${_COUNTPKG} ] 
  then
  [ $succ -ne ${_c} ] && cat <<EOF >> $_SPECFILE
Requires: ${_PKGNAME}-${succ}
EOF
   [ $number_in_range = 4 -o $number_in_range = 8 -a $number -ne ${_c} ]  && 
  cat <<EOF >> $_SPECFILE
Requires: ${_PKGNAME}-${number}
EOF
   [ $number_in_range = 1 -o $number_in_range = 7 -a $prec -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires(post): ${_PKGNAME}-${prec}
EOF
}
   [ $number_in_range = 2 -o $number_in_range = 9 -a $succ -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires(postun): ${_PKGNAME}-${succ}
EOF
}
   [ $number_in_range = 3  -a $succ -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires: /tmp/${_PKGNAME}-${succ}
EOF
}
   [ $number_in_range = 5  -a $succ -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires: /tmp/${_PKGNAME}-${succ}_ln
EOF
}
   [ $number_in_range = 6  -a $succ -ne ${_c} ] && { 
  cat <<EOF >> $_SPECFILE
Requires: /tmp/${_PKGNAME}-${succ}_dir
EOF
}
  fi
oldnumber=${number}
done

cat <<EOF >> $_SPECFILE
%description ${_c}

${_PKGNAME}-${_c}
EOF
((_c++))
done
######################
cat <<"EOF" >> $_SPECFILE
%prep
%setup -q -c -T

%build
cat <<E >README
%{description}
E



%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/tmp
cd %{buildroot}/tmp
EOF
#############################
_c=1
while [ $_c -le $_COUNTPKG ]
do
cat <<EOF >> $_SPECFILE
echo "${_PKGNAME}-${_c}" > ${_PKGNAME}-${_c}
mkdir ${_PKGNAME}-${_c}_dir
ln -s  ${_PKGNAME}-${_c} ${_PKGNAME}-${_c}_ln
EOF
((_c++))
done

cat <<EOF >> $_SPECFILE
%clean
rm -rf %{buildroot}
EOF

_c=1
while [ $_c -le $_COUNTPKG ]
do
cat <<EOF >> $_SPECFILE
%files $_c
%defattr (-,root,root)
%doc README
/tmp/${_PKGNAME}-${_c}
/tmp/${_PKGNAME}-${_c}_dir
/tmp/${_PKGNAME}-${_c}_ln
EOF
((_c++))
done
_d=$(LANG=C date +"%a %b %d %Y")
cat <<EOF >> $_SPECFILE
%changelog
* $_d - Elia Pinto <devzero2000@rpm5.org> - $_VERSION-$_RELEASE
- First Build
EOF
Info "Wrote ${_SPECFILE}"
