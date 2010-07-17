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
\nid-rpmTag OBJECT IDENTIFIER ::=\
\n  { iso(1) org(3) dod(6) internet(1) private(4) enterprise(1)\
\n		rpm(36106) tag(1) }\
\n\
\nVisibleString		::= [UNIVERSAL 26] IMPLICIT OCTET STRING\
\nUTF8String		::= [UNIVERSAL 12] IMPLICIT OCTET STRING\
\n\
\nRPM_UINT8_TYPE	::= INTEGER (0..255)\
\nRPM_UINT16_TYPE	::= INTEGER (0..65535)\
\nRPM_UINT32_TYPE	::= INTEGER (0..4294967295)\
\nRPM_UINT64_TYPE	::= INTEGER (0..18446744073709551615)\
\nRPM_STRING_TYPE	::= VisibleString\
\nRPM_BIN_TYPE		::= OCTET STRING\
\nRPM_STRING_ARRAY_TYPE	::= VisibleString\
\nRPM_I18NSTRING_TYPE	::= UTF8String\
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
    printf("\nid-%s\tOBJECT_IDENTIFIER ::= { id-rpmTag %d }", tagN, tagNum)
  }

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

  printf("\n%s ::= %sRPM_%s_TYPE", tagN, ta, tt);

}

END {

  printf("\
\nPackage ::= SEQUENCE {\
\n  name		Name,\
\n  epoch		Epoch,\
\n  version		Version,\
\n  release		Release,\
\n  summary		Summary,\
\n  description		Description,\
\n  buildtime		Buildtime,\
\n  buildhost		Buildhost,\
\n  size		Size,\
\n  license		License,\
\n  group		Group,\
\n  os			Os,\
\n  arch		Arch,\
\n  filesizes		Filesizes,\
\n  filemodes		Filemodes,\
\n  filerdevs		Filerdevs,\
\n  filemtimes		Filemtimes,\
\n  filedigests		Filedigests,\
\n  filelinktos		Filelinktos,\
\n  fileflags		Fileflags,\
\n  fileusername	Fileusername,\
\n  filegroupname	Filegroupname,\
\n  sourcerpm		Sourcerpm,\
\n  fileverifyflags	Fileverifyflags,\
\n  archivesize		Archivesize,\
\n  providename		Providename,\
\n  requireflags	Requireflags,\
\n  requirename		Requirename,\
\n  requireversion	Requireversion,\
\n  rpmversion		Rpmversion,\
\n  changelogtime	Changelogtime,\
\n  changelogname	Changelogname,\
\n  changelogtext	Changelogtext,\
\n  cookie		Cookie,\
\n  filedevices		Filedevices,\
\n  fileinodes		Fileinodes,\
\n  filelangs		Filelangs,\
\n  provideflags	Provideflags,\
\n  provideversion	Provideversion,\
\n  dirindexes		Dirindexes,\
\n  basenames		Basenames,\
\n  dirnames		Dirnames,\
\n  optflags		Optflags,\
\n  payloadformat	Payloadformat,\
\n  payloadcompressor	Payloadcompressor,\
\n  payloadflags	Payloadflags,\
\n  platform		Platform,\
\n  filecolors		Filecolors,\
\n  fileclass		Fileclass,\
\n  classdict		Classdict,\
\n  filedependsx	Filedependsx,\
\n  filedependsn	Filedependsn,\
\n  dependsdict		Dependsdict,\
\n  filecontexts	Filecontexts,\
\n  filedigestalgos	Filedigestalgos,\
\n  rpmlibversion	Rpmlibversion,\
\n  rpmlibtimestamp	Rpmlibtimestamp,\
\n  rpmlibvendor	Rpmlibvendor\
\n}\
");

  printf("\nEND\n");
}

' < rpmtag.h
