
STORE=/home/carl/.config/dcpomatic/crypt

xmlsec1 verify --pubkey-cert-pem $STORE/leaf.signed.pem --trusted-pem $STORE/intermediate.signed.pem --trusted-pem $STORE/ca.self-signed.pem --id-attr:Id http://www.smpte-ra.org/schemas/430-3/2006/ETM:AuthenticatedPublic --id-attr:Id http://www.smpte-ra.org/schemas/430-3/2006/ETM:AuthenticatedPrivate  foo.xml
#dcpo.kdm.xml