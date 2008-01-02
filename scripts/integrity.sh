#!/bin/sh -x

#   preparation
GNUPGHOME="`pwd`/.integrity"
export GNUPGHOME
if [ ! -d $GNUPGHOME ]; then
    mkdir $GNUPGHOME
    chmod 700 $GNUPGHOME
fi

#   set name of signer
integrity_signer_name="Integrity-Authority"
integrity_signer_mail="integrity-authority@example.com"

#   generate signing key pair
gpg --quiet --gen-key --batch <<EOT
Key-Type:     DSA
Key-Length:   1024
Key-Usage:    sign
Name-Real:    $integrity_signer_name
Name-Email:   $integrity_signer_mail
%commit
EOT

#   (re-)clearsign the integrity specification and processor   
clearsign () {
    perl -e '
        my $txt; { local $/; $txt = <STDIN>; }
        $txt =~ s/
            ^
            \s*
            -----BEGIN\s+PGP\s+SIGNED\s+MESSAGE-----
            .*?
            \r?\n
            \r?\n
            (.+?\r?\n)
            -----BEGIN\s+PGP\s+SIGNATURE-----\r?\n
            .*
            $
        /$1/xs;
        print $txt;
    ' <$1 >$1.tmp && \
    gpg --quiet --batch --clearsign --output - $1.tmp >$1
    rm -f $1.tmp
}
clearsign integrity.cfg
clearsign integrity.lua

#   export signing public key
gpg --quiet --export --armor "$integrity_signer_name" >integrity.pgp

#   determine signing key fingerprint
gpg --quiet --fingerprint "Integrity-Authority" 2>&1 |\
    grep "Key fingerprint" | sed -e 's;.*Key fingerprint = ;;' -e 's;[^A-Z0-9];;g' \
    >integrity.fp

#   cleanup
rm -rf $GNUPGHOME

