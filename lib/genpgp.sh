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

str="This is the plaintext"
echo "This is the plaintext" > plaintext

echo "static const char * plaintext = \"$str\";"
echo "static const char * plaintextfn = \"plaintext\";"

$gpg --detach-sign -u DSApub --output - plaintext > DSA.sig
$gpg --detach-sign -a -u DSApub --output - plaintext > DSA.sigpem
$gpg --clearsign -u DSApub --output - plaintext > DSA.pem
$gpg --export DSApub > DSA.pub
$gpg --export -a DSApub > DSA.pubpem

echo "static const char * DSAsig = \"DSA.sig\";"
echo "static const char * DSAsigpem = \"DSA.sigpem\";"
echo "static const char * DSApem = \"DSA.pem\";"
echo "static const char * DSApub = \"DSA.pub\";"
echo "static const char * DSApubpem = \"DSA.pubpem\";"

$gpg --detach-sign -u RSApub --output - plaintext > RSA.sig
$gpg --detach-sign -a -u RSApub --output - plaintext > RSA.sigpem
$gpg --clearsign -u RSApub --output - plaintext > RSA.pem
$gpg --export RSApub > RSA.pub
$gpg --export -a RSApub > RSA.pubpem

echo "static const char * RSAsig = \"RSA.sig\";"
echo "static const char * RSAsigpem = \"RSA.sigpem\";"
echo "static const char * RSApem = \"RSA.pem\";"
echo "static const char * RSApub = \"RSA.pub\";"
echo "static const char * RSApubpem = \"RSA.pubpem\";"
