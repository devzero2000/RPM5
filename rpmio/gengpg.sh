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

str="Signed\n"
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

echo "static const char * ECDSAsig ="
cat << GO_SYSIN_DD | sed -e'1,3d; $d' | sed -e's/^/"/; s/$/\\n"/; $d'
-----BEGIN PGP SIGNATURE-----
Version: PGP SDK 3.11.0

iQBXAwUBSMjHk4QJEy1tIUM0EwhkLQEAw+SAPNMClecbCzvb8Rpx6fov5eJ3ShVs
gAtyf/w/IiwBAJNz/QD/qtHN13o1rlrJKlCXrbnfEsVl5gkMIULH+eEU
=uWBi
-----END PGP SIGNATURE-----
GO_SYSIN_DD
echo ";"

echo "static const char * ECDSApub ="
cat << GO_SYSIN_DD | sed -e'1,3d; $d' | sed -e's/^/"/; s/$/\\n"/; $d'
-----BEGIN PGP PUBLIC KEY BLOCK-----
Version: PGP SDK 3.11.0

mQBSBEjIvdoTCCqGSM49AwEHAgMEilTubmrCj7X11p6CxMO+Ifg34Bzp8UZdHh5V
40oEoS7qcjFQ0e3gjWB9DfSkiTKLaOwhDBttR+Hw8TG/v4YtubQiRUNDIFAtMjU2
IDxlY2MtcC0yNTZAYnJhaW5odWIuY29tPokArAQQEwIAVAUCSMi92gUJCWYBgDAU
gAAAAAAgAAdwcmVmZXJyZWQtZW1haWwtZW5jb2RpbmdAcGdwLmNvbXBncG1pbWUD
CwkHAhkBBRsBAAAABR4BAAAAAxUICgAKCRAY1dFUqLaa54AuAQCgn5OR4KLbGzGS
4y223be1JC0KTK2GWCuJFIOE7g+llQEAgXbpaNqOcwyOV4PDdZq5Q0bP/QAIhkh8
hSF5N742KlS5AFUESMi92xIIKoZIzj0DAQcBCAcCAwRcohj5GzIlKZaKZuPqmr9l
B4jGEMvzgCglI1syEkJitvoOrR7dpAx/n56WBcMez3GPApv4ikmSGI3qB7HoqOep
iQBqBBgTAgASBQJIyL3bBQkJZgGABRsMAAAAAAoJEBjV0VSotprnk6oBAOetPj4t
5swFTcqfMZ3R4vh1RQ9k+tzlC36u8asprtxaAQCPH8HR2z/tggdsZKpdCtZyv3zn
GRTDr8dsiFqbp9mlIbkAUgRIyL3cEwgqhkjOPQMBBwIDBLUeFv+pPbEPmHfhqywM
weyvlMpIv8q/bwOhoU4hiUs+RMbrUx4tpe73T5kFKCa+9lmeurujkjx+63qpPPd2
ptyJAMoEGBMCAHIFAkjIvd0FCQlmAYAFGwIAAABfIAQZEwgABgUCSMi93AAKCRCE
CRMtbSFDNClhAQCxtGwZqqhYTw7GX1cEtxbYsWOD9ZkocZiU+x+ryCfqBgEA+WQN
Wfw5sfwG0GRScbPwFmkjlMnKwhuFK+g2V45+124ACgkQGNXRVKi2mufzNQEAuLNz
FeS/YwMWhTaedMduGYJG/sNxZhR+lVAgYNSy/FcA/RbDyY6W4C7geTB5iuTicfuK
cJmScU/alaH8fgp3uja1
=FfP1
-----END PGP PUBLIC KEY BLOCK-----
GO_SYSIN_DD
echo ";"

rm -rf $hdir

