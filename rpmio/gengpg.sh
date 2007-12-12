#!/bin/sh

hdir="`pwd`/.gnupg"
gpg="gpg --homedir $hdir"

rm -rf $hdir

$gpg --gen-key --batch << GO_SYSIN_DD
Key-Type: DSA
Key-Length: 1024
Key-Usage: sign
Name-Real: DSApub
Name-Comment: 1024
Name-Email: jbj@jbj.org
%commit
Key-Type: RSA
Key-Length: 1024
Key-Usage: sign
Name-Real: RSApub
Name-Comment: 1024
Name-Email: jbj@jbj.org
%commit
GO_SYSIN_DD

str="abc"
echo "static const char * str = \"$str\";"

echo "static const char * DSApub ="
$gpg --export -a -u DSApub | sed -e'1,3d; $d' | sed -e's/^/"/; s/$/\\n"/; $d'
echo ";"

echo "static const char * DSAsig ="
echo -n "$str" | $gpg -sab -u DSApub | sed -e'1,3d; $d' | sed -e's/^/"/; s/$/\\n"/; $d'
echo ";"

echo "static const char * RSApub ="
$gpg --export -a -u RSApub | sed -e'1,3d; $d' | sed -e's/^/"/; s/$/\\n"/; $d'
echo ";"

echo "static const char * RSAsig ="
echo -n "$str" | $gpg -sab -u RSApub | sed -e'1,3d; $d' | sed -e's/^/"/; s/$/\\n"/; $d'
echo ";"
