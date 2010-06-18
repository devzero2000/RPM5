#!/bin/sh

top="`pwd`/tmp"
hdir="$top/.gnupg"
plaintext="$top/plaintext"
DSA="$top/DSA"
RSA="$top/RSA"

gpg="gpg2 --homedir $hdir"

rm -rf $hdir
mkdir -p $hdir
chmod go-rwx $hdir

$gpg --batch --debug-quick-random --gen-key << GO_SYSIN_DD
Key-Type: DSA
Key-Length: 1024
Key-Usage: sign
Name-Real: Donald
Name-Comment: 1024
Name-Email: rpm-devel@rpm5.org
Expire-Date: 1
%commit
Key-Type: RSA
Key-Length: 4096
Key-Usage: sign,encrypt
Name-Real: Ronald
Name-Comment: 4096
Name-Email: rpm-devel@rpm5.org
Expire-Date: 1
%commit
GO_SYSIN_DD

str="test"

# Note carefully the trailing white space on 1st line below: "${str}       "
# $ od -c plaintext 
# 0000000   t   e   s   t                              \n   t   e   s   t
# 0000020  \n
# 0000021
cat << GO_SYSIN_DD > $plaintext
${str}       
${str}
GO_SYSIN_DD

echo "static const char * plaintextfn = \"$plaintext\";"

$gpg --detach-sign -u Donald --output - $plaintext > ${DSA}.sig
$gpg --detach-sign -a -u Donald --output - $plaintext > ${DSA}.sigpem
$gpg --clearsign -u Donald --output - $plaintext > ${DSA}.pem
$gpg --export Donald > ${DSA}.pub
$gpg --export -a Donald > ${DSA}.pubpem

echo "static const char * DSAsig = \"${DSA}.sig\";"
echo "static const char * DSAsigpem = \"${DSA}.sigpem\";"
echo "static const char * DSApem = \"${DSA}.pem\";"
echo "static const char * DSApub = \"${DSA}.pub\";"
echo "static const char * DSApubpem = \"${DSA}.pubpem\";"
echo "static const char * DSApubid = \"`$gpg --fingerprint DSApub | grep 'finger' | sed -e 's/.*print = //' -e 's/ //g'`\";"

$gpg --detach-sign -u Ronald --output - $plaintext > ${RSA}.sig
$gpg --detach-sign -a -u Ronald --output - $plaintext > ${RSA}.sigpem
$gpg --clearsign -u Ronald --output - $plaintext > ${RSA}.pem
$gpg --export Ronald > ${RSA}.pub
$gpg --export -a Ronald > ${RSA}.pubpem

echo "static const char * RSAsig = \"${RSA}.sig\";"
echo "static const char * RSAsigpem = \"${RSA}.sigpem\";"
echo "static const char * RSApem = \"${RSA}.pem\";"
echo "static const char * RSApub = \"${RSA}.pub\";"
echo "static const char * RSApubpem = \"${RSA}.pubpem\";"
echo "static const char * RSApubid = \"`$gpg --fingerprint RSApub | grep 'finger' | sed -e 's/.*print = //' -e 's/ //g'`\";"
