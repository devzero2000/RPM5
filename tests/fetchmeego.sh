#!/bin/sh

wget=/usr/bin/wget

uri="http://repo.meego.com/MeeGo/builds/1.0.99/daily/core/repos/ia32/packages/"
manifest=meego-everything.manifest

for A in i586 i686 noarch; do
  $wget $uri/$A
  sed -e "s,^<img.*href=\",${uri}${A}/," -e 's,".*,,' < index.html | \
  grep '^http:' >> $manifest
  rm -f index.html
done

wc $manifest


