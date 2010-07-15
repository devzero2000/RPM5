#!/bin/sh

AWK=awk

$AWK '
BEGIN {
  printf("\
rpmTagData\
\n	{ iso org(3) dod(6) internet(1) private(4) enterprise(1)\
\n		rpm(36106) tag(1) }\
\n\
\nDEFINITIONS EXPLICIT TAGS ::=\
\nBEGIN\
\n\
\nRPM_UINT8_TYPE	::= INTEGER (0..255)\
\nRPM_UINT16_TYPE	::= INTEGER (0..65535\
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
    printf("\nid-%s\tOBJECT_IDENTIFIER ::= { rpmTagData %d }", tagN, tagNum)
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
  printf("\nEND\n");
}

' < rpmtag.h
