#!/bin/sh

wget=/usr/bin/wget

$wget -O- http://download.idms-linux.org/trunk/i586/groupmap | grep ^base | 
while read af; do
  RPM="`echo $af | cut -d: -f2`"
  echo "http://download.idms-linux.org/trunk/i586/RPMS/$RPM"
done | sort -u | grep -v 'perl-PAR-Packer'

