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

$gpg --detach-sign -a -u DSApub --output - plaintext > DSA.sig
$gpg --clearsign -u DSApub --output - plaintext > DSA.pem
$gpg --export -a DSApub > DSA.pub

echo "static const char * DSAsig = \"DSA.sig\";"
echo "static const char * DSApem = \"DSA.pem\";"
echo "static const char * DSApub = \"DSA.pub\";"

$gpg --detach-sign -a -u RSApub --output - plaintext > RSA.sig
$gpg --clearsign -u RSApub --output - plaintext > RSA.pem
$gpg --export -a RSApub > RSA.pub

echo "static const char * RSAsig = \"RSA.sig\";"
echo "static const char * RSApem = \"RSA.pem\";"
echo "static const char * RSApub = \"RSA.pub\";"
