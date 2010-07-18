#!/bin/sh

AWK=awk

$AWK '
BEGIN {
  printf("\
rpmModule\
\n  { iso(1) org(3) dod(6) internet(1) private(4) enterprise(1) rpm(36106) }\
\n\
\nDEFINITIONS EXPLICIT TAGS ::=\
\nBEGIN\
\n\
\nid-rpm-tag OBJECT IDENTIFIER ::=\
\n  { iso(1) org(3) dod(6) internet(1) private(4) enterprise(1)\
\n		rpm(36106) tag(1) }\
\n\
\n-- FIXME: these are not right and should be in libtasn1 not here\
\nUTF8String            ::= [UNIVERSAL 12] IMPLICIT OCTET STRING\
\nNumericString         ::= [UNIVERSAL 18] IMPLICIT OCTET STRING\
\nPrintableString       ::= [UNIVERSAL 19] IMPLICIT OCTET STRING\
\nTeletexString         ::= [UNIVERSAL 20] IMPLICIT OCTET STRING\
\nVideotexString        ::= [UNIVERSAL 21] IMPLICIT OCTET STRING\
\nIA5String             ::= [UNIVERSAL 22] IMPLICIT OCTET STRING\
\n-- UTCTime              ::= [UNIVERSAL 23] IMPLICIT OCTET STRING\
\n-- GeneralizedTime      ::= [UNIVERSAL 24] IMPLICIT OCTET STRING\
\nGraphicString         ::= [UNIVERSAL 25] IMPLICIT OCTET STRING\
\nVisibleString         ::= [UNIVERSAL 26] IMPLICIT OCTET STRING\
\n-- GeneralString         ::= [UNIVERSAL 27] IMPLICIT OCTET STRING\
\nUniversalString       ::= [UNIVERSAL 28] IMPLICIT OCTET STRING\
\nBMPString             ::= [UNIVERSAL 30] IMPLICIT OCTET STRING\
\n\
\n-- RPM tag data primitive types\
\nRPM_UINT8_TYPE        ::= INTEGER (0..255)\
\nRPM_UINT16_TYPE       ::= INTEGER (0..65535)\
\nRPM_UINT32_TYPE       ::= INTEGER (0..4294967295)\
\nRPM_UINT64_TYPE       ::= INTEGER (0..18446744073709551615)\
\nRPM_STRING_TYPE       ::= VisibleString\
\nRPM_BIN_TYPE          ::= OCTET STRING\
\nRPM_STRING_ARRAY_TYPE ::= VisibleString\
\nRPM_I18NSTRING_TYPE   ::= UTF8String\
\n\
\n-- RPM tag data new types\
\nRPM_UTCTIME_TYPE      ::= [UNIVERSAL 23] IMPLICIT OCTET STRING\
\nRPM_TIME_TYPE         ::= [UNIVERSAL 24] IMPLICIT OCTET STRING\
\n\
\n-- RPM tag OIDs and data typing\
");
}

/[ \t](RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ {
  tt = "NULL"; ta = "ANY";

  tagOid = substr($1, length("RPMTAG_")+1);
  tagN = toupper(substr(tagOid, 1, 1)) tolower(substr(tagOid,2));
  tagNum = $3 + 0;
  if (tagNum == 0) {
	next;
  }

  if ($2 == "=") {
    printf("\nid-rpm-tag-%s\tOBJECT_IDENTIFIER ::= { id-rpm-tag %d }", tagN, tagNum)
  }

  tz = "";
  if ($5 == "c")	{
    tt = "UINT8";	ta = "";
  }
  if ($5 == "c[]")	{
    tt = "UINT8";	ta = "SEQUENCE OF ";
  }
  if ($5 == "h")	{
    tt = "UINT16";	ta = "";
  }
  if ($5 == "h[]")	{
    tt = "UINT16";	ta = "SEQUENCE OF ";
  }
  if ($5 == "i")	{
    tt = "UINT32";	ta = "";
  }
  if ($5 == "i[]")	{
    tt = "UINT32";	ta = "SEQUENCE OF ";
  }
  if ($5 == "l")	{
    tt = "UINT64";	ta = "";
  }
  if ($5 == "l[]")	{
    tt = "UINT64";	ta = "SEQUENCE OF ";
  }
  if ($5 == "s")	{
    tt = "STRING";	ta = "";
  }
  if ($5 == "s[]")	{
    tt = "STRING_ARRAY"; ta = "SEQUENCE OF ";
  }
  if ($5 == "s{}")	{
    tt = "I18NSTRING";	ta = "";
  }
  if ($5 == "x")	{
    tt = "BIN";		ta = "";
  }

  if (tagN == "Filerdevs") {
    tz = " DEFAULT 0";
  }
  if (tagN == "Fileflags") {
    tz = " DEFAULT 0";
  }
  if (tagN == "Fileverifyflags") {
    tz = " DEFAULT 0xffffffff";
  }

  if (tagN == "Buildtime") {
    tt = "UTCTIME";	ta = "";
  }
  if (tagN == "Changelogtime") {
    tt = "UTCTIME";	ta = "SEQUENCE OF ";
  }

  printf("\n%s ::= %sRPM_%s_TYPE", tagN, ta, tt, tz);
  printf("\n");

}

END {

  printf("\
\n\
\n-- RPM package header (from file)\
\nPackage ::= SEQUENCE {\
\n  name                Name,\
\n  epoch               Epoch           DEFAULT 0,\
\n  version             Version,\
\n  release             Release,\
\n  summary             Summary,\
\n  description         Description,\
\n  buildtime           Buildtime,\
\n  buildhost           Buildhost,\
\n  size                Size,\
\n  license             License,\
\n  group               Group,\
\n  os                  Os,\
\n  arch                Arch,\
\n  filesizes           Filesizes,\
\n  filemodes           Filemodes,\
\n  filerdevs           Filerdevs,\
\n  filemtimes          Filemtimes,\
\n  filedigests         Filedigests,\
\n  filelinktos         Filelinktos,\
\n  fileflags           Fileflags,\
\n  fileusername        Fileusername,\
\n  filegroupname       Filegroupname,\
\n  sourcerpm           Sourcerpm,\
\n  fileverifyflags     Fileverifyflags,\
\n  archivesize         Archivesize,\
\n  providename         Providename,\
\n  requireflags        Requireflags,\
\n  requirename         Requirename,\
\n  requireversion      Requireversion,\
\n  rpmversion          Rpmversion,\
\n  changelogtime       Changelogtime,\
\n  changelogname       Changelogname,\
\n  changelogtext       Changelogtext,\
\n  cookie              Cookie,\
\n  filedevices         Filedevices,\
\n  fileinodes          Fileinodes,\
\n  filelangs           Filelangs,\
\n  provideflags        Provideflags,\
\n  provideversion      Provideversion,\
\n  dirindexes          Dirindexes,\
\n  basenames           Basenames,\
\n  dirnames            Dirnames,\
\n  optflags            Optflags,\
\n  payloadformat       Payloadformat,\
\n  payloadcompressor   Payloadcompressor,\
\n  payloadflags        Payloadflags,\
\n  platform            Platform,\
\n  filecolors          Filecolors,\
\n  fileclass           Fileclass,\
\n  classdict           Classdict,\
\n  filedependsx        Filedependsx,\
\n  filedependsn        Filedependsn,\
\n  dependsdict         Dependsdict,\
\n  filecontexts        Filecontexts,\
\n  filedigestalgos     Filedigestalgos,\
\n  rpmlibversion       Rpmlibversion,\
\n  rpmlibtimestamp     Rpmlibtimestamp,\
\n  rpmlibvendor        Rpmlibvendor\
\n}\
");

  printf("\nEND\n");
}

' < rpmtag.h
