#!/usr/bin/perl
###########################################################################
# ABI Compliance Checker (ACC) 1.93.7
# A tool for checking backward binary compatibility of a C/C++ library API
#
# Copyright (C) 2011 Institute for System Programming, RAS
# Written by Andrey Ponomarenko
#
# PLATFORMS
# =========
#  Linux, FreeBSD, Mac OS X, Haiku, MS Windows, Symbian
#
# REQUIREMENTS
# ============
#  Linux
#    - GCC (3.0-4.6.0, recommended 4.5 or newer)
#    - GNU Binutils (readelf, c++filt, objdump)
#    - Perl (5.8-5.14)
#
#  Mac OS X
#    - Xcode (gcc, otool, c++filt)
#
#  MS Windows
#    - MinGW (gcc.exe, c++filt.exe)
#    - Active Perl
#    - MS Visual C++ (dumpbin.exe, undname.exe, cl.exe)
#    - Sigcheck v1.71 or newer
#    - Add gcc.exe path (C:\MinGW\bin\) to your system PATH variable
#    - Run vsvars32.bat (C:\Microsoft Visual Studio 9.0\Common7\Tools\)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License or the GNU Lesser
# General Public License as published by the Free Software Foundation,
# either version 2 of the Licenses, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# and the GNU Lesser General Public License along with this program.
# If not, see <http://www.gnu.org/licenses/>.
###########################################################################
use Getopt::Long;
Getopt::Long::Configure ("posix_default", "no_ignore_case");
use File::Path qw(mkpath rmtree);
use File::Temp qw(tempdir);
use File::Copy qw(copy);
use Cwd qw(abs_path cwd);
use Data::Dumper;
use Config;

my $ABI_COMPLIANCE_CHECKER_VERSION = "1.93.7";
my $ABI_DUMP_VERSION = "2.4";
my $OLDEST_SUPPORTED_VERSION = "1.18";
my $ABI_DUMP_MAJOR = majorVersion($ABI_DUMP_VERSION);
my $OSgroup = get_OSgroup();
my $ORIG_DIR = cwd();
my $TMP_DIR = tempdir(CLEANUP=>1);
if($OSgroup eq "windows"
and not get_dirname($0)) {
    $0="./".$0;
}
my $TOOL_DIR = abs_path(get_dirname($0));

# Internal modules
my %RULES_PATH = (
    "Binary" => $TOOL_DIR."/modules/RulesBin.xml",
    "Source" => $TOOL_DIR."/modules/RulesSrc.xml");

my ($Help, $ShowVersion, %Descriptor, $TargetLibraryName, $GenDescriptor,
$TestTool, $DumpAPI, $SymbolsListPath, $CheckHeadersOnly_Opt, $UseDumps,
$CheckObjectsOnly_Opt, $AppPath, $StrictCompat, $DumpVersion, $ParamNamesPath,
%RelativeDirectory, $TargetLibraryFName, $TestDump, $CheckImplementation,
%TargetVersion, $InfoMsg, $UseOldDumps, %UsedDump, $CrossGcc, %OutputLogPath,
$OutputReportPath, $OutputDumpPath, $ShowRetVal, $SystemRoot_Opt, %DumpSystem,
$CmpSystems, $TargetLibsPath, $Debug, $CrossPrefix, $UseStaticLibs, $NoStdInc,
$TargetComponent_Opt, $TargetSysInfo, $TargetHeader);

my $CmdName = get_filename($0);
my %OS_LibExt = (
    "dynamic" => {
        "default"=>"so",
        "macos"=>"dylib",
        "windows"=>"dll",
        "symbian"=>"dso"
    },
    "static" => {
        "default"=>"a",
        "windows"=>"lib",
        "symbian"=>"lib"
    }
);

my %OS_Archive = (
    "windows"=>"zip",
    "default"=>"tar.gz"
);

my %ERROR_CODE = (
    # Compatible verdict
    "Compatible"=>0,
    "Success"=>0,
    # Incompatible verdict
    "Incompatible"=>1,
    # Undifferentiated error code
    "Error"=>2,
    # System command is not found
    "Not_Found"=>3,
    # Cannot access input files
    "Access_Error"=>4,
    # Cannot compile header files
    "Cannot_Compile"=>5,
    # Header compiled with errors
    "Compile_Error"=>6,
    # Invalid input ABI dump
    "Invalid_Dump"=>7,
    # Incompatible version of ABI dump
    "Dump_Version"=>8,
    # Cannot find a module
    "Module_Error"=>9
);

my %CODE_ERROR = reverse(%ERROR_CODE);

my $ShortUsage = "ABI Compliance Checker (ACC) $ABI_COMPLIANCE_CHECKER_VERSION
A tool for checking backward binary compatibility of a C/C++ library API
Copyright (C) 2011 Institute for System Programming, RAS
License: GNU LGPLv2 or GNU GPLv2

Usage: perl $CmdName [options]
Example: perl $CmdName -lib NAME -d1 OLD.xml -d2 NEW.xml

OLD.xml and NEW.xml are XML-descriptors:

    <version>
        1.0
    </version>

    <headers>
        /path/to/headers/
    </headers>

    <libs>
        /path/to/libraries/
    </libs>

More info: perl $CmdName --help\n";

if($#ARGV==-1) {
    print $ShortUsage;
    exit(0);
}

foreach (2 .. $#ARGV)
{# correct comma separated options
    if($ARGV[$_-1] eq ",") {
        $ARGV[$_-2].=",".$ARGV[$_];
        splice(@ARGV, $_-1, 2);
    }
    elsif($ARGV[$_-1]=~/,\Z/) {
        $ARGV[$_-1].=$ARGV[$_];
        splice(@ARGV, $_, 1);
    }
    elsif($ARGV[$_]=~/\A,/
    and $ARGV[$_] ne ",") {
        $ARGV[$_-1].=$ARGV[$_];
        splice(@ARGV, $_, 1);
    }
}

GetOptions("h|help!" => \$Help,
  "i|info!" => \$InfoMsg,
  "v|version!" => \$ShowVersion,
  "dumpversion!" => \$DumpVersion,
# general options
  "l|lib|library=s" => \$TargetLibraryName,
  "d1|old|o=s" => \$Descriptor{1}{"Path"},
  "d2|new|n=s" => \$Descriptor{2}{"Path"},
  "dump|dump-abi|dump_abi=s" => \$DumpAPI,
  "old-dumps!" => \$UseOldDumps,
# extra options
  "d|descriptor-template!" => \$GenDescriptor,
  "app|application=s" => \$AppPath,
  "static-libs!" => \$UseStaticLibs,
  "cross-gcc=s" => \$CrossGcc,
  "cross-prefix=s" => \$CrossPrefix,
  "sysroot=s" => \$SystemRoot_Opt,
  "v1|version1|vnum=s" => \$TargetVersion{1},
  "v2|version2=s" => \$TargetVersion{2},
  "s|strict!" => \$StrictCompat,
  "symbols-list=s" => \$SymbolsListPath,
  "headers-only|headers_only!" => \$CheckHeadersOnly_Opt,
  "objects-only!" => \$CheckObjectsOnly_Opt,
  "check-implementation!" => \$CheckImplementation,
  "show-retval!" => \$ShowRetVal,
  "use-dumps!" => \$UseDumps,
  "nostdinc!" => \$NoStdInc,
  "dump-system=s" => \$DumpSystem{"Name"},
  "sysinfo=s" => \$TargetSysInfo,
  "cmp-systems!" => \$CmpSystems,
  "libs-list=s" => \$TargetLibsPath,
  "header=s" => \$TargetHeader,
# other options
  "test!" => \$TestTool,
  "test-dump!" => \$TestDump,
  "debug!" => \$Debug,
  "p|params=s" => \$ParamNamesPath,
  "relpath1|relpath=s" => \$RelativeDirectory{1},
  "relpath2=s" => \$RelativeDirectory{2},
  "dump-path=s" => \$OutputDumpPath,
  "report-path=s" => \$OutputReportPath,
  "log1-path|log-path=s" => \$OutputLogPath{1},
  "log2-path=s" => \$OutputLogPath{2},
  "l-full|lib-full=s" => \$TargetLibraryFName,
  "component=s" => \$TargetComponent_Opt
) or ERR_MESSAGE();

sub ERR_MESSAGE()
{
    print "\n".$ShortUsage;
    exit($ERROR_CODE{"Error"});
}

my $LIB_TYPE = $UseStaticLibs?"static":"dynamic";
my $SLIB_TYPE = $LIB_TYPE;
if($OSgroup!~/macos|windows/ and $SLIB_TYPE eq "dynamic")
{ # show as "shared" library
    $SLIB_TYPE = "shared";
}
my $LIB_EXT = $OS_LibExt{$LIB_TYPE}{$OSgroup};
if(not $LIB_EXT)
{ # default extension
    $LIB_EXT = $OS_LibExt{$LIB_TYPE}{"default"};
}
my $AR_EXT = $OS_Archive{$OSgroup};
if(not $AR_EXT) {
    $AR_EXT = $OS_Archive{"default"};
}
my $BYTE_SIZE = 8;

my $HelpMessage="
NAME:
  ABI Compliance Checker ($CmdName)
  Check backward binary compatibility of a $SLIB_TYPE C/C++ library API

DESCRIPTION:
  ABI Compliance Checker (ACC) is a tool for checking backward binary
  compatibility of a $SLIB_TYPE C/C++ library API. The tool checks header
  files and $SLIB_TYPE libraries (*.$LIB_EXT) of old and new versions and
  analyzes changes in Application Binary Interface (ABI=API+compiler ABI)
  that may break binary compatibility: changes in calling stack, v-table
  changes, removed symbols, etc. Binary incompatibility may result in
  crashing or incorrect behavior of applications built with an old version
  of a library if they run on a new one.

  The tool is intended for library developers and OS maintainers who are
  interested in ensuring binary compatibility, i.e. allow old applications
  to run with new library versions without the need to recompile. Also it
  can be used by ISVs for checking applications portability to new library
  versions. Found issues can be taken into account when adapting the
  application to a new library version.

  This tool is free software: you can redistribute it and/or modify it
  under the terms of the GNU LGPLv2 or GNU GPLv2.

USAGE:
  perl $CmdName [options]

EXAMPLE:
  perl $CmdName -lib NAME -d1 OLD.xml -d2 NEW.xml

  OLD.xml and NEW.xml are XML-descriptors:

    <version>
        1.0
    </version>

    <headers>
        /path1/to/header(s)/
        /path2/to/header(s)/
         ...
    </headers>

    <libs>
        /path1/to/library(ies)/
        /path2/to/library(ies)/
         ...
    </libs>

INFORMATION OPTIONS:
  -h|-help
      Print this help.

  -i|-info
      Print complete info.

  -v|-version
      Print version information.

  -dumpversion
      Print the tool version ($ABI_COMPLIANCE_CHECKER_VERSION) and don't do anything else.

GENERAL OPTIONS:
  -l|-lib|-library <name>
      Library name (without version).
      It affects only on the path and the title of the report.

  -d1|-old|-o <path(s)>
      Descriptor of 1st (old) library version.
      It may be one of the following:
      
         1. XML-descriptor (VERSION.xml file):

              <version>
                  1.0
              </version>

              <headers>
                  /path1/to/header(s)/
                  /path2/to/header(s)/
                   ...
              </headers>

              <libs>
                  /path1/to/library(ies)/
                  /path2/to/library(ies)/
                   ...
              </libs>

                 ... (XML-descriptor template
                         may be generated by -d option)
             
         2. Directory with headers and/or $SLIB_TYPE libraries
         3. RPM or DEB \"devel\" package
         4. Header file
         5. ".ucfirst($SLIB_TYPE)." library
         6. ABI dump (use -dump option to generate it)
         7. Comma separated list of headers and/or $SLIB_TYPE libraries
         8. Comma separated list of RPM or DEB packages

      If you are using an 2-8 descriptor types then you should
      specify version numbers with -v1 <num> and -v2 <num> options too.

      For more information, please see:
        http://ispras.linux-foundation.org/index.php/Library_Descriptor

  -d2|-new|-n <path(s)>
      Descriptor of 2nd (new) library version.

  -dump|-dump-abi <descriptor path(s)>
      Dump library ABI to gzipped TXT format file. You can transfer it
      anywhere and pass instead of the descriptor. Also it may be used
      for debugging the tool. Compatible dump versions: $ABI_DUMP_MAJOR.0<=V<=$ABI_DUMP_VERSION

  -old-dumps
      Enable support for old-version ABI dumps ($OLDEST_SUPPORTED_VERSION<=V<$ABI_DUMP_MAJOR.0).\n";

sub HELP_MESSAGE() {
    print $HelpMessage."
MORE INFO:
     perl $CmdName --info\n\n";
}

sub INFO_MESSAGE()
{
    print "$HelpMessage
EXTRA OPTIONS:
  -d|-descriptor-template
      Create XML-descriptor template ./VERSION.xml

  -app|-application <path>
      This option allows to specify the application that should be checked
      for portability to the new library version.

  -static-libs
      Check static libraries instead of the shared ones. The <libs> section
      of the XML-descriptor should point to static libraries location.

  -cross-gcc <path>
      Path to the cross GCC compiler to use instead of the usual (host) GCC.

  -cross-prefix <prefix>
      GCC toolchain prefix.

  -sysroot <dirpath>
      Specify the alternative root directory. The tool will search for include
      paths in the <dirpath>/usr/include and <dirpath>/usr/lib directories.

  -v1|-version1 <num>
      Specify 1st library version outside the descriptor. This option is needed
      if you have prefered an alternative descriptor type (see -d1 option).

      In general case you should specify it in the XML-descriptor:
          <version>
              VERSION
          </version>

  -v2|-version2 <num>
      Specify 2nd library version outside the descriptor.

  -s|-strict
      Treat all compatibility warnings as problems. Add a number of \"Low\"
      severity problems to the return value of the tool.

  -headers-only
      Check header files without $SLIB_TYPE libraries. It is easy to run, but may
      provide a low quality compatibility report with false positives and
      without detecting of added/removed symbols.
      
      Alternatively you can write \"none\" word to the <libs> section
      in the XML-descriptor:
          <libs>
              none
          </libs>

  -objects-only
      Check $SLIB_TYPE libraries without header files. It is easy to run, but may
      provide a low quality compatibility report with false positives and
      without analysis of changes in parameters and data types.

      Alternatively you can write \"none\" word to the <headers> section
      in the XML-descriptor:
          <headers>
              none
          </headers>

  -check-implementation
      Compare canonified disassembled binary code of $SLIB_TYPE libraries to
      detect changes in the implementation. Add \'Problems with Implementation\'
      section to the report.

  -show-retval
      Show the symbol's return type in the report.

  -symbols-list <path>
      This option allows to specify a file with a list of symbols (mangled
      names in C++) that should be checked, other symbols will not be checked.

  -use-dumps
      Make dumps for two versions of a library and compare dumps. This should
      increase the performance of the tool and decrease the system memory usage.

  -nostdinc
      Do not search the GCC standard system directories for header files.

  -dump-system <name> -sysroot <dirpath>
      Find all the shared libraries and header files in <dirpath> directory,
      create XML descriptors and make ABI dumps for each library. The result
      set of ABI dumps can be compared (--cmp-systems) with the other one
      created for other version of operating system in order to check them for
      compatibility. Do not forget to specify -cross-gcc option if your target
      system requires some specific version of GCC compiler (different from
      the host GCC). The system ABI dump will be generated to:
          sys_dumps/<name>/<arch>
          
  -dump-system <descriptor.xml>
      The same as the previous option but takes an XML descriptor of the target
      system as input, where you should describe it:
          
          /* Primary sections */
          
          <name>
              /* Name of the system */
          </name>
          
          <headers>
              /* The list of paths to header files and/or
                 directories with header files, one per line */
          </headers>
          
          <libs>
              /* The list of paths to shared libraries and/or
                 directories with shared libraries, one per line */
          </libs>
          
          /* Optional sections */
          
          <search_headers>
              /* List of directories to be searched
                 for header files to automatically
                 generate include paths, one per line */
          </search_headers>
          
          <search_libs>
              /* List of directories to be searched
                 for shared libraries to resolve
                 dependencies, one per line */
          </search_libs>
          
          <tools>
              /* List of directories with tools used
                 for analysis (GCC toolchain), one per line */
          </tools>
          
          <cross_prefix>
              /* GCC toolchain prefix.
                 Examples:
                     arm-linux-gnueabi
                     arm-none-symbianelf */
          </cross_prefix>
          
          <gcc_options>
              /* Additional GCC options,
                 one per line */
          </gcc_options>

  -sysinfo <dir>
      This option may be used with -dump-system to dump ABI of operating
      systems and configure the dumping process.
      Default:
          modules/SysInfo/<target> {unix, symbian, windows}

  -cmp-systems -d1 sys_dumps/<name1>/<arch> -d2 sys_dumps/<name2>/<arch>
      Compare two system ABI dumps. Create compatibility reports for each
      library and the common HTML report including the summary of test
      results for all checked libraries. Report will be generated to:
          sys_compat_reports/<name1>_to_<name2>/<arch>

  -libs-list <path>
      The file with a list of libraries, that should be dumped by
      the -dump-system option or should be checked by the -cmp-systems option.

  -header <name>
      Check/Dump ABI of this header only.

OTHER OPTIONS:
  -test
      Run internal tests. Create two binary incompatible versions of a sample
      library and run the tool to check them for compatibility. This option
      allows to check if the tool works correctly in the current environment.

  -test-dump
      Test ability to create, restore and compare ABI dumps.
      
  -debug
      Debugging mode. Print debug info on the screen. Save intermediate
      analysis stages in the debug directory:
          debug/<library>/<version>/

  -p|-params <path>
      Path to file with the function parameter names. It can be used
      for improving report view if the library header files have no
      parameter names. File format:
      
            func1;param1;param2;param3 ...
            func2;param1;param2;param3 ...
             ...

  -relpath <path>
      Replace {RELPATH} macros to <path> in the XML-descriptor used
      for dumping the library ABI (see -dump option).
  
  -relpath1 <path>
      Replace {RELPATH} macros to <path> in the 1st XML-descriptor (-d1).

  -relpath2 <path>
      Replace {RELPATH} macros to <path> in the 2nd XML-descriptor (-d2).

  -dump-path <path>
      Specify a file path (*.abi.$AR_EXT) where to generate an ABI dump.
      Default: 
          abi_dumps/<library>/<library>_<version>.abi.$AR_EXT

  -report-path <path>
      Compatibility report path.
      Default: 
          compat_reports/<library name>/<v1>_to_<v2>/abi_compat_report.html

  -log-path <path>
      Log path for -dump-abi option.
      Default:
          logs/<library>/<version>/log.txt

  -log1-path <path>
      Log path for 1st version of a library.
      Default:
          logs/<library name>/<v1>/log.txt

  -log2-path <path>
      Log path for 2nd version of a library.
      Default:
          logs/<library name>/<v2>/log.txt

  -component <name>
      The component name in the title and summary of the HTML report.
      Default:
          library
      
  -l-full|-lib-full <name>
      Change library name in the report title to <name>. By default
      will be displayed a name specified by -l option.

REPORT:
    Compatibility report will be generated to:
        compat_reports/<library name>/<v1>_to_<v2>/abi_compat_report.html

    Log will be generated to:
        logs/<library name>/<v1>/log.txt
        logs/<library name>/<v2>/log.txt

EXIT CODES:
    0 - Compatible. The tool has run without any errors.
    non-zero - Incompatible or the tool has run with errors.

Report bugs to <abi-compliance-checker\@linuxtesting.org>
For more information, please see:
    http://ispras.linux-foundation.org/index.php/ABI_compliance_checker\n";
}

my $Descriptor_Template = "
<?xml version=\"1.0\" encoding=\"utf-8\"?>
<descriptor>

/* Primary sections */

<version>
    /* Version of the library */
</version>

<headers>
    /* The list of paths to header files and/or
       directories with header files, one per line */
</headers>

<libs>
    /* The list of paths to shared libraries (*.$LIB_EXT) and/or
       directories with shared libraries, one per line */
</libs>

/* Optional sections */

<include_paths>
    /* The list of include paths that will be provided
       to GCC to compile library headers, one per line.
       NOTE: If you define this section then the tool
       will not automatically generate include paths */
</include_paths>

<add_include_paths>
    /* The list of include paths that will be added
       to the automatically generated include paths, one per line */
</add_include_paths>

<gcc_options>
    /* Additional GCC options, one per line */
</gcc_options>

<include_preamble>
    /* The list of header files that will be
       included before other headers, one per line.
       Examples:
           1) tree.h for libxml2
           2) ft2build.h for freetype2 */
</include_preamble>

<defines>
    /* The list of defines that will be added at the
       headers compiling stage, one per line:
          #define A B
          #define C D */
</defines>

<skip_types>
    /* The list of data types, that
       should not be checked, one per line */
</skip_types>

<skip_symbols>
    /* The list of functions (mangled/symbol names in C++)
       that should not be checked, one per line */
</skip_symbols>

<skip_constants>
    /* The list of constants that should
       not be checked, one name per line */
</skip_constants>

<skip_headers>
    /* The list of header files and/or directories
       with header files that should not be checked, one per line */
</skip_headers>

<skip_libs>
    /* The list of shared libraries and/or directories
       with shared libraries that should not be checked, one per line */
</skip_libs>

<search_headers>
    /* List of directories to be searched
       for header files to automatically
       generate include paths, one per line. */
</search_headers>

<search_libs>
    /* List of directories to be searched
       for shared librariess to resolve
       dependencies, one per line */
</search_libs>

<tools>
    /* List of directories with tools used
       for analysis (GCC toolchain), one per line */
</tools>

<cross_prefix>
    /* GCC toolchain prefix.
       Examples:
           arm-linux-gnueabi
           arm-none-symbianelf */
</cross_prefix>

</descriptor>";

my %Operator_Indication = (
    "not" => "~",
    "assign" => "=",
    "andassign" => "&=",
    "orassign" => "|=",
    "xorassign" => "^=",
    "or" => "|",
    "xor" => "^",
    "addr" => "&",
    "and" => "&",
    "lnot" => "!",
    "eq" => "==",
    "ne" => "!=",
    "lt" => "<",
    "lshift" => "<<",
    "lshiftassign" => "<<=",
    "rshiftassign" => ">>=",
    "call" => "()",
    "mod" => "%",
    "modassign" => "%=",
    "subs" => "[]",
    "land" => "&&",
    "lor" => "||",
    "rshift" => ">>",
    "ref" => "->",
    "le" => "<=",
    "deref" => "*",
    "mult" => "*",
    "preinc" => "++",
    "delete" => " delete",
    "vecnew" => " new[]",
    "vecdelete" => " delete[]",
    "predec" => "--",
    "postinc" => "++",
    "postdec" => "--",
    "plusassign" => "+=",
    "plus" => "+",
    "minus" => "-",
    "minusassign" => "-=",
    "gt" => ">",
    "ge" => ">=",
    "new" => " new",
    "multassign" => "*=",
    "divassign" => "/=",
    "div" => "/",
    "neg" => "-",
    "pos" => "+",
    "memref" => "->*",
    "compound" => "," );

my %CppKeywords = map {$_=>1} (
    # C++ 2003 keywords
    "public",
    "protected",
    "private",
    "default",
    "template",
    "new",
    "delete",
    "this",
    #"asm",
    "dynamic_cast",
    "auto",
    "operator",
    "try",
    "typeid",
    "namespace",
    "typename",
    "catch",
    "using",
    "reinterpret_cast",
    "friend",
    "class",
    "virtual",
    "const_cast",
    "mutable",
    "static_cast",
    "export",
    #"throw",
    #"inline",
    #"register",
    #"bool",
    # C++0x keywords
    "noexcept",
    "alignof",
    "nullptr",
    "thread_local",
    "constexpr",
    "decltype",
    "static_assert",
    "explicit",
    # cannot be used as a macro name
    # as it is an operator in C++
    "and",
    #"and_eq",
    "not",
    #"not_eq",
    "or"
    #"or_eq",
    #"bitand",
    #"bitor",
    #"xor",
    #"xor_eq",
    #"compl"
);

# Header file extensions as described by gcc
my $HEADER_EXT = "h|hh|hp|hxx|hpp|h\\+\\+";

my %IntrinsicMangling = (
    "void" => "v",
    "bool" => "b",
    "wchar_t" => "w",
    "char" => "c",
    "signed char" => "a",
    "unsigned char" => "h",
    "short" => "s",
    "unsigned short" => "t",
    "int" => "i",
    "unsigned int" => "j",
    "long" => "l",
    "unsigned long" => "m",
    "long long" => "x",
    "__int64" => "x",
    "unsigned long long" => "y",
    "__int128" => "n",
    "unsigned __int128" => "o",
    "float" => "f",
    "double" => "d",
    "long double" => "e",
    "__float80" => "e",
    "__float128" => "g",
    "..." => "z"
);

my %StdcxxMangling = (
    "3std"=>"St",
    "3std9allocator"=>"Sa",
    "3std11basic_string"=>"Sb",
    "3std12basic_stringIcE"=>"Ss",
    "3std13basic_istreamIcE"=>"Si",
    "3std13basic_ostreamIcE"=>"So",
    "3std14basic_iostreamIcE"=>"Sd"
);

my %ConstantSuffix = (
    "unsigned int"=>"u",
    "long"=>"l",
    "unsigned long"=>"ul",
    "long long"=>"ll",
    "unsigned long long"=>"ull"
);

my %ConstantSuffixR =
reverse(%ConstantSuffix);

my %OperatorMangling = (
    "~" => "co",
    "=" => "aS",
    "|" => "or",
    "^" => "eo",
    "&" => "an",#ad (addr)
    "==" => "eq",
    "!" => "nt",
    "!=" => "ne",
    "<" => "lt",
    "<=" => "le",
    "<<" => "ls",
    "<<=" => "lS",
    ">" => "gt",
    ">=" => "ge",
    ">>" => "rs",
    ">>=" => "rS",
    "()" => "cl",
    "%" => "rm",
    "[]" => "ix",
    "&&" => "aa",
    "||" => "oo",
    "*" => "ml",#de (deref)
    "++" => "pp",#
    "--" => "mm",#
    "new" => "nw",
    "delete" => "dl",
    "new[]" => "na",
    "delete[]" => "da",
    "+=" => "pL",
    "+" => "pl",#ps (pos)
    "-" => "mi",#ng (neg)
    "-=" => "mI",
    "*=" => "mL",
    "/=" => "dV",
    "&=" => "aN",
    "|=" => "oR",
    "%=" => "rM",
    "^=" => "eO",
    "/" => "dv",
    "->*" => "pm",
    "->" => "pt",#rf (ref)
    "," => "cm",
    "?" => "qu",
    "." => "dt",
    "sizeof"=> "sz"#st
);

my %GlibcHeader = map {$_=>1} (
    "aliases.h",
    "argp.h",
    "argz.h",
    "assert.h",
    "cpio.h",
    "ctype.h",
    "dirent.h",
    "envz.h",
    "errno.h",
    "error.h",
    "execinfo.h",
    "fcntl.h",
    "fstab.h",
    "ftw.h",
    "glob.h",
    "grp.h",
    "iconv.h",
    "ifaddrs.h",
    "inttypes.h",
    "langinfo.h",
    "limits.h",
    "link.h",
    "locale.h",
    "malloc.h",
    "math.h",
    "mntent.h",
    "monetary.h",
    "nl_types.h",
    "obstack.h",
    "printf.h",
    "pwd.h",
    "regex.h",
    "sched.h",
    "search.h",
    "setjmp.h",
    "shadow.h",
    "signal.h",
    "spawn.h",
    "stdarg.h",
    "stdint.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "tar.h",
    "termios.h",
    "time.h",
    "ulimit.h",
    "unistd.h",
    "utime.h",
    "wchar.h",
    "wctype.h",
    "wordexp.h" );

my %GlibcDir = map {$_=>1} (
    "arpa",
    "bits",
    "gnu",
    "netinet",
    "net",
    "nfs",
    "rpc",
    "sys",
    "linux" );

my %LocalIncludes = map {$_=>1} (
    "/usr/local/include",
    "/usr/local" );

my %OS_AddPath=(
# this data needed if tool can't determine paths automatically
"macos"=>{
    "include"=>{"/Library"=>1,"/Developer/usr/include"=>1},
    "lib"=>{"/Library"=>1,"/Developer/usr/lib"=>1},
    "bin"=>{"/Developer/usr/bin"=>1}},
"beos"=>{
    "include"=>{"/boot/common"=>1,"/boot/develop"=>1},
    "lib"=>{"/boot/common/lib"=>1,"/boot/system/lib"=>1,"/boot/apps"=>1},
    "bin"=>{"/boot/common/bin"=>1,"/boot/system/bin"=>1,"/boot/develop/abi"=>1}}
    # Haiku has GCC 2.95.3 by default, try to find GCC>=3.0 in /boot/develop/abi
);

my %Slash_Type=(
    "default"=>"/",
    "windows"=>"\\"
);

my $SLASH = $Slash_Type{$OSgroup}?$Slash_Type{$OSgroup}:$Slash_Type{"default"};

#Global variables
my %COMMON_LANGUAGE=(
  1 => "C",
  2 => "C");

my $MAX_COMMAND_LINE_ARGUMENTS = 4096;
my (%WORD_SIZE, %CPU_ARCH, %GCC_VERSION);

my $STDCXX_TESTING = 0;
my $GLIBC_TESTING = 0;

my $CheckHeadersOnly = $CheckHeadersOnly_Opt;
my $CheckObjectsOnly = $CheckObjectsOnly_Opt;
my $TargetComponent = lc($TargetComponent_Opt);
if(not $TargetComponent)
{ # default: library
  # other components: header, system, ...
    $TargetComponent = "library";
}
my $GroupByHeaders = 0;

my $SystemRoot;

my $MAIN_CPP_DIR;
my $CHECKER_VERDICT = 0;
my $CHECKER_WARNING = 0;
my $REPORT_PATH = $OutputReportPath;
my %LOG_PATH;
my %DEBUG_PATH;
my %Cache;
my %LibInfo;
my $COMPILE_ERRORS = 0;
my %CompilerOptions;
my %CheckedDyLib;
my $TargetLibraryShortName = parse_libname($TargetLibraryName, "shortest");

#Constants (Defines)
my %Constants;
my %SkipConstants;

#Types
my %TypeInfo;
my %TemplateInstance_Func;
my %TemplateInstance;
my %SkipTypes = (
  "1"=>{},
  "2"=>{} );
my %Tid_TDid = (
  "1"=>{},
  "2"=>{} );
my %CheckedTypes;
my %TName_Tid;
my %EnumMembName_Id;
my %NestedNameSpaces = (
  "1"=>{},
  "2"=>{} );
my %UsedType;
my %VirtualTable;
my %VirtualTable_Full;
my %ClassVTable;
my %ClassVTable_Content;
my %VTableClass;
my %AllocableClass;
my %ClassMethods;
my %ClassToId;
my %Class_SubClasses;
my %OverriddenMethods;
my $MAX_TID;

#Typedefs
my %Typedef_BaseName;
my %Typedef_Translations;
my %Typedef_Equivalent;
my %StdCxxTypedef;
my %MissedTypedef;

#Symbols
my %SymbolInfo;
my %tr_name;
my %mangled_name_gcc;
my %mangled_name;
my %SkipSymbols = (
  "1"=>{},
  "2"=>{} );
my %SymbolsList;
my %SymbolsList_App;
my %CheckedSymbols;
my %DepSymbols;
my %MangledNames;
my %AddIntParams;
my %Interface_Implementation;

#Headers
my %Include_Preamble;
my %Target_Headers;
my %HeaderName_Paths;
my %Header_Dependency;
my %Include_Neighbors;
my %Include_Paths;
my %INC_PATH_AUTODETECT = (
  "1"=>1,
  "2"=>1 );
my %Add_Include_Paths;
my %RegisteredHeaders;
my %RegisteredDirs;
my %RegisteredDeps;
my %Header_ErrorRedirect;
my %Header_Includes;
my %Header_ShouldNotBeUsed;
my %RecursiveIncludes;
my %Header_Include_Prefix;
my %SkipHeaders;
my %SkipLibs;
my %Include_Order;
my %TUnit_NameSpaces;

my %C99Mode = (
  "1"=>0,
  "2"=>0 );
my %AutoPreambleMode = (
  "1"=>0,
  "2"=>0 );
my %MinGWMode = (
  "1"=>0,
  "2"=>0 );

#Shared objects
my %DyLib_DefaultPath;
my %InputObject_Paths;
my %RegisteredObjDirs;

#System shared objects
my %SystemObjects;
my %DefaultLibPaths;

#System header files
my %SystemHeaders;
my %DefaultCppPaths;
my %DefaultGccPaths;
my %DefaultIncPaths;
my %DefaultCppHeader;
my %DefaultGccHeader;
my %UserIncPath;

#Merging
my %CompleteSignature;
my %Symbol_Library;
my %Library_Symbol = (
  "1"=>{},
  "2"=>{} );
my %Language;
my $Version;
my %AddedInt;
my %RemovedInt;
my %AddedInt_Virt;
my %RemovedInt_Virt;
my %VirtualReplacement;
my %ChangedTypedef;
my %CompatRules;
my %IncompleteRules;
my %UnknownRules;

#OS Compliance
my %TargetLibs;

#OS Specifics
my $OStarget = $OSgroup;
my %TargetTools;

#Compliance Report
my %Type_MaxPriority;

#Recursion locks
my @RecurLib;
my @RecurSymlink;
my @RecurTypes;
my @RecurInclude;
my @RecurConstant;

#System
my %SystemPaths;
my %DefaultBinPaths;
my $GCC_PATH;

#Symbols versioning
my %SymVer;

#Problem descriptions
my %CompatProblems;
my %ProblemsWithConstants;
my %ImplProblems;

#Rerorts
my $ContentID = 1;
my $ContentSpanStart = "<span class=\"section\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\">\n";
my $ContentSpanStart_Affected = "<span class=\"section_affected\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\">\n";
my $ContentSpanStart_Info = "<span class=\"section_info\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\">\n";
my $ContentSpanEnd = "</span>\n";
my $ContentDivStart = "<div id=\"CONTENT_ID\" style=\"display:none;\">\n";
my $ContentDivEnd = "</div>\n";
my $Content_Counter = 0;

my $JScripts = "
<script type=\"text/javascript\" language=\"JavaScript\">
<!--
function showContent(header, id)   {
    e = document.getElementById(id);
    if(e.style.display == 'none')
    {
        e.style.display = 'block';
        e.style.visibility = 'visible';
        header.innerHTML = header.innerHTML.replace(/\\\[[^0-9 ]\\\]/gi,\"[&minus;]\");
    }
    else
    {
        e.style.display = 'none';
        e.style.visibility = 'hidden';
        header.innerHTML = header.innerHTML.replace(/\\\[[^0-9 ]\\\]/gi,\"[+]\");
    }
}
-->
</script>";

sub numToStr($)
{
    my $Number = int($_[0]);
    if($Number>3) {
        return $Number."th";
    }
    elsif($Number==1) {
        return "1st";
    }
    elsif($Number==2) {
        return "2nd";
    }
    elsif($Number==3) {
        return "3rd";
    }
    else {
        return $Number;
    }
}

sub search_Tools($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    if(my @Paths = keys(%TargetTools))
    {
        foreach my $Path (@Paths)
        {
            if(-f joinPath($Path, $Name)) {
                return joinPath($Path, $Name);
            }
            if($CrossPrefix)
            { # user-defined prefix (arm-none-symbianelf, ...)
                my $Candidate = joinPath($Path, $CrossPrefix."-".$Name);
                if(-f $Candidate) {
                    return $Candidate;
                }
            }
        }
    }
    else {
        return "";
    }
}

sub synch_Cmd($)
{
    my $Name = $_[0];
    if(not $GCC_PATH)
    { # GCC was not found yet
        return "";
    }
    my $Candidate = $GCC_PATH;
    if($Candidate=~s/(\W|\A)gcc(|\.\w+)\Z/$1$Name$2/) {
        return $Candidate;
    }
    return "";
}

sub get_CmdPath($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    if(defined $Cache{"get_CmdPath"}{$Name}) {
        return $Cache{"get_CmdPath"}{$Name};
    }
    my %BinUtils = map {$_=>1} (
        "c++filt",
        "objdump",
        "readelf"
    );
    if($BinUtils{$Name}) {
        if(my $Dir = get_dirname($GCC_PATH)) {
            $TargetTools{$Dir}=1;
        }
    }
    my $Path = search_Tools($Name);
    if(not $Path and $OSgroup eq "windows") {
        $Path = search_Tools($Name.".exe");
    }
    if(not $Path and $BinUtils{$Name})
    {
        if($CrossPrefix)
        { # user-defined prefix
            $Path = search_Cmd($CrossPrefix."-".$Name);
        }
    }
    if(not $Path and $BinUtils{$Name})
    {
        if(my $Candidate = synch_Cmd($Name))
        { # synch with GCC
            if($Candidate=~/[\/\\]/)
            {# command path
                if(-f $Candidate) {
                    $Path = $Candidate;
                }
            }
            elsif($Candidate = search_Cmd($Candidate))
            {# command name
                $Path = $Candidate;
            }
        }
    }
    if(not $Path) {
        $Path = search_Cmd($Name);
    }
    if(not $Path and $OSgroup eq "windows")
    {# search for *.exe file
        $Path=search_Cmd($Name.".exe");
    }
    if($Path=~/\s/) {
        $Path = "\"".$Path."\"";
    }
    return ($Cache{"get_CmdPath"}{$Name}=$Path);
}

sub search_Cmd($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    if(defined $Cache{"search_Cmd"}{$Name}) {
        return $Cache{"search_Cmd"}{$Name};
    }
    if(my $DefaultPath = get_CmdPath_Default($Name)) {
        return ($Cache{"search_Cmd"}{$Name} = $DefaultPath);
    }
    foreach my $Path (sort {length($a)<=>length($b)} keys(%{$SystemPaths{"bin"}}))
    {
        my $CmdPath = joinPath($Path,$Name);
        if(-f $CmdPath)
        {
            if($Name=~/gcc/) {
                next if(not check_gcc_version($CmdPath, "3"));
            }
            return ($Cache{"search_Cmd"}{$Name} = $CmdPath);
        }
    }
    return ($Cache{"search_Cmd"}{$Name} = "");
}

sub get_CmdPath_Default($)
{# search in PATH
    my $Name = $_[0];
    return "" if(not $Name);
    if(defined $Cache{"get_CmdPath_Default"}{$Name}) {
        return $Cache{"get_CmdPath_Default"}{$Name};
    }
    if($Name=~/find/)
    {# special case: search for "find" utility
        if(`find . -maxdepth 0 2>$TMP_DIR/null`) {
            return ($Cache{"get_CmdPath_Default"}{$Name} = "find");
        }
    }
    elsif($Name=~/gcc/) {
        return check_gcc_version($Name, "3");
    }
    if(check_command($Name)) {
        return ($Cache{"get_CmdPath_Default"}{$Name} = $Name);
    }
    if($OSgroup eq "windows"
    and `$Name /? 2>$TMP_DIR/null`) {
        return ($Cache{"get_CmdPath_Default"}{$Name} = $Name);
    }
    if($Name!~/which/) {
        my $WhichCmd = get_CmdPath("which");
        if($WhichCmd and `$WhichCmd $Name 2>$TMP_DIR/null`) {
            return ($Cache{"get_CmdPath_Default"}{$Name} = $Name);
        }
    }
    foreach my $Path (sort {length($a)<=>length($b)} keys(%DefaultBinPaths))
    {
        if(-f $Path."/".$Name) {
            return ($Cache{"get_CmdPath_Default"}{$Name} = joinPath($Path,$Name));
        }
    }
    return ($Cache{"get_CmdPath_Default"}{$Name} = "");
}

sub clean_path($)
{
    my $Path = $_[0];
    $Path=~s/[\/\\]+\Z//g;
    return $Path;
}

sub readDescriptor($$)
{
    my ($LibVersion, $Content) = @_;
    return if(not $LibVersion);
    my $DName = $DumpAPI?"descriptor":"descriptor \"d$LibVersion\"";
    if(not $Content) {
        exitStatus("Error", "$DName is empty");
    }
    if($Content!~/\</) {
        exitStatus("Error", "descriptor should be one of the following: XML-descriptor, RPM or Deb \"devel\" package, ABI dump, standalone header/$SLIB_TYPE library or directory with headers and/or $SLIB_TYPE libraries");
    }
    $Content=~s/\/\*(.|\n)+?\*\///g;
    $Content=~s/<\!--(.|\n)+?-->//g;
    $Descriptor{$LibVersion}{"Version"} = parseTag(\$Content, "version");
    if($TargetVersion{$LibVersion}) {
        $Descriptor{$LibVersion}{"Version"} = $TargetVersion{$LibVersion};
    }
    if(not $Descriptor{$LibVersion}{"Version"}) {
        exitStatus("Error", "version in the $DName is not specified (<version> section)");
    }
    if($Content=~/{RELPATH}/)
    {
        if(my $RelDir = $RelativeDirectory{$LibVersion})  {
            $Content =~ s/{RELPATH}/$RelDir/g;
        }
        else
        {
            my $NeedRelpath = $DumpAPI?"-relpath":"-relpath$LibVersion";
            exitStatus("Error", "you have not specified $NeedRelpath option, but the $DName contains {RELPATH} macro");
        }
    }
    
    if(not $CheckObjectsOnly_Opt)
    {
        my $DHeaders = parseTag(\$Content, "headers");
        if(not $DHeaders) {
            exitStatus("Error", "header files in the $DName are not specified (<headers> section)");
        }
        elsif(lc($DHeaders) ne "none")
        {# append the descriptor headers list
            if($Descriptor{$LibVersion}{"Headers"})
            {# multiple descriptors
                $Descriptor{$LibVersion}{"Headers"} .= "\n".$DHeaders;
            }
            else {
                $Descriptor{$LibVersion}{"Headers"} = $DHeaders;
            }
            foreach my $Path (split(/\s*\n\s*/, $DHeaders))
            {
                if(not -e $Path) {
                    exitStatus("Access_Error", "can't access \'$Path\'");
                }
            }
        }
    }
    if(not $CheckHeadersOnly_Opt)
    {
        my $DObjects = parseTag(\$Content, "libs");
        if(not $DObjects) {
            exitStatus("Error", "$SLIB_TYPE libraries in the $DName are not specified (<libs> section)");
        }
        elsif(lc($DObjects) ne "none")
        {# append the descriptor libraries list
            if($Descriptor{$LibVersion}{"Libs"})
            {# multiple descriptors
                $Descriptor{$LibVersion}{"Libs"} .= "\n".$DObjects;
            }
            else {
                $Descriptor{$LibVersion}{"Libs"} .= $DObjects;
            }
            foreach my $Path (split(/\s*\n\s*/, $DObjects))
            {
                if(not -e $Path) {
                    exitStatus("Access_Error", "can't access \'$Path\'");
                }
            }
        }
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "search_headers")))
    {
        $Path = clean_path($Path);
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = path_format($Path, $OSgroup);
        $SystemPaths{"include"}{$Path}=1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "search_libs")))
    {
        $Path = clean_path($Path);
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = path_format($Path, $OSgroup);
        $SystemPaths{"lib"}{$Path}=1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "tools")))
    {
        $Path=clean_path($Path);
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = path_format($Path, $OSgroup);
        $SystemPaths{"bin"}{$Path}=1;
        $TargetTools{$Path}=1;
    }
    if(my $Prefix = parseTag(\$Content, "cross_prefix")) {
        $CrossPrefix = $Prefix;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "include_paths")))
    {
        $Path=clean_path($Path);
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = path_format($Path, $OSgroup);
        $Descriptor{$LibVersion}{"IncludePaths"}{$Path} = 1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "add_include_paths")))
    {
        $Path=clean_path($Path);
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = path_format($Path, $OSgroup);
        $Descriptor{$LibVersion}{"AddIncludePaths"}{$Path} = 1;
    }
    $Descriptor{$LibVersion}{"GccOptions"} = parseTag(\$Content, "gcc_options");
    foreach my $Option (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"GccOptions"})) {
        $CompilerOptions{$LibVersion} .= " ".$Option;
    }
    $Descriptor{$LibVersion}{"SkipHeaders"} = parseTag(\$Content, "skip_headers");
    foreach my $Name (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"SkipHeaders"}))
    {
        if($Name=~/[\*\[]/)
        {# wildcard
            $Name=~s/\*/.*/g;
            $Name=~s/\\/\\\\/g;
            $SkipHeaders{$LibVersion}{"Pattern"}{$Name} = 1;
        }
        elsif($Name=~/[\/\\]/)
        {# directory or relative path
            $SkipHeaders{$LibVersion}{"Path"}{path_format($Name, $OSgroup)} = 1;
        }
        else {
            $SkipHeaders{$LibVersion}{"Name"}{$Name} = 1;
        }
    }
    $Descriptor{$LibVersion}{"SkipLibs"} = parseTag(\$Content, "skip_libs");
    foreach my $Name (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"SkipLibs"}))
    {
        if($Name=~/[\*\[]/)
        {# wildcard
            $Name=~s/\*/.*/g;
            $Name=~s/\\/\\\\/g;
            $SkipLibs{$LibVersion}{"Pattern"}{$Name} = 1;
        }
        elsif($Name=~/[\/\\]/)
        {# directory or relative path
            $SkipLibs{$LibVersion}{"Path"}{path_format($Name, $OSgroup)} = 1;
        }
        else {
            $SkipLibs{$LibVersion}{"Name"}{$Name} = 1;
        }
    }
    if(my $DDefines = parseTag(\$Content, "defines"))
    {
        if($Descriptor{$LibVersion}{"Defines"})
        {# multiple descriptors
            $Descriptor{$LibVersion}{"Defines"} .= "\n".$DDefines;
        }
        else {
            $Descriptor{$LibVersion}{"Defines"} = $DDefines;
        }
    }
    foreach my $Order (split(/\s*\n\s*/, parseTag(\$Content, "include_order")))
    {
        if($Order=~/\A(.+):(.+)\Z/) {
            $Include_Order{$LibVersion}{$1} = $2;
        }
    }
    foreach my $Type_Name (split(/\s*\n\s*/, parseTag(\$Content, "opaque_types")),
    split(/\s*\n\s*/, parseTag(\$Content, "skip_types")))
    {# opaque_types renamed to skip_types (1.23.4)
        $SkipTypes{$LibVersion}{$Type_Name} = 1;
    }
    foreach my $Symbol (split(/\s*\n\s*/, parseTag(\$Content, "skip_interfaces")),
    split(/\s*\n\s*/, parseTag(\$Content, "skip_symbols")))
    {# skip_interfaces renamed to skip_symbols (1.22.1)
        $SkipSymbols{$LibVersion}{$Symbol} = 1;
    }
    foreach my $Constant (split(/\s*\n\s*/, parseTag(\$Content, "skip_constants"))) {
        $SkipConstants{$LibVersion}{$Constant} = 1;
    }
    if(my $DIncPreamble = parseTag(\$Content, "include_preamble"))
    {
        if($Descriptor{$LibVersion}{"IncludePreamble"})
        {# multiple descriptors
            $Descriptor{$LibVersion}{"IncludePreamble"} .= "\n".$DIncPreamble;
        }
        else {
            $Descriptor{$LibVersion}{"IncludePreamble"} = $DIncPreamble;
        }
    }
}

sub parseTag($$)
{
    my ($CodeRef, $Tag) = @_;
    return "" if(not $CodeRef or not ${$CodeRef} or not $Tag);
    if(${$CodeRef}=~s/\<\Q$Tag\E\>((.|\n)+?)\<\/\Q$Tag\E\>//)
    {
        my $Content = $1;
        $Content=~s/(\A\s+|\s+\Z)//g;
        return $Content;
    }
    else {
        return "";
    }
}

my %check_node= map {$_=>1} (
  "array_type",
  "binfo",
  "boolean_type",
  "complex_type",
  "const_decl",
  "enumeral_type",
  "field_decl",
  "function_decl",
  "function_type",
  "identifier_node",
  "integer_cst",
  "integer_type",
  "method_type",
  "namespace_decl",
  "parm_decl",
  "pointer_type",
  "real_cst",
  "real_type",
  "record_type",
  "reference_type",
  "string_cst",
  "template_decl",
  "template_type_parm",
  "tree_list",
  "tree_vec",
  "type_decl",
  "union_type",
  "var_decl",
  "void_type",
  "offset_type" );

sub getInfo($)
{
    my $InfoPath = $_[0];
    return if(not $InfoPath or not -f $InfoPath);
    my $Content = readFile($InfoPath);
    unlink($InfoPath);
    $Content=~s/\n[ ]+/ /g;
    my @Lines = split("\n", $Content);
    $Content="";# clear
    foreach (@Lines)
    {
        if(/\A\@(\d+)\s+([a-z_]+)\s+(.+)\Z/oi)
        {# get a number and attributes of node
            next if(not $check_node{$2});
            $LibInfo{$Version}{"info_type"}{$1}=$2;
            $LibInfo{$Version}{"info"}{$1}=$3;
        }
    }
    $MAX_TID = $#Lines+1;
    @Lines=();# clear
    # processing info
    setTemplateParams_All();
    getTypeInfo_All();
    renameTmplInst();
    getSymbolInfo_All();
    getVarDescr_All();
    
    # cleaning memory
    %LibInfo = ();
    %TemplateInstance = ();
    %TemplateInstance_Func = ();
    %MangledNames = ();
}

sub renameTmplInst()
{
    foreach my $Base (keys(%{$Typedef_Translations{$Version}}))
    {
        next if($Base!~/</);
        my @Translations = keys(%{$Typedef_Translations{$Version}{$Base}});
        if($#Translations==0 and length($Translations[0])<=length($Base)) {
            $Typedef_Equivalent{$Version}{$Base} = $Translations[0];
        }
    }
    foreach my $TDid (keys(%{$TypeInfo{$Version}}))
    {
        foreach my $Tid (keys(%{$TypeInfo{$Version}{$TDid}}))
        {
            my $TypeName = $TypeInfo{$Version}{$TDid}{$Tid}{"Name"};
            if($TypeName=~/>/)
            {# template instances only
                foreach my $Base (sort {length($b)<=>length($a)}
                keys(%{$Typedef_Equivalent{$Version}}))
                {
                    next if(not $Base);
                    next if(index($TypeName,$Base)==-1);
                    next if(length($Base)>=length($TypeName)-2);
                    my $TypedefName = $Typedef_Equivalent{$Version}{$Base};
                    $TypeName=~s/<\Q$Base\E(\W|\Z)/<$TypedefName$1/g;
                    $TypeName=~s/<\Q$Base\E(\w|\Z)/<$TypedefName $1/g;
                    last if($TypeName!~/>/);
                }
                if($TypeName ne $TypeInfo{$Version}{$TDid}{$Tid}{"Name"})
                {
                    $TypeInfo{$Version}{$TDid}{$Tid}{"Name"} = correctName($TypeName);
                    $TName_Tid{$Version}{$TypeName} = $Tid;
                }
            }
        }
    }
}

sub setTemplateParams_All()
{
    foreach (keys(%{$LibInfo{$Version}{"info"}}))
    {
        if($LibInfo{$Version}{"info_type"}{$_} eq "template_decl") {
            setTemplateParams($_);
        }
    }
}

sub setTemplateParams($)
{
    my $TypeInfoId = $_[0];
    if($LibInfo{$Version}{"info"}{$TypeInfoId}=~/(inst|spcs)[ ]*:[ ]*@(\d+) /)
    {
        my $TmplInst_InfoId = $2;
        setTemplateInstParams($TmplInst_InfoId);
        my $TmplInst_Info = $LibInfo{$Version}{"info"}{$TmplInst_InfoId};
        while($TmplInst_Info=~/(chan|chain)[ ]*:[ ]*@(\d+) /)
        {
            $TmplInst_InfoId = $2;
            $TmplInst_Info = $LibInfo{$Version}{"info"}{$TmplInst_InfoId};
            setTemplateInstParams($TmplInst_InfoId);
        }
    }
}

sub setTemplateInstParams($)
{
    my $TmplInst_Id = $_[0];
    my $Info = $LibInfo{$Version}{"info"}{$TmplInst_Id};
    my ($Params_InfoId, $ElemId) = ();
    if($Info=~/purp[ ]*:[ ]*@(\d+) /) {
        $Params_InfoId = $1;
    }
    if($Info=~/valu[ ]*:[ ]*@(\d+) /) {
        $ElemId = $1;
    }
    if($Params_InfoId and $ElemId)
    {
        my $Params_Info = $LibInfo{$Version}{"info"}{$Params_InfoId};
        while($Params_Info=~s/ (\d+)[ ]*:[ ]*\@(\d+) / /)
        {
            my ($Param_Pos, $Param_TypeId) = ($1, $2);
            return if($LibInfo{$Version}{"info_type"}{$Param_TypeId} eq "template_type_parm");
            if($LibInfo{$Version}{"info_type"}{$ElemId} eq "function_decl") {
                $TemplateInstance_Func{$Version}{$ElemId}{$Param_Pos} = $Param_TypeId;
            }
            else {
                $TemplateInstance{$Version}{getTypeDeclId($ElemId)}{$ElemId}{$Param_Pos} = $Param_TypeId;
            }
        }
    }
}

sub getTypeDeclId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/name[ ]*:[ ]*@(\d+)/) {
        return $1;
    }
    else {
        return "";
    }
}

sub isFuncPtr($)
{
    my $Ptd = pointTo($_[0]);
    return 0 if(not $Ptd);
    if($LibInfo{$Version}{"info"}{$_[0]}=~/unql[ ]*:/
    and $LibInfo{$Version}{"info"}{$_[0]}!~/qual[ ]*:/) {
        return 0;
    }
    elsif($LibInfo{$Version}{"info_type"}{$_[0]} eq "pointer_type"
    and $LibInfo{$Version}{"info_type"}{$Ptd} eq "function_type") {
        return 1;
    }
    return 0;
}

sub isMethodPtr($)
{
    my $Ptd = pointTo($_[0]);
    return 0 if(not $Ptd);
    if($LibInfo{$Version}{"info_type"}{$_[0]} eq "record_type"
    and $LibInfo{$Version}{"info_type"}{$Ptd} eq "method_type"
    and $LibInfo{$Version}{"info"}{$_[0]}=~/ ptrmem /) {
        return 1;
    }
    return 0;
}

sub isFieldPtr($)
{
    if($LibInfo{$Version}{"info_type"}{$_[0]} eq "offset_type"
    and $LibInfo{$Version}{"info"}{$_[0]}=~/ ptrmem /) {
        return 1;
    }
    return 0;
}

sub pointTo($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/ptd[ ]*:[ ]*@(\d+)/) {
        return $1;
    }
    else {
        return "";
    }
}

sub getTypeInfo_All()
{
    if(not check_gcc_version($GCC_PATH, "4.5"))
    { # missed typedefs: QStyle::State is typedef to QFlags<QStyle::StateFlag>
      # but QStyleOption.state is of type QFlags<QStyle::StateFlag> in the TU dump
      # FIXME: check GCC versions
        addMissedTypes();
    }
    foreach (sort {int($a)<=>int($b)} keys(%{$LibInfo{$Version}{"info"}}))
    {
        my $IType = $LibInfo{$Version}{"info_type"}{$_};
        if($IType=~/_type\Z/ and $IType ne "function_type"
        and $IType ne "method_type") {
            getTypeInfo(getTypeDeclId("$_"), "$_");
        }
    }
    $TypeInfo{$Version}{""}{-1}{"Name"} = "...";
    $TypeInfo{$Version}{""}{-1}{"Type"} = "Intrinsic";
    $TypeInfo{$Version}{""}{-1}{"Tid"} = -1;
}

sub addMissedTypes()
{
    foreach my $MissedTDid (sort {int($a)<=>int($b)} keys(%{$LibInfo{$Version}{"info"}}))
    { # detecting missed typedefs
        if($LibInfo{$Version}{"info_type"}{$MissedTDid} eq "type_decl")
        {
            my $TypeId = getTreeAttr($MissedTDid, "type");
            next if(not $TypeId);
            my $TypeType = getTypeType($MissedTDid, $TypeId);
            if($TypeType eq "Unknown")
            { # template_type_parm
                next;
            }
            my $TypeDeclId = getTypeDeclId($TypeId);
            next if($TypeDeclId eq $MissedTDid);#or not $TypeDeclId
            my $TypedefName = getNameByInfo($MissedTDid);
            next if(not $TypedefName);
            next if($TypedefName eq "__float80");
            next if(isAnon($TypedefName));
            if(not $TypeDeclId
            or getNameByInfo($TypeDeclId) ne $TypedefName) {
                $MissedTypedef{$Version}{$TypeId}{"$MissedTDid"} = 1;
            }
        }
    }
    foreach my $Tid (keys(%{$MissedTypedef{$Version}}))
    { # add missed typedefs
        my @Missed = keys(%{$MissedTypedef{$Version}{$Tid}});
        if(not @Missed or $#Missed>=1) {
            delete($MissedTypedef{$Version}{$Tid});
            next;
        }
        my $MissedTDid = $Missed[0];
        my $TDid = getTypeDeclId($Tid);
        my ($TypedefName, $TypedefNS) = getTrivialName($MissedTDid, $Tid);
        my %MissedInfo = ( # typedef info
            "Name" => $TypedefName,
            "NameSpace" => $TypedefNS,
            "BaseType" => {
                            "TDid" => $TDid,
                            "Tid" => $Tid
                          },
            "Type" => "Typedef",
            "Tid" => ++$MAX_TID,
            "TDid" => $MissedTDid );
        my ($H, $L) = getLocation($MissedTDid);
        $MissedInfo{"Header"} = $H;
        $MissedInfo{"Line"} = $L;
        $MissedInfo{"Size"} = getSize($Tid)/$BYTE_SIZE;
        my $MName = $MissedInfo{"Name"};
        next if(not $MName);
        if($MName=~/\*|\&|\s/)
        { # other types
            next;
        }
        if($MName=~/>(::\w+)+\Z/)
        { # QFlags<Qt::DropAction>::enum_type
            delete($MissedTypedef{$Version}{$Tid});
            next;
        }
        if(getTypeType($TDid, $Tid)=~/\A(Intrinsic|Union|Struct|Enum|Class)\Z/)
        { # double-check for the name of typedef
            my ($TName, $TNS) = getTrivialName($TDid, $Tid); # base type info
            next if(not $TName);
            if(length($MName)>=length($TName))
            { # too long typedef
                delete($MissedTypedef{$Version}{$Tid});
                next;
            }
            if($TName=~/\A\Q$MName\E</) {
                next;
            }
            if($MName=~/\A\Q$TName\E/)
            { # QDateTimeEdit::Section and QDateTimeEdit::Sections::enum_type
                delete($MissedTypedef{$Version}{$Tid});
                next;
            }
            if(get_depth($MName)==0 and get_depth($TName)!=0)
            { # std::_Vector_base and std::vector::_Base
                delete($MissedTypedef{$Version}{$Tid});
                next;
            }
        }
        %{$TypeInfo{$Version}{$MissedTDid}{$MissedInfo{"Tid"}}} = %MissedInfo;
        $Tid_TDid{$Version}{$MissedInfo{"Tid"}} = $MissedTDid;
        delete($TypeInfo{$Version}{$MissedTDid}{$Tid});
        # register typedef
        $MissedTypedef{$Version}{$Tid}{"TDid"} = $MissedTDid;
        $MissedTypedef{$Version}{$Tid}{"Tid"} = $MissedInfo{"Tid"};
    }
}

sub getTypeInfo($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}} = getTypeAttr($TypeDeclId, $TypeId);
    my $TName = $TypeInfo{$Version}{$TypeDeclId}{$TypeId}{"Name"};
    if(not $TName) {
        delete($TypeInfo{$Version}{$TypeDeclId}{$TypeId});
        return;
    }
    if($TypeDeclId) {
        $Tid_TDid{$Version}{$TypeId} = $TypeDeclId;
    }
    if(not $TName_Tid{$Version}{$TName}) {
        $TName_Tid{$Version}{$TName} = $TypeId;
    }
}

sub getArraySize($$)
{
    my ($TypeId, $BaseName) = @_;
    my $SizeBytes = getSize($TypeId)/$BYTE_SIZE;
    while($BaseName=~s/\s*\[(\d+)\]//) {
        $SizeBytes/=$1;
    }
    my $BasicId = $TName_Tid{$Version}{$BaseName};
    my $BasicSize = $TypeInfo{$Version}{getTypeDeclId($BasicId)}{$BasicId}{"Size"};
    if($BasicSize) {
        $SizeBytes/=$BasicSize;
    }
    return $SizeBytes;
}

sub getTParams($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    my @Template_Params = ();
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$TemplateInstance{$Version}{$TypeDeclId}{$TypeId}}))
    {
        my $Param_TypeId = $TemplateInstance{$Version}{$TypeDeclId}{$TypeId}{$Param_Pos};
        my $Param = get_TemplateParam($Param_TypeId);
        if($Param eq "") {
            return ();
        }
        elsif($Param_Pos>=1
        and $Param=~/\Astd::(allocator|less|((char|regex)_traits)|((i|o)streambuf_iterator))\</)
        { # template<typename _Tp, typename _Alloc = std::allocator<_Tp> >
          # template<typename _Key, typename _Compare = std::less<_Key>
          # template<typename _CharT, typename _Traits = std::char_traits<_CharT> >
          # template<typename _Ch_type, typename _Rx_traits = regex_traits<_Ch_type> >
          # template<typename _CharT, typename _InIter = istreambuf_iterator<_CharT> >
          # template<typename _CharT, typename _OutIter = ostreambuf_iterator<_CharT> >
            next;
        }
        elsif($Param ne "\@skip\@") {
            @Template_Params = (@Template_Params, $Param);
        }
    }
    return @Template_Params;
}

sub getTypeAttr($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    my ($BaseTypeSpec, %TypeAttr) = ();
    if($TypeInfo{$Version}{$TypeDeclId}{$TypeId}{"Name"}) {
        return %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}};
    }
    $TypeAttr{"Tid"} = $TypeId;
    $TypeAttr{"TDid"} = $TypeDeclId;
    $TypeAttr{"Type"} = getTypeType($TypeDeclId, $TypeId);
    if($TypeAttr{"Type"} eq "Unknown") {
        return ();
    }
    elsif($TypeAttr{"Type"}=~/(Func|Method|Field)Ptr/)
    {
        %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}} = getMemPtrAttr(pointTo($TypeId), $TypeDeclId, $TypeId, $TypeAttr{"Type"});
        $TName_Tid{$Version}{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}};
    }
    elsif($TypeAttr{"Type"} eq "Array")
    {
        ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}, $BaseTypeSpec) = selectBaseType($TypeDeclId, $TypeId);
        my %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        if($TypeAttr{"Size"} = getArraySize($TypeId, $BaseTypeAttr{"Name"}))
        {
            if($BaseTypeAttr{"Name"}=~/\A([^\[\]]+)(\[(\d+|)\].*)\Z/) {
                $TypeAttr{"Name"} = $1."[".$TypeAttr{"Size"}."]".$2;
            }
            else {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}."[".$TypeAttr{"Size"}."]";
            }
        }
        else
        {
            if($BaseTypeAttr{"Name"}=~/\A([^\[\]]+)(\[(\d+|)\].*)\Z/) {
                $TypeAttr{"Name"} = $1."[]".$2;
            }
            else {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}."[]";
            }
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
        if($BaseTypeAttr{"Header"})  {
            $TypeAttr{"Header"} = $BaseTypeAttr{"Header"};
        }
        %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}} = %TypeAttr;
        $TName_Tid{$Version}{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %TypeAttr;
    }
    elsif($TypeAttr{"Type"}=~/\A(Intrinsic|Union|Struct|Enum|Class)\Z/)
    {
        %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}} = getTrivialTypeAttr($TypeDeclId, $TypeId);
        return %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}};
    }
    else
    {
        ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}, $BaseTypeSpec) = selectBaseType($TypeDeclId, $TypeId);
        if(my $MissedTDid = $MissedTypedef{$Version}{$TypeAttr{"BaseType"}{"Tid"}}{"TDid"})
        {
            if($MissedTDid ne $TypeDeclId)
            {
                $TypeAttr{"BaseType"}{"TDid"} = $MissedTDid;
                $TypeAttr{"BaseType"}{"Tid"} = $MissedTypedef{$Version}{$TypeAttr{"BaseType"}{"Tid"}}{"Tid"};
            }
        }
        my %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        if($BaseTypeAttr{"Type"} eq "Typedef")
        {# relinking typedefs
            my %BaseBase = get_Type($BaseTypeAttr{"BaseType"}{"TDid"},$BaseTypeAttr{"BaseType"}{"Tid"}, $Version);
            if($BaseTypeAttr{"Name"} eq $BaseBase{"Name"}) {
                ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}) = ($BaseBase{"Tid"}, $BaseBase{"TDid"});
            }
        }
        if($BaseTypeSpec and $BaseTypeAttr{"Name"})
        {
            if($TypeAttr{"Type"} eq "Pointer"
            and $BaseTypeAttr{"Name"}=~/\([\*]+\)/) {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"};
                $TypeAttr{"Name"}=~s/\(([*]+)\)/($1*)/g;
            }
            else {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}." ".$BaseTypeSpec;
            }
        }
        elsif($BaseTypeAttr{"Name"}) {
            $TypeAttr{"Name"} = $BaseTypeAttr{"Name"};
        }
        if($TypeAttr{"Type"} eq "Typedef")
        {
            $TypeAttr{"Name"} = getNameByInfo($TypeDeclId);
            if(my $NS = getNameSpace($TypeDeclId))
            {
                my $TypeName = $TypeAttr{"Name"};
                if($NS=~/\A(struct |union |class |)((.+)::|)\Q$TypeName\E\Z/)
                {# "some_type" is the typedef to "struct some_type" in C++
                    if($3) {
                        $TypeAttr{"Name"} = $3."::".$TypeName;
                    }
                }
                else
                {
                    $TypeAttr{"NameSpace"} = $NS;
                    $TypeAttr{"Name"} = $TypeAttr{"NameSpace"}."::".$TypeAttr{"Name"};
                    if($TypeAttr{"NameSpace"}=~/\Astd(::|\Z)/ and $BaseTypeAttr{"NameSpace"}=~/\Astd(::|\Z)/
                    and $BaseTypeAttr{"Name"}=~/</ and $TypeAttr{"Name"}!~/>(::\w+)+\Z/)
                    {
                        $StdCxxTypedef{$Version}{$BaseTypeAttr{"Name"}} = $TypeAttr{"Name"};
                        if(length($TypeAttr{"Name"})<=length($BaseTypeAttr{"Name"})) {
                            $Typedef_Equivalent{$Version}{$BaseTypeAttr{"Name"}} = $TypeAttr{"Name"};
                        }
                    }
                }
            }
            if($TypeAttr{"Name"} ne $BaseTypeAttr{"Name"}
            and $TypeAttr{"Name"}!~/>(::\w+)+\Z/ and $BaseTypeAttr{"Name"}!~/>(::\w+)+\Z/)
            {
                $Typedef_BaseName{$Version}{$TypeAttr{"Name"}} = $BaseTypeAttr{"Name"};
                $Typedef_Translations{$Version}{$BaseTypeAttr{"Name"}}{$TypeAttr{"Name"}} = 1;
            }
            ($TypeAttr{"Header"}, $TypeAttr{"Line"}) = getLocation($TypeDeclId);
        }
        if(not $TypeAttr{"Size"})
        {
            if($TypeAttr{"Type"} eq "Pointer") {
                $TypeAttr{"Size"} = $WORD_SIZE{$Version};
            }
            else {
                $TypeAttr{"Size"} = $BaseTypeAttr{"Size"};
            }
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
        if(not $TypeAttr{"Header"} and $BaseTypeAttr{"Header"})  {
            $TypeAttr{"Header"} = $BaseTypeAttr{"Header"};
        }
        %{$TypeInfo{$Version}{$TypeDeclId}{$TypeId}} = %TypeAttr;
        if(not $TName_Tid{$Version}{$TypeAttr{"Name"}}) {
            $TName_Tid{$Version}{$TypeAttr{"Name"}} = $TypeId;
        }
        return %TypeAttr;
    }
}

sub get_TemplateParam($)
{
    my $Type_Id = $_[0];
    return "" if(not $Type_Id);
    if($Cache{"get_TemplateParam"}{$Type_Id}) {
        return $Cache{"get_TemplateParam"}{$Type_Id};
    }
    if(getNodeType($Type_Id) eq "integer_cst")
    {# int (1), unsigned (2u), char ('c' as 99), ...
        my $CstTid = getTreeAttr($Type_Id, "type");
        my %CstType = getTypeAttr(getTypeDeclId($CstTid), $CstTid);
        my $Num = getNodeIntCst($Type_Id);
        if(my $CstSuffix = $ConstantSuffix{$CstType{"Name"}}) {
            return $Num.$CstSuffix;
        }
        else {
            return "(".$CstType{"Name"}.")".$Num;
        }
    }
    elsif(getNodeType($Type_Id) eq "string_cst") {
        return getNodeStrCst($Type_Id);
    }
    elsif(getNodeType($Type_Id) eq "tree_vec") {
        return "\@skip\@";
    }
    else
    {
        my $Type_DId = getTypeDeclId($Type_Id);
        my %ParamAttr = getTypeAttr($Type_DId, $Type_Id);
        if(not $ParamAttr{"Name"}) {
            return "";
        }
        if($ParamAttr{"Name"}=~/\>/)
        {
            if(my $Cover = cover_stdcxx_typedef($ParamAttr{"Name"})) {
                return $Cover;
            }
            else {
                return $ParamAttr{"Name"};
            }
        }
        else {
            return $ParamAttr{"Name"};
        }
    }
}

sub cover_stdcxx_typedef($)
{
    my $TypeName = $_[0];
    if(my $Cover = $StdCxxTypedef{$Version}{$TypeName}) {
        return $Cover;
    }
    my $TypeName_Covered = $TypeName;
    while($TypeName=~s/>[ ]*(const|volatile|restrict| |\*|\&)\Z/>/g){};
    if(my $Cover = $StdCxxTypedef{$Version}{$TypeName})
    {
        $TypeName_Covered=~s/(\W|\A)\Q$TypeName\E(\W|\Z)/$1$Cover$2/g;
        $TypeName_Covered=~s/(\W|\A)\Q$TypeName\E(\w|\Z)/$1$Cover $2/g;
    }
    return correctName($TypeName_Covered);
}

sub getNodeType($)
{
    return $LibInfo{$Version}{"info_type"}{$_[0]};
}

sub getNodeIntCst($)
{
    my $CstId = $_[0];
    my $CstTypeId = getTreeAttr($CstId, "type");
    if($EnumMembName_Id{$Version}{$CstId}) {
        return $EnumMembName_Id{$Version}{$CstId};
    }
    elsif((my $Value = getTreeValue($CstId)) ne "")
    {
        if($Value eq "0") {
            if(getNodeType($CstTypeId) eq "boolean_type") {
                return "false";
            }
            else {
                return "0";
            }
        }
        elsif($Value eq "1") {
            if(getNodeType($CstTypeId) eq "boolean_type") {
                return "true";
            }
            else {
                return "1";
            }
        }
        else {
            return $Value;
        }
    }
    else {
        return "";
    }
}

sub getNodeStrCst($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/strg[ ]*: (.+) lngt:[ ]*(\d+)/)
    { # string length is N-1 because of the null terminator
        return substr($1, 0, $2-1);
    }
    else {
        return "";
    }
}

sub getMemPtrAttr($$$$)
{# function, method and field pointers
    my ($PtrId, $TypeDeclId, $TypeId, $Type) = @_;
    my $MemInfo = $LibInfo{$Version}{"info"}{$PtrId};
    if($Type eq "FieldPtr") {
        $MemInfo = $LibInfo{$Version}{"info"}{$TypeId};
    }
    my $MemInfo_Type = $LibInfo{$Version}{"info_type"}{$PtrId};
    my $MemPtrName = "";
    my %TypeAttr = ("Size"=>$WORD_SIZE{$Version}, "Type"=>$Type, "TDid"=>$TypeDeclId, "Tid"=>$TypeId);
    # Return
    if($Type eq "FieldPtr")
    {
        my %ReturnAttr = getTypeAttr(getTypeDeclId($PtrId), $PtrId);
        $MemPtrName .= $ReturnAttr{"Name"};
        $TypeAttr{"Return"} = $PtrId;
    }
    else
    {
        if($MemInfo=~/retn[ ]*:[ ]*\@(\d+) /)
        {
            my $ReturnTypeId = $1;
            my %ReturnAttr = getTypeAttr(getTypeDeclId($ReturnTypeId), $ReturnTypeId);
            $MemPtrName .= $ReturnAttr{"Name"};
            $TypeAttr{"Return"} = $ReturnTypeId;
        }
    }
    # Class
    if($MemInfo=~/(clas|cls)[ ]*:[ ]*@(\d+) /)
    {
        $TypeAttr{"Class"} = $2;
        my %Class = getTypeAttr(getTypeDeclId($TypeAttr{"Class"}), $TypeAttr{"Class"});
        if($Class{"Name"}) {
            $MemPtrName .= " (".$Class{"Name"}."\:\:*)";
        }
        else {
            $MemPtrName .= " (*)";
        }
    }
    else {
        $MemPtrName .= " (*)";
    }
    # Parameters
    if($Type eq "FuncPtr"
    or $Type eq "MethodPtr")
    {
        my @ParamTypeName = ();
        if($MemInfo=~/prms[ ]*:[ ]*@(\d+) /)
        {
            my $ParamTypeInfoId = $1;
            my $Position = 0;
            while($ParamTypeInfoId)
            {
                my $ParamTypeInfo = $LibInfo{$Version}{"info"}{$ParamTypeInfoId};
                last if($ParamTypeInfo!~/valu[ ]*:[ ]*@(\d+) /);
                my $ParamTypeId = $1;
                my %ParamAttr = getTypeAttr(getTypeDeclId($ParamTypeId), $ParamTypeId);
                last if($ParamAttr{"Name"} eq "void");
                if($Position!=0 or $Type ne "MethodPtr")
                {
                    $TypeAttr{"Param"}{$Position}{"type"} = $ParamTypeId;
                    push(@ParamTypeName, $ParamAttr{"Name"});
                }
                last if($ParamTypeInfo!~/(chan|chain)[ ]*:[ ]*@(\d+) /);
                $ParamTypeInfoId = $2;
                $Position+=1;
            }
        }
        $MemPtrName .= " (".join(", ", @ParamTypeName).")";
    }
    $TypeAttr{"Name"} = correctName($MemPtrName);
    return %TypeAttr;
}

sub getTreeTypeName($)
{
    my $Info = $LibInfo{$Version}{"info"}{$_[0]};
    if($Info=~/name[ ]*:[ ]*@(\d+) /) {
        return getNameByInfo($1);
    }
    else
    {
        if($LibInfo{$Version}{"info_type"}{$_[0]} eq "integer_type")
        {
            if($LibInfo{$Version}{"info"}{$_[0]}=~/unsigned/) {
                return "unsigned int";
            }
            else {
                return "int";
            }
        }
        else {
            return "";
        }
    }
}

sub getQual($)
{
    my $TypeId = $_[0];
    if($LibInfo{$Version}{"info"}{$TypeId}=~ /qual[ ]*:[ ]*(r|c|v)/)
    {
        my $Qual = $1;
        if($Qual eq "r") {
            return "restrict";
        }
        elsif($Qual eq "v") {
            return "volatile";
        }
        elsif($Qual eq "c") {
            return "const";
        }
    }
    return "";
}

sub selectBaseType($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    if($MissedTypedef{$Version}{$TypeId}{"TDid"}
    and $MissedTypedef{$Version}{$TypeId}{"TDid"} eq $TypeDeclId) {
        return ($TypeId, getTypeDeclId($TypeId), "");
    }
    my $TInfo = $LibInfo{$Version}{"info"}{$TypeId};
    if($TInfo=~/unql[ ]*:/ and $TInfo=~/qual[ ]*:/
    and $TInfo=~/name[ ]*:[ ]*\@(\d+) /
    and (getTypeId($1) ne $TypeId)) {
        return (getTypeId($1), $1, getQual($TypeId));
    }
    elsif($TInfo!~/qual[ ]*:/
    and $TInfo=~/unql[ ]*:[ ]*\@(\d+) /
    and getNameByInfo($TypeDeclId))
    {# typedefs
        return ($1, getTypeDeclId($1), "");
    }
    elsif($TInfo=~/qual[ ]*:[ ]*c /
    and $TInfo=~/unql[ ]*:[ ]*\@(\d+) /) {
        return ($1, getTypeDeclId($1), "const");
    }
    elsif($TInfo=~/qual[ ]*:[ ]*r /
    and $TInfo=~/unql[ ]*:[ ]*\@(\d+) /) {
        return ($1, getTypeDeclId($1), "restrict");
    }
    elsif($TInfo=~/qual[ ]*:[ ]*v /
    and $TInfo=~/unql[ ]*:[ ]*\@(\d+) /) {
        return ($1, getTypeDeclId($1), "volatile");
    }
    elsif($LibInfo{$Version}{"info_type"}{$TypeId} eq "reference_type")
    {
        if($TInfo=~/refd[ ]*:[ ]*@(\d+) /) {
            return ($1, getTypeDeclId($1), "&");
        }
        else {
            return (0, 0, "");
        }
    }
    elsif($LibInfo{$Version}{"info_type"}{$TypeId} eq "array_type")
    {
        if($TInfo=~/elts[ ]*:[ ]*@(\d+) /) {
            return ($1, getTypeDeclId($1), "");
        }
        else {
            return (0, 0, "");
        }
    }
    elsif($LibInfo{$Version}{"info_type"}{$TypeId} eq "pointer_type")
    {
        if($TInfo=~/ptd[ ]*:[ ]*@(\d+) /) {
            return ($1, getTypeDeclId($1), "*");
        }
        else {
            return (0, 0, "");
        }
    }
    else {
        return (0, 0, "");
    }
}

sub getSymbolInfo_All()
{
    foreach (sort {int($b)<=>int($a)} keys(%{$LibInfo{$Version}{"info"}}))
    {
        if($LibInfo{$Version}{"info_type"}{$_} eq "function_decl") {
            getSymbolInfo("$_");
        }
    }
}

sub getVarDescr_All()
{
    foreach (sort {int($b)<=>int($a)} keys(%{$LibInfo{$Version}{"info"}}))
    {
        if($LibInfo{$Version}{"info_type"}{$_} eq "var_decl") {
            getVarDescr("$_");
        }
    }
}

sub getVarDescr($)
{
    my $InfoId = $_[0];
    if($LibInfo{$Version}{"info_type"}{getNameSpaceId($InfoId)} eq "function_decl") {
        return;
    }
    ($SymbolInfo{$Version}{$InfoId}{"Header"}, $SymbolInfo{$Version}{$InfoId}{"Line"}) = getLocation($InfoId);
    if(not $SymbolInfo{$Version}{$InfoId}{"Header"}
    or $SymbolInfo{$Version}{$InfoId}{"Header"}=~/\<built\-in\>|\<internal\>|\A\./) {
        delete($SymbolInfo{$Version}{$InfoId});
        return;
    }
    my $ShortName = $SymbolInfo{$Version}{$InfoId}{"ShortName"} = getVarShortName($InfoId);
    if($SymbolInfo{$Version}{$InfoId}{"ShortName"}=~/\Atmp_add_class_\d+\Z/) {
        delete($SymbolInfo{$Version}{$InfoId});
        return;
    }
    $SymbolInfo{$Version}{$InfoId}{"MnglName"} = getFuncMnglName($InfoId);
    if($SymbolInfo{$Version}{$InfoId}{"MnglName"}
    and $SymbolInfo{$Version}{$InfoId}{"MnglName"}!~/\A_Z/)
    {# validate mangled name
        delete($SymbolInfo{$Version}{$InfoId});
        return;
    }
    $SymbolInfo{$Version}{$InfoId}{"Data"} = 1;
    set_Class_And_Namespace($InfoId);
    if($COMMON_LANGUAGE{$Version} eq "C++" or $CheckHeadersOnly)
    {
        if(not $SymbolInfo{$Version}{$InfoId}{"MnglName"})
        {# for some symbols (_ZTI) the short name is the mangled name
            if($ShortName=~/\A_Z/) {
                $SymbolInfo{$Version}{$InfoId}{"MnglName"} = $ShortName;
            }
        }
        if(not $SymbolInfo{$Version}{$InfoId}{"MnglName"})
        {# try to mangle symbol (link with libraries)
            $SymbolInfo{$Version}{$InfoId}{"MnglName"} = linkSymbol($InfoId);
        }
        if($OStarget eq "windows")
        {
            if(my $Mangled = $mangled_name{$Version}{synchTr(construct_signature($InfoId))})
            { # link MS C++ symbols from library with GCC symbols from headers
                $SymbolInfo{$Version}{$InfoId}{"MnglName"} = $Mangled;
            }
        }
    }
    if(not $SymbolInfo{$Version}{$InfoId}{"MnglName"}) {
        $SymbolInfo{$Version}{$InfoId}{"MnglName"} = $ShortName;
    }
    if(not link_symbol($SymbolInfo{$Version}{$InfoId}{"MnglName"}, $Version, "-Deps")
    and not $CheckHeadersOnly)
    {
        if(link_symbol($ShortName, $Version, "-Deps")
        and $SymbolInfo{$Version}{$InfoId}{"MnglName"}=~/_ZL\d+$ShortName\Z/)
        {# const int global_data is mangled as _ZL11global_data in the tree
            $SymbolInfo{$Version}{$InfoId}{"MnglName"} = $ShortName;
        }
        else {
            delete($SymbolInfo{$Version}{$InfoId});
            return;
        }
    }
    if(not $SymbolInfo{$Version}{$InfoId}{"MnglName"}) {
        delete($SymbolInfo{$Version}{$InfoId});
        return;
    }
    $SymbolInfo{$Version}{$InfoId}{"Return"} = getTypeId($InfoId);
    delete($SymbolInfo{$Version}{$InfoId}{"Return"}) if(not $SymbolInfo{$Version}{$InfoId}{"Return"});
    setFuncAccess($InfoId);
    if($SymbolInfo{$Version}{$InfoId}{"MnglName"}=~/\A_ZTV/) {
        delete($SymbolInfo{$Version}{$InfoId}{"Return"});
    }
    if($ShortName=~/\A(_Z|\?)/) {
        delete($SymbolInfo{$Version}{$InfoId}{"ShortName"});
    }
}

sub getTrivialName($$)
{
    my ($TypeInfoId, $TypeId) = @_;
    my %TypeAttr = ();
    $TypeAttr{"Name"} = getNameByInfo($TypeInfoId);
    if(not $TypeAttr{"Name"}) {
        $TypeAttr{"Name"} = getTreeTypeName($TypeId);
    }
    $TypeAttr{"Name"}=~s/<(.+)\Z//g; # GCC 3.4.4 add template params to the name
    if(my $NameSpaceId = getNameSpaceId($TypeInfoId)) {
        if($NameSpaceId ne $TypeId) {
            $TypeAttr{"NameSpace"} = getNameSpace($TypeInfoId);
        }
    }
    if($TypeAttr{"NameSpace"} and isNotAnon($TypeAttr{"Name"})) {
        $TypeAttr{"Name"} = $TypeAttr{"NameSpace"}."::".$TypeAttr{"Name"};
    }
    $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
    if(isAnon($TypeAttr{"Name"}))
    {# anon-struct-header.h-line
        $TypeAttr{"Name"} = "anon-".lc(getTypeType($TypeInfoId, $TypeId))."-";
        $TypeAttr{"Name"} .= $TypeAttr{"Header"}."-".$TypeAttr{"Line"};
    }
    if(defined $TemplateInstance{$Version}{$TypeInfoId}{$TypeId})
    {
        my @TParams = getTParams($TypeInfoId, $TypeId);
        if(not @TParams)
        { # template declarations with abstract params
            # vector (tree_vec) of template_type_parm nodes in the TU dump
            return ("", "");
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"}."< ".join(", ", @TParams)." >");
    }
    return ($TypeAttr{"Name"}, $TypeAttr{"NameSpace"});
}

sub getTrivialTypeAttr($$)
{
    my ($TypeInfoId, $TypeId) = @_;
    my %TypeAttr = ();
    if(getTypeTypeByTypeId($TypeId)!~/\A(Intrinsic|Union|Struct|Enum|Class)\Z/) {
        return ();
    }
    setTypeAccess($TypeId, \%TypeAttr);
    ($TypeAttr{"Header"}, $TypeAttr{"Line"}) = getLocation($TypeInfoId);
    if($TypeAttr{"Header"}=~/\<built\-in\>|\<internal\>|\A\./)
    {
        delete($TypeAttr{"Header"});
        delete($TypeAttr{"Line"});
    }
    $TypeAttr{"Type"} = getTypeType($TypeInfoId, $TypeId);
    ($TypeAttr{"Name"}, $TypeAttr{"NameSpace"}) = getTrivialName($TypeInfoId, $TypeId);
    if(not $TypeAttr{"Name"}) {
        return ();
    }
    if(not $TypeAttr{"NameSpace"}) {
        delete($TypeAttr{"NameSpace"});
    }
    if(isAnon($TypeAttr{"Name"}))
    {
        $TypeAttr{"Name"} = "anon-".lc($TypeAttr{"Type"})."-";
        $TypeAttr{"Name"} .= $TypeAttr{"Header"}."-".$TypeAttr{"Line"};
    }
    $TypeAttr{"Size"} = getSize($TypeId)/$BYTE_SIZE;
    if($TypeAttr{"Type"} eq "Struct"
    and detect_lang($TypeId))
    {
        $TypeAttr{"Type"} = "Class";
        $TypeAttr{"Copied"} = 1;# default, will be changed in getSymbolInfo()
    }
    if($TypeAttr{"Type"} eq "Struct"
    or $TypeAttr{"Type"} eq "Class") {
        setBaseClasses($TypeInfoId, $TypeId, \%TypeAttr);
    }
    setTypeMemb($TypeId, \%TypeAttr);
    $TypeAttr{"Tid"} = $TypeId;
    $TypeAttr{"TDid"} = $TypeInfoId;
    if($TypeInfoId) {
        $Tid_TDid{$Version}{$TypeId} = $TypeInfoId;
    }
    if(not $TName_Tid{$Version}{$TypeAttr{"Name"}}) {
        $TName_Tid{$Version}{$TypeAttr{"Name"}} = $TypeId;
    }
    if(my $VTable = $ClassVTable_Content{$Version}{$TypeAttr{"Name"}})
    {
        my @Entries = split(/\n/, $VTable);
        foreach (1 .. $#Entries)
        {
            my $Entry = $Entries[$_];
            if($Entry=~/\A(\d+)\s+(.+)\Z/) {
                $TypeAttr{"VTable"}{$1} = $2;
            }
        }
    }
    return %TypeAttr;
}

sub detect_lang($)
{
    my $TypeId = $_[0];
    my $Info = $LibInfo{$Version}{"info"}{$TypeId};
    if(check_gcc_version($GCC_PATH, "4"))
    {# GCC 4 fncs-node points to only non-artificial methods
        return ($Info=~/(fncs)[ ]*:[ ]*@(\d+) /);
    }
    else
    {# GCC 3
        my $Fncs = getTreeAttr($TypeId, "fncs");
        while($Fncs)
        {
            my $Info = $LibInfo{$Version}{"info"}{$Fncs};
            if($Info!~/artificial/) {
                return 1;
            }
            $Fncs = getTreeAttr($Fncs, "chan");
        }
    }
    return 0;
}

sub setBaseClasses($$$)
{
    my ($TypeInfoId, $TypeId, $TypeAttr) = @_;
    my $Info = $LibInfo{$Version}{"info"}{$TypeId};
    if($Info=~/binf[ ]*:[ ]*@(\d+) /)
    {
        $Info = $LibInfo{$Version}{"info"}{$1};
        my $Pos = 0;
        while($Info=~s/(pub|public|prot|protected|priv|private|)[ ]+binf[ ]*:[ ]*@(\d+) //)
        {
            my ($Access, $BInfoId) = ($1, $2);
            my $ClassId = getBinfClassId($BInfoId);
            my $BaseInfo = $LibInfo{$Version}{"info"}{$BInfoId};
            if($Access=~/prot/)
            {
                $TypeAttr->{"Base"}{$ClassId}{"access"} = "protected";
            }
            elsif($Access=~/priv/)
            {
                $TypeAttr->{"Base"}{$ClassId}{"access"} = "private";
            }
            $TypeAttr->{"Base"}{$ClassId}{"pos"} = $Pos++;
            if($BaseInfo=~/virt/)
            {# virtual base
                $TypeAttr->{"Base"}{$ClassId}{"virtual"} = 1;
            }
            $Class_SubClasses{$Version}{$ClassId}{$TypeId}=1;
        }
    }
}

sub getBinfClassId($)
{
    my $Info = $LibInfo{$Version}{"info"}{$_[0]};
    $Info=~/type[ ]*:[ ]*@(\d+) /;
    return $1;
}

sub construct_signature($)
{
    my $InfoId = $_[0];
    if($Cache{"construct_signature"}{$Version}{$InfoId}) {
        return $Cache{"construct_signature"}{$Version}{$InfoId};
    }
    my $PureSignature = $SymbolInfo{$Version}{$InfoId}{"ShortName"};
    if($SymbolInfo{$Version}{$InfoId}{"Destructor"}) {
        $PureSignature = "~".$PureSignature;
    }
    if(not $SymbolInfo{$Version}{$InfoId}{"Data"})
    {
        my @ParamTypes = ();
        foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$SymbolInfo{$Version}{$InfoId}{"Param"}}))
        {# checking parameters
            my $ParamType_Id = $SymbolInfo{$Version}{$InfoId}{"Param"}{$ParamPos}{"type"};
            my $ParamType_Name = uncover_typedefs(get_TypeName($ParamType_Id, $Version), $Version);
            if($OSgroup eq "windows") {
                $ParamType_Name=~s/(\W|\A)long long(\W|\Z)/$1__int64$2/;
            }
            @ParamTypes = (@ParamTypes, $ParamType_Name);
        }
        $PureSignature .= "(".join(", ", @ParamTypes).")";
        $PureSignature = delete_keywords($PureSignature);
    }
    if(my $ClassId = $SymbolInfo{$Version}{$InfoId}{"Class"}) {
        $PureSignature = get_TypeName($ClassId, $Version)."::".$PureSignature;
    }
    elsif(my $NS = $SymbolInfo{$Version}{$InfoId}{"NameSpace"}) {
        $PureSignature = $NS."::".$PureSignature;
    }
    if($SymbolInfo{$Version}{$InfoId}{"Const"}) {
        $PureSignature .= " const";
    }
    if(defined $TemplateInstance_Func{$Version}{$InfoId}
    and keys(%{$TemplateInstance_Func{$Version}{$InfoId}}))
    { # mangled names for template function specializations include return value
        if(my $Return = $SymbolInfo{$Version}{$InfoId}{"Return"}) {
            $PureSignature = get_TypeName($Return, $Version)." ".$PureSignature;
        }
    }
    return ($Cache{"construct_signature"}{$Version}{$InfoId} = correctName($PureSignature));
}

sub mangle_symbol($$)
{ # mangling for simple methods
  # see gcc-4.6.0/gcc/cp/mangle.c
    my ($InfoId, $Compiler) = @_;
    if($Cache{"mangle_symbol"}{$Version}{$InfoId}{$Compiler}) {
        return $Cache{"mangle_symbol"}{$Version}{$InfoId}{$Compiler};
    }
    my $Mangled = "";
    if($Compiler eq "GCC") {
        $Mangled = mangle_symbol_gcc($InfoId);
    }
    elsif($Compiler eq "MSVC") {
        $Mangled = mangle_symbol_msvc($InfoId);
    }
    return ($Cache{"mangle_symbol"}{$Version}{$InfoId}{$Compiler} = $Mangled);
}

sub mangle_symbol_msvc($)
{
    my $InfoId = $_[0];
    return "";
}

sub mangle_symbol_gcc($)
{ # see gcc-4.6.0/gcc/cp/mangle.c
    my $InfoId = $_[0];
    my ($Mangled, $ClassId, $NameSpace) = ("_Z", 0, "");
    my %Repl = ();# SN_ replacements
    if($ClassId = $SymbolInfo{$Version}{$InfoId}{"Class"})
    {
        my $MangledClass = mangle_param($ClassId, \%Repl);
        if($MangledClass!~/\AN/) {
            $MangledClass = "N".$MangledClass;
        }
        else {
            $MangledClass=~s/E\Z//;
        }
        if($SymbolInfo{$Version}{$InfoId}{"Const"}) {
            $MangledClass=~s/\AN/NK/;
        }
        
        $Mangled .= $MangledClass;
    }
    elsif($NameSpace = $SymbolInfo{$Version}{$InfoId}{"NameSpace"})
    {
        my $MangledNS = "";
        my @Parts = split(/::/, $NameSpace);
        foreach my $Num (0 .. $#Parts)
        {
            $MangledNS .= length($Parts[$Num]).$Parts[$Num];
            add_substitution($MangledNS, \%Repl);
        }
        $Mangled .= "N".$MangledNS;
    }
    my @TParams = getTParams_Func($InfoId);
    if($SymbolInfo{$Version}{$InfoId}{"Constructor"}) {
        $Mangled .= "C1";
    }
    elsif($SymbolInfo{$Version}{$InfoId}{"Destructor"}) {
        $Mangled .= "D0";
    }
    elsif(my $ShortName = $SymbolInfo{$Version}{$InfoId}{"ShortName"})
    {
        if($NameSpace eq "__gnu_cxx"
        and not $ClassId)
        { # _ZN9__gnu_cxxL25__exchange_and_add_singleEPii
          # _ZN9__gnu_cxxL19__atomic_add_singleEPii
            $Mangled .= "L";
        }
        $ShortName=~s/ <.+>\Z//g;#templates
        if($ShortName=~/\Aoperator(\W.*)\Z/)
        {
            my $Op = $1;
            $Op=~s/\A[ ]+//g;
            if(my $OpMngl = $OperatorMangling{$Op}) {
                $Mangled .= $OpMngl;
            }
            else {# conversion operator
                $Mangled .= "cv".mangle_param(getTypeIdByName($Op, $Version), \%Repl);
            }
        }
        else {
            $Mangled .= length($ShortName).$ShortName;
        }
        if(@TParams)
        {# templates
            $Mangled .= "I";
            foreach my $TParam (@TParams) {
                $Mangled .= mangle_template_param($TParam, \%Repl);
            }
            $Mangled .= "E";
        }
        if(not $ClassId) {
            add_substitution($ShortName, \%Repl);
        }
    }
    if($ClassId or $NameSpace) {
        $Mangled .= "E";
    }
    if(@TParams) {
        if(my $Return = $SymbolInfo{$Version}{$InfoId}{"Return"}) {
            $Mangled .= mangle_param($Return, \%Repl);
        }
    }
    if(not $SymbolInfo{$Version}{$InfoId}{"Data"})
    {
        my @Params = keys(%{$SymbolInfo{$Version}{$InfoId}{"Param"}});
        foreach my $ParamPos (sort {int($a) <=> int($b)} @Params)
        {# checking parameters
            my $ParamType_Id = $SymbolInfo{$Version}{$InfoId}{"Param"}{$ParamPos}{"type"};
            $Mangled .= mangle_param($ParamType_Id, \%Repl);
        }
        if(not @Params) {
            $Mangled .= "v";
        }
    }
    # optimization: one of SN_ is the superposition of others
    my %Dups = ();
    foreach my $RType1 (sort {$Repl{$b}<=>$Repl{$a}} keys(%Repl))
    {
        my $Num1 = $Repl{$RType1};
        my $Converted = $RType1;
        foreach my $RType2 (keys(%Repl))
        {
            next if($RType1 eq $RType2);
            my $Replace = macro_mangle($Repl{$RType2});
            $Converted=~s/\Q$RType2\E/$Replace/g;
        }
        foreach my $RType2 (sort {$Repl{$a}<=>$Repl{$b}} keys(%Repl))
        {
            my $Num2 = $Repl{$RType2};
            if($Num1!=$Num2)
            {
                if($Converted eq $RType2
                or $Converted eq macro_mangle($Num2)) {
                    $Dups{$RType2}=macro_mangle($Num1);
                }
            }
        }
    }
    foreach my $Dup (keys(%Dups))
    {
        my $Replace = $Dups{$Dup};
        $Mangled=~s/\Q$Dup\E/$Replace/g;
    }
    $Mangled = correct_incharge($InfoId, $Mangled);
    $Mangled = write_stdcxx_substitution($Mangled);
    return $Mangled;
}

sub correct_incharge($$)
{
    my ($InfoId, $Mangled) = @_;
    if($SymbolInfo{$Version}{$InfoId}{"Constructor"})
    {
        if($MangledNames{$Version}{$Mangled}) {
            $Mangled=~s/C1E/C2E/;
        }
    }
    elsif($SymbolInfo{$Version}{$InfoId}{"Destructor"})
    {
        if($MangledNames{$Version}{$Mangled}) {
            $Mangled=~s/D0E/D1E/;
        }
        if($MangledNames{$Version}{$Mangled}) {
            $Mangled=~s/D1E/D2E/;
        }
    }
    return $Mangled;
}

sub mangle_param($$)
{
    my ($PTid, $Repl) = @_;
    my ($MPrefix, $Mangled) = ("", "");
    my %ReplCopy = %{$Repl};
    my %BaseType = get_BaseType($Tid_TDid{$Version}{$PTid}, $PTid, $Version);
    my $BaseType_Name = $BaseType{"Name"};
    my $ShortName = $BaseType_Name;
    $ShortName=~s/<(.*)\Z//g;#templates
    my $Suffix = get_BaseTypeQual($Tid_TDid{$Version}{$PTid}, $PTid, $Version);
    $Suffix=~s/(const|volatile)\Z//g;
    while($Suffix=~/(&|\*|const)\Z/)
    {
        if($Suffix=~s/[ ]*&\Z//) {
            $MPrefix .= "R";
        }
        if($Suffix=~s/[ ]*\*\Z//) {
            $MPrefix .= "P";
        }
        if($Suffix=~s/[ ]*const\Z//)
        {
            if($MPrefix=~/R|P/
            or $Suffix=~/&|\*/) {
                $MPrefix .= "K";
            }
        }
        if($Suffix=~s/[ ]*volatile\Z//) {
            $MPrefix .= "V";
        }
        if($Suffix=~s/[ ]*restrict\Z//) {
            $MPrefix .= "r";
        }
    }
    if(my $Token = $IntrinsicMangling{$BaseType_Name}) {
        $Mangled .= $Token;
    }
    elsif($BaseType{"Type"}=~/(Class|Struct|Union|Enum)/)
    {
        $Mangled .= "N";
        my @TParams = getTParams($BaseType{"TDid"}, $BaseType{"Tid"});
        my $MangledNS = "";
        my @Parts = split(/::/, $ShortName);
        foreach my $Num (0 .. $#Parts)
        {
            $MangledNS .= length($Parts[$Num]).$Parts[$Num];
            if($Num!=$#Parts or @TParams) {
                add_substitution($MangledNS, $Repl);
            }
        }
        $Mangled .= $MangledNS;
        if(@TParams)
        {# templates
            $Mangled .= "I";
            foreach my $TParam (@TParams) {
                $Mangled .= mangle_template_param($TParam, $Repl);
            }
            $Mangled .= "E";
        }
        $Mangled .= "E";
    }
    elsif($BaseType{"Type"}=~/(FuncPtr|MethodPtr)/)
    {
        if($BaseType{"Type"} eq "MethodPtr") {
            $Mangled .= "M".mangle_param($BaseType{"Class"}, $Repl)."F";
        }
        else {
            $Mangled .= "PF";
        }
        $Mangled .= mangle_param($BaseType{"Return"}, $Repl);
        foreach my $Num (sort {int($a)<=>int($b)} keys(%{$BaseType{"Param"}})) {
            $Mangled .= mangle_param($BaseType{"Param"}{$Num}{"type"}, $Repl);
        }
        $Mangled .= "E";
    }
    elsif($BaseType{"Type"} eq "FieldPtr")
    {
        $Mangled .= "M".mangle_param($BaseType{"Class"}, $Repl);
        $Mangled .= mangle_param($BaseType{"Return"}, $Repl);
    }
    if(my $Optimized = write_substitution($Mangled, \%ReplCopy))
    {
        if($Mangled eq $Optimized)
        {
            if($ShortName!~/::/) {
                $Mangled=~s/\AN(.+)E\Z/$1/g;
            }
        }
        else {
            $Mangled = $Optimized;
        }
    }
    $Mangled = $MPrefix.$Mangled;# add prefix (RPK)
    add_substitution($Mangled, $Repl);
    return $Mangled;
}

sub mangle_template_param($$)
{# types + literals
    my ($TParam, $Repl) = @_;
    if(my $TPTid = $TName_Tid{$Version}{$TParam}) {
        return mangle_param($TPTid, $Repl);
    }
    elsif($TParam=~/\A(\d+)(\w+)\Z/)
    {# class_name<1u>::method(...)
        return "L".$IntrinsicMangling{$ConstantSuffixR{$2}}.$1."E";
    }
    elsif($TParam=~/\A\(([\w ]+)\)(\d+)\Z/)
    { # class_name<(signed char)1>::method(...)
        return "L".$IntrinsicMangling{$1}.$2."E";
    }
    elsif($TParam eq "true")
    { # class_name<true>::method(...)
        return "Lb1E";
    }
    elsif($TParam eq "false")
    { # class_name<true>::method(...)
        return "Lb0E";
    }
    else { # internal error
        return length($TParam).$TParam;
    }
}

sub add_substitution($$)
{
    my ($Value, $Repl) = @_;
    return if($Value=~/\AS(\d*)_\Z/);
    return if(defined $Repl->{$Value});
    return if(length($Value)<=1);
    return if($StdcxxMangling{$Value});
    $Value=~s/\AN(.+)E\Z/$1/g;
    my @Repls = sort {$b<=>$a} values(%{$Repl});
    if(@Repls) {
        $Repl->{$Value} = $Repls[0]+1;
    }
    else {
        $Repl->{$Value} = -1;
    }
}

sub macro_mangle($)
{
    my $Num = $_[0];
    if($Num==-1) {
        return "S_";
    }
    else {
        return "S".$Num."_";
    }
}

sub write_stdcxx_substitution($)
{
    my $Mangled = $_[0];
    if($StdcxxMangling{$Mangled}) {
        return $StdcxxMangling{$Mangled};
    }
    else
    {
        my @Repls = keys(%StdcxxMangling);
        @Repls = sort {length($b)<=>length($a)} @Repls;
        foreach my $MangledType (@Repls)
        {
            my $Replace = $StdcxxMangling{$MangledType};
            if($Mangled!~/$Replace/) {
                $Mangled=~s/N\Q$MangledType\EE/$Replace/g;
                $Mangled=~s/\Q$MangledType\E/$Replace/g;
            }
        }
    }
    return $Mangled;
}

sub write_substitution($$)
{
    my ($Mangled, $Repl) = @_;
    if(defined $Repl->{$Mangled}
    and my $MnglNum = $Repl->{$Mangled}) {
        $Mangled = macro_mangle($MnglNum);
    }
    else
    {
        my @Repls = keys(%{$Repl});
        #@Repls = sort {$Repl->{$a}<=>$Repl->{$b}} @Repls;
        # FIXME: how to apply replacements? by num or by pos
        @Repls = sort {length($b)<=>length($a)} @Repls;
        foreach my $MangledType (@Repls)
        {
            my $Replace = macro_mangle($Repl->{$MangledType});
            if($Mangled!~/$Replace/) {
                $Mangled=~s/N\Q$MangledType\EE/$Replace/g;
                $Mangled=~s/\Q$MangledType\E/$Replace/g;
            }
        }
    }
    return $Mangled;
}

sub delete_keywords($)
{
    my $TypeName = $_[0];
    $TypeName=~s/(\W|\A)(enum |struct |union |class )/$1/g;
    return $TypeName;
}

my %Intrinsic_Keywords = map {$_=>1} (
    "true",
    "false",
    "_Bool",
    "_Complex",
    "const",
    "int",
    "long",
    "void",
    "short",
    "float",
    "volatile",
    "restrict",
    "unsigned",
    "signed",
    "char",
    "double",
    "class",
    "struct",
    "union",
    "enum"
);

sub uncover_typedefs($$)
{
    my ($TypeName, $LibVersion) = @_;
    return "" if(not $TypeName);
    if(defined $Cache{"uncover_typedefs"}{$LibVersion}{$TypeName}) {
        return $Cache{"uncover_typedefs"}{$LibVersion}{$TypeName};
    }
    my ($TypeName_New, $TypeName_Pre) = (correctName($TypeName), "");
    while($TypeName_New ne $TypeName_Pre)
    {
        $TypeName_Pre = $TypeName_New;
        my $TypeName_Copy = $TypeName_New;
        my %Words = ();
        while($TypeName_Copy=~s/(\W|\A)([a-z_][\w:]*)(\W|\Z)//io)
        {
            my $Word = $2;
            next if(not $Word or $Intrinsic_Keywords{$Word});
            $Words{$Word} = 1;
        }
        foreach my $Word (keys(%Words))
        {
            my $BaseType_Name = $Typedef_BaseName{$LibVersion}{$Word};
            next if(not $BaseType_Name);
            next if($TypeName_New=~/(\A|\W)(struct|union|enum)\s\Q$Word\E(\W|\Z)/);
            if($BaseType_Name=~/\([\*]+\)/)
            {# FuncPtr
                if($TypeName_New=~/\Q$Word\E(.*)\Z/)
                {
                    my $Type_Suffix = $1;
                    $TypeName_New = $BaseType_Name;
                    if($TypeName_New=~s/\(([\*]+)\)/($1 $Type_Suffix)/) {
                        $TypeName_New = correctName($TypeName_New);
                    }
                }
            }
            else
            {
                if($TypeName_New=~s/(\W|\A)\Q$Word\E(\W|\Z)/$1$BaseType_Name$2/g) {
                    $TypeName_New = correctName($TypeName_New);
                }
            }
        }
    }
    $TypeName_New=~s/>>/> >/g; # double templates
    return ($Cache{"uncover_typedefs"}{$Version}{$LibVersion} = $TypeName_New);
}

sub isInternal($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    return 0 if($FuncInfo!~/mngl[ ]*:[ ]*@(\d+) /);
    my $FuncMnglNameInfoId = $1;
    return ($LibInfo{$Version}{"info"}{$FuncMnglNameInfoId}=~/\*[ ]*INTERNAL[ ]*\*/);
}

sub set_Class_And_Namespace($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$FuncInfoId};
    if($FuncInfo=~/scpe[ ]*:[ ]*@(\d+) /)
    {
        my $NameSpaceInfoId = $1;
        if($LibInfo{$Version}{"info_type"}{$NameSpaceInfoId} eq "namespace_decl") {
            $SymbolInfo{$Version}{$FuncInfoId}{"NameSpace"} = getNameSpace($FuncInfoId);
        }
        elsif($LibInfo{$Version}{"info_type"}{$NameSpaceInfoId} eq "record_type") {
            $SymbolInfo{$Version}{$FuncInfoId}{"Class"} = $NameSpaceInfoId;
        }
    }
}

sub linkSymbol($)
{ # link symbols from shared libraries
  # with the symbols from header files
    my $InfoId = $_[0];
    # try to mangle symbol
    if(not $SymbolInfo{$Version}{$InfoId}{"Class"}
    or not check_gcc_version($GCC_PATH, "4") or $CheckHeadersOnly)
    {
        if(my $Mangled = $mangled_name_gcc{construct_signature($InfoId)})
        { # for C++ functions that mangled in library, but are not mangled in TU dump (using GCC 4.x)
            $Mangled = correct_incharge($InfoId, $Mangled);
            return $Mangled;
        }
        elsif(my $Mangled = mangle_symbol($InfoId, "GCC"))
        { # 1. for GCC 3.4.4 that doesn't mangle names in the TU dump (only functions and global data)
          # 2. for --headers-only option
            return $Mangled;
        }
        else
        { # internal error
            return "";
        }
    }
    return "";
}

sub getTParams_Func($)
{
    my $FuncInfoId = $_[0];
    my @TmplParams = ();
    foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$TemplateInstance_Func{$Version}{$FuncInfoId}}))
    {
        my $Param = get_TemplateParam($TemplateInstance_Func{$Version}{$FuncInfoId}{$ParamPos});
        if($Param eq "") {
            delete($SymbolInfo{$Version}{$FuncInfoId});
            return;
        }
        elsif($Param ne "\@skip\@") {
            push(@TmplParams, $Param);
        }
    }
    return @TmplParams;
}

sub getSymbolInfo($)
{
    my $FuncInfoId = $_[0];
    return if(isInternal($FuncInfoId));
    ($SymbolInfo{$Version}{$FuncInfoId}{"Header"}, $SymbolInfo{$Version}{$FuncInfoId}{"Line"}) = getLocation($FuncInfoId);
    if(not $SymbolInfo{$Version}{$FuncInfoId}{"Header"}
    or $SymbolInfo{$Version}{$FuncInfoId}{"Header"}=~/\<built\-in\>|\<internal\>|\A\./) {
        delete($SymbolInfo{$Version}{$FuncInfoId});
        return;
    }
    setFuncAccess($FuncInfoId);
    setFuncKind($FuncInfoId);
    if($SymbolInfo{$Version}{$FuncInfoId}{"PseudoTemplate"}) {
        delete($SymbolInfo{$Version}{$FuncInfoId});
        return;
    }
    $SymbolInfo{$Version}{$FuncInfoId}{"Type"} = getFuncType($FuncInfoId);
    $SymbolInfo{$Version}{$FuncInfoId}{"Return"} = getFuncReturn($FuncInfoId);
    if(my $AddedTid = $MissedTypedef{$Version}{$SymbolInfo{$Version}{$FuncInfoId}{"Return"}}{"Tid"}) {
        $SymbolInfo{$Version}{$FuncInfoId}{"Return"} = $AddedTid;
    }
    if(not $SymbolInfo{$Version}{$FuncInfoId}{"Return"}) {
        delete($SymbolInfo{$Version}{$FuncInfoId}{"Return"});
    }
    $SymbolInfo{$Version}{$FuncInfoId}{"ShortName"} = getFuncShortName(getFuncOrig($FuncInfoId));
    if($SymbolInfo{$Version}{$FuncInfoId}{"ShortName"}=~/\._/) {
        delete($SymbolInfo{$Version}{$FuncInfoId});
        return;
    }
    if(defined $TemplateInstance_Func{$Version}{$FuncInfoId})
    {
        my $PrmsInLine = join(", ", getTParams_Func($FuncInfoId));
        if($PrmsInLine=~/>\Z/) {
            $PrmsInLine = " ".$PrmsInLine." ";
        }
        $SymbolInfo{$Version}{$FuncInfoId}{"ShortName"} .= " <".$PrmsInLine.">";
    }
    $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"} = getFuncMnglName($FuncInfoId);
    if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}
    and $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}!~/\A_Z/)
    {
        delete($SymbolInfo{$Version}{$FuncInfoId});
        return;
    }
    if($SymbolInfo{$FuncInfoId}{"MnglName"} and not $STDCXX_TESTING)
    {# stdc++ interfaces
        if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}=~/\A(_ZS|_ZNS|_ZNKS)/) {
            delete($SymbolInfo{$Version}{$FuncInfoId});
            return;
        }
    }
    if(not $SymbolInfo{$Version}{$FuncInfoId}{"Destructor"})
    {# destructors have an empty parameter list
        setFuncParams($FuncInfoId);
    }
    set_Class_And_Namespace($FuncInfoId);
    if((link_symbol($SymbolInfo{$Version}{$FuncInfoId}{"ShortName"}, $Version, "-Deps") or $CheckHeadersOnly)
    and $SymbolInfo{$Version}{$FuncInfoId}{"Type"} eq "Function") {
        # functions (C++): not mangled in library, but are mangled in TU dump
        if(not $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}
          or not link_symbol($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}, $Version, "-Deps")) {
            $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"} = $SymbolInfo{$Version}{$FuncInfoId}{"ShortName"};
        }
    }
    if($COMMON_LANGUAGE{$Version} eq "C++"
    or $CheckHeadersOnly)
    {# correct mangled & short names
        if($SymbolInfo{$Version}{$FuncInfoId}{"ShortName"}=~/\A__(comp|base|deleting)_(c|d)tor\Z/)
        { # support for old GCC versions: reconstruct real names for constructors and destructors
            $SymbolInfo{$Version}{$FuncInfoId}{"ShortName"} = getNameByInfo(getTypeDeclId($SymbolInfo{$Version}{$FuncInfoId}{"Class"}));
        }
        if(not $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"})
        {# try to mangle symbol (link with libraries)
            $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"} = linkSymbol($FuncInfoId);
        }
        if($OStarget eq "windows")
        { # link MS C++ symbols from library with GCC symbols from headers
            if(my $Mangled = $mangled_name{$Version}{synchTr(construct_signature($FuncInfoId))})
            { # exported symbols
                $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"} = $Mangled;
            }
            elsif(my $Mangled = mangle_symbol($FuncInfoId, "MSVC"))
            { # pure virtual symbols
                $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"} = $Mangled;
            }
        }
    }
    if(not $SymbolInfo{$Version}{$FuncInfoId}{"MnglName"})
    {# can't detect symbol name
        delete($SymbolInfo{$Version}{$FuncInfoId});
        return;
    }
    if(getFuncSpec($FuncInfoId) eq "Virt")
    {# virtual methods
        $SymbolInfo{$Version}{$FuncInfoId}{"Virt"} = 1;
    }
    if(getFuncSpec($FuncInfoId) eq "PureVirt")
    {# pure virtual methods
        $SymbolInfo{$Version}{$FuncInfoId}{"PureVirt"} = 1;
    }
    if(isInline($FuncInfoId)) {
        $SymbolInfo{$Version}{$FuncInfoId}{"InLine"} = 1;
    }
    if($SymbolInfo{$Version}{$FuncInfoId}{"Constructor"}
    and my $ClassId = $SymbolInfo{$Version}{$FuncInfoId}{"Class"})
    {
        if(not $SymbolInfo{$Version}{$FuncInfoId}{"InLine"}
        and $LibInfo{$Version}{"info"}{$FuncInfoId}!~/ artificial /i)
        {# inline or auto-generated constructor
            delete($TypeInfo{$Version}{$Tid_TDid{$Version}{$ClassId}}{$ClassId}{"Copied"});
        }
    }
    if(not link_symbol($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}, $Version, "-Deps")
    and not $SymbolInfo{$Version}{$FuncInfoId}{"Virt"}
    and not $SymbolInfo{$Version}{$FuncInfoId}{"PureVirt"})
    {# removing src only and external non-virtual functions
     # non-virtual template instances going here
        if(not $CheckHeadersOnly) {
            delete($SymbolInfo{$Version}{$FuncInfoId});
            return;
        }
    }
    if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"})
    {
        if($MangledNames{$Version}{$SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}})
        {# one instance for one mangled name only
            delete($SymbolInfo{$Version}{$FuncInfoId});
            return;
        }
        else {
            $MangledNames{$Version}{$SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}} = 1;
        }
    }
    if($SymbolInfo{$Version}{$FuncInfoId}{"Constructor"}
    or $SymbolInfo{$Version}{$FuncInfoId}{"Destructor"}) {
        delete($SymbolInfo{$Version}{$FuncInfoId}{"Return"});
    }
    if($SymbolInfo{$Version}{$FuncInfoId}{"Type"} eq "Method"
    or $SymbolInfo{$Version}{$FuncInfoId}{"Constructor"}
    or $SymbolInfo{$Version}{$FuncInfoId}{"Destructor"}) {
        if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}!~/\A(_Z|\?)/) {
            delete($SymbolInfo{$Version}{$FuncInfoId});
            return;
        }
    }
    if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}=~/\A(_Z|\?)/
    and $SymbolInfo{$Version}{$FuncInfoId}{"Class"})
    {
        if($SymbolInfo{$Version}{$FuncInfoId}{"Type"} eq "Function")
        {# static methods
            $SymbolInfo{$Version}{$FuncInfoId}{"Static"} = 1;
        }
    }
    if(getFuncLink($FuncInfoId) eq "Static") {
        $SymbolInfo{$Version}{$FuncInfoId}{"Static"} = 1;
    }
    if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}=~/\A(_Z|\?)/
    and $tr_name{$SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}}=~/\.\_\d/) {
        delete($SymbolInfo{$Version}{$FuncInfoId});
        return;
    }
    delete($SymbolInfo{$Version}{$FuncInfoId}{"Type"});
    if($SymbolInfo{$Version}{$FuncInfoId}{"MnglName"}=~/\A_ZNK/) {
        $SymbolInfo{$Version}{$FuncInfoId}{"Const"} = 1;
    }
}

sub isInline($)
{# "body: undefined" in the tree
 # -fkeep-inline-functions GCC option should be specified
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    if($FuncInfo=~/ undefined /i) {
        return 0;
    }
    return 1;
}

sub getTypeId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/type[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub setTypeMemb($$)
{
    my ($TypeId, $TypeAttr) = @_;
    my $TypeType = $TypeAttr->{"Type"};
    my ($Position, $UnnamedPos) = (0, 0);
    if($TypeType eq "Enum")
    {
        my $TypeMembInfoId = getEnumMembInfoId($TypeId);
        while($TypeMembInfoId)
        {
            $TypeAttr->{"Memb"}{$Position}{"value"} = getEnumMembVal($TypeMembInfoId);
            my $MembName = getEnumMembName($TypeMembInfoId);
            $TypeAttr->{"Memb"}{$Position}{"name"} = getEnumMembName($TypeMembInfoId);
            $EnumMembName_Id{$Version}{getTreeAttr($TypeMembInfoId, "valu")} = ($TypeAttr->{"NameSpace"})?$TypeAttr->{"NameSpace"}."::".$MembName:$MembName;
            $TypeMembInfoId = getNextMembInfoId($TypeMembInfoId);
            $Position += 1;
        }
    }
    elsif($TypeType=~/\A(Struct|Class|Union)\Z/)
    {
        my $TypeMembInfoId = getStructMembInfoId($TypeId);
        while($TypeMembInfoId)
        {
            if($LibInfo{$Version}{"info_type"}{$TypeMembInfoId} ne "field_decl") {
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            my $StructMembName = getStructMembName($TypeMembInfoId);
            if($StructMembName=~/_vptr\./)
            {# virtual tables
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            if(not $StructMembName)
            { # unnamed fields
                if($TypeAttr->{"Name"}!~/_type_info_pseudo/)
                {
                    my $UnnamedTid = getTreeAttr($TypeMembInfoId, "type");
                    my $UnnamedTName = getNameByInfo(getTypeDeclId($UnnamedTid));
                    if(isAnon($UnnamedTName))
                    { # rename unnamed fields to unnamed0, unnamed1, ...
                        $StructMembName = "unnamed".($UnnamedPos++);
                    }
                }
            }
            if(not $StructMembName)
            { # unnamed fields and base classes
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            my $MembTypeId = getTreeAttr($TypeMembInfoId, "type");
            if(my $AddedTid = $MissedTypedef{$Version}{$MembTypeId}{"Tid"}) {
                $MembTypeId = $AddedTid;
            }
            $TypeAttr->{"Memb"}{$Position}{"type"} = $MembTypeId;
            $TypeAttr->{"Memb"}{$Position}{"name"} = $StructMembName;
            if((my $Access = getTreeAccess($TypeMembInfoId)) ne "public")
            {# marked only protected and private, public by default
                $TypeAttr->{"Memb"}{$Position}{"access"} = $Access;
            }
            if(my $BFSize = getStructMembBitFieldSize($TypeMembInfoId)) {
                $TypeAttr->{"Memb"}{$Position}{"bitfield"} = $BFSize;
            }
            else
            { # set alignment for non-bit fields
              # alignment for bitfields is always equal to 1 bit
                $TypeAttr->{"Memb"}{$Position}{"algn"} = getAlgn($TypeMembInfoId)/$BYTE_SIZE;
            }
            $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
            $Position += 1;
        }
    }
}

sub setFuncParams($)
{
    my $FuncInfoId = $_[0];
    my $ParamInfoId = getFuncParamInfoId($FuncInfoId);
    my $FunctionType = getFuncType($FuncInfoId);
    if($FunctionType eq "Method")
    {
        my $ObjectTypeId = getFuncParamType($ParamInfoId);
        if(get_TypeName($ObjectTypeId, $Version)=~/ const\*const\Z/) {
            $SymbolInfo{$Version}{$FuncInfoId}{"Const"} = 1;
        }
        $ParamInfoId = getNextElem($ParamInfoId);
    }
    my ($Position, $Vtt_Pos) = (0, -1);
    while($ParamInfoId)
    {
        my $ParamTypeId = getFuncParamType($ParamInfoId);
        my $ParamName = getFuncParamName($ParamInfoId);
        last if($TypeInfo{$Version}{getTypeDeclId($ParamTypeId)}{$ParamTypeId}{"Name"} eq "void");
        if($TypeInfo{$Version}{getTypeDeclId($ParamTypeId)}{$ParamTypeId}{"Type"} eq "Restrict")
        {# delete restrict spec
            $ParamTypeId = getRestrictBase($ParamTypeId);
        }
        if(my $AddedTid = $MissedTypedef{$Version}{$ParamTypeId}{"Tid"}) {
            $ParamTypeId = $AddedTid;
        }
        if($ParamName eq "__vtt_parm"
        and get_TypeName($ParamTypeId, $Version) eq "void const**")
        {
            $Vtt_Pos = $Position;
            $ParamInfoId = getNextElem($ParamInfoId);
            next;
        }
        $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"type"} = $ParamTypeId;
        $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"} = $ParamName;
        $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"algn"} = getAlgn($ParamInfoId)/$BYTE_SIZE;
        if(not $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"}) {
            $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"} = "p".($Position+1);
        }
        $ParamInfoId = getNextElem($ParamInfoId);
        $Position += 1;
    }
    if(detect_nolimit_args($FuncInfoId, $Vtt_Pos)) {
        $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"type"} = -1;
    }
}

sub detect_nolimit_args($$)
{
    my ($FuncInfoId, $Vtt_Pos) = @_;
    my $FuncTypeId = getFuncTypeId($FuncInfoId);
    my $ParamListElemId = getTreeAttr($FuncTypeId, "prms");
    if(getFuncType($FuncInfoId) eq "Method") {
        $ParamListElemId = getNextElem($ParamListElemId);
    }
    return 1 if(not $ParamListElemId);# foo(...)
    my $HaveVoid = 0;
    my $Position = 0;
    while($ParamListElemId)
    {
        if($Vtt_Pos!=-1 and $Position==$Vtt_Pos)
        {
            $Vtt_Pos=-1;
            $ParamListElemId = getNextElem($ParamListElemId);
            next;
        }
        my $ParamTypeId = getTreeAttr($ParamListElemId, "valu");
        if(my $PurpId = getTreeAttr($ParamListElemId, "purp"))
        {
            if($LibInfo{$Version}{"info_type"}{$PurpId} eq "integer_cst") {
                $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"default"} = getTreeValue($PurpId);
            }
            elsif($LibInfo{$Version}{"info_type"}{$PurpId} eq "string_cst") {
                $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"default"} = getNodeStrCst($PurpId);
            }
        }
        if($TypeInfo{$Version}{getTypeDeclId($ParamTypeId)}{$ParamTypeId}{"Name"} eq "void") {
            $HaveVoid = 1;
            last;
        }
        elsif(not defined $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"type"})
        {
            $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"type"} = $ParamTypeId;
            if(not $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"}) {
                $SymbolInfo{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"} = "p".($Position+1);
            }
        }
        $ParamListElemId = getNextElem($ParamListElemId);
        $Position += 1;
    }
    return ($Position>=1 and not $HaveVoid);
}

sub getTreeAttr($$)
{
    my $Attr = $_[1];
    if($LibInfo{$Version}{"info"}{$_[0]}=~/\Q$Attr\E\s*:\s*\@(\d+) /) {
        return $1;
    }
    return "";
}

sub getTreeValue($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/low[ ]*:[ ]*([^ ]+) /) {
        return $1;
    }
    return "";
}

sub getRestrictBase($)
{
    my $TypeId = $_[0];
    my $TypeDeclId = getTypeDeclId($TypeId);
    return $TypeInfo{$Version}{$TypeDeclId}{$TypeId}{"BaseType"}{"Tid"};
}

sub getTreeAccess($)
{
    my $InfoId = $_[0];
    if($LibInfo{$Version}{"info"}{$InfoId}=~/accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
        my $Access = $1;
        if($Access eq "prot") {
            return "protected";
        }
        elsif($Access eq "priv") {
            return "private";
        }
    }
    elsif($LibInfo{$Version}{"info"}{$InfoId}=~/ protected /)
    {# support for old GCC versions
        return "protected";
    }
    elsif($LibInfo{$Version}{"info"}{$InfoId}=~/ private /)
    {# support for old GCC versions
        return "private";
    }
    return "public";
}

sub setFuncAccess($)
{
    my $FuncInfoId = $_[0];
    my $Access = getTreeAccess($FuncInfoId);
    if($Access eq "protected") {
        $SymbolInfo{$Version}{$FuncInfoId}{"Protected"} = 1;
    }
    elsif($Access eq "private") {
        $SymbolInfo{$Version}{$FuncInfoId}{"Private"} = 1;
    }
}

sub setTypeAccess($$)
{
    my ($TypeId, $TypeAttr) = @_;
    my $Access = getTreeAccess($TypeId);
    if($Access eq "protected") {
        $TypeAttr->{"Protected"} = 1;
    }
    elsif($Access eq "private") {
        $TypeAttr->{"Private"} = 1;
    }
}

sub setFuncKind($)
{
    my $FuncInfoId = $_[0];
    if($LibInfo{$Version}{"info"}{$FuncInfoId}=~/pseudo tmpl/) {
        $SymbolInfo{$Version}{$FuncInfoId}{"PseudoTemplate"} = 1;
    }
    elsif($LibInfo{$Version}{"info"}{$FuncInfoId}=~/ constructor /) {
        $SymbolInfo{$Version}{$FuncInfoId}{"Constructor"} = 1;
    }
    elsif($LibInfo{$Version}{"info"}{$FuncInfoId}=~/ destructor /) {
        $SymbolInfo{$Version}{$FuncInfoId}{"Destructor"} = 1;
    }
}

sub getFuncSpec($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$FuncInfoId};
    if($FuncInfo=~/spec[ ]*:[ ]*pure /) {
        return "PureVirt";
    }
    elsif($FuncInfo=~/spec[ ]*:[ ]*virt /) {
        return "Virt";
    }
    elsif($FuncInfo=~/ pure\s+virtual /)
    {# support for old GCC versions
        return "PureVirt";
    }
    elsif($FuncInfo=~/ virtual /)
    {# support for old GCC versions
        return "Virt";
    }
    return "";
}

sub getFuncClass($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$FuncInfoId};
    if($FuncInfo=~/scpe[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub getFuncLink($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$FuncInfoId};
    if($FuncInfo=~/link[ ]*:[ ]*static /) {
        return "Static";
    }
    else {
        if($FuncInfo=~/link[ ]*:[ ]*([a-zA-Z]+) /) {
            return $1;
        }
        else {
            return "";
        }
    }
}

sub getNextElem($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$FuncInfoId};
    if($FuncInfo=~/(chan|chain)[ ]*:[ ]*@(\d+) /) {
        return $2;
    }
    else {
        return "";
    }
}

sub getFuncParamInfoId($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{"info"}{$FuncInfoId};
    if($FuncInfo=~/args[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub getFuncParamType($)
{
    my $ParamInfoId = $_[0];
    my $ParamInfo = $LibInfo{$Version}{"info"}{$ParamInfoId};
    if($ParamInfo=~/type[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub getFuncParamName($)
{
    my $ParamInfoId = $_[0];
    my $ParamInfo = $LibInfo{$Version}{"info"}{$ParamInfoId};
    if($ParamInfo=~/name[ ]*:[ ]*@(\d+) /) {
        return getTreeStr($1);
    }
    return "";
}

sub getEnumMembInfoId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/csts[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub getStructMembInfoId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/flds[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub get_IntNameSpace($$)
{
    my ($Interface, $LibVersion) = @_;
    return "" if(not $Interface or not $LibVersion);
    return $Cache{"get_IntNameSpace"}{$Interface}{$LibVersion} if(defined $Cache{"get_IntNameSpace"}{$Interface}{$LibVersion});
    my $Signature = get_Signature($Interface, $LibVersion);
    if($Signature=~/\:\:/)
    {
        my $FounNameSpace = 0;
        foreach my $NameSpace (sort {get_depth($b)<=>get_depth($a)} keys(%{$NestedNameSpaces{$LibVersion}}))
        {
            if($Signature=~/\A\Q$NameSpace\E\:\:/
            or $Signature=~/\s+for\s+\Q$NameSpace\E\:\:/) {
                $Cache{"get_IntNameSpace"}{$Interface}{$LibVersion} = $NameSpace;
                return $NameSpace;
            }
        }
    }
    else {
        $Cache{"get_IntNameSpace"}{$Interface}{$LibVersion} = "";
        return "";
    }
}

sub get_TypeNameSpace($$)
{
    my ($TypeName, $LibVersion) = @_;
    return "" if(not $TypeName or not $LibVersion);
    if(defined $Cache{"get_TypeNameSpace"}{$TypeName}{$LibVersion}) {
        return $Cache{"get_TypeNameSpace"}{$TypeName}{$LibVersion};
    }
    if($TypeName=~/\:\:/)
    {
        my $FounNameSpace = 0;
        foreach my $NameSpace (sort {get_depth($b)<=>get_depth($a)} keys(%{$NestedNameSpaces{$LibVersion}}))
        {
            if($TypeName=~/\A\Q$NameSpace\E\:\:/) {
                $Cache{"get_TypeNameSpace"}{$TypeName}{$LibVersion} = $NameSpace;
                return $NameSpace;
            }
        }
    }
    else {
        $Cache{"get_TypeNameSpace"}{$TypeName}{$LibVersion} = "";
        return "";
    }
}

sub getNameSpace($)
{
    my $TypeInfoId = $_[0];
    my $NameSpaceInfoId = getTreeAttr($TypeInfoId, "scpe");
    return "" if(not $NameSpaceInfoId);
    if($LibInfo{$Version}{"info_type"}{$NameSpaceInfoId} eq "namespace_decl")
    {
        my $NameSpaceInfo = $LibInfo{$Version}{"info"}{$NameSpaceInfoId};
        if($NameSpaceInfo=~/name[ ]*:[ ]*@(\d+) /)
        {
            my $NameSpace = getTreeStr($1);
            return "" if($NameSpace eq "::");
            if(my $BaseNameSpace = getNameSpace($NameSpaceInfoId)) {
                $NameSpace = $BaseNameSpace."::".$NameSpace;
            }
            $NestedNameSpaces{$Version}{$NameSpace} = 1;
            return $NameSpace;
        }
        else {
            return "";
        }
    }
    elsif($LibInfo{$Version}{"info_type"}{$NameSpaceInfoId} eq "record_type")
    {# inside data type
        my ($Name, $NameNS) = getTrivialName(getTypeDeclId($NameSpaceInfoId), $NameSpaceInfoId);
        return $Name;
    }
    else {
        return "";
    }
}

sub getNameSpaceId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/scpe[ ]*:[ ]*\@(\d+)/) {
        return $1;
    }
    else {
        return "";
    }
}

sub getEnumMembName($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/purp[ ]*:[ ]*\@(\d+)/) {
        return getTreeStr($1);
    }
    else {
        return "";
    }
}

sub getStructMembName($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/name[ ]*:[ ]*\@(\d+)/) {
        return getTreeStr($1);
    }
    else {
        return "";
    }
}

sub getEnumMembVal($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/valu[ ]*:[ ]*\@(\d+)/)
    {
        if($LibInfo{$Version}{"info"}{$1}=~/cnst[ ]*:[ ]*\@(\d+)/)
        {# in newer versions of GCC the value is in the "const_decl->cnst" node
            return getTreeValue($1);
        }
        else
        {# some old versions of GCC (3.3) have the value in the "integer_cst" node
            return getTreeValue($1);
        }
    }
    return "";
}

sub getSize($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/size[ ]*:[ ]*\@(\d+)/) {
        return getTreeValue($1);
    }
    else {
        return 0;
    }
}

sub getAlgn($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/algn[ ]*:[ ]*(\d+) /) {
        return $1;
    }
    else {
        return "";
    }
}

sub getStructMembBitFieldSize($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/ bitfield /) {
        return getSize($_[0]);
    }
    else {
        return 0;
    }
}

sub getNextMembInfoId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/(chan|chain)[ ]*:[ ]*@(\d+) /) {
        return $2;
    }
    else {
        return "";
    }
}

sub getNextStructMembInfoId($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/(chan|chain)[ ]*:[ ]*@(\d+) /) {
        return $2;
    }
    else {
        return "";
    }
}

sub fieldHasName($)
{
    my $TypeMembInfoId = $_[0];
    if($LibInfo{$Version}{"info_type"}{$TypeMembInfoId} eq "field_decl")
    {
        if($LibInfo{$Version}{"info"}{$TypeMembInfoId}=~/name[ ]*:[ ]*@(\d+) /) {
            return $1;
        }
        else {
            return "";
        }
    }
    else {
        return 0;
    }
}

sub register_header($$)
{# input: header absolute path, relative path or name
    my ($Header, $LibVersion) = @_;
    return "" if(not $Header);
    if(is_abs($Header) and not -f $Header) {
        exitStatus("Access_Error", "can't access \'$Header\'");
    }
    return "" if(skip_header($Header, $LibVersion));
    my $Header_Path = identify_header($Header, $LibVersion);
    return "" if(not $Header_Path);
    detect_header_includes($Header_Path, $LibVersion);
    if(my $RHeader_Path = $Header_ErrorRedirect{$LibVersion}{$Header_Path})
    {
        return "" if(skip_header($RHeader_Path, $LibVersion));
        $Header_Path = $RHeader_Path;
        return "" if($RegisteredHeaders{$LibVersion}{$Header_Path});
    }
    elsif($Header_ShouldNotBeUsed{$LibVersion}{$Header_Path}) {
        return "";
    }
    $Target_Headers{$LibVersion}{$Header_Path}{"Identity"} = get_filename($Header_Path);
    $HeaderName_Paths{$LibVersion}{get_filename($Header_Path)}{$Header_Path} = 1;
    $RegisteredHeaders{$LibVersion}{$Header_Path} = 1;
    return $Header_Path;
}

sub get_abs_path($)
{ # abs_path() should NOT be called for absolute inputs
  # because it can change them
    my $Path = $_[0];
    if(not is_abs($Path)) {
        $Path = abs_path($Path);
    }
    return $Path;
}

sub register_directory($$$)
{
    my ($Dir, $WithDeps, $LibVersion) = @_;
    $Dir=~s/[\/\\]+\Z//g;
    return if(not $LibVersion or not $Dir or not -d $Dir);
    return if(skip_header($Dir, $LibVersion));
    $Dir = get_abs_path($Dir);
    my $Mode = "All";
    if($WithDeps) {
        if($RegisteredDirs{$LibVersion}{$Dir}{1}) {
            return;
        }
        elsif($RegisteredDirs{$LibVersion}{$Dir}{0}) {
            $Mode = "DepsOnly";
        }
    }
    else {
        if($RegisteredDirs{$LibVersion}{$Dir}{1}
        or $RegisteredDirs{$LibVersion}{$Dir}{0}) {
            return;
        }
    }
    $Header_Dependency{$LibVersion}{$Dir} = 1;
    $RegisteredDirs{$LibVersion}{$Dir}{$WithDeps} = 1;
    if($Mode eq "DepsOnly")
    {
        foreach my $Path (cmd_find($Dir,"d","","")) {
            $Header_Dependency{$LibVersion}{$Path} = 1;
        }
        return;
    }
    foreach my $Path (sort {length($b)<=>length($a)} cmd_find($Dir,"f","",""))
    {
        if($WithDeps)
        { 
            my $SubDir = $Path;
            while(($SubDir = get_dirname($SubDir)) ne $Dir)
            { # register all sub directories
                $Header_Dependency{$LibVersion}{$SubDir} = 1;
            }
        }
        next if(is_not_header($Path));
        next if(ignore_path($Path));
        next if(skip_header($Path, $LibVersion));
        foreach my $Part (get_path_prefixes($Path)) {
            $Include_Neighbors{$LibVersion}{$Part} = $Path;
        }
    }
    if(get_filename($Dir) eq "include")
    {# search for "lib/include/" directory
        my $LibDir = $Dir;
        if($LibDir=~s/([\/\\])include\Z/$1lib/g and -d $LibDir) {
            register_directory($LibDir, $WithDeps, $LibVersion);
        }
    }
}

sub parse_redirect($$$)
{
    my ($Content, $Path, $LibVersion) = @_;
    if(defined $Cache{"parse_redirect"}{$LibVersion}{$Path}) {
        return $Cache{"parse_redirect"}{$LibVersion}{$Path};
    }
    return "" if(not $Content);
    my @Errors = ();
    while($Content=~s/#\s*error\s+([^\n]+?)\s*(\n|\Z)//) {
        push(@Errors, $1);
    }
    my $Redirect = "";
    foreach (@Errors)
    {
        s/\s{2,}/ /g;
        if(/(only|must\ include
        |update\ to\ include
        |replaced\ with
        |replaced\ by|renamed\ to
        |is\ in|use)\ (<[^<>]+>|[\w\-\/\\]+\.($HEADER_EXT))/ix)
        {
            $Redirect = $2;
            last;
        }
        elsif(/(include|use|is\ in)\ (<[^<>]+>|[\w\-\/\\]+\.($HEADER_EXT))\ instead/i)
        {
            $Redirect = $2;
            last;
        }
        elsif(/this\ header\ should\ not\ be\ used
         |programs\ should\ not\ directly\ include
         |you\ should\ not\ (include|be\ (including|using)\ this\ (file|header))
         |is\ not\ supported\ API\ for\ general\ use
         |cannot\ be\ included\ directly/ix and not /\ from\ /i) {
            $Header_ShouldNotBeUsed{$LibVersion}{$Path} = 1;
        }
    }
    $Redirect=~s/\A<//g;
    $Redirect=~s/>\Z//g;
    return ($Cache{"parse_redirect"}{$LibVersion}{$Path} = $Redirect);
}

sub parse_includes($$)
{
    my ($Content, $Path) = @_;
    my %Includes = ();
    while($Content=~s/#([ \t]*)(include|include_next|import)([ \t]*)(<|")([^<>"]+)(>|")//)
    {# C/C++: include, Objective C/C++: import directive
        my ($Header, $Method) = ($5, $4);
        $Header = path_format($Header, $OSgroup);
        if(($Method eq "\"" and -e joinPath(get_dirname($Path), $Header))
        or is_abs($Header)) {
        # include "path/header.h" that doesn't exist is equal to include <path/header.h>
            $Includes{$Header} = -1;
        }
        else {
            $Includes{$Header} = 1;
        }
    }
    return \%Includes;
}

sub ignore_path($)
{
    my $Path = $_[0];
    if($Path=~/\~\Z/)
    {# skipping system backup files
        return 1;
    }
    if($Path=~/(\A|[\/\\]+)(\.(svn|git|bzr|hg)|CVS)([\/\\]+|\Z)/)
    {# skipping hidden .svn, .git, .bzr, .hg and CVS directories
        return 1;
    }
    return 0;
}

sub sort_by_word($$)
{
    my ($ArrRef, $W) = @_;
    return if(length($W)<2);
    @{$ArrRef} = sort {get_filename($b)=~/\Q$W\E/i<=>get_filename($a)=~/\Q$W\E/i} @{$ArrRef};
}

sub natural_sorting($$)
{
    my ($H1, $H2) = @_;
    $H1=~s/\.[a-z]+\Z//ig;
    $H2=~s/\.[a-z]+\Z//ig;
    my ($HDir1, $Hname1) = separate_path($H1);
    my ($HDir2, $Hname2) = separate_path($H2);
    my $Dirname1 = get_filename($HDir1);
    my $Dirname2 = get_filename($HDir2);
    if($H1 eq $H2) {
        return 0;
    }
    elsif($H1=~/\A\Q$H2\E/) {
        return 1;
    }
    elsif($H2=~/\A\Q$H1\E/) {
        return -1;
    }
    elsif($HDir1=~/\Q$Hname1\E/i
    and $HDir2!~/\Q$Hname2\E/i)
    {# include/glib-2.0/glib.h
        return -1;
    }
    elsif($HDir2=~/\Q$Hname2\E/i
    and $HDir1!~/\Q$Hname1\E/i)
    {# include/glib-2.0/glib.h
        return 1;
    }
    elsif($Hname1=~/\Q$Dirname1\E/i
    and $Hname2!~/\Q$Dirname2\E/i)
    {# include/hildon-thumbnail/hildon-thumbnail-factory.h
        return -1;
    }
    elsif($Hname2=~/\Q$Dirname2\E/i
    and $Hname1!~/\Q$Dirname1\E/i)
    {# include/hildon-thumbnail/hildon-thumbnail-factory.h
        return 1;
    }
    elsif($Hname1=~/(config|lib)/i
    and $Hname2!~/(config|lib)/i)
    {# include/alsa/asoundlib.h
        return -1;
    }
    elsif($Hname2=~/(config|lib)/i
    and $Hname1!~/(config|lib)/i)
    {# include/alsa/asoundlib.h
        return 1;
    }
    elsif(checkRelevance($H1)
    and not checkRelevance($H2))
    {# libebook/e-book.h
        return -1;
    }
    elsif(checkRelevance($H2)
    and not checkRelevance($H1))
    {# libebook/e-book.h
        return 1;
    }
    else {
        return (lc($H1) cmp lc($H2));
    }
}

sub searchForHeaders($)
{
    my $LibVersion = $_[0];
    # gcc standard include paths
    find_gcc_cxx_headers($LibVersion);
    # processing header paths
    foreach my $Path (keys(%{$Descriptor{$LibVersion}{"IncludePaths"}}),
    keys(%{$Descriptor{$LibVersion}{"AddIncludePaths"}}))
    {
        my $IPath = $Path;
        if(not -e $Path) {
            exitStatus("Access_Error", "can't access \'$Path\'");
        }
        elsif(-f $Path) {
            exitStatus("Access_Error", "\'$Path\' - not a directory");
        }
        elsif(-d $Path)
        {
            $Path = get_abs_path($Path);
            register_directory($Path, 0, $LibVersion);
            if($Descriptor{$LibVersion}{"AddIncludePaths"}{$IPath}) {
                $Add_Include_Paths{$LibVersion}{$Path} = 1;
            }
            else {
                $Include_Paths{$LibVersion}{$Path} = 1;
            }
        }
    }
    if(keys(%{$Include_Paths{$LibVersion}})) {
        $INC_PATH_AUTODETECT{$LibVersion} = 0;
    }
    # registering directories
    foreach my $Path (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"Headers"}))
    {
        next if(not -e $Path);
        $Path = get_abs_path($Path);
        $Path = path_format($Path, $OSgroup);
        if(-d $Path) {
            register_directory($Path, 1, $LibVersion);
        }
        elsif(-f $Path)
        {
            my $Dir = get_dirname($Path);
            if(not $SystemPaths{"include"}{$Dir}
            and not $LocalIncludes{$Dir})
            {
                register_directory($Dir, 1, $LibVersion);
                if(my $OutDir = get_dirname($Dir))
                {# registering the outer directory
                    if(not $SystemPaths{"include"}{$OutDir}
                    and not $LocalIncludes{$OutDir}) {
                        register_directory($OutDir, 0, $LibVersion);
                    }
                }
            }
        }
    }
    # registering headers
    my $Position = 0;
    foreach my $Dest (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"Headers"}))
    {
        if(is_abs($Dest) and not -e $Dest) {
            exitStatus("Access_Error", "can't access \'$Dest\'");
        }
        $Dest = path_format($Dest, $OSgroup);
        if(is_header($Dest, 1, $LibVersion))
        {
            if(my $HPath = register_header($Dest, $LibVersion))
            {
                $Target_Headers{$LibVersion}{$HPath}{"Position"} = $Position;
                $Position += 1;
            }
        }
        elsif(-d $Dest)
        {
            my @Registered = ();
            foreach my $Path (cmd_find($Dest,"f","",""))
            {
                next if(ignore_path($Path));
                next if(not is_header($Path, 0, $LibVersion));
                if(my $HPath = register_header($Path, $LibVersion)) {
                    push(@Registered, $HPath);
                }
            }
            @Registered = sort {natural_sorting($a, $b)} @Registered;
            sort_by_word(\@Registered, $TargetLibraryShortName);
            foreach my $Path (@Registered)
            {
                $Target_Headers{$LibVersion}{$Path}{"Position"} = $Position;
                $Position += 1;
            }
        }
        else {
            exitStatus("Access_Error", "can't identify \'$Dest\' as a header file");
        }
    }
    # preparing preamble headers
    my $Preamble_Position=0;
    foreach my $Header (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"IncludePreamble"}))
    {
        if(is_abs($Header) and not -f $Header) {
            exitStatus("Access_Error", "can't access file \'$Header\'");
        }
        $Header = path_format($Header, $OSgroup);
        if(my $Header_Path = is_header($Header, 1, $LibVersion))
        {
            next if(skip_header($Header_Path, $LibVersion));
            $Include_Preamble{$LibVersion}{$Header_Path}{"Position"} = $Preamble_Position++;
        }
        else {
            exitStatus("Access_Error", "can't identify \'$Header\' as a header file");
        }
    }
    foreach my $Header_Name (keys(%{$HeaderName_Paths{$LibVersion}}))
    {# set relative paths (for duplicates)
        if(keys(%{$HeaderName_Paths{$LibVersion}{$Header_Name}})>=2)
        {# search for duplicates
            my $FirstPath = (keys(%{$HeaderName_Paths{$LibVersion}{$Header_Name}}))[0];
            my $Prefix = get_dirname($FirstPath);
            while($Prefix=~/\A(.+)[\/\\]+[^\/\\]+\Z/)
            {# detect a shortest distinguishing prefix
                my $NewPrefix = $1;
                my %Identity = ();
                foreach my $Path (keys(%{$HeaderName_Paths{$LibVersion}{$Header_Name}}))
                {
                    if($Path=~/\A\Q$Prefix\E[\/\\]+(.*)\Z/) {
                        $Identity{$Path} = $1;
                    }
                }
                if(keys(%Identity)==keys(%{$HeaderName_Paths{$LibVersion}{$Header_Name}}))
                {# all names are differend with current prefix
                    foreach my $Path (keys(%{$HeaderName_Paths{$LibVersion}{$Header_Name}})) {
                        $Target_Headers{$LibVersion}{$Path}{"Identity"} = $Identity{$Path};
                    }
                    last;
                }
                $Prefix = $NewPrefix; # increase prefix
            }
        }
    }
    foreach my $HeaderName (keys(%{$Include_Order{$LibVersion}}))
    {# ordering headers according to descriptor
        my $PairName=$Include_Order{$LibVersion}{$HeaderName};
        my ($Pos, $PairPos)=(-1, -1);
        my ($Path, $PairPath)=();
        foreach my $Header_Path (sort {int($Target_Headers{$LibVersion}{$a}{"Position"})<=>int($Target_Headers{$LibVersion}{$b}{"Position"})}
        keys(%{$Target_Headers{$LibVersion}}))  {
            if(get_filename($Header_Path) eq $PairName)
            {
                $PairPos = $Target_Headers{$LibVersion}{$Header_Path}{"Position"};
                $PairPath = $Header_Path;
            }
            if(get_filename($Header_Path) eq $HeaderName)
            {
                $Pos = $Target_Headers{$LibVersion}{$Header_Path}{"Position"};
                $Path = $Header_Path;
            }
        }
        if($PairPos!=-1 and $Pos!=-1
        and int($PairPos)<int($Pos))
        {
            my %Tmp = %{$Target_Headers{$LibVersion}{$Path}};
            %{$Target_Headers{$LibVersion}{$Path}} = %{$Target_Headers{$LibVersion}{$PairPath}};
            %{$Target_Headers{$LibVersion}{$PairPath}} = %Tmp;
        }
    }
    if(not keys(%{$Target_Headers{$LibVersion}})) {
        exitStatus("Error", "header files are not found in the ".$Descriptor{$LibVersion}{"Version"});
    }
}

sub detect_real_includes($$)
{
    my ($AbsPath, $LibVersion) = @_;
    return () if(not $LibVersion or not $AbsPath or not -e $AbsPath);
    if($Cache{"detect_real_includes"}{$LibVersion}{$AbsPath}
    or keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}})) {
        return keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}});
    }
    my $Path = callPreprocessor($AbsPath, "", $LibVersion);
    return () if(not $Path);
    open(PREPROC, $Path);
    while(<PREPROC>)
    {
        if(/#\s+\d+\s+"([^"]+)"[\s\d]*\n/)
        {
            my $Include = path_format($1, $OSgroup);
            if($Include=~/\<(built\-in|internal|command(\-|\s)line)\>|\A\./) {
                next;
            }
            if($Include eq $AbsPath) {
                next;
            }
            $RecursiveIncludes{$LibVersion}{$AbsPath}{$Include} = 1;
        }
    }
    close(PREPROC);
    $Cache{"detect_real_includes"}{$LibVersion}{$AbsPath}=1;
    return keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}});
}

sub detect_header_includes($$)
{
    my ($Path, $LibVersion) = @_;
    return if(not $LibVersion or not $Path or not -e $Path);
    return if($Cache{"detect_header_includes"}{$LibVersion}{$Path});
    my $Content = readFile($Path);
    if($Content=~/#[ \t]*error[ \t]+/ and (my $Redirect = parse_redirect($Content, $Path, $LibVersion)))
    {# detecting error directive in the headers
        if(my $RedirectPath = identify_header($Redirect, $LibVersion))
        {
            if($RedirectPath=~/\/usr\/include\// and $Path!~/\/usr\/include\//) {
                $RedirectPath = identify_header(get_filename($Redirect), $LibVersion);
            }
            if($RedirectPath ne $Path) {
                $Header_ErrorRedirect{$LibVersion}{$Path} = $RedirectPath;
            }
        }
    }
    my $Inc = parse_includes($Content, $Path);
    foreach my $Include (keys(%{$Inc}))
    {# detecting includes
        $Header_Includes{$LibVersion}{$Path}{$Include} = $Inc->{$Include};
    }
    $Cache{"detect_header_includes"}{$LibVersion}{$Path} = 1;
}

sub simplify_path($)
{
    my $Path = $_[0];
    while($Path=~s&([\/\\])[^\/\\]+[\/\\]\.\.[\/\\]&$1&){};
    return $Path;
}

sub fromLibc($)
{ # GLIBC header
    my $Path = $_[0];
    my ($Dir, $Name) = separate_path($Path);
    if(get_filename($Dir)=~/\A(include|libc)\Z/ and $GlibcHeader{$Name})
    { # /usr/include/{stdio.h, ...}
      # epoc32/include/libc/{stdio.h, ...}
        return 1;
    }
    if(isLibcDir($Dir)) {
        return 1;
    }
    return 0;
}

sub isLibcDir($)
{ # GLIBC directory
    my $Dir = $_[0];
    my ($OutDir, $Name) = separate_path($Dir);
    if(get_filename($OutDir)=~/\A(include|libc)\Z/
    and ($Name=~/\Aasm(|-.+)\Z/ or $GlibcDir{$Name}))
    { # /usr/include/{sys,bits,asm,asm-*}/*.h
        return 1;
    }
    return 0;
}

sub detect_recursive_includes($$)
{
    my ($AbsPath, $LibVersion) = @_;
    return () if(not $AbsPath);
    if(isCyclical(\@RecurInclude, $AbsPath)) {
        return keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}});
    }
    my ($AbsDir, $Name) = separate_path($AbsPath);
    if(isLibcDir($AbsDir))
    { # GLIBC internals
        return ();
    }
    if(keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}})) {
        return keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}});
    }
    return () if($OSgroup ne "windows" and $Name=~/windows|win32|win64/i);
    return () if($MAIN_CPP_DIR and $AbsPath=~/\A\Q$MAIN_CPP_DIR\E/ and not $STDCXX_TESTING);
    push(@RecurInclude, $AbsPath);
    if($DefaultGccPaths{$AbsDir}
    or fromLibc($AbsPath))
    { # check "real" (non-"model") include paths
        my @Paths = detect_real_includes($AbsPath, $LibVersion);
        pop(@RecurInclude);
        return @Paths;
    }
    if(not keys(%{$Header_Includes{$LibVersion}{$AbsPath}})) {
        detect_header_includes($AbsPath, $LibVersion);
    }
    foreach my $Include (keys(%{$Header_Includes{$LibVersion}{$AbsPath}}))
    {
        my $HPath = "";
        if($Header_Includes{$LibVersion}{$AbsPath}{$Include}==-1
        and -f joinPath($AbsDir, $Include))
        { # for #include "..."
            $HPath = simplify_path(joinPath($AbsDir, $Include));
        }
        if(not $HPath) {
            $HPath = identify_header($Include, $LibVersion);
        }
        next if(not $HPath);
        $RecursiveIncludes{$LibVersion}{$AbsPath}{$HPath} = 1;
        if($Header_Includes{$LibVersion}{$AbsPath}{$Include}==1)
        { # only include <...>, skip include "..." prefixes
            $Header_Include_Prefix{$LibVersion}{$AbsPath}{$HPath}{get_dirname($Include)} = 1;
        }
        foreach my $IncPath (detect_recursive_includes($HPath, $LibVersion))
        {
            $RecursiveIncludes{$LibVersion}{$AbsPath}{$IncPath} = 1;
            foreach my $Prefix (keys(%{$Header_Include_Prefix{$LibVersion}{$HPath}{$IncPath}})) {
                $Header_Include_Prefix{$LibVersion}{$AbsPath}{$IncPath}{$Prefix} = 1;
            }
        }
        foreach my $Dep (keys(%{$Header_Include_Prefix{$LibVersion}{$AbsPath}}))
        {
            if($GlibcHeader{get_filename($Dep)} and keys(%{$Header_Include_Prefix{$LibVersion}{$AbsPath}{$Dep}})>=2
            and defined $Header_Include_Prefix{$LibVersion}{$AbsPath}{$Dep}{""})
            { # distinguish math.h from glibc and math.h from the tested library
                delete($Header_Include_Prefix{$LibVersion}{$AbsPath}{$Dep}{""});
                last;
            }
        }
    }
    pop(@RecurInclude);
    return keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}});
}

sub find_in_framework($$$)
{
    my ($Header, $Framework, $LibVersion) = @_;
    return "" if(not $Header or not $Framework or not $LibVersion);
    if(defined $Cache{"find_in_framework"}{$LibVersion}{$Framework}{$Header}) {
        return $Cache{"find_in_framework"}{$LibVersion}{$Framework}{$Header};
    }
    foreach my $Dependency (sort {get_depth($a)<=>get_depth($b)} keys(%{$Header_Dependency{$LibVersion}}))
    {
        if(get_filename($Dependency) eq $Framework
        and -f get_dirname($Dependency)."/".$Header) {
            return ($Cache{"find_in_framework"}{$LibVersion}{$Framework}{$Header} = get_dirname($Dependency));
        }
    }
    return ($Cache{"find_in_framework"}{$LibVersion}{$Framework}{$Header} = "");
}

sub find_in_defaults($)
{
    my $Header = $_[0];
    return "" if(not $Header);
    foreach my $Dir (sort {get_depth($a)<=>get_depth($b)}
    (keys(%DefaultIncPaths), keys(%DefaultGccPaths), keys(%DefaultCppPaths), keys(%UserIncPath)))
    {
        next if(not $Dir);
        if(-f $Dir."/".$Header) {
            return $Dir;
        }
    }
    return "";
}

sub cmp_paths($$)
{
    my ($Path1, $Path2) = @_;
    my @Parts1 = split(/[\/\\]/, $Path1);
    my @Parts2 = split(/[\/\\]/, $Path2);
    foreach my $Num (0 .. $#Parts1)
    {
        my $Part1 = $Parts1[$Num];
        my $Part2 = $Parts2[$Num];
        if($GlibcDir{$Part1}
        and not $GlibcDir{$Part2}) {
            return 1;
        }
        elsif($GlibcDir{$Part2}
        and not $GlibcDir{$Part1}) {
            return -1;
        }
        elsif($Part1=~/glib/
        and $Part2!~/glib/) {
            return 1;
        }
        elsif($Part1!~/glib/
        and $Part2=~/glib/) {
            return -1;
        }
        elsif(my $CmpRes = ($Part1 cmp $Part2)) {
            return $CmpRes;
        }
    }
    return 0;
}

sub checkRelevance($)
{
    my ($Path) = @_;
    return 0 if(not $Path);
    if($SystemRoot) {
        $Path=~s/\A\Q$SystemRoot\E//g;
    }
    my ($Dir, $Name) = separate_path($Path);
    $Name=~s/\.\w+\Z//g;# remove extension (.h)
    my @Tokens = split(/[_\d\W]+/, $Name);
    foreach (@Tokens)
    {
        next if(not $_);
        if($Dir=~/(\A|lib|[_\d\W])\Q$_\E([_\d\W]|lib|\Z)/i
        or length($_)>=4 and $Dir=~/\Q$_\E/i)
        { # include/gupnp-1.0/libgupnp/gupnp-context.h
          # include/evolution-data-server-1.4/libebook/e-book.h
            return 1;
        }
    }
    return 0;
}

sub checkFamily(@)
{
    my @Paths = @_;
    return 1 if($#Paths<=0);
    my %Prefix = ();
    foreach my $Path (@Paths)
    {
        if($SystemRoot) {
            $Path = cut_path_prefix($Path, $SystemRoot);
        }
        if(my $Dir = get_dirname($Path))
        {
            $Prefix{$Dir} += 1;
            $Prefix{get_dirname($Dir)} += 1;
        }
    }
    foreach (keys(%Prefix)) {
        if($Prefix{$_}==$#Paths+1) {
            return 1;
        }
    }
    return 0;
}

sub isAcceptable($$)
{
    my ($Header, $Candidate) = @_;
    my $HName = get_filename($Header);
    if(get_dirname($Header))
    { # with prefix
        return 1;
    }
    if($HName=~/config|setup/i and $Candidate=~/[\/\\]lib\d*[\/\\]/)
    { # allow to search for glibconfig.h in /usr/lib/glib-2.0/include/
        return 1;
    }
    if(checkRelevance($Candidate))
    { # allow to search for atk.h in /usr/include/atk-1.0/atk/
        return 1;
    }
    if(checkFamily(keys(%{$SystemHeaders{$HName}})))
    { # /usr/include/qt4/QtNetwork/qsslconfiguration.h
      # /usr/include/qt4/Qt/qsslconfiguration.h
        return 1;
    }
    if($OStarget eq "symbian")
    {
        if($Candidate=~/[\/\\]stdapis[\/\\]/) {
            return 1;
        }
    }
    return 0;
}

sub isRelevant($$$)
{ # disallow to search for "abstract" headers in too deep directories
    my ($Header, $Candidate, $LibVersion) = @_;
    my $HName = get_filename($Header);
    if($OStarget eq "symbian")
    {
        if($Candidate=~/[\/\\](tools|stlportv5)[\/\\]/) {
            return 0;
        }
    }
    if($Candidate=~/c\+\+[\/\\]\d+/ and $MAIN_CPP_DIR
    and $Candidate!~/\A\Q$MAIN_CPP_DIR\E/)
    { # skip ../c++/3.3.3/ if using ../c++/4.5/
        return 0;
    }
    if($Candidate=~/[\/\\]asm-/
    and (my $Arch = getArch($LibVersion)) ne "unknown")
    { # arch-specific header files
        if($Candidate!~/[\/\\]asm-\Q$Arch\E/)
        {# skip ../asm-arm/ if using x86 architecture
            return 0;
        }
    }
    if(keys(%{$SystemHeaders{$HName}})==1)
    { # unique header
        return 1;
    }
    if(keys(%{$SystemHeaders{$Header}})==1)
    { # unique name
        return 1;
    }
    my $SystemDepth = $SystemRoot?get_depth($SystemRoot):0;
    if(get_depth($Candidate)-$SystemDepth>=5)
    { # abstract headers in too deep directories
      # sstream.h or typeinfo.h in /usr/include/wx-2.9/wx/
        if(not isAcceptable($Header, $Candidate)) {
            return 0;
        }
    }
    if($Header eq "parser.h"
    and $Candidate!~/\/libxml2\//)
    { # select parser.h from xml2 library
        return 0;
    }
    return 1;
}

sub selectSystemHeader($$)
{
    my ($Header, $LibVersion) = @_;
    return $Header if(-f $Header);
    return "" if(is_abs($Header) and not -f $Header);
    return "" if($Header=~/\A(atomic|config|configure|build|conf|setup)\.h\Z/i);
    if($OSgroup ne "windows")
    {
        if(get_filename($Header)=~/windows|win32|win64|\A(dos|process|winsock)\.h\Z/i) {
            return "";
        }
        elsif($Header=~/\A(mem)\.h\Z/)
        { # pngconf.h include mem.h for __MSDOS__
            return "";
        }
    }
    if(defined $Cache{"selectSystemHeader"}{$LibVersion}{$Header}) {
        return $Cache{"selectSystemHeader"}{$LibVersion}{$Header};
    }
    foreach my $Path (keys(%{$SystemPaths{"include"}}))
    { # search in default paths
        if(-f $Path."/".$Header) {
            return ($Cache{"selectSystemHeader"}{$LibVersion}{$Header} = joinPath($Path,$Header));
        }
    }
    if(not keys(%SystemHeaders)) {
        detectSystemHeaders();
    }
    my @Candidates = keys(%{$SystemHeaders{$Header}});
    foreach my $Candidate (sort {get_depth($a)<=>get_depth($b)}
    sort {cmp_paths($b, $a)} @Candidates)
    {
        if(isRelevant($Header, $Candidate, $LibVersion)) {
            return ($Cache{"selectSystemHeader"}{$LibVersion}{$Header} = $Candidate);
        }
    }
    return ($Cache{"selectSystemHeader"}{$LibVersion}{$Header} = ""); # error
}

sub cut_path_prefix($$)
{
    my ($Path, $Prefix) = @_;
    return $Path if(not $Prefix);
    $Prefix=~s/[\/\\]+\Z//;
    $Path=~s/\A\Q$Prefix\E([\/\\]+|\Z)//;
    return $Path;
}

sub is_default_include_dir($)
{
    my $Dir = $_[0];
    $Dir=~s/[\/\\]+\Z//;
    return ($DefaultGccPaths{$Dir} or $DefaultCppPaths{$Dir} or $DefaultIncPaths{$Dir});
}

sub identify_header($$)
{
    my ($Header, $LibVersion) = @_;
    $Header=~s/\A(\.\.[\\\/])+//g;
    if(defined $Cache{"identify_header"}{$Header}{$LibVersion}) {
        return $Cache{"identify_header"}{$Header}{$LibVersion};
    }
    my $Path = identify_header_internal($Header, $LibVersion);
    if(not $Path and $OSgroup eq "macos" and my $Dir = get_dirname($Header))
    {# search in frameworks: "OpenGL/gl.h" is "OpenGL.framework/Headers/gl.h"
        my $RelPath = "Headers\/".get_filename($Header);
        if(my $HeaderDir = find_in_framework($RelPath, $Dir.".framework", $LibVersion)) {
            $Path = joinPath($HeaderDir, $RelPath);
        }
    }
    return ($Cache{"identify_header"}{$Header}{$LibVersion} = $Path);
}

sub identify_header_internal($$)
{# search for header by absolute path, relative path or name
    my ($Header, $LibVersion) = @_;
    return "" if(not $Header);
    if(-f $Header)
    {# it's relative or absolute path
        return get_abs_path($Header);
    }
    elsif($GlibcHeader{$Header} and not $GLIBC_TESTING
    and my $HeaderDir = find_in_defaults($Header))
    {# search for libc headers in the /usr/include
     # for non-libc target library before searching
     # in the library paths
        return joinPath($HeaderDir,$Header);
    }
    elsif(my $Path = $Include_Neighbors{$LibVersion}{$Header})
    {# search in the target library paths
        return $Path;
    }
    elsif($DefaultGccHeader{$Header})
    {# search in the internal GCC include paths
        return $DefaultGccHeader{$Header};
    }
    elsif(my $HeaderDir = find_in_defaults($Header))
    {# search in the default GCC include paths
        return joinPath($HeaderDir,$Header);
    }
    elsif($DefaultCppHeader{$Header})
    {# search in the default G++ include paths
        return $DefaultCppHeader{$Header};
    }
    elsif(my $AnyPath = selectSystemHeader($Header, $LibVersion))
    {# search everywhere in the system
        return $AnyPath;
    }
    else
    {# cannot find anything
        return "";
    }
}

sub get_filename($)
{# much faster than basename() from File::Basename module
    my $Path = $_[0];
    $Path=~s/[\/\\]+\Z//;
    return $Cache{"get_filename"}{$Path} if($Cache{"get_filename"}{$Path});
    if($Path=~/([^\/\\]+)\Z/) {
        return ($Cache{"get_filename"}{$Path} = $1);
    }
    return "";
}

sub get_dirname($)
{# much faster than dirname() from File::Basename module
    my $Path = $_[0];
    $Path=~s/[\/\\]+\Z//;
    if($Path=~/\A(.*)[\/\\]+([^\/\\]*)\Z/) {
        return $1;
    }
    return "";
}

sub separate_path($) {
    return (get_dirname($_[0]), get_filename($_[0]));
}

sub esc($)
{
    my $Str = $_[0];
    $Str=~s/([()\[\]{}$ &'"`;,<>\+])/\\$1/g;
    return $Str;
}

sub getLocation($)
{
    if($LibInfo{$Version}{"info"}{$_[0]}=~/srcp[ ]*:[ ]*([\w\-\<\>\.\+\/\\]+):(\d+) /) {
        return ($1, $2);
    }
    else {
        return ();
    }
}

sub getTypeType($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    if($MissedTypedef{$Version}{$TypeId}{"TDid"}
    and $MissedTypedef{$Version}{$TypeId}{"TDid"} eq $TypeDeclId) {
        return "Typedef";
    }
    my $Info = $LibInfo{$Version}{"info"}{$TypeId};
    if($Info=~/qual[ ]*:[ ]*c /
    and $Info=~/unql[ ]*:[ ]*\@/) {
        return "Const";
    }
    return "Typedef" if($Info=~/unql[ ]*:/ and $Info!~/qual[ ]*:/ and getNameByInfo($TypeDeclId));
    return "Volatile" if($Info=~/qual[ ]*:[ ]*v / and $Info=~/unql[ ]*:[ ]*\@/);
    return "Restrict" if($Info=~/qual[ ]*:[ ]*r / and $Info=~/unql[ ]*:[ ]*\@/);
    my $TypeType = getTypeTypeByTypeId($TypeId);
    if($TypeType eq "Struct")
    {
        if($TypeDeclId
        and $LibInfo{$Version}{"info_type"}{$TypeDeclId} eq "template_decl") {
            return "Template";
        }
        else {
            return "Struct";
        }
    }
    else {
        return $TypeType;
    }
    
}

sub getTypeTypeByTypeId($)
{
    my $TypeId = $_[0];
    my $TypeType = $LibInfo{$Version}{"info_type"}{$TypeId};
    if($TypeType=~/integer_type|real_type|boolean_type|void_type|complex_type/)
    {
        return "Intrinsic";
    }
    elsif(isFuncPtr($TypeId)) {
        return "FuncPtr";
    }
    elsif(isMethodPtr($TypeId)) {
        return "MethodPtr";
    }
    elsif(isFieldPtr($TypeId)) {
        return "FieldPtr";
    }
    elsif($TypeType eq "pointer_type") {
        return "Pointer";
    }
    elsif($TypeType eq "reference_type") {
        return "Ref";
    }
    elsif($TypeType eq "union_type") {
        return "Union";
    }
    elsif($TypeType eq "enumeral_type") {
        return "Enum";
    }
    elsif($TypeType eq "record_type") {
        return "Struct";
    }
    elsif($TypeType eq "array_type") {
        return "Array";
    }
    elsif($TypeType eq "complex_type") {
        return "Intrinsic";
    }
    elsif($TypeType eq "function_type") {
        return "FunctionType";
    }
    elsif($TypeType eq "method_type") {
        return "MethodType";
    }
    else {
        return "Unknown";
    }
}

sub getNameByInfo($)
{
    return "" if($LibInfo{$Version}{"info"}{$_[0]}!~/name[ ]*:[ ]*@(\d+) /);
    if($LibInfo{$Version}{"info"}{$1}=~/strg[ ]*:[ ]*(.*?)[ ]+lngt/)
    {# short unsigned int (may include spaces)
        return $1;
    }
    else {
        return "";
    }
}

sub getTreeStr($)
{
    my $Info = $LibInfo{$Version}{"info"}{$_[0]};
    if($Info=~/strg[ ]*:[ ]*([^ ]*)/)
    {
        my $Str = $1;
        if($C99Mode{$Version} and $Str=~/\Ac99_(.+)\Z/) {
            if($CppKeywords{$1}) {
                $Str=$1;
            }
        }
        return $Str;
    }
    else {
        return "";
    }
}

sub getVarShortName($)
{
    my $VarInfo = $LibInfo{$Version}{"info"}{$_[0]};
    return "" if($VarInfo!~/name[ ]*:[ ]*@(\d+) /);
    return getTreeStr($1);
}

sub getFuncShortName($)
{
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    if($FuncInfo=~/ operator /)
    {
        if($FuncInfo=~/ conversion /) {
            return "operator ".get_TypeName($SymbolInfo{$Version}{$_[0]}{"Return"}, $Version);
        }
        else
        {
            return "" if($FuncInfo!~/ operator[ ]+([a-zA-Z]+) /);
            return "operator".$Operator_Indication{$1};
        }
    }
    else
    {
        return "" if($FuncInfo!~/name[ ]*:[ ]*@(\d+) /);
        return getTreeStr($1);
    }
}

sub getFuncMnglName($)
{
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    return "" if($FuncInfo!~/mngl[ ]*:[ ]*@(\d+) /);
    return getTreeStr($1);
}

sub getFuncReturn($)
{
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    return "" if($FuncInfo!~/type[ ]*:[ ]*@(\d+) /);
    my $FuncTypeInfoId = $1;
    return "" if($LibInfo{$Version}{"info"}{$FuncTypeInfoId}!~/retn[ ]*:[ ]*@(\d+) /);
    my $FuncReturnTypeId = $1;
    if($TypeInfo{$Version}{getTypeDeclId($FuncReturnTypeId)}{$FuncReturnTypeId}{"Type"} eq "Restrict")
    {# delete restrict spec
        $FuncReturnTypeId = getRestrictBase($FuncReturnTypeId);
    }
    return $FuncReturnTypeId;
}

sub getFuncOrig($)
{
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    if($FuncInfo=~/orig[ ]*:[ ]*@(\d+) /) {
        return $1;
    }
    else {
        return $_[0];
    }
}

sub unmangleArray(@)
{
    if(@_[0]=~/\A\?/)
    {# "cl" mangling
        my $UndNameCmd = get_CmdPath("undname");
        if(not $UndNameCmd) {
            exitStatus("Not_Found", "can't find \"undname\"");
        }
        writeFile("$TMP_DIR/unmangle", join("\n", @_));
        return split(/\n/, `$UndNameCmd 0x8386 $TMP_DIR/unmangle`);
    }
    else
    {# "gcc" mangling
        my $CppFiltCmd = get_CmdPath("c++filt");
        if(not $CppFiltCmd) {
            exitStatus("Not_Found", "can't find c++filt in PATH");
        }
        my $Info = `$CppFiltCmd -h 2>&1`;
        if($Info=~/\@<file>/)
        {# new version of c++filt can take a file
            my $NoStrip = "";
            if($OSgroup eq "macos"
            or $OSgroup eq "windows") {
                $NoStrip = "-n";
            }
            writeFile("$TMP_DIR/unmangle", join("\n", @_));
            return split(/\n/, `$CppFiltCmd $NoStrip \@\"$TMP_DIR/unmangle\"`);
        }
        else
        { # old-style unmangling
            if($#_>$MAX_COMMAND_LINE_ARGUMENTS) {
                my @Half = splice(@_, 0, ($#_+1)/2);
                return (unmangleArray(@Half), unmangleArray(@_))
            }
            else
            {
                my $NoStrip = "";
                if($OSgroup eq "macos"
                or $OSgroup eq "windows") {
                    $NoStrip = "-n";
                }
                my $Strings = join(" ", @_);
                return split(/\n/, `$CppFiltCmd $NoStrip $Strings`);
            }
        }
    }
}

sub get_SignatureNoInfo($$)
{
    my ($Interface, $LibVersion) = @_;
    return $Cache{"get_SignatureNoInfo"}{$LibVersion}{$Interface} if($Cache{"get_SignatureNoInfo"}{$LibVersion}{$Interface});
    my ($MnglName, $VersionSpec, $SymbolVersion) = separate_symbol($Interface);
    my $Signature = $tr_name{$MnglName}?$tr_name{$MnglName}:$MnglName;
    if($Interface=~/\A(_Z|\?)/)
    {# C++
        $Signature=~s/\Qstd::basic_string<char, std::char_traits<char>, std::allocator<char> >\E/std::string/g;
        $Signature=~s/\Qstd::map<std::string, std::string, std::less<std::string >, std::allocator<std::pair<std::string const, std::string > > >\E/std::map<std::string, std::string>/g;
    }
    if(not $CheckObjectsOnly or $OSgroup=~/linux|bsd|beos/)
    {# ELF format marks data as OBJECT
        if($CompleteSignature{$LibVersion}{$Interface}{"Object"}) {
            $Signature .= " [data]";
        }
        elsif($Interface!~/\A(_Z|\?)/) {
            $Signature .= "(...)";
        }
    }
    if(my $ChargeLevel = get_ChargeLevel($Interface, $LibVersion))
    {
        my $ShortName = substr($Signature, 0, detect_center($Signature));
        $Signature=~s/\A\Q$ShortName\E/$ShortName $ChargeLevel/g;
    }
    $Signature .= $VersionSpec.$SymbolVersion if($SymbolVersion);
    $Cache{"get_SignatureNoInfo"}{$LibVersion}{$Interface} = $Signature;
    return $Signature;
}

sub get_ChargeLevel($$)
{
    my ($Interface, $LibVersion) = @_;
    return "" if($Interface!~/\A(_Z|\?)/);
    if(defined $CompleteSignature{$LibVersion}{$Interface}
    and $CompleteSignature{$LibVersion}{$Interface}{"Header"})
    {
        if($CompleteSignature{$LibVersion}{$Interface}{"Constructor"})
        {
            if($Interface=~/C1E/) {
                return "[in-charge]";
            }
            elsif($Interface=~/C2E/) {
                return "[not-in-charge]";
            }
        }
        elsif($CompleteSignature{$LibVersion}{$Interface}{"Destructor"})
        {
            if($Interface=~/D1E/) {
                return "[in-charge]";
            }
            elsif($Interface=~/D2E/) {
                return "[not-in-charge]";
            }
            elsif($Interface=~/D0E/) {
                return "[in-charge-deleting]";
            }
        }
    }
    else
    {
        if($Interface=~/C1E/) {
            return "[in-charge]";
        }
        elsif($Interface=~/C2E/) {
            return "[not-in-charge]";
        }
        elsif($Interface=~/D1E/) {
            return "[in-charge]";
        }
        elsif($Interface=~/D2E/) {
            return "[not-in-charge]";
        }
        elsif($Interface=~/D0E/) {
            return "[in-charge-deleting]";
        }
    }
    return "";
}

sub get_Signature($$)
{
    my ($Interface, $LibVersion) = @_;
    return $Cache{"get_Signature"}{$LibVersion}{$Interface} if($Cache{"get_Signature"}{$LibVersion}{$Interface});
    my ($MnglName, $VersionSpec, $SymbolVersion) = separate_symbol($Interface);
    if(skipGlobalData($MnglName) or not $CompleteSignature{$LibVersion}{$Interface}{"Header"}) {
        return get_SignatureNoInfo($Interface, $LibVersion);
    }
    my ($Func_Signature, @Param_Types_FromUnmangledName) = ();
    my $ShortName = $CompleteSignature{$LibVersion}{$Interface}{"ShortName"};
    if($Interface=~/\A(_Z|\?)/)
    {
        if(my $ClassId = $CompleteSignature{$LibVersion}{$Interface}{"Class"}) {
            $Func_Signature = get_TypeName($ClassId, $LibVersion)."::".(($CompleteSignature{$LibVersion}{$Interface}{"Destructor"})?"~":"").$ShortName;
        }
        elsif(my $NameSpace = $CompleteSignature{$LibVersion}{$Interface}{"NameSpace"}) {
            $Func_Signature = $NameSpace."::".$ShortName;
        }
        else {
            $Func_Signature = $ShortName;
        }
        @Param_Types_FromUnmangledName = get_signature_parts($tr_name{$MnglName}, 0);
    }
    else {
        $Func_Signature = $MnglName;
    }
    my @ParamArray = ();
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{$LibVersion}{$Interface}{"Param"}}))
    {
        next if($Pos eq "");
        my $ParamTypeId = $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$Pos}{"type"};
        next if(not $ParamTypeId);
        my $ParamTypeName = get_TypeName($ParamTypeId, $LibVersion);
        $ParamTypeName = $Param_Types_FromUnmangledName[$Pos] if(not $ParamTypeName);
        foreach my $Typedef (keys(%ChangedTypedef))
        {
            my $Base = $Typedef_BaseName{$LibVersion}{$Typedef};
            $ParamTypeName=~s/(\A|\W)\Q$Typedef\E(\W|\Z)/$1$Base$2/g;
        }
        if(my $ParamName = $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$Pos}{"name"}) {
            push(@ParamArray, create_member_decl($ParamTypeName, $ParamName));
        }
        else {
            push(@ParamArray, $ParamTypeName);
        }
    }
    if($CompleteSignature{$LibVersion}{$Interface}{"Data"}
    or $CompleteSignature{$LibVersion}{$Interface}{"Object"}) {
        $Func_Signature .= " [data]";
    }
    else
    {
        if(my $ChargeLevel = get_ChargeLevel($Interface, $LibVersion))
        {# add [in-charge]
            $Func_Signature .= " ".$ChargeLevel;
        }
        $Func_Signature .= " (".join(", ", @ParamArray).")";
        if($Interface=~/\A_ZNK/) {
            $Func_Signature .= " const";
        }
        if($CompleteSignature{$LibVersion}{$Interface}{"Static"}
        and $Interface=~/\A(_Z|\?)/)
        {# for static methods
            $Func_Signature .= " [static]";
        }
    }
    if(defined $ShowRetVal
    and my $ReturnTId = $CompleteSignature{$LibVersion}{$Interface}{"Return"}) {
        $Func_Signature .= ":".get_TypeName($ReturnTId, $LibVersion);
    }
    if($SymbolVersion) {
        $Func_Signature .= $VersionSpec.$SymbolVersion;
    }
    $Cache{"get_Signature"}{$LibVersion}{$Interface} = $Func_Signature;
    return $Func_Signature;
}

sub create_member_decl($$)
{
    my ($TName, $Member) = @_;
    if($TName=~/\([\*]+\)/) {
        $TName=~s/\(([\*]+)\)/\($1$Member\)/;
        return $TName;
    }
    else
    {
        my @ArraySizes = ();
        while($TName=~s/(\[[^\[\]]*\])\Z//) {
            push(@ArraySizes, $1);
        }
        return $TName." ".$Member.join("", @ArraySizes);
    }
}

sub getFuncType($)
{
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    return "" if($FuncInfo!~/type[ ]*:[ ]*@(\d+) /);
    my $FuncTypeInfoId = $1;
    my $FunctionType = $LibInfo{$Version}{"info_type"}{$FuncTypeInfoId};
    if($FunctionType eq "method_type") {
        return "Method";
    }
    elsif($FunctionType eq "function_type") {
        return "Function";
    }
    else {
        return $FunctionType;
    }
}

sub getFuncTypeId($)
{
    my $FuncInfo = $LibInfo{$Version}{"info"}{$_[0]};
    if($FuncInfo=~/type[ ]*:[ ]*@(\d+)( |\Z)/) {
        return $1;
    }
    else {
        return 0;
    }
}

sub isNotAnon($) {
    return (not isAnon($_[0]));
}

sub isAnon($)
{# "._N" or "$_N" in older GCC versions
    return ($_[0]=~/(\.|\$)\_\d+|anon\-/);
}

sub unmangled_Compact($)
{ # Removes all non-essential (for C++ language) whitespace from a string.  If 
  # the whitespace is essential it will be replaced with exactly one ' ' 
  # character. Works correctly only for unmangled names.
    return $Cache{"unmangled_Compact"}{$_[0]} if(defined $Cache{"unmangled_Compact"}{$_[0]});
    my $result=$_[0];
    # First, we reduce all spaces that we can
    my $coms='[-()<>:*&~!|+=%@~"?.,/[^'."']";
    my $coms_nobr='[-()<:*&~!|+=%@~"?.,'."']";
    my $clos='[),;:\]]';
    $result=~s/^\s+//gm;
    $result=~s/\s+$//gm;
    $result=~s/((?!\n)\s)+/ /g;
    $result=~s/(\w+)\s+($coms+)/$1$2/gm;
    $result=~s/($coms+)\s+(\w+)/$1$2/gm;
    $result=~s/(\w)\s+($clos)/$1$2/gm;
    $result=~s/($coms+)\s+($coms+)/$1 $2/gm;
    $result=~s/($coms_nobr+)\s+($coms+)/$1$2/gm;
    $result=~s/($coms+)\s+($coms_nobr+)/$1$2/gm;
    # don't forget about >> and <:.  In unmangled names global-scope modifier 
    # is not used, so <: will always be a digraph and requires no special treatment.
    # We also try to remove other parts that are better to be removed here than in other places
    # double-cv
    $result=~s/\bconst\s+const\b/const/gm;
    $result=~s/\bvolatile\s+volatile\b/volatile/gm;
    $result=~s/\bconst\s+volatile\b\s+const\b/const volatile/gm;
    $result=~s/\bvolatile\s+const\b\s+volatile\b/const volatile/gm;
    # Place cv in proper order
    $result=~s/\bvolatile\s+const\b/const volatile/gm;
    $Cache{"unmangled_Compact"}{$_[0]} = $result;
    return $result;
}

sub unmangled_PostProcess($)
{
    my $result = $_[0];
    #s/\bunsigned int\b/unsigned/g;
    $result=~s/\bshort unsigned int\b/unsigned short/g;
    $result=~s/\bshort int\b/short/g;
    $result=~s/\blong long unsigned int\b/unsigned long long/g;
    $result=~s/\blong unsigned int\b/unsigned long/g;
    $result=~s/\blong long int\b/long long/g;
    $result=~s/\blong int\b/long/g;
    $result=~s/\)const\b/\) const/g;
    $result=~s/\blong long unsigned\b/unsigned long long/g;
    $result=~s/\blong unsigned\b/unsigned long/g;
    return $result;
}

sub correctName($)
{# type name correction
    my $CorrectName = $_[0];
    $CorrectName = unmangled_Compact($CorrectName);
    $CorrectName = unmangled_PostProcess($CorrectName);
    return $CorrectName;
}

sub get_HeaderDeps($$)
{
    my ($AbsPath, $LibVersion) = @_;
    return () if(not $AbsPath or not $LibVersion);
    if(defined $Cache{"get_HeaderDeps"}{$LibVersion}{$AbsPath}) {
        return @{$Cache{"get_HeaderDeps"}{$LibVersion}{$AbsPath}};
    }
    my %IncDir = ();
    detect_recursive_includes($AbsPath, $LibVersion);
    foreach my $HeaderPath (keys(%{$RecursiveIncludes{$LibVersion}{$AbsPath}}))
    {
        next if(not $HeaderPath);
        next if($MAIN_CPP_DIR and $HeaderPath=~/\A\Q$MAIN_CPP_DIR\E([\/\\]|\Z)/);
        my $Dir = get_dirname($HeaderPath);
        foreach my $Prefix (keys(%{$Header_Include_Prefix{$LibVersion}{$AbsPath}{$HeaderPath}}))
        {
            my $Dep = $Dir;
            if($Prefix)
            {
                if($OSgroup eq "windows")
                { # case insensitive seach on windows
                    if(not $Dep=~s/[\/\\]+\Q$Prefix\E\Z//ig) {
                        next;
                    }
                }
                elsif($OSgroup eq "macos")
                { # seach in frameworks
                    if(not $Dep=~s/[\/\\]+\Q$Prefix\E\Z//g)
                    {
                        if($HeaderPath=~/(.+\.framework)\/Headers\/([^\/]+)/)
                        {# frameworks
                            my ($HFramework, $HName) = ($1, $2);
                            $Dep = $HFramework;
                        }
                        else
                        {# mismatch
                            next;
                        }
                    }
                }
                elsif(not $Dep=~s/[\/\\]+\Q$Prefix\E\Z//g)
                { # Linux, FreeBSD
                    next;
                }
            }
            if(not $Dep)
            { # nothing to include
                next;
            }
            if(is_default_include_dir($Dep))
            { # included by the compiler
                next;
            }
            if(get_depth($Dep)==1)
            { # too short
                next;
            }
            if(isLibcDir($Dep))
            { # do NOT include /usr/include/{sys,bits}
                next;
            }
            $IncDir{$Dep}=1;
        }
    }
    $Cache{"get_HeaderDeps"}{$LibVersion}{$AbsPath} = sortIncPaths([keys(%IncDir)], $LibVersion);
    return @{$Cache{"get_HeaderDeps"}{$LibVersion}{$AbsPath}};
}

sub sortIncPaths($$)
{
    my ($ArrRef, $LibVersion) = @_;
    @{$ArrRef} = sort {$b cmp $a} @{$ArrRef};
    @{$ArrRef} = sort {get_depth($a)<=>get_depth($b)} @{$ArrRef};
    @{$ArrRef} = sort {$Header_Dependency{$LibVersion}{$b}<=>$Header_Dependency{$LibVersion}{$a}} @{$ArrRef};
    return $ArrRef;
}

sub joinPath($$) {
    return join($SLASH, @_);
}

sub get_namespace_additions($)
{
    my $NameSpaces = $_[0];
    my ($Additions, $AddNameSpaceId) = ("", 1);
    foreach my $NS (sort {$a=~/_/ <=> $b=~/_/} sort {lc($a) cmp lc($b)} keys(%{$NameSpaces}))
    {
        next if(not $NS or $NameSpaces->{$NS}==-1);
        next if($NS=~/(\A|::)iterator(::|\Z)/i);
        next if($NS=~/\A__/i);
        next if(($NS=~/\Astd::/ or $NS=~/\A(std|tr1|rel_ops|fcntl)\Z/) and not $STDCXX_TESTING);
        $NestedNameSpaces{$Version}{$NS} = 1;# for future use in reports
        my ($TypeDecl_Prefix, $TypeDecl_Suffix) = ();
        my @NS_Parts = split(/::/, $NS);
        next if($#NS_Parts==-1);
        next if($NS_Parts[0]=~/\A(random|or)\Z/);
        foreach my $NS_Part (@NS_Parts)
        {
            $TypeDecl_Prefix .= "namespace $NS_Part\{";
            $TypeDecl_Suffix .= "}";
        }
        my $TypeDecl = $TypeDecl_Prefix."typedef int tmp_add_type_$AddNameSpaceId;".$TypeDecl_Suffix;
        my $FuncDecl = "$NS\:\:tmp_add_type_$AddNameSpaceId tmp_add_func_$AddNameSpaceId(){return 0;};";
        $Additions.="  $TypeDecl\n  $FuncDecl\n";
        $AddNameSpaceId+=1;
    }
    return $Additions;
}

sub path_format($$)
{# forward slash to pass into MinGW GCC
    my ($Path, $Fmt) = @_;
    if($Fmt eq "windows") {
        $Path=~s/\//\\/g;
        $Path=lc($Path);
    }
    else {
        $Path=~s/\\/\//g;
    }
    return $Path;
}

sub inc_opt($$)
{
    my ($Path, $Style) = @_;
    if($Style eq "GCC")
    {# GCC options
        if($OSgroup eq "windows")
        {# to MinGW GCC
            return "-I\"".path_format($Path, "unix")."\"";
        }
        elsif($OSgroup eq "macos"
        and $Path=~/\.framework\Z/)
        {# to Apple's GCC
            return "-F".esc(get_dirname($Path));
        }
        else {
            return "-I".esc($Path);
        }
    }
    elsif($Style eq "CL") {
        return "/I \"$Path\"";
    }
    return "";
}

sub platformSpecs($)
{
    my $LibVersion = $_[0];
    my $Arch = getArch($LibVersion);
    if($OStarget eq "symbian")
    { # options for GCCE compiler
        my %Symbian_Opts = map {$_=>1} (
            "-D__GCCE__",
            "-DUNICODE",
            "-fexceptions",
            "-D__SYMBIAN32__",
            "-D__MARM_INTERWORK__",
            "-D_UNICODE",
            "-D__S60_50__",
            "-D__S60_3X__",
            "-D__SERIES60_3X__",
            "-D__EPOC32__",
            "-D__MARM__",
            "-D__EABI__",
            "-D__MARM_ARMV5__",
            "-D__SUPPORT_CPP_EXCEPTIONS__",
            "-march=armv5t",
            "-mapcs",
            "-mthumb-interwork",
            "-DEKA2",
            "-DSYMBIAN_ENABLE_SPLIT_HEADERS"
        );
        return join(" ", keys(%Symbian_Opts));
    }
    elsif($OSgroup eq "windows"
    and get_dumpmachine($GCC_PATH)=~/mingw/i)
    { # add options to MinGW compiler
      # to simulate the MSVC compiler
        my %MinGW_Opts = map {$_=>1} (
            "-D_WIN32",
            "-D_STDCALL_SUPPORTED",
            "-D__int64=\"long long\"",
            "-D__int32=int",
            "-D__int16=short",
            "-D__int8=char",
            "-D__possibly_notnullterminated=\" \"",
            "-D__nullterminated=\" \"",
            "-D__nullnullterminated=\" \"",
            "-D__w64=\" \"",
            "-D__ptr32=\" \"",
            "-D__ptr64=\" \"",
            "-D__forceinline=inline",
            "-D__inline=inline",
            "-D__uuidof(x)=IID()",
            "-D__try=",
            "-D__except(x)=",
            "-D__declspec(x)=__attribute__((x))",
            "-D__pragma(x)=",
            "-D_inline=inline",
            "-D__forceinline=__inline",
            "-D__stdcall=__attribute__((__stdcall__))",
            "-D__cdecl=__attribute__((__cdecl__))",
            "-D__fastcall=__attribute__((__fastcall__))",
            "-D__thiscall=__attribute__((__thiscall__))",
            "-D_stdcall=__attribute__((__stdcall__))",
            "-D_cdecl=__attribute__((__cdecl__))",
            "-D_fastcall=__attribute__((__fastcall__))",
            "-D_thiscall=__attribute__((__thiscall__))",
            "-DSHSTDAPI_(x)=x",
            "-D_MSC_EXTENSIONS",
            "-DSECURITY_WIN32",
            "-D_MSC_VER=1500",
            "-D_USE_DECLSPECS_FOR_SAL",
            "-D__noop=\" \"",
            "-DDECLSPEC_DEPRECATED=\" \"",
            "-D__builtin_alignof(x)=__alignof__(x)",
            "-DSORTPP_PASS");
        if($Arch eq "x86") {
            $MinGW_Opts{"-D_M_IX86=300"}=1;
        }
        elsif($Arch eq "x86_64") {
            $MinGW_Opts{"-D_M_AMD64=300"}=1;
        }
        elsif($Arch eq "ia64") {
            $MinGW_Opts{"-D_M_IA64=300"}=1;
        }
        return join(" ", keys(%MinGW_Opts));
    }
    return "";
}

my %C_Structure = map {$_=>1} (
# FIXME: Can't separate union and struct data types before dumping,
# so it sometimes cause compilation errors for unknown reason
# when trying to declare TYPE* tmp_add_class_N
# This is a list of such structures + list of other C structures
    "sigval",
    "sigevent",
    "sigaction",
    "sigvec",
    "sigstack",
    "timeval",
    "timezone",
    "rusage",
    "rlimit",
    "wait",
    "flock",
    "stat",
    "stat64",
    "if_nameindex",
    "usb_device",
    "sigaltstack",
    "sysinfo",
    "timeLocale",
    "tcp_debug",
    "rpc_createerr",
# Other C structures appearing in every dump
    "timespec",
    "random_data",
    "drand48_data",
    "_IO_marker",
    "_IO_FILE",
    "lconv",
    "sched_param",
    "tm",
    "itimerspec",
    "_pthread_cleanup_buffer",
    "fd_set",
    "siginfo"
);

sub getCompileCmd($$$)
{
    my ($Path, $Opt, $Inc) = @_;
    my $GccCall = $GCC_PATH;
    if($Opt) {
        $GccCall .= " ".$Opt;
    }
    $GccCall .= " -x ";
    if($OSgroup eq "macos") {
        $GccCall .= "objective-";
    }
    if(check_gcc_version($GCC_PATH, "4"))
    { # compile as "C++" header
      # to obtain complete dump using GCC 4.0
        $GccCall .= "c++-header";
    }
    else
    { # compile as "C++" source
      # GCC 3.3 cannot compile headers
        $GccCall .= "c++";
    }
    if(my $Opts = platformSpecs($Version))
    {# platform-specific options
        $GccCall .= " ".$Opts;
    }
    # allow extra qualifications
    # and other nonconformant code
    $GccCall .= " -fpermissive -w";
    if($NoStdInc)
    {
        $GccCall .= " -nostdinc";
        $GccCall .= " -nostdinc++";
    }
    if($CompilerOptions{$Version})
    { # user-defined options
        $GccCall .= " ".$CompilerOptions{$Version};
    }
    $GccCall .= " \"$Path\"";
    if($Inc)
    { # include paths
        $GccCall .= " ".$Inc;
    }
    return $GccCall;
}

sub getDump()
{
    if(not $GCC_PATH) {
        exitStatus("Error", "internal error - GCC path is not set");
    }
    my %HeaderElems = (
        # Types
        "stdio.h" => ["FILE", "va_list"],
        "stddef.h" => ["NULL"],
        "time.h" => ["time_t"],
        "sys/types.h" => ["ssize_t", "u_int32_t", "u_short", "u_char",
             "u_int", "off_t", "u_quad_t", "u_long", "size_t"],
        "unistd.h" => ["gid_t", "uid_t"],
        "stdbool.h" => ["_Bool"],
        "rpc/xdr.h" => ["bool_t"],
        "in_systm.h" => ["n_long", "n_short"],
        # Fields
        "arpa/inet.h" => ["fw_src", "ip_src"]
    );
    my %AutoPreamble = ();
    foreach (keys(%HeaderElems)) {
        foreach my $Elem (@{$HeaderElems{$_}}) {
            $AutoPreamble{$Elem}=$_;
        }
    }
    my $TmpHeaderPath = "$TMP_DIR/dump$Version.h";
    my $MHeaderPath = $TmpHeaderPath;
    open(LIB_HEADER, ">$TmpHeaderPath") || die ("can't open file \'$TmpHeaderPath\': $!\n");
    if(my $AddDefines = $Descriptor{$Version}{"Defines"})
    {
        $AddDefines=~s/\n\s+/\n  /g;
        print LIB_HEADER "\n  // add defines\n  ".$AddDefines."\n";
    }
    print LIB_HEADER "\n  // add includes\n";
    my @PreambleHeaders = keys(%{$Include_Preamble{$Version}});
    @PreambleHeaders = sort {int($Include_Preamble{$Version}{$a}{"Position"})<=>int($Include_Preamble{$Version}{$b}{"Position"})} @PreambleHeaders;
    foreach my $Header_Path (@PreambleHeaders) {
        print LIB_HEADER "  #include \"".path_format($Header_Path, "unix")."\"\n";
    }
    my @Headers = keys(%{$Target_Headers{$Version}});
    @Headers = sort {int($Target_Headers{$Version}{$a}{"Position"})<=>int($Target_Headers{$Version}{$b}{"Position"})} @Headers;
    foreach my $Header_Path (@Headers) {
        next if($Include_Preamble{$Version}{$Header_Path});
        print LIB_HEADER "  #include \"".path_format($Header_Path, "unix")."\"\n";
    }
    close(LIB_HEADER);
    my $IncludeString = getIncString(getIncPaths(@PreambleHeaders, @Headers), "GCC");
    if($Debug)
    { # debug mode
        writeFile($DEBUG_PATH{$Version}."/recursive-includes.txt", Dumper(\%RecursiveIncludes));
        writeFile($DEBUG_PATH{$Version}."/include-paths.txt", Dumper($Cache{"get_HeaderDeps"}));
        writeFile($DEBUG_PATH{$Version}."/includes.txt", Dumper(\%Header_Includes));
        writeFile($DEBUG_PATH{$Version}."/default-includes.txt", Dumper(\%DefaultIncPaths));
    }
    # preprocessing stage
    checkPreprocessedUnit(callPreprocessor($TmpHeaderPath, $IncludeString, $Version));
    my $MContent = "";
    my $PreprocessCmd = getCompileCmd($TmpHeaderPath, "-E", $IncludeString);
    if($OStarget eq "windows"
    and get_dumpmachine($GCC_PATH)=~/mingw/i
    and $MinGWMode{$Version}!=-1)
    { # modify headers to compile by MinGW
        if(not $MContent)
        { # preprocessing
            $MContent = `$PreprocessCmd 2>$TMP_DIR/null`;
        }
        if($MContent=~s/__asm\s*(\{[^{}]*?\}|[^{};]*)//g)
        { # __asm { ... }
            $MinGWMode{$Version}=1;
        }
        if($MContent=~s/\s+(\/ \/.*?)\n/\n/g)
        { # comments after preprocessing
            $MinGWMode{$Version}=1;
        }
        if($MContent=~s/(\W)(0x[a-f]+|\d+)(i|ui)(8|16|32|64)(\W)/$1$2$5/g)
        { # 0xffui8
            $MinGWMode{$Version}=1;
        }
        if($MinGWMode{$Version}) {
            print "Using MinGW compatibility mode\n";
            $MHeaderPath = "$TMP_DIR/dump$Version.i";
        }
    }
    if($COMMON_LANGUAGE{$Version} eq "C"
    and $C99Mode{$Version}!=-1)
    { # rename "C++" keywords in "C" code
        if(not $MContent)
        { # preprocessing
            $MContent = `$PreprocessCmd 2>$TMP_DIR/null`;
        }
        if($MContent!~/\w::\w|\~(\w+\s*)\(/i)
        { # check lang
            my $RegExp = join("|", keys(%CppKeywords));
            if($MContent=~s/(\*\s*|\s+|\@|\()($RegExp)(\s*(\,|\(|\)|\;|\-\>|\.|\:\s*\d))/$1c99_$2$3/g)
            { # int operator(...);
              # int foo(int new, int class, int (*this)(int));
              # unsigned private: 8;
                $C99Mode{$Version} = 1;
            }
            if($MContent=~s/(\s+)(bool)(\s*;)/$1c99_$2$3/g)
            { # int bool;
                $C99Mode{$Version} = 1;
            }
            if($C99Mode{$Version}==1)
            { # try to change class to c99_class
                print "Using C99 compatibility mode\n";
                $MHeaderPath = "$TMP_DIR/dump$Version.i";
            }
        }
    }
    if($C99Mode{$Version}==1
    or $MinGWMode{$Version}==1)
    { # compile the corrected preprocessor output
        writeFile($MHeaderPath, $MContent);
    }
    if($COMMON_LANGUAGE{$Version} eq "C++" or $CheckHeadersOnly)
    { # add classes and namespaces to the dump
        my $CHdump = "-fdump-class-hierarchy -c";
        if($C99Mode{$Version}==1
        or $MinGWMode{$Version}==1) {
            $CHdump .= " -fpreprocessed";
        }
        my $ClassHierarchyCmd = getCompileCmd($MHeaderPath, $CHdump, $IncludeString);
        chdir($TMP_DIR);
        system("$ClassHierarchyCmd >null 2>&1");
        chdir($ORIG_DIR);
        if(my $ClassDump = (cmd_find($TMP_DIR,"f","*.class",1))[0])
        {
            my %AddClasses = ();
            my $Content = readFile($ClassDump);
            foreach my $ClassInfo (split(/\n\n/, $Content))
            {
                if($ClassInfo=~/\AClass\s+(.+)\s*/i)
                {
                    my $CName = $1;
                    next if($CName=~/\A(__|_objc_|_opaque_)/);
                    $TUnit_NameSpaces{$Version}{$CName} = -1;
                    if($CName=~/\A[\w:]+\Z/)
                    {# classes
                        $AddClasses{$CName} = 1;
                    }
                    if($CName=~/(\w[\w:]*)::/)
                    {# namespaces
                        my $NS = $1;
                        if(not defined $TUnit_NameSpaces{$Version}{$NS}) {
                            $TUnit_NameSpaces{$Version}{$NS} = 1;
                        }
                    }
                }
                elsif($ClassInfo=~/\AVtable\s+for\s+(.+)\n((.|\n)+)\Z/i)
                {# read v-tables (advanced approach)
                    my ($CName, $VTable) = ($1, $2);
                    $ClassVTable_Content{$Version}{$CName} = $VTable;
                }
            }
            if($Debug)
            { # debug mode
                mkpath($DEBUG_PATH{$Version});
                copy($ClassDump, $DEBUG_PATH{$Version}."/class-hierarchy.txt");
            }
            unlink($ClassDump);
            if(my $NS_Add = get_namespace_additions($TUnit_NameSpaces{$Version}))
            {# GCC on all supported platforms does not include namespaces to the dump by default
                appendFile($MHeaderPath, "\n  // add namespaces\n".$NS_Add);
            }
            # some GCC versions don't include class methods to the TU dump by default
            my ($AddClass, $ClassNum) = ("", 0);
            foreach my $CName (sort keys(%AddClasses))
            {
                next if($C_Structure{$CName});
                next if(not $STDCXX_TESTING and $CName=~/\Astd::/);
                next if(($CName=~tr![:]!!)>2);
                next if($SkipTypes{$Version}{$CName});
                if($CName=~/\A(.+)::[^:]+\Z/
                and $AddClasses{$1}) {
                    next;
                }
                $AddClass .= "  $CName* tmp_add_class_".($ClassNum++).";\n";
            }
            appendFile($MHeaderPath, "\n  // add classes\n".$AddClass);
        }
    }
    appendFile($LOG_PATH{$Version}, "Temporary header file \'$TmpHeaderPath\' with the following content will be compiled to create GCC translation unit dump:\n".readFile($TmpHeaderPath)."\n");
    # create TU dump
    my $TUdump = "-fdump-translation-unit -fkeep-inline-functions -c";
    if($C99Mode{$Version}==1
    or $MinGWMode{$Version}==1) {
        $TUdump .= " -fpreprocessed";
    }
    my $SyntaxTreeCmd = getCompileCmd($MHeaderPath, $TUdump, $IncludeString);
    appendFile($LOG_PATH{$Version}, "The GCC parameters:\n  $SyntaxTreeCmd\n\n");
    chdir($TMP_DIR);
    system($SyntaxTreeCmd." >$TMP_DIR/tu_errors 2>&1");
    if($?)
    {# failed to compile, but the TU dump still can be created
        my $Errors = readFile("$TMP_DIR/tu_errors");
        if($Errors=~/c99_/)
        {# disable c99 mode
            $C99Mode{$Version}=-1;
            print "Disabling C99 compatibility mode\n";
            unlink($LOG_PATH{$Version});
            $TMP_DIR = tempdir(CLEANUP=>1);
            return getDump();
        }
        elsif($AutoPreambleMode{$Version}!=-1
        and my @Types = keys(%AutoPreamble))
        {
            my %AddHeaders = ();
            foreach my $Type (@Types)
            {
                if($Errors=~/error\:\s*(field\s*|)\W\Q$Type\E\W/)
                { # error: 'FILE' has not been declared
                    $AddHeaders{path_format($AutoPreamble{$Type}, $OSgroup)}=1;
                }
            }
            if(my @Headers = sort {$b cmp $a} keys(%AddHeaders))
            { # sys/types.h should be the first
                foreach my $Num (0 .. $#Headers)
                {
                    my $Name = $Headers[$Num];
                    if(my $Path = identify_header($Name, $Version))
                    {# add automatic preamble headers
                        $Include_Preamble{$Version}{$Path}{"Position"} = keys(%{$Include_Preamble{$Version}});
                        print "Add $Name preamble header\n";
                    }
                }
                $AutoPreambleMode{$Version}=-1;
                unlink($LOG_PATH{$Version});
                $TMP_DIR = tempdir(CLEANUP=>1);
                return getDump();
            }
        }
        elsif($MinGWMode{$Version}!=-1)
        {
            $MinGWMode{$Version}=-1;
            unlink($LOG_PATH{$Version});
            $TMP_DIR = tempdir(CLEANUP=>1);
            return getDump();
        }
        # FIXME: handle other errors and try to recompile
        appendFile($LOG_PATH{$Version}, $Errors);
        print STDERR "\nERROR: some errors occurred, see error log for details:\n  ".$LOG_PATH{$Version}."\n\n";
        $COMPILE_ERRORS = $ERROR_CODE{"Compile_Error"};
        appendFile($LOG_PATH{$Version}, "\n");# new line
    }
    chdir($ORIG_DIR);
    unlink($TmpHeaderPath, $MHeaderPath);
    return (cmd_find($TMP_DIR,"f","*.tu",1))[0];
}

sub cmd_file($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    if(my $CmdPath = get_CmdPath("file")) {
        return `$CmdPath -b \"$Path\"`;
    }
    return "";
}

sub getIncString($$)
{
    my ($ArrRef, $Style) = @_;
    return if(not $ArrRef or $#{$ArrRef}<0);
    my $String = "";
    foreach (@{$ArrRef}) {
        $String .= " ".inc_opt($_, $Style);
    }
    return $String;
}

sub getIncPaths(@)
{
    my @HeaderPaths = @_;
    my @IncPaths = ();
    if($INC_PATH_AUTODETECT{$Version})
    { # auto-detecting dependencies
        my %Includes = ();
        foreach my $HPath (@HeaderPaths) {
            foreach my $Dir (get_HeaderDeps($HPath, $Version)) {
                $Includes{$Dir}=1;
            }
        }
        foreach my $Dir (keys(%{$Add_Include_Paths{$Version}}))
        {
            next if($Includes{$Dir});
            push(@IncPaths, $Dir);
        }
        foreach my $Dir (@{sortIncPaths([keys(%Includes)], $Version)}) {
            push(@IncPaths, $Dir);
        }
    }
    else
    { # user-defined paths
        foreach my $Dir (sort {get_depth($a)<=>get_depth($b)}
        sort {$b cmp $a} keys(%{$Include_Paths{$Version}})) {
            push(@IncPaths, $Dir);
        }
    }
    return \@IncPaths;
}

sub callPreprocessor($$$)
{
    my ($Path, $Inc, $LibVersion) = @_;
    return "" if(not $Path or not -f $Path);
    my $IncludeString=$Inc;
    if(not $Inc) {
        $IncludeString = getIncString(getIncPaths($Path), "GCC");
    }
    my $Cmd = getCompileCmd($Path, "-dD -E", $IncludeString);
    my $Path = "$TMP_DIR/preprocessed";
    system("$Cmd >$Path 2>$TMP_DIR/null");
    return $Path;
}

sub cmd_find($$$$)
{ # native "find" is much faster than File::Find (~6x)
  # also the File::Find doesn't support --maxdepth N option
  # so using the cross-platform wrapper for the native one
    my ($Path, $Type, $Name, $MaxDepth) = @_;
    return () if(not $Path or not -e $Path);
    if($OSgroup eq "windows")
    {
        my $DirCmd = get_CmdPath("dir");
        if(not $DirCmd) {
            exitStatus("Not_Found", "can't find \"dir\" command");
        }
        $Path=~s/[\\]+\Z//;
        $Path = get_abs_path($Path);
        $Path = path_format($Path, $OSgroup);
        my $Cmd = $DirCmd." \"$Path\" /B /O";
        if($MaxDepth!=1) {
            $Cmd .= " /S";
        }
        if($Type eq "d") {
            $Cmd .= " /AD";
        }
        my @Files = ();
        if($Name)
        { # FIXME: how to search file names in MS shell?
            $Name=~s/\*/.*/g if($Name!~/\]/);
            foreach my $File (split(/\n/, `$Cmd`))
            {
                if($File=~/$Name\Z/i) {
                    push(@Files, $File);
                }
            }
        }
        else {
            @Files = split(/\n/, `$Cmd 2>$TMP_DIR/null`);
        }
        my @AbsPaths = ();
        foreach my $File (@Files)
        {
            if(not is_abs($File)) {
                $File = joinPath($Path, $File);
            }
            if($Type eq "f" and not -f $File)
            { # skip dirs
                next;
            }
            push(@AbsPaths, path_format($File, $OSgroup));
        }
        if($Type eq "d") {
            push(@AbsPaths, $Path);
        }
        return @AbsPaths;
    }
    else
    {
        my $FindCmd = get_CmdPath("find");
        if(not $FindCmd) {
            exitStatus("Not_Found", "can't find a \"find\" command");
        }
        $Path = get_abs_path($Path);
        if(-d $Path and -l $Path
        and $Path!~/\/\Z/)
        { # for directories that are symlinks
            $Path.="/";
        }
        my $Cmd = $FindCmd." \"$Path\"";
        if($MaxDepth) {
            $Cmd .= " -maxdepth $MaxDepth";
        }
        if($Type) {
            $Cmd .= " -type $Type";
        }
        if($Name)
        { # file name
            if($Name=~/\]/) {
                $Cmd .= " -regex \"$Name\"";
            }
            else {
                $Cmd .= " -name \"$Name\"";
            }
        }
        return split(/\n/, `$Cmd 2>$TMP_DIR/null`);
    }
}

sub unpackDump($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    $Path = get_abs_path($Path);
    $Path = path_format($Path, $OSgroup);
    my ($Dir, $FileName) = separate_path($Path);
    if($FileName=~s/\Q.zip\E\Z//g)
    { # *.zip
        my $UnzipCmd = get_CmdPath("unzip");
        if(not $UnzipCmd) {
            exitStatus("Not_Found", "can't find \"unzip\" command");
        }
        chdir($TMP_DIR);
        system("$UnzipCmd \"$Path\" >$TMP_DIR/contents.txt");
        if($?) {
            exitStatus("Error", "can't extract \'$Path\'");
        }
        chdir($ORIG_DIR);
        my @Contents = ();
        foreach (split("\n", readFile("$TMP_DIR/contents.txt")))
        {
            if(/inflating:\s*([^\s]+)/) {
                push(@Contents, $1);
            }
        }
        if(not @Contents) {
            exitStatus("Error", "can't extract \'$Path\'");
        }
        return joinPath($TMP_DIR, $Contents[0]);
    }
    elsif($FileName=~s/\Q.tar.gz\E\Z//g)
    { # *.tar.gz
        if($OSgroup eq "windows")
        { # -xvzf option is not implemented in tar.exe (2003)
          # use "gzip.exe -k -d -f" + "tar.exe -xvf" instead
            my $TarCmd = get_CmdPath("tar");
            if(not $TarCmd) {
                exitStatus("Not_Found", "can't find \"tar\" command");
            }
            my $GzipCmd = get_CmdPath("gzip");
            if(not $GzipCmd) {
                exitStatus("Not_Found", "can't find \"gzip\" command");
            }
            chdir($TMP_DIR);
            system("$GzipCmd -k -d -f \"$Path\"");# keep input files (-k)
            if($?) {
                exitStatus("Error", "can't extract \'$Path\'");
            }
            system("$TarCmd -xvf \"$Dir\\$FileName.tar\" >$TMP_DIR/contents.txt");
            if($?) {
                exitStatus("Error", "can't extract \'$Path\'");
            }
            chdir($ORIG_DIR);
            unlink($Dir."/".$FileName.".tar");
            my @Contents = split("\n", readFile("$TMP_DIR/contents.txt"));
            if(not @Contents) {
                exitStatus("Error", "can't extract \'$Path\'");
            }
            return joinPath($TMP_DIR, $Contents[0]);
        }
        else
        { # Unix
            my $TarCmd = get_CmdPath("tar");
            if(not $TarCmd) {
                exitStatus("Not_Found", "can't find \"tar\" command");
            }
            chdir($TMP_DIR);
            system("$TarCmd -xvzf \"$Path\" >$TMP_DIR/contents.txt");
            if($?) {
                exitStatus("Error", "can't extract \'$Path\'");
            }
            chdir($ORIG_DIR);
            # The content file name may be different
            # from the package file name
            my @Contents = split("\n", readFile("$TMP_DIR/contents.txt"));
            if(not @Contents) {
                exitStatus("Error", "can't extract \'$Path\'");
            }
            return joinPath($TMP_DIR, $Contents[0]);
        }
    }
}

sub createArchive($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    my ($Dir, $Name) = separate_path($Path);
    if($OSgroup eq "windows")
    { # *.zip
        my $ZipCmd = get_CmdPath("zip");
        if(not $ZipCmd) {
            exitStatus("Not_Found", "can't find \"zip\"");
        }
        my $Package = $Path.".zip";
        unlink($Package);
        chdir($Dir);
        system("$ZipCmd \"$Name.zip\" \"$Name\" >$TMP_DIR/null");
        if($?)
        {# cannot allocate memory (or other problems with "zip")
            unlink($Path);
            exitStatus("Error", "can't pack the ABI dump: ".$!);
        }
        chdir($ORIG_DIR);
        unlink($Path);
        return $Package;
    }
    else
    { # *.tar.gz
        my $TarCmd = get_CmdPath("tar");
        if(not $TarCmd) {
            exitStatus("Not_Found", "can't find \"tar\"");
        }
        my $GzipCmd = get_CmdPath("gzip");
        if(not $GzipCmd) {
            exitStatus("Not_Found", "can't find \"gzip\"");
        }
        my $Package = $Path.".tar.gz";
        unlink($Package);
        chdir($Dir);
        system($TarCmd, "-cf", "$Name.tar", $Name);
        if($?)
        {# cannot allocate memory (or other problems with "tar")
            unlink($Path);
            exitStatus("Error", "can't pack the ABI dump: ".$!);
        }
        system($GzipCmd, "$Name.tar");
        if($?)
        {# cannot allocate memory (or other problems with "gzip")
            unlink($Path);
            exitStatus("Error", "can't pack the ABI dump: ".$!);
        }
        chdir($ORIG_DIR);
        unlink($Path);
        return $Package;
    }
}

sub is_header_file($)
{
    if($_[0]=~/\.($HEADER_EXT)\Z/i) {
        return $_[0];
    }
    return 0;
}

sub is_not_header($)
{
    if($_[0]=~/\.\w+\Z/
    and $_[0]!~/\.($HEADER_EXT)\Z/i) {
        return 1;
    }
    return 0;
}

sub is_header($$$)
{
    my ($Header, $UserDefined, $LibVersion) = @_;
    return 0 if(-d $Header);
    if(-f $Header) {
        $Header = get_abs_path($Header);
    }
    else {
        if(is_abs($Header)) {
            # incorrect absolute path
            return 0;
        }
        if(my $HPath = identify_header($Header, $LibVersion)) {
            $Header = $HPath;
        }
        else {
            # can't find header
            return 0;
        }
    }
    if($Header=~/\.\w+\Z/) {
        # have an extension
        return is_header_file($Header);
    }
    else {
        if($UserDefined) {
            # user defined file without extension
            # treated as header file by default
            return $Header;
        }
        else {
            # directory content
            if(cmd_file($Header)=~/C[\+]*\s+program/i) {
                # !~/HTML|XML|shared|dynamic/i
                return $Header;
            }
        }
    }
    return 0;
}

sub readHeaders($)
{
    $Version = $_[0];
    print "checking header(s) ".$Descriptor{$Version}{"Version"}." ...\n";
    my $DumpPath = getDump();
    if(not $DumpPath) {
        exitStatus("Cannot_Compile", "can't compile header(s)");
    }
    if($Debug)
    { # debug mode
        mkpath($DEBUG_PATH{$Version});
        copy($DumpPath, $DEBUG_PATH{$Version}."/translation-unit.txt");
    }
    getInfo($DumpPath);
}

sub prepareInterfaces($)
{
    my $LibVersion = $_[0];
    foreach my $FuncInfoId (keys(%{$SymbolInfo{$LibVersion}}))
    {
        my $MnglName = $SymbolInfo{$LibVersion}{$FuncInfoId}{"MnglName"};
        if(not $MnglName)
        { # ABI dumps don't contain mangled names for C-functions
            $MnglName = $SymbolInfo{$LibVersion}{$FuncInfoId}{"ShortName"};
            $SymbolInfo{$LibVersion}{$FuncInfoId}{"MnglName"} = $MnglName;
        }
        %{$CompleteSignature{$LibVersion}{$MnglName}} = %{$SymbolInfo{$LibVersion}{$FuncInfoId}};
        # symbol and its symlink have same signatures
        if($SymVer{$LibVersion}{$MnglName}) {
            %{$CompleteSignature{$LibVersion}{$SymVer{$LibVersion}{$MnglName}}} = %{$SymbolInfo{$LibVersion}{$FuncInfoId}};
        }
        delete($SymbolInfo{$LibVersion}{$FuncInfoId});
    }
    if($COMMON_LANGUAGE{$LibVersion} eq "C++"
    or $CheckHeadersOnly or $OSgroup eq "windows") {
        translateSymbols(keys(%{$CompleteSignature{$LibVersion}}), $LibVersion);
        translateSymbols(keys(%{$DepSymbols{$LibVersion}}), $LibVersion);
    }
    if(not keys(%{$CompleteSignature{$LibVersion}}))
    {
        if($CheckHeadersOnly or $CheckObjectsOnly) {
            exitStatus("Error", "the set of public symbols is empty (".$Descriptor{$Version}{"Version"}.")");
        }
        else {
            exitStatus("Error", "the sets of public symbols in headers and $SLIB_TYPE libraries have empty intersection (".$Descriptor{$Version}{"Version"}.")");
        }
        
    }
    foreach my $MnglName (keys(%{$CompleteSignature{$LibVersion}}))
    {# detect allocable classes with public exported constructors
      # or classes with auto-generated or inline-only constructors
        if(my $ClassId = $CompleteSignature{$LibVersion}{$MnglName}{"Class"})
        {
            my $ClassName = get_TypeName($ClassId, $LibVersion);
            if($CompleteSignature{$LibVersion}{$MnglName}{"Constructor"}
            and not $CompleteSignature{$LibVersion}{$MnglName}{"InLine"})
            {# Class() { ... } will not be exported
                if(not $CompleteSignature{$LibVersion}{$MnglName}{"Private"})
                {
                    if(link_symbol($MnglName, $LibVersion, "-Deps")) {
                        $AllocableClass{$LibVersion}{$ClassName} = 1;
                    }
                }
            }
            if(not $CompleteSignature{$LibVersion}{$MnglName}{"Private"})
            {# all imported class methods
                if($CheckHeadersOnly)
                {
                    if(not $CompleteSignature{$LibVersion}{$MnglName}{"InLine"}
                    or $CompleteSignature{$LibVersion}{$MnglName}{"Virt"})
                    {# all symbols except non-virtual inline
                        $ClassMethods{$LibVersion}{$ClassName}{$MnglName} = 1;
                    }
                }
                elsif(link_symbol($MnglName, $LibVersion, "-Deps"))
                {# all symbols
                    $ClassMethods{$LibVersion}{$ClassName}{$MnglName} = 1;
                }
            }
            $ClassToId{$LibVersion}{$ClassName} = $ClassId;
        }
    }
    foreach my $MnglName (keys(%VTableClass))
    {# reconstruct header name for v-tables
        if($MnglName=~/\A_ZTV/)
        {
            if(my $ClassName = $VTableClass{$MnglName})
            {
                if(my $ClassId = $TName_Tid{$LibVersion}{$ClassName}) {
                    $CompleteSignature{$LibVersion}{$MnglName}{"Header"} = get_TypeHeader($ClassId, $LibVersion);
                }
            }
        }
    }
}

sub synchTr($)
{
    my $Translation = $_[0];
    if($OSgroup eq "windows") {
        $Translation=~s/\(\)(|\s*const)\Z/\(void\)$1/g;
    }
    return $Translation;
}

sub formatDump($)
{ # remove unnecessary data from the ABI dump
    my $LibVersion = $_[0];
    foreach my $FuncInfoId (keys(%{$SymbolInfo{$LibVersion}}))
    {
        my $MnglName = $SymbolInfo{$LibVersion}{$FuncInfoId}{"MnglName"};
        if(not $MnglName) {
            delete($SymbolInfo{$LibVersion}{$FuncInfoId});
            next;
        }
        if($MnglName eq $SymbolInfo{$LibVersion}{$FuncInfoId}{"ShortName"}) {
            delete($SymbolInfo{$LibVersion}{$FuncInfoId}{"MnglName"});
        }
        if($TargetHeader
        and $SymbolInfo{$LibVersion}{$FuncInfoId}{"Header"} ne $TargetHeader)
        { # user-defined header
            delete($SymbolInfo{$LibVersion}{$FuncInfoId});
            next;
        }
        if(not link_symbol($MnglName, $LibVersion, "-Deps")
        and not $SymbolInfo{$LibVersion}{$FuncInfoId}{"Virt"}
        and not $SymbolInfo{$LibVersion}{$FuncInfoId}{"PureVirt"})
        { # removing src only and all external functions
            if(not $CheckHeadersOnly) {
                delete($SymbolInfo{$LibVersion}{$FuncInfoId});
                next;
            }
        }
        if(not symbolFilter($MnglName, $LibVersion, "Public")) {
            delete($SymbolInfo{$LibVersion}{$FuncInfoId});
            next;
        }
        my %FuncInfo = %{$SymbolInfo{$LibVersion}{$FuncInfoId}};
        register_TypeUsing($Tid_TDid{$LibVersion}{$FuncInfo{"Return"}}, $FuncInfo{"Return"}, $LibVersion);
        register_TypeUsing($Tid_TDid{$LibVersion}{$FuncInfo{"Class"}}, $FuncInfo{"Class"}, $LibVersion);
        foreach my $Param_Pos (keys(%{$FuncInfo{"Param"}}))
        {
            my $Param_TypeId = $FuncInfo{"Param"}{$Param_Pos}{"type"};
            register_TypeUsing($Tid_TDid{$LibVersion}{$Param_TypeId}, $Param_TypeId, $LibVersion);
        }
        if(not keys(%{$SymbolInfo{$LibVersion}{$FuncInfoId}{"Param"}})) {
            delete($SymbolInfo{$LibVersion}{$FuncInfoId}{"Param"});
        }
    }
    foreach my $TDid (keys(%{$TypeInfo{$LibVersion}}))
    {
        if(not keys(%{$TypeInfo{$LibVersion}{$TDid}})) {
            delete($TypeInfo{$LibVersion}{$TDid});
        }
        else
        {
            foreach my $Tid (keys(%{$TypeInfo{$LibVersion}{$TDid}}))
            {
                if(not $UsedType{$LibVersion}{$TDid}{$Tid})
                {
                    delete($TypeInfo{$LibVersion}{$TDid}{$Tid});
                    if(not keys(%{$TypeInfo{$LibVersion}{$TDid}})) {
                        delete($TypeInfo{$LibVersion}{$TDid});
                    }
                    delete($Tid_TDid{$LibVersion}{$Tid}) if($Tid_TDid{$LibVersion}{$Tid} eq $TDid);
                }
                else
                {# clean attributes
                    if(not $TypeInfo{$LibVersion}{$TDid}{$Tid}{"TDid"}) {
                        delete($TypeInfo{$LibVersion}{$TDid}{$Tid}{"TDid"});
                    }
                    if(not $TypeInfo{$LibVersion}{$TDid}{$Tid}{"NameSpace"}) {
                        delete($TypeInfo{$LibVersion}{$TDid}{$Tid}{"NameSpace"});
                    }
                    if(defined $TypeInfo{$LibVersion}{$TDid}{$Tid}{"BaseType"}
                    and not $TypeInfo{$LibVersion}{$TDid}{$Tid}{"BaseType"}{"TDid"}) {
                        delete($TypeInfo{$LibVersion}{$TDid}{$Tid}{"BaseType"}{"TDid"});
                    }
                    if(not $TypeInfo{$LibVersion}{$TDid}{$Tid}{"Header"}) {
                        delete($TypeInfo{$LibVersion}{$TDid}{$Tid}{"Header"});
                    }
                    if(not $TypeInfo{$LibVersion}{$TDid}{$Tid}{"Line"}) {
                        delete($TypeInfo{$LibVersion}{$TDid}{$Tid}{"Line"});
                    }
                }
            }
        }
    }
}

sub register_TypeUsing($$$)
{
    my ($TypeDeclId, $TypeId, $LibVersion) = @_;
    return if($UsedType{$LibVersion}{$TypeDeclId}{$TypeId});
    my %Type = get_Type($TypeDeclId, $TypeId, $LibVersion);
    if($Type{"Type"}=~/\A(Struct|Union|Class|FuncPtr|MethodPtr|FieldPtr|Enum)\Z/)
    {
        $UsedType{$LibVersion}{$TypeDeclId}{$TypeId} = 1;
        if($Type{"Type"}=~/\A(Struct|Class)\Z/)
        {
            if(my $ThisPtrId = getTypeIdByName(get_TypeName($TypeId, $LibVersion)."*const", $LibVersion))
            {# register "this" pointer
                my $ThisPtrDId = $Tid_TDid{$LibVersion}{$ThisPtrId};
                my %ThisPtrType = get_Type($ThisPtrDId, $ThisPtrId, $LibVersion);
                $UsedType{$LibVersion}{$ThisPtrDId}{$ThisPtrId} = 1;
                register_TypeUsing($ThisPtrType{"BaseType"}{"TDid"}, $ThisPtrType{"BaseType"}{"Tid"}, $LibVersion);
            }
            foreach my $BaseId (keys(%{$Type{"Base"}}))
            {# register base classes
                register_TypeUsing($Tid_TDid{$LibVersion}{$BaseId}, $BaseId, $LibVersion);
            }
        }
        foreach my $Memb_Pos (keys(%{$Type{"Memb"}}))
        {
            my $Member_TypeId = $Type{"Memb"}{$Memb_Pos}{"type"};
            register_TypeUsing($Tid_TDid{$LibVersion}{$Member_TypeId}, $Member_TypeId, $LibVersion);
        }
        if($Type{"Type"} eq "FuncPtr"
        or $Type{"Type"} eq "MethodPtr") {
            my $ReturnType = $Type{"Return"};
            register_TypeUsing($Tid_TDid{$LibVersion}{$ReturnType}, $ReturnType, $LibVersion);
            foreach my $Memb_Pos (keys(%{$Type{"Param"}}))
            {
                my $Member_TypeId = $Type{"Param"}{$Memb_Pos}{"type"};
                register_TypeUsing($Tid_TDid{$LibVersion}{$Member_TypeId}, $Member_TypeId, $LibVersion);
            }
        }
    }
    elsif($Type{"Type"}=~/\A(Const|Pointer|Ref|Volatile|Restrict|Array|Typedef)\Z/)
    {
        $UsedType{$LibVersion}{$TypeDeclId}{$TypeId} = 1;
        register_TypeUsing($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    }
    elsif($Type{"Type"} eq "Intrinsic") {
        $UsedType{$LibVersion}{$TypeDeclId}{$TypeId} = 1;
    }
    else
    {
        delete($TypeInfo{$LibVersion}{$TypeDeclId}{$TypeId});
        if(not keys(%{$TypeInfo{$LibVersion}{$TypeDeclId}})) {
            delete($TypeInfo{$LibVersion}{$TypeDeclId});
        }
        if($Tid_TDid{$LibVersion}{$TypeId} eq $TypeDeclId) {
            delete($Tid_TDid{$LibVersion}{$TypeId});
        }
    }
}

sub findMethod($$$)
{
    my ($VirtFunc, $ClassId, $LibVersion) = @_;
    foreach my $BaseClass_Id (keys(%{$TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$ClassId}}{$ClassId}{"Base"}}))
    {
        if(my $VirtMethodInClass = findMethod_Class($VirtFunc, $BaseClass_Id, $LibVersion)) {
            return $VirtMethodInClass;
        }
        elsif(my $VirtMethodInBaseClasses = findMethod($VirtFunc, $BaseClass_Id, $LibVersion)) {
            return $VirtMethodInBaseClasses;
        }
    }
    return "";
}

sub findMethod_Class($$$)
{
    my ($VirtFunc, $ClassId, $LibVersion) = @_;
    my $ClassName = get_TypeName($ClassId, $LibVersion);
    return "" if(not defined $VirtualTable{$LibVersion}{$ClassName});
    my $TargetSuffix = get_symbol_suffix($VirtFunc);
    my $TargetShortName = $CompleteSignature{$LibVersion}{$VirtFunc}{"ShortName"};
    foreach my $Candidate (keys(%{$VirtualTable{$LibVersion}{$ClassName}}))
    {# search for interface with the same parameters suffix (overridden)
        if($TargetSuffix eq get_symbol_suffix($Candidate))
        {
            if($CompleteSignature{$LibVersion}{$VirtFunc}{"Destructor"}) {
                if($CompleteSignature{$LibVersion}{$Candidate}{"Destructor"}) {
                    if(($VirtFunc=~/D0E/ and $Candidate=~/D0E/)
                    or ($VirtFunc=~/D1E/ and $Candidate=~/D1E/)
                    or ($VirtFunc=~/D2E/ and $Candidate=~/D2E/)) {
                        return $Candidate;
                    }
                }
            }
            else {
                if($TargetShortName eq $CompleteSignature{$LibVersion}{$Candidate}{"ShortName"}) {
                    return $Candidate;
                }
            }
        }
    }
    return "";
}

sub registerVirtualTable($)
{
    my $LibVersion = $_[0];
    foreach my $Interface (keys(%{$CompleteSignature{$LibVersion}}))
    {
        if($CompleteSignature{$LibVersion}{$Interface}{"Virt"}
        or $CompleteSignature{$LibVersion}{$Interface}{"PureVirt"})
        {
            my $ClassName = get_TypeName($CompleteSignature{$LibVersion}{$Interface}{"Class"}, $LibVersion);
            next if(not $STDCXX_TESTING and $ClassName=~/\A(std::|__cxxabi)/);
            if($CompleteSignature{$LibVersion}{$Interface}{"Destructor"}
            and $Interface=~/D2E/)
            { # pure virtual D2-destructors are marked as "virt" in the dump
              # virtual D2-destructors are NOT marked as "virt" in the dump
              # both destructors are not presented in the v-table
                next;
            }
            my ($MnglName, $VersionSpec, $SymbolVersion) = separate_symbol($Interface);
            $VirtualTable{$LibVersion}{$ClassName}{$MnglName} = 1;
        }
        if($CheckHeadersOnly
        and $CompleteSignature{$LibVersion}{$Interface}{"Virt"})
        { # Register added and removed virtual symbols
          # This is necessary for --headers-only mode
          # Virtual function cannot be inline, so:
          # presence in headers <=> presence in shared libs
            if($LibVersion==2 and not $CompleteSignature{1}{$Interface}{"Header"})
            { # not presented in old-version headers
                $AddedInt{$Interface} = 1;
            }
            if($LibVersion==1 and not $CompleteSignature{2}{$Interface}{"Header"})
            { # not presented in new-version headers
                $RemovedInt{$Interface} = 1;
            }
        }
    }
}

sub registerOverriding($)
{
    my $LibVersion = $_[0];
    foreach my $ClassName (keys(%{$VirtualTable{$LibVersion}}))
    {
        foreach my $VirtFunc (keys(%{$VirtualTable{$LibVersion}{$ClassName}}))
        {
            next if($CompleteSignature{$LibVersion}{$VirtFunc}{"PureVirt"});
            if(my $OverriddenMethod = findMethod($VirtFunc, $TName_Tid{$LibVersion}{$ClassName}, $LibVersion))
            {# both overridden virtual and implemented pure virtual functions
                $CompleteSignature{$LibVersion}{$VirtFunc}{"Override"} = $OverriddenMethod;
                $OverriddenMethods{$LibVersion}{$OverriddenMethod}{$VirtFunc} = 1;
                delete($VirtualTable{$LibVersion}{$ClassName}{$VirtFunc});
            }
        }
        if(not keys(%{$VirtualTable{$LibVersion}{$ClassName}})) {
            delete($VirtualTable{$LibVersion}{$ClassName});
        }
    }
}

sub setVirtFuncPositions($)
{
    my $LibVersion = $_[0];
    foreach my $ClassName (keys(%{$VirtualTable{$LibVersion}}))
    {
        my ($Num, $RelPos, $AbsNum) = (1, 0, 1);
        foreach my $VirtFunc (sort {int($CompleteSignature{$LibVersion}{$a}{"Line"}) <=> int($CompleteSignature{$LibVersion}{$b}{"Line"})}
        sort keys(%{$VirtualTable{$LibVersion}{$ClassName}}))
        {
            if(not $CompleteSignature{1}{$VirtFunc}{"Override"}
            and not $CompleteSignature{2}{$VirtFunc}{"Override"})
            {
                if(defined $VirtualTable{1}{$ClassName} and defined $VirtualTable{1}{$ClassName}{$VirtFunc}
                and defined $VirtualTable{2}{$ClassName} and defined $VirtualTable{2}{$ClassName}{$VirtFunc})
                {# relative position excluding added and removed virtual functions
                    $CompleteSignature{$LibVersion}{$VirtFunc}{"RelPos"} = $RelPos++;
                }
                $VirtualTable{$LibVersion}{$ClassName}{$VirtFunc}=$Num++;
            }
            
        }
    }
    foreach my $ClassName (keys(%{$ClassToId{$LibVersion}}))
    {
        my $AbsNum = 1;
        foreach my $VirtFunc (getVTable($ClassToId{$LibVersion}{$ClassName}, $LibVersion)) {
            $VirtualTable_Full{$LibVersion}{$ClassName}{$VirtFunc}=$AbsNum++;
        }
    }
}

sub get_class_bases($$$)
{
    my ($ClassId, $LibVersion, $Recursive) = @_;
    my %ClassType = get_Type($Tid_TDid{$LibVersion}{$ClassId}, $ClassId, $LibVersion);
    return () if(not defined $ClassType{"Base"});
    my (@Bases, %Added) = ();
    foreach my $BaseId (sort {int($ClassType{"Base"}{$a}{"pos"})<=>int($ClassType{"Base"}{$b}{"pos"})}
    keys(%{$ClassType{"Base"}}))
    {
        if($Recursive) {
            foreach my $SubBaseId (get_class_bases($BaseId, $LibVersion, 1)) {
                push(@Bases, $SubBaseId);
            }
        }
        push(@Bases, $BaseId);
    }
    return @Bases;
}

sub getVTable($$)
{# return list of v-table elements
    my ($ClassId, $LibVersion) = @_;
    my @Bases = get_class_bases($ClassId, $LibVersion, 1);
    my @Elements = ();
    foreach my $BaseId (@Bases, $ClassId)
    {
        my $BName = get_TypeName($BaseId, $LibVersion);
        my @VFunctions = keys(%{$VirtualTable{$LibVersion}{$BName}});
        @VFunctions = sort {int($CompleteSignature{$LibVersion}{$a}{"Line"}) <=> int($CompleteSignature{$LibVersion}{$b}{"Line"})} @VFunctions;
        foreach my $VFunc (@VFunctions)
        {
            push(@Elements, $VFunc);
        }
    }
    return @Elements;
}

sub getVShift($$)
{
    my ($ClassId, $LibVersion) = @_;
    my @Bases = get_class_bases($ClassId, $LibVersion, 1);
    my $VShift = 0;
    foreach my $BaseId (@Bases)
    {
        my $BName = get_TypeName($BaseId, $LibVersion);
        if(defined $VirtualTable{$LibVersion}{$BName}) {
            $VShift+=keys(%{$VirtualTable{$LibVersion}{$BName}});
        }
    }
    return $VShift;
}

sub getShift($$)
{
    my ($ClassId, $LibVersion) = @_;
    my @Bases = get_class_bases($ClassId, $LibVersion, 0);
    my $Shift = 0;
    foreach my $BaseId (@Bases)
    {
        my $Size = get_TypeSize($BaseId, $LibVersion);
        if($Size!=1) {
            # empty base class
            $Shift+=get_TypeSize($BaseId, $LibVersion);
        }
    }
    return $Shift;
}

sub getVSize($$)
{
    my ($ClassName, $LibVersion) = @_;
    if(defined $VirtualTable{$LibVersion}{$ClassName})  {
        return keys(%{$VirtualTable{$LibVersion}{$ClassName}});
    }
    else {
        return 0;
    }
}

sub isCopyingClass($$)
{
    my ($TypeId, $LibVersion) = @_;
    return $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Copied"};
}

sub isLeafClass($$)
{
    my ($ClassId, $LibVersion) = @_;
    return (not keys(%{$Class_SubClasses{$LibVersion}{$ClassId}}));
}

sub havePubFields($)
{# check structured type for public fields
    return isAccessible($_[0], (), 0, -1);
}

sub isAccessible($$$$)
{# check interval in structured type for public fields
    my ($TypePtr, $Skip, $Start, $End) = @_;
    return 0 if(not $TypePtr);
    if($End==-1) {
        $End = keys(%{$TypePtr->{"Memb"}})-1;
    }
    foreach my $MemPos (keys(%{$TypePtr->{"Memb"}}))
    {
        if($Skip and $Skip->{$MemPos})
        { # skip removed/added fields
            next;
        }
        if(int($MemPos)>=$Start and int($MemPos)<=$End)
        {
            if(isPublic($TypePtr, $MemPos)) {
                return ($MemPos+1);
            }
        }
    }
    return 0;
}

sub getAlignment($$$)
{
    my ($Pos, $TypePtr, $LibVersion) = @_;
    my ($Alignment, $MSize) = ();
    my $Tid = $TypePtr->{"Memb"}{$Pos}{"type"};
    my %Type = get_Type($Tid_TDid{$LibVersion}{$Tid}, $Tid, $LibVersion);
    my $TSize = $Type{"Size"}*$BYTE_SIZE;
    $MSize = $Type{"Size"}*$BYTE_SIZE;
    if(my $BSize = $TypePtr->{"Memb"}{$Pos}{"bitfield"})
    {# bitfields
        ($TSize, $MSize) = ($WORD_SIZE{$LibVersion}*$BYTE_SIZE, $BSize);
    }
    # alignment
    $Alignment = $WORD_SIZE{$LibVersion}*$BYTE_SIZE;# default
    if(my $Computed = $TypePtr->{"Memb"}{$Pos}{"algn"})
    { # computed by GCC
        $Alignment = $Computed*$BYTE_SIZE;
    }
    elsif($TypePtr->{"Memb"}{$Pos}{"bitfield"})
    { # bitfields are 1 bit aligned
        $Alignment = 1;
    }
    elsif($TSize<$WORD_SIZE{$LibVersion})
    { # model
        $Alignment = $TSize;
    }
    return ($Alignment, $MSize);
}

sub getOffset($$$)
{ # offset of the field including padding
    my ($FieldPos, $TypePtr, $LibVersion) = @_;
    my $Offset = 0;
    foreach my $Pos (0 .. keys(%{$TypePtr->{"Memb"}})-1)
    {
        my ($Alignment, $MSize) = getAlignment($Pos, $TypePtr, $LibVersion);
        # padding
        my $Padding = 0;
        if($Offset % $Alignment!=0)
        { # not aligned, add padding
            $Padding = $Alignment - $Offset % $Alignment;
        }
        $Offset += $Padding;
        if($Pos==$FieldPos)
        { # after the padding
          # before the field
            return $Offset;
        }
        $Offset += $MSize;
    }
    return $FieldPos;# if something is going wrong
}

sub isMemPadded($$$$$)
{ # check if the target field can be added/removed/changed
  # without shifting other fields because of padding bits
    my ($FieldPos, $Size, $TypePtr, $Skip, $LibVersion) = @_;
    return 0 if($FieldPos==0);
    my $Offset = 0;
    my (%Alignment, %MSize) = ();
    my $MaxAlgn = 0;
    my $End = keys(%{$TypePtr->{"Memb"}})-1;
    my $NextField = $FieldPos+1;
    foreach my $Pos (0 .. $End)
    {
        if($Skip and $Skip->{$Pos})
        { # skip removed/added fields
            if($Pos > $FieldPos)
            { # after the target
                $NextField += 1;
                next;
            }
        }
        ($Alignment{$Pos}, $MSize{$Pos}) = getAlignment($Pos, $TypePtr, $LibVersion);
        if($Alignment{$Pos}>$MaxAlgn) {
            $MaxAlgn = $Alignment{$Pos};
        }
        if($Pos==$FieldPos)
        {
            if($Size==-1)
            { # added/removed fields
                if($Pos!=$End)
                { # skip target field and see
                  # if enough padding will be
                  # created on the next step
                  # to include this field
                    next;
                }
            }
        }
        # padding
        my $Padding = 0;
        if($Offset % $Alignment{$Pos}!=0)
        { # not aligned, add padding
            $Padding = $Alignment{$Pos} - $Offset % $Alignment{$Pos};
        }
        if($Pos==$NextField)
        { # try to place target field in the padding
            if($Size==-1)
            { # added/removed fields
                my $TPadding = 0;
                if($Offset % $Alignment{$FieldPos}!=0)
                {# padding of the target field
                    $TPadding = $Alignment{$FieldPos} - $Offset % $Alignment{$FieldPos};
                }
                if($TPadding+$MSize{$FieldPos}<=$Padding)
                { # enough padding to place target field
                    return 1;
                }
                else {
                    return 0;
                }
            }
            else
            { # changed fields
                my $Delta = $Size-$MSize{$FieldPos};
                if($Delta>=0)
                { # increased
                    if($Size-$MSize{$FieldPos}<=$Padding)
                    { # enough padding to change target field
                        return 1;
                    }
                    else {
                        return 0;
                    }
                }
                else
                { # decreased
                    $Delta = abs($Delta);
                    if($Delta+$Padding>=$MSize{$Pos})
                    { # try to place the next field
                        if(($Offset-$Delta) % $Alignment{$Pos} != 0)
                        { # padding of the next field in new place
                            my $NPadding = $Alignment{$Pos} - ($Offset-$Delta) % $Alignment{$Pos};
                            if($NPadding+$MSize{$Pos}<=$Delta+$Padding)
                            { # enough delta+padding to store next field
                                return 0;
                            }
                        }
                        else
                        {
                            return 0;
                        }
                    }
                    return 1;
                }
            }
        }
        elsif($Pos==$End)
        { # target field is the last field
            if($Size==-1)
            { # added/removed fields
                if($Offset % $MaxAlgn!=0)
                { # tail padding
                    my $TailPadding = $MaxAlgn - $Offset % $MaxAlgn;
                    if($Padding+$MSize{$Pos}<=$TailPadding)
                    { # enough tail padding to place the last field
                        return 1;
                    }
                }
                return 0;
            }
            else
            { # changed fields
                # scenario #1
                my $Offset1 = $Offset+$Padding+$MSize{$Pos};
                if($Offset1 % $MaxAlgn != 0)
                { # tail padding
                    $Offset1 += $MaxAlgn - $Offset1 % $MaxAlgn;
                }
                # scenario #2
                my $Offset2 = $Offset+$Padding+$Size;
                if($Offset2 % $MaxAlgn != 0)
                { # tail padding
                    $Offset2 += $MaxAlgn - $Offset2 % $MaxAlgn;
                }
                if($Offset1!=$Offset2)
                { # different sizes of structure
                    return 0;
                }
                return 1;
            }
        }
        $Offset += $Padding+$MSize{$Pos};
    }
    return 0;
}

sub isReserved($)
{ # reserved fields == private
    my $MName = $_[0];
    if($MName=~/reserved|padding|f_spare/i) {
        return 1;
    }
    if($MName=~/\A[_]*(spare|pad|unused)[_]*\Z/i) {
        return 1;
    }
    if($MName=~/(pad\d+)/i) {
        return 1;
    }
    return 0;
}

sub isPublic($$)
{
    my ($TypePtr, $FieldPos) = @_;
    return 0 if(not $TypePtr);
    return 0 if(not defined $TypePtr->{"Memb"}{$FieldPos});
    return 0 if(not defined $TypePtr->{"Memb"}{$FieldPos}{"name"});
    if(not $TypePtr->{"Memb"}{$FieldPos}{"access"})
    { # by name in C language
      # FIXME: add other methods to detect private members
        my $MName = $TypePtr->{"Memb"}{$FieldPos}{"name"};
        if($MName=~/priv|abidata|parent_object/i)
        { # C-styled private data
            return 0;
        }
        if(lc($MName) eq "abi")
        { # ABI information/reserved field
            return 0;
        }
        if(isReserved($MName))
        { # reserved fields
            return 0;
        }
        return 1;
    }
    elsif($TypePtr->{"Memb"}{$FieldPos}{"access"} ne "private")
    { # by access in C++ language
        return 1;
    }
    return 0;
}

sub mergeBases()
{
    foreach my $ClassName (keys(%{$ClassToId{1}}))
    {# detect added and removed virtual functions
        my $ClassId = $ClassToId{1}{$ClassName};
        next if(not $ClassId);
        foreach my $VirtFunc (keys(%{$VirtualTable{2}{$ClassName}}))
        {
            if($ClassToId{1}{$ClassName} and not defined $VirtualTable{1}{$ClassName}{$VirtFunc})
            {# added to virtual table
                if($AddedInt{$VirtFunc}
                # added pure virtual method (without a symbol in shared library)
                or (not $CompleteSignature{1}{$VirtFunc}{"Header"} and $CompleteSignature{2}{$VirtFunc}{"PureVirt"})
                # presented as non-virtual in old-version headers
                or ($CompleteSignature{1}{$VirtFunc}{"Header"} and not $CompleteSignature{1}{$VirtFunc}{"Virt"}))
                {# reasons: added, became virtual, added pure virtual, added to header
                    if(not $CompleteSignature{2}{$VirtFunc}{"Override"}) {
                        $AddedInt_Virt{$ClassName}{$VirtFunc} = 1;
                    }
                }
            }
        }
        foreach my $VirtFunc (keys(%{$VirtualTable{1}{$ClassName}}))
        {
            if($ClassToId{2}{$ClassName} and not defined $VirtualTable{2}{$ClassName}{$VirtFunc})
            {# removed from virtual table
                if($RemovedInt{$VirtFunc}
                # removed pure virtual method (without a symbol in shared library)
                or ($CompleteSignature{1}{$VirtFunc}{"PureVirt"} and not $CompleteSignature{2}{$VirtFunc}{"Header"})
                # presented as non-virtual in new-version headers
                or ($CompleteSignature{2}{$VirtFunc}{"Header"} and not $CompleteSignature{2}{$VirtFunc}{"Virt"}))
                {# reasons: removed, became non-virtual, removed pure virtual, removed from header
                    if(not $CompleteSignature{1}{$VirtFunc}{"Override"}) {
                        $RemovedInt_Virt{$ClassName}{$VirtFunc} = 1;
                    }
                }
            }
        }
        my %Class_Type = get_Type($Tid_TDid{1}{$ClassId}, $ClassId, 1);
        foreach my $AddedVFunc (keys(%{$AddedInt_Virt{$ClassName}}))
        {# check replacements, including pure virtual methods
            my $AddedPos = $VirtualTable{2}{$ClassName}{$AddedVFunc};
            foreach my $RemovedVFunc (keys(%{$RemovedInt_Virt{$ClassName}}))
            {
                my $RemovedPos = $VirtualTable{1}{$ClassName}{$RemovedVFunc};
                if($AddedPos==$RemovedPos)
                {
                    $VirtualReplacement{$AddedVFunc} = $RemovedVFunc;
                    $VirtualReplacement{$RemovedVFunc} = $AddedVFunc;
                    last;# other methods will be reported as "added" or "removed"
                }
            }
            if(my $RemovedVFunc = $VirtualReplacement{$AddedVFunc})
            {
                if(lc($AddedVFunc) eq lc($RemovedVFunc))
                { # skip: DomUi => DomUI parameter (qt 4.2.3 to 4.3.0)
                    next;
                }
                my $ProblemType = "Virtual_Replacement";
                my @Affected = ($RemovedVFunc);
                if($CompleteSignature{1}{$RemovedVFunc}{"PureVirt"})
                { # pure methods
                    if(not isUsedClass($ClassName, 1))
                    { # not a parameter of some exported method
                        next;
                    }
                    $ProblemType = "Pure_Virtual_Replacement";
                    @Affected = (keys(%{$ClassMethods{1}{$ClassName}}))
                }
                foreach my $AffectedInt (@Affected)
                {
                    if($CompleteSignature{1}{$AffectedInt}{"PureVirt"})
                    { # affected exported methods only
                        next;
                    }
                    %{$CompatProblems{$AffectedInt}{$ProblemType}{$tr_name{$AddedVFunc}}}=(
                        "Type_Name"=>$Class_Type{"Name"},
                        "Type_Type"=>"Class",
                        "Target"=>get_Signature($AddedVFunc, 2),
                        "Old_Value"=>get_Signature($RemovedVFunc, 1));
                }
            }
        }
    }
    if($UsedDump{1}
    and cmpVersions($UsedDump{1}, "2.0")<0)
    {# support for old dumps
        # "Base" attribute introduced in ACC 1.22 (dump 2.0 format)
        return;
    }
    if($UsedDump{2}
    and cmpVersions($UsedDump{2}, "2.0")<0)
    {# support for old dumps
        # "Base" attribute introduced in ACC 1.22 (dump 2.0 format)
        return;
    }
    foreach my $ClassName (keys(%{$ClassToId{1}}))
    {
        my $ClassId_Old = $ClassToId{1}{$ClassName};
        next if(not $ClassId_Old);
        if(not $AllocableClass{1}{$ClassName} and not isCopyingClass($ClassId_Old, 1)) {
            # skip classes without public constructors (including auto-generated)
            # example: class has only a private exported or private inline constructor
            next;
        }
        if($ClassName=~/>/) {
            # skip affected template instances
            next;
        }
        my %Class_Old = get_Type($Tid_TDid{1}{$ClassId_Old}, $ClassId_Old, 1);
        my $ClassId_New = $ClassToId{2}{$ClassName};
        next if(not $ClassId_New);
        my %Class_New = get_Type($Tid_TDid{2}{$ClassId_New}, $ClassId_New, 2);
        my @Bases_Old = sort {$Class_Old{"Base"}{$a}{"pos"}<=>$Class_Old{"Base"}{$b}{"pos"}} keys(%{$Class_Old{"Base"}});
        my @Bases_New = sort {$Class_New{"Base"}{$a}{"pos"}<=>$Class_New{"Base"}{$b}{"pos"}} keys(%{$Class_New{"Base"}});
        my ($BNum1, $BNum2) = (1, 1);
        my %BasePos_Old = map {get_TypeName($_, 1) => $BNum1++} @Bases_Old;
        my %BasePos_New = map {get_TypeName($_, 2) => $BNum2++} @Bases_New;
        my %ShortBase_Old = map {get_ShortName($_, 1) => 1} @Bases_Old;
        my %ShortBase_New = map {get_ShortName($_, 2) => 1} @Bases_New;
        my $Shift_Old = getShift($ClassId_Old, 1);
        my $Shift_New = getShift($ClassId_New, 2);
        my %BaseId_New = map {get_TypeName($_, 2) => $_} @Bases_New;
        my ($Added, $Removed) = (0, 0);
        my @StableBases_Old = ();
        foreach my $BaseId (@Bases_Old)
        {
            my $BaseName = get_TypeName($BaseId, 1);
            if($BasePos_New{$BaseName}) {
                push(@StableBases_Old, $BaseId);
            }
            elsif(not $ShortBase_New{$BaseName}
            and not $ShortBase_New{get_ShortName($BaseId, 1)})
            {# removed base
             # excluding namespace::SomeClass to SomeClass renaming
                my $ProblemKind = "Removed_Base_Class";
                if($Shift_Old ne $Shift_New
                and (havePubFields(\%Class_Old) or $Class_Old{"Size"} ne $Class_New{"Size"})) {
                    $ProblemKind = "Removed_Base_Class_And_Shift";
                }
                foreach my $Interface (keys(%{$ClassMethods{1}{$ClassName}}))
                {
                    %{$CompatProblems{$Interface}{$ProblemKind}{"this"}}=(
                        "Type_Name"=>$ClassName,
                        "Type_Type"=>"Class",
                        "Target"=>$BaseName,
                        "Old_Size"=>$Class_Old{"Size"}*$BYTE_SIZE,
                        "New_Size"=>$Class_New{"Size"}*$BYTE_SIZE,
                        "Shift"=>abs($Shift_New-$Shift_Old)  );
                }
                $Removed+=1;
            }
        }
        my @StableBases_New = ();
        foreach my $BaseId (@Bases_New)
        {
            my $BaseName = get_TypeName($BaseId, 2);
            if($BasePos_Old{$BaseName}) {
                push(@StableBases_New, $BaseId);
            }
            elsif(not $ShortBase_Old{$BaseName}
            and not $ShortBase_Old{get_ShortName($BaseId, 2)})
            {# added base
             # excluding namespace::SomeClass to SomeClass renaming
                my $ProblemKind = "Added_Base_Class";
                if($Shift_Old ne $Shift_New
                and (havePubFields(\%Class_Old) or $Class_Old{"Size"} ne $Class_New{"Size"})) {
                    $ProblemKind = "Added_Base_Class_And_Shift";
                }
                foreach my $Interface (keys(%{$ClassMethods{1}{$ClassName}}))
                {
                    %{$CompatProblems{$Interface}{$ProblemKind}{"this"}}=(
                        "Type_Name"=>$ClassName,
                        "Type_Type"=>"Class",
                        "Target"=>$BaseName,
                        "Old_Size"=>$Class_Old{"Size"}*$BYTE_SIZE,
                        "New_Size"=>$Class_New{"Size"}*$BYTE_SIZE,
                        "Shift"=>abs($Shift_New-$Shift_Old)  );
                }
                $Added+=1;
            }
        }
        ($BNum1, $BNum2) = (1, 1);
        my %BaseRelPos_Old = map {get_TypeName($_, 1) => $BNum1++} @StableBases_Old;
        my %BaseRelPos_New = map {get_TypeName($_, 2) => $BNum2++} @StableBases_New;
        foreach my $BaseId (@Bases_Old)
        {
            my $BaseName = get_TypeName($BaseId, 1);
            if(my $NewPos = $BaseRelPos_New{$BaseName})
            {
                my $BaseNewId = $BaseId_New{$BaseName};
                my $OldPos = $BaseRelPos_Old{$BaseName};
                if($NewPos!=$OldPos)
                {# changed position of the base class
                    foreach my $Interface (keys(%{$ClassMethods{1}{$ClassName}}))
                    {
                        %{$CompatProblems{$Interface}{"Base_Class_Position"}{"this"}}=(
                            "Type_Name"=>$ClassName,
                            "Type_Type"=>"Class",
                            "Target"=>$BaseName,
                            "Old_Value"=>$OldPos-1,
                            "New_Value"=>$NewPos-1  );
                    }
                }
                if($Class_Old{"Base"}{$BaseId}{"virtual"}
                and not $Class_New{"Base"}{$BaseNewId}{"virtual"})
                {# became non-virtual base
                    foreach my $Interface (keys(%{$ClassMethods{1}{$ClassName}}))
                    {
                        %{$CompatProblems{$Interface}{"Base_Class_Became_Non_Virtually_Inherited"}{"this->".$BaseName}}=(
                            "Type_Name"=>$ClassName,
                            "Type_Type"=>"Class",
                            "Target"=>$BaseName  );
                    }
                }
                elsif(not $Class_Old{"Base"}{$BaseId}{"virtual"}
                and $Class_New{"Base"}{$BaseNewId}{"virtual"})
                {# became virtual base
                    foreach my $Interface (keys(%{$ClassMethods{1}{$ClassName}}))
                    {
                        %{$CompatProblems{$Interface}{"Base_Class_Became_Virtually_Inherited"}{"this->".$BaseName}}=(
                            "Type_Name"=>$ClassName,
                            "Type_Type"=>"Class",
                            "Target"=>$BaseName  );
                    }
                }
            }
        }
        # detect size changes in base classes
        if($Shift_Old!=$Shift_New)
        {# size of allocable class
            foreach my $BaseId (@StableBases_Old)
            {# search for changed base
                my %BaseType = get_Type($Tid_TDid{1}{$BaseId}, $BaseId, 1);
                my $Size_Old = get_TypeSize($BaseId, 1);
                my $Size_New = get_TypeSize($BaseId_New{$BaseType{"Name"}}, 2);
                if($Size_Old ne $Size_New
                and $Size_Old and $Size_New)
                {
                    my $ProblemType = "";
                    if(isCopyingClass($BaseId, 1)) {
                        $ProblemType = "Size_Of_Copying_Class";
                    }
                    elsif($AllocableClass{1}{$BaseType{"Name"}})
                    {
                        if($Size_New>$Size_Old) {
                            # increased size
                            $ProblemType = "Size_Of_Allocable_Class_Increased";
                        }
                        else {
                            # decreased size
                            $ProblemType = "Size_Of_Allocable_Class_Decreased";
                            if(not havePubFields(\%Class_Old)) {
                                # affected class has no public members
                                next;
                            }
                        }
                    }
                    next if(not $ProblemType);
                    foreach my $Interface (keys(%{$ClassMethods{1}{$ClassName}}))
                    {# base class size changes affecting current class
                        %{$CompatProblems{$Interface}{$ProblemType}{"this->".$BaseType{"Name"}}}=(
                            "Type_Name"=>$BaseType{"Name"},
                            "Type_Type"=>"Class",
                            "Target"=>$BaseType{"Name"},
                            "Old_Size"=>$Size_Old*$BYTE_SIZE,
                            "New_Size"=>$Size_New*$BYTE_SIZE  );
                    }
                }
            }
        }
        if(my @VFunctions = keys(%{$VirtualTable{1}{$ClassName}}))
        {# compare virtual tables size in base classes
            my $VShift_Old = getVShift($ClassId_Old, 1);
            my $VShift_New = getVShift($ClassId_New, 2);
            if($VShift_Old ne $VShift_New)
            {# changes in the base class or changes in the list of base classes
                my @AllBases_Old = get_class_bases($ClassId_Old, 1, 1);
                my @AllBases_New = get_class_bases($ClassId_New, 2, 1);
                ($BNum1, $BNum2) = (1, 1);
                my %StableBase = map {get_TypeName($_, 2) => $_} @AllBases_New;
                foreach my $BaseId (@AllBases_Old)
                {
                    my %BaseType = get_Type($Tid_TDid{1}{$BaseId}, $BaseId, 1);
                    if(not $StableBase{$BaseType{"Name"}}) {
                        # lost base
                        next;
                    }
                    my $VSize_Old = getVSize($BaseType{"Name"}, 1);
                    my $VSize_New = getVSize($BaseType{"Name"}, 2);
                    if($VSize_Old!=$VSize_New)
                    {
                        my $VRealSize_Old = get_VTableSymbolSize($BaseType{"Name"}, 1);
                        my $VRealSize_New = get_VTableSymbolSize($BaseType{"Name"}, 2);
                        if(not $VRealSize_Old or not $VRealSize_New) {
                            # try to compute a model v-table size
                            $VRealSize_Old = ($VSize_Old+2+getVShift($BaseId, 1))*$WORD_SIZE{1};
                            $VRealSize_New = ($VSize_New+2+getVShift($StableBase{$BaseType{"Name"}}, 2))*$WORD_SIZE{2};
                        }
                        foreach my $Interface (@VFunctions)
                        {
                            if(not defined $VirtualTable{2}{$ClassName}{$Interface})
                            {# Removed_Virtual_Method, will be registered in mergeVirtualTables()
                                next;
                            }
                            if($VirtualTable{2}{$ClassName}{$Interface}-$VirtualTable{1}{$ClassName}{$Interface}+$VSize_New-$VSize_Old==0)
                            {# skip interfaces that have not changed the absolute virtual position
                                next;
                            }
                            if(not link_symbol($Interface, 1, "-Deps")
                            and not $CheckHeadersOnly)
                            {# affected symbols in shared library
                                next;
                            }
                            %{$CompatProblems{$Interface}{"Virtual_Table_Size"}{$BaseType{"Name"}}}=(
                                "Type_Name"=>$BaseType{"Name"},
                                "Type_Type"=>"Class",
                                "Target"=>get_Signature($Interface, 1),
                                "Old_Size"=>$VRealSize_Old*$BYTE_SIZE,
                                "New_Size"=>$VRealSize_New*$BYTE_SIZE  );
                            foreach my $VirtFunc (keys(%{$AddedInt_Virt{$BaseType{"Name"}}}))
                            {# the reason of the layout change: added virtual functions
                                next if($VirtualReplacement{$VirtFunc});
                                my $ProblemType = "Added_Virtual_Method";
                                if($CompleteSignature{2}{$VirtFunc}{"PureVirt"}) {
                                    $ProblemType = "Added_Pure_Virtual_Method";
                                }
                                %{$CompatProblems{$Interface}{$ProblemType}{get_Signature($VirtFunc, 2)}}=(
                                    "Type_Name"=>$BaseType{"Name"},
                                    "Type_Type"=>"Class",
                                    "Target"=>get_Signature($VirtFunc, 2)  );
                            }
                            foreach my $VirtFunc (keys(%{$RemovedInt_Virt{$BaseType{"Name"}}}))
                            {# the reason of the layout change: removed virtual functions
                                next if($VirtualReplacement{$VirtFunc});
                                %{$CompatProblems{$Interface}{"Removed_Virtual_Method"}{get_Signature($VirtFunc, 1)}}=(
                                    "Type_Name"=>$BaseType{"Name"},
                                    "Type_Type"=>"Class",
                                    "Target"=>get_Signature($VirtFunc, 1)  );
                            }
                        }
                    }
                }
            }
        }
    }
}

sub isUsedClass($$)
{# FIXME: check usage of the abstract class as the parameter of some exported method
    my ($CName, $LibVersion) = @_;
    return (keys(%{$ClassMethods{1}{$CName}})!=0);
}

sub mergeVirtualTables($)
{# check for changes in the virtual table
    my $Interface = $_[0];
    # affected method:
    #  - virtual
    #  - pure-virtual
    #  - non-virtual
    my $Class_Id = $CompleteSignature{1}{$Interface}{"Class"};
    my $CName = get_TypeName($Class_Id, 1);
    if($CompleteSignature{1}{$Interface}{"PureVirt"}
    and not isUsedClass($CName, 1))
    {# pure virtuals should not be affected
     # if there are no exported methods using this class
        return;
    }
    $CheckedTypes{$CName} = 1;
    # check virtual table structure
    foreach my $AddedVFunc (keys(%{$AddedInt_Virt{$CName}}))
    {
        next if($Interface eq $AddedVFunc);
        next if($VirtualReplacement{$AddedVFunc});
        my $VPos_Added = $VirtualTable{2}{$CName}{$AddedVFunc};
        if($CompleteSignature{2}{$AddedVFunc}{"PureVirt"})
        { # pure virtual methods affect all others (virtual and non-virtual)
            %{$CompatProblems{$Interface}{"Added_Pure_Virtual_Method"}{$tr_name{$AddedVFunc}}}=(
                "Type_Name"=>$CName,
                "Type_Type"=>"Class",
                "Target"=>get_Signature($AddedVFunc, 2)  );
        }
        elsif($VPos_Added>keys(%{$VirtualTable{1}{$CName}}))
        {# added virtual function at the end of v-table
            if(not keys(%{$VirtualTable_Full{1}{$CName}}))
            {# became polymorphous class, added v-table pointer
                %{$CompatProblems{$Interface}{"Added_First_Virtual_Method"}{$tr_name{$AddedVFunc}}}=(
                    "Type_Name"=>$CName,
                    "Type_Type"=>"Class",
                    "Target"=>get_Signature($AddedVFunc, 2)  );
            }
            elsif(isCopyingClass($Class_Id, 1))
            {# class has no constructors and v-table will be copied by applications, it may affect all methods
                my $VSize_Old = getVSize($CName, 1);
                my $VSize_New = getVSize($CName, 2);
                next if($VSize_Old==$VSize_New);# exception: register as removed and added virtual method
                my $ProblemType = "Added_Virtual_Method";
                if(isLeafClass($Class_Id, 1)) {
                    $ProblemType = "Added_Virtual_Method_At_End";
                }
                %{$CompatProblems{$Interface}{$ProblemType}{$tr_name{$AddedVFunc}}}=(
                    "Type_Name"=>$CName,
                    "Type_Type"=>"Class",
                    "Target"=>get_Signature($AddedVFunc, 2)  );
            }
            else {
                # safe
            }
        }
        elsif($CompleteSignature{1}{$Interface}{"Virt"}
        or $CompleteSignature{1}{$Interface}{"PureVirt"})
        {
            my $VPos_Old = $VirtualTable{1}{$CName}{$Interface};
            my $VPos_New = $VirtualTable{2}{$CName}{$Interface};
            if($VPos_Added<=$VPos_Old and $VPos_Old!=$VPos_New) {
                my @Affected = ($Interface, keys(%{$OverriddenMethods{1}{$Interface}}));
                foreach my $AffectedInterface (@Affected)
                {
                    if(not $CompleteSignature{1}{$AffectedInterface}{"PureVirt"}
                    and not link_symbol($AffectedInterface, 1, "-Deps")) {
                        next;
                    }
                    %{$CompatProblems{$AffectedInterface}{"Added_Virtual_Method"}{$tr_name{$AddedVFunc}}}=(
                        "Type_Name"=>$CName,
                        "Type_Type"=>"Class",
                        "Target"=>get_Signature($AddedVFunc, 2)  );
                }
            }
        }
        else {
            # safe
        }
    }
    foreach my $RemovedVFunc (keys(%{$RemovedInt_Virt{$CName}}))
    {
        next if($VirtualReplacement{$RemovedVFunc});
        if($RemovedVFunc eq $Interface
        and $CompleteSignature{1}{$RemovedVFunc}{"PureVirt"})
        { # This case is for removed virtual methods
          # implemented in both versions of a library
            next;
        }
        if(not keys(%{$VirtualTable_Full{2}{$CName}}))
        {# became non-polymorphous class, removed v-table pointer
            %{$CompatProblems{$Interface}{"Removed_Last_Virtual_Method"}{$tr_name{$RemovedVFunc}}}=(
                "Type_Name"=>$CName,
                "Type_Type"=>"Class",
                "Target"=>get_Signature($RemovedVFunc, 1)  );
        }
        elsif($CompleteSignature{1}{$Interface}{"Virt"}
        or $CompleteSignature{1}{$Interface}{"PureVirt"})
        {
            my $VPos_Removed = $VirtualTable{1}{$CName}{$RemovedVFunc};
            my $VPos_Old = $VirtualTable{1}{$CName}{$Interface};
            my $VPos_New = $VirtualTable{2}{$CName}{$Interface};
            if($VPos_Removed<=$VPos_Old and $VPos_Old!=$VPos_New) {
                my @Affected = ($Interface, keys(%{$OverriddenMethods{1}{$Interface}}));
                foreach my $AffectedInterface (@Affected)
                {
                    if(not $CompleteSignature{1}{$AffectedInterface}{"PureVirt"}
                    and not link_symbol($AffectedInterface, 1, "-Deps")) {
                        next;
                    }
                    %{$CompatProblems{$AffectedInterface}{"Removed_Virtual_Method"}{$tr_name{$RemovedVFunc}}}=(
                        "Type_Name"=>$CName,
                        "Type_Type"=>"Class",
                        "Target"=>get_Signature($RemovedVFunc, 1)  );
                }
            }
        }
    }
}

sub find_MemberPair_Pos_byName($$)
{
    my ($Member_Name, $Pair_Type) = @_;
    $Member_Name=~s/\A[_]+|[_]+\Z//g;
    foreach my $MemberPair_Pos (sort {int($a)<=>int($b)} keys(%{$Pair_Type->{"Memb"}}))
    {
        if(defined $Pair_Type->{"Memb"}{$MemberPair_Pos})
        {
            my $Name = $Pair_Type->{"Memb"}{$MemberPair_Pos}{"name"};
            $Name=~s/\A[_]+|[_]+\Z//g;
            if($Name eq $Member_Name) {
                return $MemberPair_Pos;
            }
        }
    }
    return "lost";
}

sub find_MemberPair_Pos_byVal($$)
{
    my ($Member_Value, $Pair_Type) = @_;
    foreach my $MemberPair_Pos (sort {int($a)<=>int($b)} keys(%{$Pair_Type->{"Memb"}}))
    {
        if(defined $Pair_Type->{"Memb"}{$MemberPair_Pos}
        and $Pair_Type->{"Memb"}{$MemberPair_Pos}{"value"} eq $Member_Value) {
            return $MemberPair_Pos;
        }
    }
    return "lost";
}

my %Priority_Value=(
    "High"=>3,
    "Medium"=>2,
    "Low"=>1);

sub max_priority($$)
{
    my ($Priority1, $Priority2) = @_;
    if(cmp_priority($Priority1, $Priority2)) {
        return $Priority1;
    }
    else {
        return $Priority2;
    }
}

sub cmp_priority($$)
{
    my ($Priority1, $Priority2) = @_;
    return ($Priority_Value{$Priority1}>$Priority_Value{$Priority2});
}

sub getProblemSeverity($$)
{
    my ($Level, $Kind) = @_;
    return $CompatRules{$Level}{$Kind}{"Severity"};
}

sub isRecurType($$$$)
{
    foreach (@RecurTypes)
    {
        if($_->{"Tid1"} eq $_[0]
        and $_->{"TDid1"} eq $_[1]
        and $_->{"Tid2"} eq $_[2]
        and $_->{"TDid2"} eq $_[3])
        {
            return 1;
        }
    }
    return 0;
}

sub pushType($$$$)
{
    my %TypeIDs=(
        "Tid1"  => $_[0],
        "TDid1" => $_[1],
        "Tid2"  => $_[2],
        "TDid2" => $_[3]  );
    push(@RecurTypes, \%TypeIDs);
}

sub isRenamed($$$$$)
{
    my ($MemPos, $Type1, $LVersion1, $Type2, $LVersion2) = @_;
    my $Member_Name = $Type1->{"Memb"}{$MemPos}{"name"};
    my $MemberType_Id = $Type1->{"Memb"}{$MemPos}{"type"};
    my %MemberType_Pure = get_PureType($Tid_TDid{$LVersion1}{$MemberType_Id}, $MemberType_Id, $LVersion1);
    if(not defined $Type2->{"Memb"}{$MemPos}) {
        return "";
    }
    my $StraightPairType_Id = $Type2->{"Memb"}{$MemPos}{"type"};
    my %StraightPairType_Pure = get_PureType($Tid_TDid{$LVersion2}{$StraightPairType_Id}, $StraightPairType_Id, $LVersion2);
    
    my $StraightPair_Name = $Type2->{"Memb"}{$MemPos}{"name"};
    my $MemberPair_Pos_Rev = ($Member_Name eq $StraightPair_Name)?$MemPos:find_MemberPair_Pos_byName($StraightPair_Name, $Type1);
    if($MemberPair_Pos_Rev eq "lost")
    {
        if($MemberType_Pure{"Name"} eq $StraightPairType_Pure{"Name"})
        {# base type match
            return $StraightPair_Name;
        }
        if(get_TypeName($MemberType_Id, $LVersion1) eq get_TypeName($StraightPairType_Id, $LVersion2))
        {# exact type match
            return $StraightPair_Name;
        }
        if($MemberType_Pure{"Size"} eq $StraightPairType_Pure{"Size"})
        {# size match
            return $StraightPair_Name;
        }
        if(isReserved($StraightPair_Name))
        {# reserved fields
            return $StraightPair_Name;
        }
    }
    return "";
}

sub nonComparable($$)
{
    my ($T1, $T2) = @_;
    if($T1->{"Name"} ne $T2->{"Name"}
    and not isAnon($T1->{"Name"})
    and not isAnon($T2->{"Name"}))
    { # different names
        if($T1->{"Type"} ne "Pointer"
        or $T2->{"Type"} ne "Pointer")
        {
            return 1;
        }
        if($T1->{"Name"}!~/\Avoid\s*\*/
        and $T2->{"Name"}=~/\Avoid\s*\*/)
        {
            return 1;
        }
    }
    elsif($T1->{"Type"} ne $T2->{"Type"})
    { # different types
        if($T1->{"Type"} eq "Class"
        and $T2->{"Type"} eq "Struct")
        { # allow class=>struct
            return 0;
        }
        elsif($T2->{"Type"} eq "Class"
        and $T1->{"Type"} eq "Struct")
        { # allow struct=>class
            return 0;
        }
        else
        { # class<=>enum
          # union<=>class
          #  ...
            return 1;
        }
    }
    return 0;
}

sub mergeTypes($$$$)
{
    my ($Type1_Id, $Type1_DId, $Type2_Id, $Type2_DId) = @_;
    return () if((not $Type1_Id and not $Type1_DId) or (not $Type2_Id and not $Type2_DId));
    my (%Sub_SubProblems, %SubProblems) = ();
    if($Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId})
    {# already merged
        return %{$Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId}};
    }
    my %Type1 = get_Type($Type1_DId, $Type1_Id, 1);
    my %Type2 = get_Type($Type2_DId, $Type2_Id, 2);
    my %Type1_Pure = get_PureType($Type1_DId, $Type1_Id, 1);
    my %Type2_Pure = get_PureType($Type2_DId, $Type2_Id, 2);
    $CheckedTypes{$Type1{"Name"}}=1;
    $CheckedTypes{$Type1_Pure{"Name"}}=1;
    return () if(not $Type1_Pure{"Size"} or not $Type2_Pure{"Size"});
    if(isRecurType($Type1_Pure{"Tid"}, $Type1_Pure{"TDid"}, $Type2_Pure{"Tid"}, $Type2_Pure{"TDid"}))
    {# skip recursive declarations
        return ();
    }
    return () if(not $Type1_Pure{"Name"} or not $Type2_Pure{"Name"});
    return () if($SkipTypes{1}{$Type1_Pure{"Name"}});
    return () if($SkipTypes{1}{$Type1{"Name"}});
    
    my %Typedef_1 = goToFirst($Type1{"TDid"}, $Type1{"Tid"}, 1, "Typedef");
    my %Typedef_2 = goToFirst($Type2{"TDid"}, $Type2{"Tid"}, 2, "Typedef");
    if($Typedef_1{"Type"} eq "Typedef" and $Typedef_2{"Type"} eq "Typedef"
    and $Typedef_1{"Name"} eq $Typedef_2{"Name"} and not $UseOldDumps)
    {
        my %Base_1 = get_OneStep_BaseType($Typedef_1{"TDid"}, $Typedef_1{"Tid"}, 1);
        my %Base_2 = get_OneStep_BaseType($Typedef_2{"TDid"}, $Typedef_2{"Tid"}, 2);
        if(differentFmts())
        {# different GCC versions or different dumps
            $Base_1{"Name"} = uncover_typedefs($Base_1{"Name"}, 1);
            $Base_2{"Name"} = uncover_typedefs($Base_2{"Name"}, 2);
            # std::__va_list and __va_list
            $Base_1{"Name"}=~s/\A(\w+::)+//;
            $Base_2{"Name"}=~s/\A(\w+::)+//;
            $Base_1{"Name"} = correctName($Base_1{"Name"});
            $Base_2{"Name"} = correctName($Base_2{"Name"});
        }
        if($Base_1{"Name"}!~/anon\-/ and $Base_2{"Name"}!~/anon\-/
        and $Base_1{"Name"} ne $Base_2{"Name"})
        {
            %{$SubProblems{"Typedef_BaseType"}{$Typedef_1{"Name"}}}=(
                "Target"=>$Typedef_1{"Name"},
                "Type_Name"=>$Typedef_1{"Name"},
                "Type_Type"=>"Typedef",
                "Old_Value"=>$Base_1{"Name"},
                "New_Value"=>$Base_2{"Name"}  );
        }
    }
    if(nonComparable(\%Type1_Pure, \%Type2_Pure))
    { # different types (reported in detectTypeChange(...))
        %{$Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId}} = %SubProblems;
        return %SubProblems;
    }
    pushType($Type1_Pure{"Tid"}, $Type1_Pure{"TDid"},
             $Type2_Pure{"Tid"}, $Type2_Pure{"TDid"});
    if(($Type1_Pure{"Name"} eq $Type2_Pure{"Name"}
    or (isAnon($Type1_Pure{"Name"}) and isAnon($Type2_Pure{"Name"})))
    and $Type1_Pure{"Type"}=~/\A(Struct|Class|Union)\Z/)
    {# checking size
        if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
        {
            my $ProblemKind = "DataType_Size";
            if($Type1_Pure{"Type"} eq "Class"
            and keys(%{$ClassMethods{1}{$Type1_Pure{"Name"}}}))
            {
                if(isCopyingClass($Type1_Pure{"Tid"}, 1)) {
                    $ProblemKind = "Size_Of_Copying_Class";
                }
                elsif($AllocableClass{1}{$Type1_Pure{"Name"}})
                {
                    if(int($Type2_Pure{"Size"})>int($Type1_Pure{"Size"})) {
                        $ProblemKind = "Size_Of_Allocable_Class_Increased";
                    }
                    else {
                        # descreased size of allocable class
                        # it has no special effects
                    }
                }
            }
            %{$SubProblems{$ProblemKind}{$Type1_Pure{"Name"}}}=(
                "Target"=>$Type1_Pure{"Name"},
                "Type_Name"=>$Type1_Pure{"Name"},
                "Type_Type"=>$Type1_Pure{"Type"},
                "Old_Size"=>$Type1_Pure{"Size"}*$BYTE_SIZE,
                "New_Size"=>$Type2_Pure{"Size"}*$BYTE_SIZE,
                "InitialType_Type"=>$Type1_Pure{"Type"});
        }
    }
    if($Type1_Pure{"BaseType"}{"Tid"} and $Type2_Pure{"BaseType"}{"Tid"})
    {# checking base types
        %Sub_SubProblems = mergeTypes($Type1_Pure{"BaseType"}{"Tid"}, $Type1_Pure{"BaseType"}{"TDid"},
                                       $Type2_Pure{"BaseType"}{"Tid"}, $Type2_Pure{"BaseType"}{"TDid"});
        foreach my $Sub_SubProblemType (keys(%Sub_SubProblems))
        {
            foreach my $Sub_SubLocation (keys(%{$Sub_SubProblems{$Sub_SubProblemType}}))
            {
                foreach my $Attr (keys(%{$Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}})) {
                    $SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}{$Attr} = $Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}{$Attr};
                }
                $SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}{"InitialType_Type"} = $Type1_Pure{"Type"};
            }
        }
    }
    my (%AddedField, %RemovedField, %RenamedField, %RenamedField_Rev, %RelatedField, %RelatedField_Rev) = ();
    my %NameToPosA = map {$Type1_Pure{"Memb"}{$_}{"name"}=>$_} keys(%{$Type1_Pure{"Memb"}});
    my %NameToPosB = map {$Type2_Pure{"Memb"}{$_}{"name"}=>$_} keys(%{$Type2_Pure{"Memb"}});
    foreach my $Member_Pos (sort {int($a) <=> int($b)} keys(%{$Type1_Pure{"Memb"}}))
    { # detect removed and renamed fields
        my $Member_Name = $Type1_Pure{"Memb"}{$Member_Pos}{"name"};
        next if(not $Member_Name);
        my $MemberPair_Pos = (defined $Type2_Pure{"Memb"}{$Member_Pos} and $Type2_Pure{"Memb"}{$Member_Pos}{"name"} eq $Member_Name)?$Member_Pos:find_MemberPair_Pos_byName($Member_Name, \%Type2_Pure);
        if($MemberPair_Pos eq "lost")
        {
            if($Type2_Pure{"Type"}=~/\A(Struct|Class|Union)\Z/)
            {
                if(isUnnamed($Member_Name))
                { # support for old-version dumps
                  # unnamed fields have been introduced in the ACC 1.23 (dump 2.1 format)
                    if($UsedDump{2}
                    and cmpVersions($UsedDump{2}, "2.1")<0) {
                        next;
                    }
                }
                if(my $RenamedTo = isRenamed($Member_Pos, \%Type1_Pure, 1, \%Type2_Pure, 2))
                { # renamed
                    $RenamedField{$Member_Pos}=$RenamedTo;
                    $RenamedField_Rev{$NameToPosB{$RenamedTo}}=$Member_Name;
                }
                else
                { # removed
                    $RemovedField{$Member_Pos}=1;
                }
            }
            elsif($Type1_Pure{"Type"} eq "Enum")
            {
                my $Member_Value1 = $Type1_Pure{"Memb"}{$Member_Pos}{"value"};
                next if($Member_Value1 eq "");
                $MemberPair_Pos = find_MemberPair_Pos_byVal($Member_Value1, \%Type2_Pure);
                if($MemberPair_Pos ne "lost")
                { # renamed
                    $RenamedField{$Member_Pos}=$Type2_Pure{"Memb"}{$MemberPair_Pos}{"name"};
                }
                else
                { # removed
                    $RemovedField{$Member_Pos}=1;
                }
            }
        }
        else
        { # related
            $RelatedField{$Member_Pos} = $MemberPair_Pos;
            $RelatedField_Rev{$MemberPair_Pos} = $Member_Pos;
        }
    }
    foreach my $Member_Pos (sort {int($a) <=> int($b)} keys(%{$Type2_Pure{"Memb"}}))
    {# detect added fields
        my $Member_Name = $Type2_Pure{"Memb"}{$Member_Pos}{"name"};
        next if(not $Member_Name);
        my $MemberPair_Pos = (defined $Type1_Pure{"Memb"}{$Member_Pos} and $Type1_Pure{"Memb"}{$Member_Pos}{"name"} eq $Member_Name)?$Member_Pos:find_MemberPair_Pos_byName($Member_Name, \%Type1_Pure);
        if($MemberPair_Pos eq "lost")
        {
            if(isUnnamed($Member_Name))
            { # support for old-version dumps
              # unnamed fields have been introduced in the ACC 1.23 (dump 2.1 format)
                if($UsedDump{1}
                and cmpVersions($UsedDump{1}, "2.1")<0) {
                    next;
                }
            }
            if($Type2_Pure{"Type"}=~/\A(Struct|Class|Union)\Z/)
            {
                if(not $RenamedField_Rev{$Member_Pos})
                { # added
                    $AddedField{$Member_Pos}=1;
                }
            }
        }
    }
    if($Type2_Pure{"Type"}=~/\A(Struct|Class)\Z/)
    { # detect moved fields
        my (%RelPos, %RelPosName, %AbsPos) = ();
        my $Pos = 0;
        foreach my $Member_Pos (sort {int($a) <=> int($b)} keys(%{$Type1_Pure{"Memb"}}))
        { # relative positions in 1st version
            my $Member_Name = $Type1_Pure{"Memb"}{$Member_Pos}{"name"};
            next if(not $Member_Name);
            if(not $RemovedField{$Member_Pos})
            { # old type without removed fields
                $RelPos{1}{$Member_Name}=$Pos;
                $RelPosName{1}{$Pos} = $Member_Name;
                $AbsPos{1}{$Pos++} = $Member_Pos;
            }
        }
        $Pos = 0;
        foreach my $Member_Pos (sort {int($a) <=> int($b)} keys(%{$Type2_Pure{"Memb"}}))
        { # relative positions in 2nd version
            my $Member_Name = $Type2_Pure{"Memb"}{$Member_Pos}{"name"};
            next if(not $Member_Name);
            if(not $AddedField{$Member_Pos})
            { # new type without added fields
                $RelPos{2}{$Member_Name}=$Pos;
                $RelPosName{2}{$Pos} = $Member_Name;
                $AbsPos{2}{$Pos++} = $Member_Pos;
            }
        }
        foreach my $Member_Name (keys(%{$RelPos{1}}))
        {
            my $RPos1 = $RelPos{1}{$Member_Name};
            my $AbsPos1 = $NameToPosA{$Member_Name};
            my $Member_Name2 = $Member_Name;
            if(my $RenamedTo = $RenamedField{$AbsPos1})
            { # renamed
                $Member_Name2 = $RenamedTo;
            }
            my $RPos2 = $RelPos{2}{$Member_Name2};
            if($RPos2 ne "" and $RPos1 ne $RPos2)
            { # different relative positions
                my $AbsPos2 = $NameToPosB{$Member_Name2};
                if($AbsPos1 ne $AbsPos2)
                { # different absolute positions
                    my $ProblemType = "Moved_Field";
                    if(not isPublic(\%Type1_Pure, $AbsPos1))
                    { # may change layout and size of type
                        $ProblemType = "Moved_Private_Field";
                    }
                    if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                    { # affected size
                        my $MemSize1 = get_TypeSize($Type1_Pure{"Memb"}{$AbsPos1}{"type"}, 1);
                        my $MovedAbsPos = $AbsPos{1}{$RPos2};
                        my $MemSize2 = get_TypeSize($Type1_Pure{"Memb"}{$MovedAbsPos}{"type"}, 1);
                        if($MemSize1 ne $MemSize2) {
                            $ProblemType .= "_And_Size";
                        }
                    }
                    if($ProblemType eq "Moved_Private_Field") {
                        next;
                    }
                    %{$SubProblems{$ProblemType}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Old_Value"=>$RPos1,
                        "New_Value"=>$RPos2 );
                }
            }
        }
    }
    foreach my $Member_Pos (sort {int($a) <=> int($b)} keys(%{$Type1_Pure{"Memb"}}))
    {# check older fields, public and private
        my $Member_Name = $Type1_Pure{"Memb"}{$Member_Pos}{"name"};
        next if(not $Member_Name);
        if(my $RenamedTo = $RenamedField{$Member_Pos})
        { # renamed
            if($Type2_Pure{"Type"}=~/\A(Struct|Class|Union)\Z/)
            {
                if(isPublic(\%Type1_Pure, $Member_Pos))
                {
                    %{$SubProblems{"Renamed_Field"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Old_Value"=>$Member_Name,
                        "New_Value"=>$RenamedTo  );
                }
            }
            elsif($Type1_Pure{"Type"} eq "Enum")
            {
                %{$SubProblems{"Enum_Member_Name"}{$Type1_Pure{"Memb"}{$Member_Pos}{"value"}}}=(
                    "Target"=>$Type1_Pure{"Memb"}{$Member_Pos}{"value"},
                    "Type_Name"=>$Type1_Pure{"Name"},
                    "Type_Type"=>$Type1_Pure{"Type"},
                    "Old_Value"=>$Member_Name,
                    "New_Value"=>$RenamedTo  );
            }
        }
        elsif($RemovedField{$Member_Pos})
        { # removed
            if($Type2_Pure{"Type"}=~/\A(Struct|Class)\Z/)
            {
                my $ProblemType = "Removed_Field";
                if(not isPublic(\%Type1_Pure, $Member_Pos)
                or isUnnamed($Member_Name)) {
                    $ProblemType = "Removed_Private_Field";
                }
                if(not isMemPadded($Member_Pos, -1, \%Type1_Pure, \%RemovedField, 1))
                {
                    if(my $MNum = isAccessible(\%Type1_Pure, \%RemovedField, $Member_Pos+1, -1))
                    { # affected fields
                        if(getOffset($MNum-1, \%Type1_Pure, 1)!=getOffset($RelatedField{$MNum-1}, \%Type2_Pure, 2))
                        { # changed offset
                            $ProblemType .= "_And_Layout";
                        }
                    }
                    if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                    { # affected size
                        $ProblemType .= "_And_Size";
                    }
                }
                if($ProblemType eq "Removed_Private_Field") {
                    next;
                }
                %{$SubProblems{$ProblemType}{$Member_Name}}=(
                    "Target"=>$Member_Name,
                    "Type_Name"=>$Type1_Pure{"Name"},
                    "Type_Type"=>$Type1_Pure{"Type"}  );
            }
            elsif($Type2_Pure{"Type"} eq "Union")
            {
                if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                {
                    %{$SubProblems{"Removed_Union_Field_And_Size"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"}  );
                }
                else
                {
                    %{$SubProblems{"Removed_Union_Field"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"}  );
                }
            }
            elsif($Type1_Pure{"Type"} eq "Enum")
            {
                %{$SubProblems{"Enum_Member_Removed"}{$Member_Name}}=(
                    "Target"=>$Member_Name,
                    "Type_Name"=>$Type1_Pure{"Name"},
                    "Type_Type"=>$Type1_Pure{"Type"},
                    "Old_Value"=>$Member_Name  );
            }
        }
        else
        { # changed
            my $MemberPair_Pos = $RelatedField{$Member_Pos};
            if($Type1_Pure{"Type"} eq "Enum")
            {
                my $Member_Value1 = $Type1_Pure{"Memb"}{$Member_Pos}{"value"};
                next if($Member_Value1 eq "");
                my $Member_Value2 = $Type2_Pure{"Memb"}{$MemberPair_Pos}{"value"};
                next if($Member_Value2 eq "");
                if($Member_Value1 ne $Member_Value2)
                {
                    my $ProblemType = "Enum_Member_Value";
                    if($Member_Name=~/last|count|max/i
                    or $Member_Name=~/END|NLIMITS\Z/) {
                        # GST_LEVEL_COUNT, GST_RTSP_ELAST
                        # __RLIMIT_NLIMITS
                        $ProblemType = "Enum_Last_Member_Value";
                    }
                    %{$SubProblems{$ProblemType}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Old_Value"=>$Member_Value1,
                        "New_Value"=>$Member_Value2  );
                }
            }
            elsif($Type2_Pure{"Type"}=~/\A(Struct|Class|Union)\Z/)
            {
                my $MemberType1_Id = $Type1_Pure{"Memb"}{$Member_Pos}{"type"};
                my $MemberType2_Id = $Type2_Pure{"Memb"}{$MemberPair_Pos}{"type"};
                my $SizeV1 = get_TypeSize($MemberType1_Id, 1)*$BYTE_SIZE;
                if(my $BSize1 = $Type1_Pure{"Memb"}{$Member_Pos}{"bitfield"}) {
                    $SizeV1 = $BSize1;
                }
                my $SizeV2 = get_TypeSize($MemberType2_Id, 2)*$BYTE_SIZE;
                if(my $BSize2 = $Type2_Pure{"Memb"}{$MemberPair_Pos}{"bitfield"}) {
                    $SizeV2 = $BSize2;
                }
                my $MemberType1_Name = get_TypeName($MemberType1_Id, 1);
                my $MemberType2_Name = get_TypeName($MemberType2_Id, 2);
                if($SizeV1 ne $SizeV2)
                {
                    if($MemberType1_Name eq $MemberType2_Name or (isAnon($MemberType1_Name) and isAnon($MemberType2_Name))
                    or ($Type1_Pure{"Memb"}{$Member_Pos}{"bitfield"} and $Type2_Pure{"Memb"}{$MemberPair_Pos}{"bitfield"}))
                    { # field size change (including anon-structures and unions)
                      # - same types
                      # - unnamed types
                      # - bitfields
                        my $ProblemType = "Field_Size";
                        if(not isPublic(\%Type1_Pure, $Member_Pos)
                        or isUnnamed($Member_Name))
                        { # should not be accessed by applications, goes to "Low Severity"
                          # example: "abidata" members in GStreamer types
                            $ProblemType = "Private_".$ProblemType;
                        }
                        if(not isMemPadded($Member_Pos, get_TypeSize($MemberType2_Id, 2)*$BYTE_SIZE, \%Type1_Pure, \%RemovedField, 1))
                        { # check an effect
                            if(my $MNum = isAccessible(\%Type1_Pure, \%RemovedField, $Member_Pos+1, -1))
                            { # public fields after the current
                                if(getOffset($MNum-1, \%Type1_Pure, 1)!=getOffset($RelatedField{$MNum-1}, \%Type2_Pure, 2))
                                { # changed offset
                                    $ProblemType .= "_And_Layout";
                                }
                            }
                            if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"}) {
                                $ProblemType .= "_And_Type_Size";
                            }
                        }
                        if($ProblemType eq "Private_Field_Size")
                        { # private field size with no effect
                            $ProblemType = "";
                        }
                        if($ProblemType)
                        { # register a problem
                            %{$SubProblems{$ProblemType}{$Member_Name}}=(
                                "Target"=>$Member_Name,
                                "Type_Name"=>$Type1_Pure{"Name"},
                                "Type_Type"=>$Type1_Pure{"Type"},
                                "Old_Size"=>$SizeV1,
                                "New_Size"=>$SizeV2);
                        }
                    }
                }
                if($Type1_Pure{"Memb"}{$Member_Pos}{"bitfield"}
                or $Type2_Pure{"Memb"}{$MemberPair_Pos}{"bitfield"})
                { # do NOT check bitfield type changes
                    next;
                }
                %Sub_SubProblems = detectTypeChange($MemberType1_Id, $MemberType2_Id, "Field");
                foreach my $ProblemType (keys(%Sub_SubProblems))
                {
                    my $ProblemType_Init = $ProblemType;
                    if($ProblemType eq "Field_Type_And_Size")
                    {
                        if(not isPublic(\%Type1_Pure, $Member_Pos)
                        or isUnnamed($Member_Name)) {
                            $ProblemType = "Private_".$ProblemType;
                        }
                        if(not isMemPadded($Member_Pos, get_TypeSize($MemberType2_Id, 2)*$BYTE_SIZE, \%Type1_Pure, \%RemovedField, 1))
                        { # check an effect
                            if(my $MNum = isAccessible(\%Type1_Pure, \%RemovedField, $Member_Pos+1, -1))
                            { # public fields after the current
                                if(getOffset($MNum-1, \%Type1_Pure, 1)!=getOffset($RelatedField{$MNum-1}, \%Type2_Pure, 2))
                                { # changed offset
                                    $ProblemType .= "_And_Layout";
                                }
                            }
                            if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"}) {
                                $ProblemType .= "_And_Type_Size";
                            }
                        }
                    }
                    else
                    {
                        if(not isPublic(\%Type1_Pure, $Member_Pos)
                        or isUnnamed($Member_Name)) {
                            next;
                        }
                    }
                    if($ProblemType eq "Private_Field_Type_And_Size")
                    { # private field change with no effect
                        next;
                    }
                    %{$SubProblems{$ProblemType}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"}  );
                    foreach my $Attr (keys(%{$Sub_SubProblems{$ProblemType_Init}}))
                    { # other properties
                        $SubProblems{$ProblemType}{$Member_Name}{$Attr} = $Sub_SubProblems{$ProblemType_Init}{$Attr};
                    }
                }
                if(not isPublic(\%Type1_Pure, $Member_Pos))
                { # do NOT check internal type changes
                    next;
                }
                if($MemberType1_Id and $MemberType2_Id)
                {# checking member type changes (replace)
                    %Sub_SubProblems = mergeTypes($MemberType1_Id, $Tid_TDid{1}{$MemberType1_Id},
                                                  $MemberType2_Id, $Tid_TDid{2}{$MemberType2_Id});
                    foreach my $Sub_SubProblemType (keys(%Sub_SubProblems))
                    {
                        foreach my $Sub_SubLocation (keys(%{$Sub_SubProblems{$Sub_SubProblemType}}))
                        {
                            my $NewLocation = ($Sub_SubLocation)?$Member_Name."->".$Sub_SubLocation:$Member_Name;
                            $SubProblems{$Sub_SubProblemType}{$NewLocation}{"IsInTypeInternals"}=1;
                            foreach my $Attr (keys(%{$Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}})) {
                                $SubProblems{$Sub_SubProblemType}{$NewLocation}{$Attr} = $Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}{$Attr};
                            }
                            if($Sub_SubLocation!~/\-\>/) {
                                $SubProblems{$Sub_SubProblemType}{$NewLocation}{"Start_Type_Name"} = $MemberType1_Name;
                            }
                        }
                    }
                }
            }
        }
    }
    foreach my $Member_Pos (sort {int($a) <=> int($b)} keys(%{$Type2_Pure{"Memb"}}))
    {# checking added members, public and private
        my $Member_Name = $Type2_Pure{"Memb"}{$Member_Pos}{"name"};
        next if(not $Member_Name);
        if($AddedField{$Member_Pos})
        { # added
            if($Type2_Pure{"Type"}=~/\A(Struct|Class)\Z/)
            {
                my $ProblemType = "Added_Field";
                if(not isPublic(\%Type2_Pure, $Member_Pos)
                or isUnnamed($Member_Name)) {
                    $ProblemType = "Added_Private_Field";
                }
                if(not isMemPadded($Member_Pos, -1, \%Type2_Pure, \%AddedField, 2))
                {
                    if(my $MNum = isAccessible(\%Type2_Pure, \%AddedField, $Member_Pos, -1))
                    {# public fields after the current
                        if(getOffset($MNum-1, \%Type2_Pure, 2)!=getOffset($RelatedField_Rev{$MNum-1}, \%Type1_Pure, 1))
                        { # changed offset
                            $ProblemType .= "_And_Layout";
                        }
                    }
                    if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"}) {
                        $ProblemType .= "_And_Size";
                    }
                }
                if($ProblemType eq "Added_Private_Field")
                { # skip added private fields
                    next;
                }
                %{$SubProblems{$ProblemType}{$Member_Name}}=(
                    "Target"=>$Member_Name,
                    "Type_Name"=>$Type1_Pure{"Name"},
                    "Type_Type"=>$Type1_Pure{"Type"}  );
            }
            elsif($Type2_Pure{"Type"} eq "Union")
            {
                if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                {
                    %{$SubProblems{"Added_Union_Field_And_Size"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"}  );
                }
                else
                {
                    %{$SubProblems{"Added_Union_Field"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"}  );
                }
            }
        }
    }
    %{$Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId}} = %SubProblems;
    pop(@RecurTypes);
    return %SubProblems;
}

sub isUnnamed($) {
    return $_[0]=~/\Aunnamed\d+\Z/;
}

sub get_TypeName($$)
{
    my ($TypeId, $LibVersion) = @_;
    return $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Name"};
}

sub get_ClassVTable($$)
{
    my ($ClassId, $LibVersion) = @_;
    return $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$ClassId}}{$ClassId}{"VTable"};
}

sub get_ShortName($$)
{
    my ($TypeId, $LibVersion) = @_;
    my $TypeName = $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Name"};
    my $NameSpace = $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"NameSpace"};
    $TypeName=~s/\A$NameSpace\:\://g;
    return $TypeName;
}

sub get_TypeType($$)
{
    my ($TypeId, $LibVersion) = @_;
    return $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Type"};
}

sub get_TypeSize($$)
{
    my ($TypeId, $LibVersion) = @_;
    return $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Size"};
}

sub get_TypeHeader($$)
{
    my ($TypeId, $LibVersion) = @_;
    return $TypeInfo{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Header"};
}

sub goToFirst($$$$)
{
    my ($TypeDId, $TypeId, $LibVersion, $Type_Type) = @_;
    if(defined $Cache{"goToFirst"}{$TypeDId}{$TypeId}{$LibVersion}{$Type_Type}) {
        return %{$Cache{"goToFirst"}{$TypeDId}{$TypeId}{$LibVersion}{$Type_Type}};
    }
    return () if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
    return () if(not $Type{"Type"});
    if($Type{"Type"} ne $Type_Type)
    {
        return () if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
        %Type = goToFirst($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion, $Type_Type);
    }
    $Cache{"goToFirst"}{$TypeDId}{$TypeId}{$LibVersion}{$Type_Type} = \%Type;
    return %Type;
}

my %TypeSpecAttributes = (
    "Const" => 1,
    "Volatile" => 1,
    "Restrict" => 1,
    "Typedef" => 1
);

sub get_PureType($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_PureType"}{$TypeDId}{$TypeId}{$LibVersion}) {
        return %{$Cache{"get_PureType"}{$TypeDId}{$TypeId}{$LibVersion}};
    }
    return "" if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    if($TypeSpecAttributes{$Type{"Type"}}) {
        %Type = get_PureType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    }
    $Cache{"get_PureType"}{$TypeDId}{$TypeId}{$LibVersion} = \%Type;
    return %Type;
}

sub get_PointerLevel($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return 0 if(not $TypeId);
    if(defined $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId}{$LibVersion}) {
        return $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId}{$LibVersion};
    }
    return "" if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
    return 1 if($Type{"Type"}=~/FuncPtr|MethodPtr|FieldPtr/);
    return 0 if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    my $PointerLevel = 0;
    if($Type{"Type"} =~/Pointer|Ref|FuncPtr|MethodPtr|FieldPtr/) {
        $PointerLevel += 1;
    }
    $PointerLevel += get_PointerLevel($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId}{$LibVersion} = $PointerLevel;
    return $PointerLevel;
}

sub get_BaseType($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_BaseType"}{$TypeDId}{$TypeId}{$LibVersion}) {
        return %{$Cache{"get_BaseType"}{$TypeDId}{$TypeId}{$LibVersion}};
    }
    return "" if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    %Type = get_BaseType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    $Cache{"get_BaseType"}{$TypeDId}{$TypeId}{$LibVersion} = \%Type;
    return %Type;
}

sub get_BaseTypeQual($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    return "" if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
    return "" if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    my $Qual = "";
    if($Type{"Type"} eq "Const") {
        $Qual .= "const";
    }
    elsif($Type{"Type"} eq "Pointer") {
        $Qual .= "*";
    }
    elsif($Type{"Type"} eq "Ref") {
        $Qual .= "&";
    }
    elsif($Type{"Type"} eq "Volatile") {
        $Qual .= "volatile";
    }
    elsif($Type{"Type"} eq "Restrict") {
        $Qual .= "restrict";
    }
    my $BQual = get_BaseTypeQual($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    return $BQual.$Qual;
}

sub get_OneStep_BaseType($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    return "" if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    return get_Type($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
}

sub get_Type($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    return "" if(not $TypeInfo{$LibVersion}{$TypeDId}{$TypeId});
    return %{$TypeInfo{$LibVersion}{$TypeDId}{$TypeId}};
}

sub is_template_instance($$)
{
    my ($Interface, $LibVersion) = @_;
    return 0 if($Interface!~/\A(_Z|\?)/);
    my $Signature = get_SignatureNoInfo($Interface, $LibVersion);
    return 0 if($Signature!~/>/);
    my $ShortName = substr($Signature, 0, detect_center($Signature));
    $ShortName=~s/::operator .*//;# class::operator template<instance>
    return ($ShortName=~/>/);
}

sub skipGlobalData($)
{
    my $Symbol = $_[0];
    return ($Symbol=~/\A(_ZGV|_ZTI|_ZTS|_ZTT|_ZTV|_ZTC|_ZThn|_ZTv0_n)/);
}

sub symbolFilter($$$)
{ # some special cases when the symbol cannot be imported
    my ($Interface, $LibVersion, $Type) = @_;
    if(skipGlobalData($Interface))
    { # non-public global data
        return 0;
    }
    if($CheckObjectsOnly) {
        return 0 if($Interface=~/\A(_init|_fini)\Z/);
    }
    if($Type eq "Imported")
    {
        if(not $STDCXX_TESTING and $Interface=~/\A(_ZS|_ZNS|_ZNKS)/)
        { # stdc++ interfaces
            return 0;
        }
        if($CompleteSignature{$LibVersion}{$Interface}{"InLine"}
        and not $CompleteSignature{$LibVersion}{$Interface}{"Virt"})
        { # inline non-virtual methods
            return 0;
        }
        if(is_template_instance($Interface, $LibVersion))
        {
            if(defined $CompleteSignature{$LibVersion}{$Interface}
            and ($CompleteSignature{$LibVersion}{$Interface}{"Virt"}
            or $CompleteSignature{$LibVersion}{$Interface}{"PureVirt"}))
            { # virtual (pure) template instances can be imported
            }
            else
            { # cannot be imported (will be copied)
              return 0;
            }
        }
        if($SkipSymbols{$LibVersion}{$Interface})
        { # user defined symbols to ignore
            return 0;
        }
        if($SymbolsListPath and not $SymbolsList{$Interface})
        { # user defined symbols
            return 0;
        }
        if($AppPath and not $SymbolsList_App{$Interface})
        { # user defined symbols (in application)
            return 0;
        }
    }
    return 1;
}

sub mergeImplementations()
{
    my $DiffCmd = get_CmdPath("diff");
    if(not $DiffCmd) {
        exitStatus("Not_Found", "can't find \"diff\"");
    }
    foreach my $Interface (sort keys(%{$Symbol_Library{1}}))
    {# implementation changes
        next if($CompleteSignature{1}{$Interface}{"Private"});
        next if(not $CompleteSignature{1}{$Interface}{"Header"} and not $CheckObjectsOnly);
        next if(not $Symbol_Library{2}{$Interface} and not $Symbol_Library{2}{$SymVer{2}{$Interface}});
        next if(not symbolFilter($Interface, 1, "Imported"));
        my $Impl1 = canonify_implementation($Interface_Implementation{1}{$Interface});
        next if(not $Impl1);
        my $Impl2 = canonify_implementation($Interface_Implementation{2}{$Interface});
        next if(not $Impl2);
        if($Impl1 ne $Impl2)
        {
            writeFile("$TMP_DIR/impl1", $Impl1);
            writeFile("$TMP_DIR/impl2", $Impl2);
            my $Diff = `$DiffCmd -rNau $TMP_DIR/impl1 $TMP_DIR/impl2`;
            $Diff=~s/(---|\+\+\+).+\n//g;
            $Diff=~s/[ ]{3,}/ /g;
            $Diff=~s/\n\@\@/\n \n\@\@/g;
            unlink("$TMP_DIR/impl1", "$TMP_DIR/impl2");
            %{$ImplProblems{$Interface}}=(
                "Diff" => get_CodeView($Diff)  );
        }
    }
}

sub canonify_implementation($)
{
    my $FuncBody=  $_[0];
    return "" if(not $FuncBody);
    $FuncBody=~s/0x[a-f\d]+/0x?/g;# addr
    $FuncBody=~s/((\A|\n)[a-z]+[\t ]+)[a-f\d]+([^x]|\Z)/$1?$3/g;# call, jump
    $FuncBody=~s/# [a-f\d]+ /# ? /g;# call, jump
    $FuncBody=~s/%([a-z]+[a-f\d]*)/\%reg/g;# registers
    while($FuncBody=~s/\nnop[ \t]*(\n|\Z)/$1/g){};# empty op
    $FuncBody=~s/<.+?\.cpp.+?>/<name.cpp>/g;
    $FuncBody=~s/(\A|\n)[a-f\d]+ </$1? </g;# 5e74 <_ZN...
    $FuncBody=~s/\.L\d+/.L/g;
    $FuncBody=~s/#(-?)\d+/#$1?/g;# r3, [r3, #120]
    $FuncBody=~s/[\n]{2,}/\n/g;
    return $FuncBody;
}

sub get_CodeView($)
{
    my $Code = $_[0];
    my $View = "";
    foreach my $Line (split(/\n/, $Code))
    {
        if($Line=~s/\A(\+|-)/$1 /g)
        {# bold line
            $View .= "<tr><td><b>".htmlSpecChars($Line)."</b></td></tr>\n";
        }
        else {
            $View .= "<tr><td>".htmlSpecChars($Line)."</td></tr>\n";
        }
    }
    return "<table class='code_view'>$View</table>\n";
}

sub getImplementations($$)
{
    my ($LibVersion, $Path) = @_;
    return if(not $LibVersion or not -e $Path);
    if($OSgroup eq "macos")
    {
        my $OtoolCmd = get_CmdPath("otool");
        if(not $OtoolCmd) {
            exitStatus("Not_Found", "can't find \"otool\"");
        }
        my $CurInterface = "";
        foreach my $Line (split(/\n/, `$OtoolCmd -tv $Path 2>$TMP_DIR/null`))
        {
            if($Line=~/\A\s*_(\w+)\s*:/i) {
                $CurInterface = $1;
            }
            elsif($Line=~/\A\s*[\da-z]+\s+(.+?)\Z/i) {
                $Interface_Implementation{$LibVersion}{$CurInterface} .= "$1\n";
            }
        }
    }
    else
    {
        my $ObjdumpCmd = get_CmdPath("objdump");
        if(not $ObjdumpCmd) {
            exitStatus("Not_Found", "can't find \"objdump\"");
        }
        my $CurInterface = "";
        foreach my $Line (split(/\n/, `$ObjdumpCmd -d $Path 2>$TMP_DIR/null`))
        {
            if($Line=~/\A[\da-z]+\s+<(\w+)>/i) {
                $CurInterface = $1;
            }
            else
            { # x86:    51fa:(\t)89 e5               (\t)mov    %esp,%ebp
              # arm:    5020:(\t)e24cb004(\t)sub(\t)fp, ip, #4(\t); 0x4
                if($Line=~/\A\s*[a-f\d]+:\s+([a-f\d]+\s+)+([a-z]+\s+.*?)\s*(;.*|)\Z/i) {
                    $Interface_Implementation{$LibVersion}{$CurInterface} .= "$2\n";
                }
            }
        }
    }
}

sub detectAdded()
{
    foreach my $Symbol (keys(%{$Symbol_Library{2}}))
    {
        if(link_symbol($Symbol, 1, "+Deps"))
        { # linker can find a new symbol
          # in the old-version library
          # So, it's not a new symbol
            next;
        }
        if(my $VSym = $SymVer{2}{$Symbol}
        and $Symbol!~/\@/) {
            next;
        }
        $AddedInt{$Symbol} = 1;
    }
}

sub detectRemoved()
{
    foreach my $Symbol (keys(%{$Symbol_Library{1}}))
    {
        if($CheckObjectsOnly) {
            $CheckedSymbols{$Symbol} = 1;
        }
        if(link_symbol($Symbol, 2, "+Deps"))
        { # linker can find an old symbol
          # in the new-version library
            next;
        }
        if(my $VSym = $SymVer{1}{$Symbol}
        and $Symbol!~/\@/) {
            next;
        }
        $RemovedInt{$Symbol} = 1;
    }
}

sub mergeLibs()
{
    foreach my $Symbol (sort keys(%AddedInt))
    { # checking added symbols
        next if($CompleteSignature{2}{$Symbol}{"Private"});
        next if(not $CompleteSignature{2}{$Symbol}{"Header"} and not $CheckObjectsOnly);
        next if(not symbolFilter($Symbol, 2, "Imported"));
        %{$CompatProblems{$Symbol}{"Added_Interface"}{""}}=();
    }
    foreach my $Symbol (sort keys(%RemovedInt))
    { # checking removed symbols
        next if($CompleteSignature{1}{$Symbol}{"Private"});
        next if(not $CompleteSignature{1}{$Symbol}{"Header"} and not $CheckObjectsOnly);
        if($Symbol=~/\A_ZTV/) {
            # skip v-tables for templates, that should not be imported by applications
            next if($tr_name{$Symbol}=~/</);
            if(not keys(%{$ClassMethods{1}{$VTableClass{$Symbol}}}))
            { # vtables for "private" classes
              # use case: vtable for QDragManager (Qt 4.5.3 to 4.6.0) became HIDDEN symbol
                next;
            }
        }
        else {
            next if(not symbolFilter($Symbol, 1, "Imported"));
        }
        if($CompleteSignature{1}{$Symbol}{"PureVirt"})
        { # symbols for pure virtual methods cannot be called by clients
            next;
        }
        %{$CompatProblems{$Symbol}{"Removed_Interface"}{""}}=();
    }
}

sub detectAdded_H()
{
    foreach my $Symbol (sort keys(%{$CompleteSignature{2}}))
    {
        if(not $CompleteSignature{1}{$Symbol}) {
            $AddedInt{$Symbol} = 1;
        }
    }
}

sub detectRemoved_H()
{
    foreach my $Symbol (sort keys(%{$CompleteSignature{1}}))
    {
        if(not $CompleteSignature{2}{$Symbol}) {
            $RemovedInt{$Symbol} = 1;
        }
    }
}

sub mergeHeaders()
{
    foreach my $Symbol (sort keys(%AddedInt))
    { # checking added symbols
        next if($CompleteSignature{2}{$Symbol}{"PureVirt"});
        next if($CompleteSignature{2}{$Symbol}{"InLine"});
        %{$CompatProblems{$Symbol}{"Added_Interface"}{""}}=();
    }
    foreach my $Symbol (sort keys(%RemovedInt))
    { # checking removed symbols
        next if($CompleteSignature{1}{$Symbol}{"PureVirt"});
        next if($CompleteSignature{1}{$Symbol}{"InLine"});
        %{$CompatProblems{$Symbol}{"Removed_Interface"}{""}}=();
    }
}

sub add_absent_param_names($)
{
    my $LibraryVersion = $_[0];
    return if(not keys(%AddIntParams));
    my $SecondVersion = $LibraryVersion==1?2:1;
    foreach my $Interface (sort keys(%{$CompleteSignature{$LibraryVersion}}))
    {
        next if(not keys(%{$AddIntParams{$Interface}}));
        foreach my $ParamPos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$LibraryVersion}{$Interface}{"Param"}}))
        {# add absent parameter names
            my $ParamName = $CompleteSignature{$LibraryVersion}{$Interface}{"Param"}{$ParamPos}{"name"};
            if($ParamName=~/\Ap\d+\Z/ and my $NewParamName = $AddIntParams{$Interface}{$ParamPos})
            {# names from the external file
                if(defined $CompleteSignature{$SecondVersion}{$Interface}
                and defined $CompleteSignature{$SecondVersion}{$Interface}{"Param"}{$ParamPos})
                {
                    if($CompleteSignature{$SecondVersion}{$Interface}{"Param"}{$ParamPos}{"name"}=~/\Ap\d+\Z/) {
                        $CompleteSignature{$LibraryVersion}{$Interface}{"Param"}{$ParamPos}{"name"} = $NewParamName;
                    }
                }
                else {
                    $CompleteSignature{$LibraryVersion}{$Interface}{"Param"}{$ParamPos}{"name"} = $NewParamName;
                }
            }
        }
    }
}

sub detectChangedTypedefs()
{# detect changed typedefs to create correct function signatures
    foreach my $Typedef (keys(%{$Typedef_BaseName{1}}))
    {
        next if(not $Typedef);
        next if(isAnon($Typedef_BaseName{1}{$Typedef}));
        next if(isAnon($Typedef_BaseName{2}{$Typedef}));
        next if(not $Typedef_BaseName{1}{$Typedef});
        next if(not $Typedef_BaseName{2}{$Typedef});# exclude added/removed
        if($Typedef_BaseName{1}{$Typedef} ne $Typedef_BaseName{2}{$Typedef}) {
            $ChangedTypedef{$Typedef} = 1;
        }
    }
}

sub get_symbol_suffix($)
{
    my $Interface = $_[0];
    $Interface=~s/\A([^\@\$\?]+)[\@\$]+/$1/g;# remove version
    my $Signature = $tr_name{$Interface};
    my $Suffix = substr($Signature, detect_center($Signature));
    return $Suffix;
}

sub get_symbol_prefix($$)
{
    my ($Symbol, $LibVersion) = @_;
    my $ShortName = $CompleteSignature{$LibVersion}{$Symbol}{"ShortName"};
    if(my $ClassId = $CompleteSignature{$LibVersion}{$Symbol}{"Class"})
    {# methods
        $ShortName = get_TypeName($ClassId, $LibVersion)."::".$ShortName;
    }
    return $ShortName;
}

sub mergeSignatures()
{
    my %SubProblems = ();
    
    registerVirtualTable(1);
    registerVirtualTable(2);
    
    registerOverriding(1);
    registerOverriding(2);
    
    setVirtFuncPositions(1);
    setVirtFuncPositions(2);
    
    add_absent_param_names(1);
    add_absent_param_names(2);

    detectChangedTypedefs();

    mergeBases();
    
    my %AddedOverloads = ();
    foreach my $Interface (sort keys(%AddedInt))
    {# check all added exported symbols
        next if(not $CompleteSignature{2}{$Interface}{"Header"});
        if($TargetHeader
        and $CompleteSignature{2}{$Interface}{"Header"} ne $TargetHeader)
        { # user-defined header
            next;
        }
        if(defined $CompleteSignature{1}{$Interface}
        and $CompleteSignature{1}{$Interface}{"Header"}) {
            # double-check added symbol
            next;
        }
        next if(not symbolFilter($Interface, 2, "Imported"));
        if($Interface=~/\A(_Z|\?)/)
        { # C++
            $AddedOverloads{get_symbol_prefix($Interface, 2)}{get_symbol_suffix($Interface)} = $Interface;
        }
        if(my $OverriddenMethod = $CompleteSignature{2}{$Interface}{"Override"})
        {# register virtual redefinition
            my $AffectedClass_Name = get_TypeName($CompleteSignature{2}{$Interface}{"Class"}, 2);
            if(defined $CompleteSignature{1}{$OverriddenMethod}
            and $CompleteSignature{1}{$OverriddenMethod}{"Virt"} and $ClassToId{1}{$AffectedClass_Name}
            and not $CompleteSignature{1}{$OverriddenMethod}{"Private"})
            { # public virtual methods, virtual destructors: class should exist in previous version
                if(isCopyingClass($ClassToId{1}{$AffectedClass_Name}, 1)) {
                    # old v-table (copied) will be used by applications
                    next;
                }
                if(defined $CompleteSignature{1}{$Interface}
                and $CompleteSignature{1}{$Interface}{"InLine"}) {
                    # auto-generated virtual destructors stay in the header (and v-table), added to library
                    # use case: Ice 3.3.1 -> 3.4.0
                    next;
                }
                %{$CompatProblems{$OverriddenMethod}{"Overridden_Virtual_Method"}{$tr_name{$Interface}}}=(
                    "Type_Name"=>$AffectedClass_Name,
                    "Type_Type"=>"Class",
                    "Target"=>get_Signature($Interface, 2),
                    "Old_Value"=>get_Signature($OverriddenMethod, 2),
                    "New_Value"=>get_Signature($Interface, 2)  );
            }
        }
    }
    foreach my $Interface (sort keys(%RemovedInt))
    {
        next if(not $CompleteSignature{1}{$Interface}{"Header"});
        if($TargetHeader
        and $CompleteSignature{1}{$Interface}{"Header"} ne $TargetHeader)
        { # user-defined header
            next;
        }
        if(defined $CompleteSignature{2}{$Interface}
        and $CompleteSignature{2}{$Interface}{"Header"}) {
            # double-check removed symbol
            next;
        }
        next if($CompleteSignature{1}{$Interface}{"Private"}); # skip private methods
        next if(not symbolFilter($Interface, 1, "Imported"));
        $CheckedSymbols{$Interface} = 1;
        if(my $OverriddenMethod = $CompleteSignature{1}{$Interface}{"Override"})
        {# register virtual redefinition
            my $AffectedClass_Name = get_TypeName($CompleteSignature{1}{$Interface}{"Class"}, 1);
            if(defined $CompleteSignature{2}{$OverriddenMethod}
            and $CompleteSignature{2}{$OverriddenMethod}{"Virt"} and $ClassToId{2}{$AffectedClass_Name})
            {# virtual methods, virtual destructors: class should exist in newer version
                if(isCopyingClass($CompleteSignature{1}{$Interface}{"Class"}, 1)) {
                    # old v-table (copied) will be used by applications
                    next;
                }
                if(defined $CompleteSignature{2}{$Interface}
                and $CompleteSignature{2}{$Interface}{"InLine"}) {
                    # auto-generated virtual destructors stay in the header (and v-table), removed from library
                    # use case: Ice 3.3.1 -> 3.4.0
                    next;
                }
                %{$CompatProblems{$Interface}{"Overridden_Virtual_Method_B"}{$tr_name{$OverriddenMethod}}}=(
                    "Type_Name"=>$AffectedClass_Name,
                    "Type_Type"=>"Class",
                    "Target"=>get_Signature($OverriddenMethod, 1),
                    "Old_Value"=>get_Signature($Interface, 1),
                    "New_Value"=>get_Signature($OverriddenMethod, 1)  );
            }
        }
        if($OSgroup eq "windows")
        {# register the reason of symbol name change
            if(my $NewSymbol = $mangled_name{2}{$tr_name{$Interface}})
            {
                if($AddedInt{$NewSymbol})
                {
                    if($CompleteSignature{1}{$Interface}{"Static"} ne $CompleteSignature{2}{$NewSymbol}{"Static"})
                    {
                        if($CompleteSignature{2}{$NewSymbol}{"Static"}) {
                            %{$CompatProblems{$Interface}{"Symbol_Changed_Static"}{$tr_name{$Interface}}}=(
                                "Target"=>$tr_name{$Interface},
                                "Old_Value"=>$Interface,
                                "New_Value"=>$NewSymbol  );
                        }
                        else {
                            %{$CompatProblems{$Interface}{"Symbol_Changed_NonStatic"}{$tr_name{$Interface}}}=(
                                "Target"=>$tr_name{$Interface},
                                "Old_Value"=>$Interface,
                                "New_Value"=>$NewSymbol  );
                        }
                    }
                    if($CompleteSignature{1}{$Interface}{"Virt"} ne $CompleteSignature{2}{$NewSymbol}{"Virt"})
                    {
                        if($CompleteSignature{2}{$NewSymbol}{"Virt"}) {
                            %{$CompatProblems{$Interface}{"Symbol_Changed_Virtual"}{$tr_name{$Interface}}}=(
                                "Target"=>$tr_name{$Interface},
                                "Old_Value"=>$Interface,
                                "New_Value"=>$NewSymbol  );
                        }
                        else {
                            %{$CompatProblems{$Interface}{"Symbol_Changed_NonVirtual"}{$tr_name{$Interface}}}=(
                                "Target"=>$tr_name{$Interface},
                                "Old_Value"=>$Interface,
                                "New_Value"=>$NewSymbol  );
                        }
                    }
                    my $ReturnTypeName1 = get_TypeName($CompleteSignature{1}{$Interface}{"Return"}, 1);
                    my $ReturnTypeName2 = get_TypeName($CompleteSignature{2}{$NewSymbol}{"Return"}, 2);
                    if($ReturnTypeName1 ne $ReturnTypeName2)
                    {
                        %{$CompatProblems{$Interface}{"Symbol_Changed_Return"}{$tr_name{$Interface}}}=(
                            "Target"=>$tr_name{$Interface},
                            "Old_Type"=>$ReturnTypeName1,
                            "New_Type"=>$ReturnTypeName2,
                            "Old_Value"=>$Interface,
                            "New_Value"=>$NewSymbol  );
                    }
                }
            }
        }
        if($Interface=~/\A(_Z|\?)/)
        { # C++
            my $Prefix = get_symbol_prefix($Interface, 1);
            if(my @Overloads = sort keys(%{$AddedOverloads{$Prefix}})
            and not $AddedOverloads{$Prefix}{get_symbol_suffix($Interface)})
            {# changed signature
                my $NewSymbol = $AddedOverloads{$Prefix}{$Overloads[0]};
                if($CompleteSignature{1}{$Interface}{"Constructor"}) {
                    if($Interface=~/(C1E|C2E)/) {
                        my $CtorType = $1;
                        $NewSymbol=~s/(C1E|C2E)/$CtorType/g;
                    }
                }
                elsif($CompleteSignature{1}{$Interface}{"Destructor"}) {
                    if($Interface=~/(D0E|D1E|D2E)/) {
                        my $DtorType = $1;
                        $NewSymbol=~s/(D0E|D1E|D2E)/$DtorType/g;
                    }
                }
                %{$CompatProblems{$Interface}{"Symbol_Changed_Signature"}{$tr_name{$Interface}}}=(
                    "Target"=>$tr_name{$Interface},
                    "New_Signature"=>get_Signature($NewSymbol, 2),
                    "Old_Value"=>$Interface,
                    "New_Value"=>$NewSymbol  );
            }
        }
    }
    foreach my $Interface (sort keys(%{$CompleteSignature{1}}))
    { # checking interfaces
        if($Interface!~/[\@\$\?]/)
        { # symbol without version
            if(my $VSym = $SymVer{1}{$Interface})
            { # the symbol is linked with versioned symbol
                if($CompleteSignature{2}{$VSym}{"MnglName"})
                { # show report for symbol@ver only
                    next;
                }
                elsif(not link_symbol($VSym, 2, "-Deps"))
                { # changed version: sym@v1 to sym@v2
                  # do NOT show report for symbol
                    next;
                }
            }
        }
        if($CompleteSignature{1}{$Interface}{"Private"})
        { # private symbols
            next;
        }
        if(not $CompleteSignature{1}{$Interface}{"MnglName"}
        or not $CompleteSignature{2}{$Interface}{"MnglName"})
        { # absent mangled name
            next;
        }
        if(not $CompleteSignature{1}{$Interface}{"Header"}
        or not $CompleteSignature{2}{$Interface}{"Header"})
        { # without a header
            next;
        }
        if($TargetHeader
        and $CompleteSignature{1}{$Interface}{"Header"} ne $TargetHeader)
        { # user-defined header
            next;
        }
        if($CheckHeadersOnly)
        { # skip added and removed pure virtual methods
            next if(not $CompleteSignature{1}{$Interface}{"PureVirt"} and $CompleteSignature{2}{$Interface}{"PureVirt"});
            next if($CompleteSignature{1}{$Interface}{"PureVirt"} and not $CompleteSignature{2}{$Interface}{"PureVirt"});
        }
        else
        { # skip external, added and removed functions except pure virtual methods
            if(not link_symbol($Interface, 1, "-Deps")
            or not link_symbol($Interface, 2, "-Deps"))
            { # symbols from target library(ies) only
              # excluding dependent libraries
                if(not $CompleteSignature{1}{$Interface}{"PureVirt"}
                or not $CompleteSignature{2}{$Interface}{"PureVirt"}) {
                    next;
                }
            }
        }
        if(not symbolFilter($Interface, 1, "Imported"))
        {# symbols that cannot be imported
            next;
        }
        # checking virtual table
        if($CompleteSignature{1}{$Interface}{"Class"}) {
            mergeVirtualTables($Interface);
        }
        if($COMPILE_ERRORS)
        { # if some errors occurred at the compiling stage
          # then some false positives can be skipped here
            if(not $CompleteSignature{1}{$Interface}{"Data"} and $CompleteSignature{2}{$Interface}{"Data"}
            and not $CompleteSignature{2}{$Interface}{"Object"})
            { # missed information about parameters in newer version
                next;
            }
            if($CompleteSignature{1}{$Interface}{"Data"} and not $CompleteSignature{1}{$Interface}{"Object"}
            and not $CompleteSignature{2}{$Interface}{"Data"})
            {# missed information about parameters in older version
                next;
            }
        }
        my ($MnglName, $VersionSpec, $SymbolVersion) = separate_symbol($Interface);
        # checking attributes
        if($CompleteSignature{2}{$Interface}{"Static"}
        and not $CompleteSignature{1}{$Interface}{"Static"} and $Interface=~/\A(_Z|\?)/) {
            %{$CompatProblems{$Interface}{"Method_Became_Static"}{""}}=();
        }
        elsif(not $CompleteSignature{2}{$Interface}{"Static"}
        and $CompleteSignature{1}{$Interface}{"Static"} and $Interface=~/\A(_Z|\?)/) {
            %{$CompatProblems{$Interface}{"Method_Became_NonStatic"}{""}}=();
        }
        if(($CompleteSignature{1}{$Interface}{"Virt"} and $CompleteSignature{2}{$Interface}{"Virt"})
        or ($CompleteSignature{1}{$Interface}{"PureVirt"} and $CompleteSignature{2}{$Interface}{"PureVirt"}))
        { # relative position of virtual and pure virtual methods
            if($CompleteSignature{1}{$Interface}{"RelPos"}!=$CompleteSignature{2}{$Interface}{"RelPos"})
            {
                my $Class_Id = $CompleteSignature{1}{$Interface}{"Class"};
                if($VirtualTable{1}{get_TypeName($Class_Id, 1)}{$Interface}!=$VirtualTable{2}{get_TypeName($Class_Id, 1)}{$Interface})
                { # check the absolute position of virtual method (including added and removed methods)
                    my %Class_Type = get_Type($Tid_TDid{1}{$Class_Id}, $Class_Id, 1);
                    my $ProblemType = "Virtual_Method_Position";
                    if($CompleteSignature{1}{$Interface}{"PureVirt"}) {
                        $ProblemType = "Pure_Virtual_Method_Position";
                    }
                    if(isUsedClass(get_TypeName($Class_Id, 1), 1))
                    {
                        my @Affected = ($Interface, keys(%{$OverriddenMethods{1}{$Interface}}));
                        foreach my $AffectedInterface (@Affected) {
                            %{$CompatProblems{$AffectedInterface}{$ProblemType}{$tr_name{$MnglName}}}=(
                                "Type_Name"=>$Class_Type{"Name"},
                                "Type_Type"=>"Class",
                                "Old_Value"=>$CompleteSignature{1}{$Interface}{"RelPos"},
                                "New_Value"=>$CompleteSignature{2}{$Interface}{"RelPos"},
                                "Target"=>get_Signature($Interface, 1)  );
                        }
                    }
                }
            }
        }
        if($CompleteSignature{1}{$Interface}{"PureVirt"}
        or $CompleteSignature{2}{$Interface}{"PureVirt"})
        { # do NOT check type changes in pure virtuals
            next;
        }
        $CheckedSymbols{$Interface}=1;
        if($Interface=~/\A(_Z|\?)/
        or keys(%{$CompleteSignature{1}{$Interface}{"Param"}})==keys(%{$CompleteSignature{2}{$Interface}{"Param"}}))
        { # C/C++: changes in parameters
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
            { # checking parameters
                mergeParameters($Interface, $ParamPos, $ParamPos);
            }
        }
        else
        { # C: added/removed parameters
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{2}{$Interface}{"Param"}}))
            { # checking added parameters
                my $ParamType2_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"type"};
                last if(get_TypeName($ParamType2_Id, 2) eq "...");
                my $Parameter_Name = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"name"};
                my $Parameter_OldName = (defined $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos})?$CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"name"}:"";
                my $ParamPos_Prev = "-1";
                if($Parameter_Name=~/\Ap\d+\Z/i)
                { # added unnamed parameter ( pN )
                    my @Positions1 = find_ParamPair_Pos_byTypeAndPos(get_TypeName($ParamType2_Id, 2), $ParamPos, "backward", $Interface, 1);
                    my @Positions2 = find_ParamPair_Pos_byTypeAndPos(get_TypeName($ParamType2_Id, 2), $ParamPos, "backward", $Interface, 2);
                    if($#Positions1==-1 or $#Positions2>$#Positions1) {
                        $ParamPos_Prev = "lost";
                    }
                }
                else {
                    $ParamPos_Prev = find_ParamPair_Pos_byName($Parameter_Name, $Interface, 1);
                }
                if($ParamPos_Prev eq "lost")
                {
                    if($ParamPos>keys(%{$CompleteSignature{1}{$Interface}{"Param"}})-1)
                    {
                        my $ProblemType = "Added_Parameter";
                        if($Parameter_Name=~/\Ap\d+\Z/) {
                            $ProblemType = "Added_Unnamed_Parameter";
                        }
                        %{$CompatProblems{$Interface}{$ProblemType}{numToStr($ParamPos+1)." Parameter"}}=(
                            "Target"=>$Parameter_Name,
                            "Param_Pos"=>$ParamPos,
                            "Param_Type"=>get_TypeName($ParamType2_Id, 2),
                            "New_Signature"=>get_Signature($Interface, 2)  );
                    }
                    else
                    {
                        my %ParamType_Pure = get_PureType($Tid_TDid{2}{$ParamType2_Id}, $ParamType2_Id, 2);
                        my $ParamStraightPairType_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"type"};
                        my %ParamStraightPairType_Pure = get_PureType($Tid_TDid{1}{$ParamStraightPairType_Id}, $ParamStraightPairType_Id, 1);
                        if(($ParamType_Pure{"Name"} eq $ParamStraightPairType_Pure{"Name"} or get_TypeName($ParamType2_Id, 2) eq get_TypeName($ParamStraightPairType_Id, 1))
                        and find_ParamPair_Pos_byName($Parameter_OldName, $Interface, 2) eq "lost")
                        {
                            if($Parameter_OldName!~/\Ap\d+\Z/ and $Parameter_Name!~/\Ap\d+\Z/)
                            {
                                %{$CompatProblems{$Interface}{"Renamed_Parameter"}{numToStr($ParamPos+1)." Parameter"}}=(
                                    "Target"=>$Parameter_OldName,
                                    "Param_Pos"=>$ParamPos,
                                    "Param_Type"=>get_TypeName($ParamType2_Id, 2),
                                    "Old_Value"=>$Parameter_OldName,
                                    "New_Value"=>$Parameter_Name,
                                    "New_Signature"=>get_Signature($Interface, 2)  );
                            }
                        }
                        else
                        {
                            my $ProblemType = "Added_Middle_Parameter";
                            if($Parameter_Name=~/\Ap\d+\Z/) {
                                $ProblemType = "Added_Middle_Unnamed_Parameter";
                            }
                            %{$CompatProblems{$Interface}{$ProblemType}{numToStr($ParamPos+1)." Parameter"}}=(
                                "Target"=>$Parameter_Name,
                                "Param_Pos"=>$ParamPos,
                                "Param_Type"=>get_TypeName($ParamType2_Id, 2),
                                "New_Signature"=>get_Signature($Interface, 2)  );
                        }
                    }
                }
            }
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
            { # check relevant parameters
                my $ParamType1_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"type"};
                my $ParamName1 = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"name"};
                if(defined $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos})
                {
                    my $ParamType2_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"type"};
                    my $ParamName2 = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"name"};
                    if(($ParamName1!~/\Ap\d+\Z/i and $ParamName1 eq $ParamName2)
                    or get_TypeName($ParamType1_Id, 1) eq get_TypeName($ParamType2_Id, 2)) {
                        mergeParameters($Interface, $ParamPos, $ParamPos);
                    }
                }
            }
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
            { # checking removed parameters
                my $ParamType1_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"type"};
                last if(get_TypeName($ParamType1_Id, 1) eq "...");
                my $Parameter_Name = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"name"};
                my $Parameter_NewName = (defined $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos})?$CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"name"}:"";
                my $ParamPos_New = "-1";
                if($Parameter_Name=~/\Ap\d+\Z/i)
                { # removed unnamed parameter ( pN )
                    my @Positions1 = find_ParamPair_Pos_byTypeAndPos(get_TypeName($ParamType1_Id, 1), $ParamPos, "forward", $Interface, 1);
                    my @Positions2 = find_ParamPair_Pos_byTypeAndPos(get_TypeName($ParamType1_Id, 1), $ParamPos, "forward", $Interface, 2);
                    if($#Positions2==-1 or $#Positions2<$#Positions1) {
                        $ParamPos_New = "lost";
                    }
                }
                else {
                    $ParamPos_New = find_ParamPair_Pos_byName($Parameter_Name, $Interface, 2);
                }
                if($ParamPos_New eq "lost")
                {
                    if($ParamPos>keys(%{$CompleteSignature{2}{$Interface}{"Param"}})-1)
                    {
                        my $ProblemType = "Removed_Parameter";
                        if($Parameter_Name=~/\Ap\d+\Z/) {
                            $ProblemType = "Removed_Unnamed_Parameter";
                        }
                        %{$CompatProblems{$Interface}{$ProblemType}{numToStr($ParamPos+1)." Parameter"}}=(
                            "Target"=>$Parameter_Name,
                            "Param_Pos"=>$ParamPos,
                            "Param_Type"=>get_TypeName($ParamType1_Id, 1),
                            "New_Signature"=>get_Signature($Interface, 2)  );
                    }
                    elsif($ParamPos<keys(%{$CompleteSignature{1}{$Interface}{"Param"}})-1)
                    {
                        my %ParamType_Pure = get_PureType($Tid_TDid{1}{$ParamType1_Id}, $ParamType1_Id, 1);
                        my $ParamStraightPairType_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"type"};
                        my %ParamStraightPairType_Pure = get_PureType($Tid_TDid{2}{$ParamStraightPairType_Id}, $ParamStraightPairType_Id, 2);
                        if(($ParamType_Pure{"Name"} eq $ParamStraightPairType_Pure{"Name"} or get_TypeName($ParamType1_Id, 1) eq get_TypeName($ParamStraightPairType_Id, 2))
                        and find_ParamPair_Pos_byName($Parameter_NewName, $Interface, 1) eq "lost")
                        {
                            if($Parameter_NewName!~/\Ap\d+\Z/ and $Parameter_Name!~/\Ap\d+\Z/)
                            {
                                %{$CompatProblems{$Interface}{"Renamed_Parameter"}{numToStr($ParamPos+1)." Parameter"}}=(
                                    "Target"=>$Parameter_Name,
                                    "Param_Pos"=>$ParamPos,
                                    "Param_Type"=>get_TypeName($ParamType1_Id, 1),
                                    "Old_Value"=>$Parameter_Name,
                                    "New_Value"=>$Parameter_NewName,
                                    "New_Signature"=>get_Signature($Interface, 2)  );
                            }
                        }
                        else
                        {
                            my $ProblemType = "Removed_Middle_Parameter";
                            if($Parameter_Name=~/\Ap\d+\Z/) {
                                $ProblemType = "Removed_Middle_Unnamed_Parameter";
                            }
                            %{$CompatProblems{$Interface}{$ProblemType}{numToStr($ParamPos+1)." Parameter"}}=(
                                "Target"=>$Parameter_Name,
                                "Param_Pos"=>$ParamPos,
                                "Param_Type"=>get_TypeName($ParamType1_Id, 1),
                                "New_Signature"=>get_Signature($Interface, 2)  );
                        }
                    }
                }
            }
        }
        # checking return type
        my $ReturnType1_Id = $CompleteSignature{1}{$Interface}{"Return"};
        my $ReturnType2_Id = $CompleteSignature{2}{$Interface}{"Return"};
        %SubProblems = detectTypeChange($ReturnType1_Id, $ReturnType2_Id, "Return");
        foreach my $SubProblemType (keys(%SubProblems))
        {
            my $New_Value = $SubProblems{$SubProblemType}{"New_Value"};
            my $Old_Value = $SubProblems{$SubProblemType}{"Old_Value"};
            if($SubProblemType eq "Return_Type_Became_Void"
            and keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
            { # parameters stack has been affected
                %{$SubProblems{"Return_Type_Became_Void_And_Stack_Layout"}} = %{$SubProblems{$SubProblemType}};
                delete($SubProblems{$SubProblemType});
                $SubProblemType = "Return_Type_Became_Void_And_Stack_Layout";
            }
            elsif($SubProblemType eq "Return_Type_From_Void")
            { # parameters stack has been affected
                if(keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
                {
                    %{$SubProblems{"Return_Type_From_Void_And_Stack_Layout"}} = %{$SubProblems{$SubProblemType}};
                    delete($SubProblems{$SubProblemType});
                    $SubProblemType = "Return_Type_From_Void_And_Stack_Layout";
                }
                else
                { # safe
                    delete($SubProblems{$SubProblemType});
                }
            }
            elsif($SubProblemType eq "Return_Type_And_Size"
            and $CompleteSignature{1}{$Interface}{"Data"})
            {
                %{$SubProblems{"Global_Data_Type_And_Size"}} = %{$SubProblems{$SubProblemType}};
                delete($SubProblems{$SubProblemType});
                $SubProblemType = "Global_Data_Type_And_Size";
            }
            elsif($SubProblemType eq "Return_Type"
            and $CompleteSignature{1}{$Interface}{"Data"})
            {
                if($Old_Value=~/\A\Q$New_Value\E const\Z/)
                { # const -> non-const global data
                    %{$SubProblems{"Global_Data_Became_Non_Const"}} = %{$SubProblems{$SubProblemType}};
                    delete($SubProblems{$SubProblemType});
                    $SubProblemType = "Global_Data_Became_Non_Const";
                }
                else
                {
                    %{$SubProblems{"Global_Data_Type"}} = %{$SubProblems{$SubProblemType}};
                    delete($SubProblems{$SubProblemType});
                    $SubProblemType = "Global_Data_Type";
                }
            }
            @{$CompatProblems{$Interface}{$SubProblemType}{"RetVal"}}{keys(%{$SubProblems{$SubProblemType}})} = values %{$SubProblems{$SubProblemType}};
        }
        if($ReturnType1_Id and $ReturnType2_Id)
        {
            @RecurTypes = ();
            %SubProblems = mergeTypes($ReturnType1_Id, $Tid_TDid{1}{$ReturnType1_Id},
                                      $ReturnType2_Id, $Tid_TDid{2}{$ReturnType2_Id});
            foreach my $SubProblemType (keys(%SubProblems))
            { # add "Global_Data_Size" problem
                if($SubProblemType eq "DataType_Size"
                and $CompleteSignature{1}{$Interface}{"Data"}
                and get_PointerLevel($Tid_TDid{1}{$ReturnType1_Id}, $ReturnType1_Id, 1)==0) {
                    %{$SubProblems{"Global_Data_Size"}} = %{$SubProblems{$SubProblemType}};
                }
            }
            foreach my $SubProblemType (keys(%SubProblems))
            {
                foreach my $SubLocation (keys(%{$SubProblems{$SubProblemType}}))
                {
                    my $NewLocation = ($SubLocation)?"RetVal->".$SubLocation:"RetVal";
                    %{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}=(
                        "Return_Type_Name"=>get_TypeName($ReturnType1_Id, 1) );
                    @{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}{keys(%{$SubProblems{$SubProblemType}{$SubLocation}})} = values %{$SubProblems{$SubProblemType}{$SubLocation}};
                    if($SubLocation!~/\-\>/) {
                        $CompatProblems{$Interface}{$SubProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($ReturnType1_Id, 1);
                    }
                }
            }
        }
        
        # checking object type
        my $ObjectType1_Id = $CompleteSignature{1}{$Interface}{"Class"};
        my $ObjectType2_Id = $CompleteSignature{2}{$Interface}{"Class"};
        if($ObjectType1_Id and $ObjectType2_Id
        and not $CompleteSignature{1}{$Interface}{"Static"})
        {
            my $ThisPtr1_Id = getTypeIdByName(get_TypeName($ObjectType1_Id, 1)."*const", 1);
            my $ThisPtr2_Id = getTypeIdByName(get_TypeName($ObjectType2_Id, 2)."*const", 2);
            if($ThisPtr1_Id and $ThisPtr2_Id)
            {
                @RecurTypes = ();
                %SubProblems = mergeTypes($ThisPtr1_Id, $Tid_TDid{1}{$ThisPtr1_Id},
                                          $ThisPtr2_Id, $Tid_TDid{2}{$ThisPtr2_Id});
                foreach my $SubProblemType (keys(%SubProblems))
                {
                    foreach my $SubLocation (keys(%{$SubProblems{$SubProblemType}}))
                    {
                        my $NewLocation = ($SubLocation)?"this->".$SubLocation:"this";
                        %{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}=(
                            "Object_Type_Name"=>get_TypeName($ObjectType1_Id, 1) );
                        @{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}{keys(%{$SubProblems{$SubProblemType}{$SubLocation}})} = values %{$SubProblems{$SubProblemType}{$SubLocation}};
                        if($SubLocation!~/\-\>/) {
                            $CompatProblems{$Interface}{$SubProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($ObjectType1_Id, 1);
                        }
                    }
                }
            }
        }
    }
}

sub mergeDefaultParams($$$)
{
    my ($Interface, $ParamPos1, $ParamPos2) = @_;
    if($UsedDump{1}
    and cmpVersions($UsedDump{1}, "2.0")<0)
    {# support for old dumps
        # "default" attribute added in ACC 1.22 (dump 2.0 format)
        return;
    }
    if($UsedDump{2}
    and cmpVersions($UsedDump{2}, "2.0")<0)
    {# support for old dumps
        # "default" attribute added in ACC 1.22 (dump 2.0 format)
        return;
    }
    my $ParamType1_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos1}{"type"};
    my $Parameter_Name = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos1}{"name"};
    my $ParamType2_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos2}{"type"};
    return if(not $ParamType1_Id or not $ParamType2_Id);
    my %PType1 = get_PureType($Tid_TDid{1}{$ParamType1_Id}, $ParamType1_Id, 1);
    my $Parameter_Location = ($Parameter_Name)?$Parameter_Name:numToStr($ParamPos1+1)." Parameter";
    my $DefaultValue_Old = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos1}{"default"};
    my $DefaultValue_New = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos2}{"default"};
    if($PType1{"Name"}=~/\A(char\*|char const\*)\Z/)
    {
        if($DefaultValue_Old)
        {# FIXME: how to distinguish "0" and 0 (NULL)
            $DefaultValue_Old = "\"$DefaultValue_Old\"";
        }
        if($DefaultValue_New) {
            $DefaultValue_New = "\"$DefaultValue_New\"";
        }
    }
    elsif($PType1{"Name"}=~/\A(char)\Z/)
    {
        if($DefaultValue_Old) {
            $DefaultValue_Old = "\'$DefaultValue_Old\'";
        }
        if($DefaultValue_New) {
            $DefaultValue_New = "\'$DefaultValue_New\'";
        }
    }
    return if($DefaultValue_Old eq "");
    if($DefaultValue_New ne "")
    {
        if($DefaultValue_Old ne $DefaultValue_New)
        {
            %{$CompatProblems{$Interface}{"Parameter_Default_Value_Changed"}{$Parameter_Location}}=(
                "Target"=>$Parameter_Name,
                "Param_Pos"=>$ParamPos1,
                "Old_Value"=>$DefaultValue_Old,
                "New_Value"=>$DefaultValue_New  );
        }
    }
    else
    {
        %{$CompatProblems{$Interface}{"Parameter_Default_Value_Removed"}{$Parameter_Location}}=(
                "Target"=>$Parameter_Name,
                "Param_Pos"=>$ParamPos1,
                "Old_Value"=>$DefaultValue_Old  );
    }
}

sub mergeParameters($$$)
{
    my ($Interface, $ParamPos1, $ParamPos2) = @_;
    return if(not $Interface);
    return if(not defined $CompleteSignature{1}{$Interface}{"Param"});
    return if(not defined $CompleteSignature{2}{$Interface}{"Param"});
    my $ParamType1_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos1}{"type"};
    my $Parameter_Name = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos1}{"name"};
    my $ParamType2_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos2}{"type"};
    return if(not $ParamType1_Id or not $ParamType2_Id);
    mergeDefaultParams($Interface, $ParamPos1, $ParamPos2);
    my $Parameter_Location = ($Parameter_Name)?$Parameter_Name:numToStr($ParamPos1+1)." Parameter";
    if($Interface!~/\A(_Z|\?)/)
    {
        # checking type change (replace)
        my %SubProblems = detectTypeChange($ParamType1_Id, $ParamType2_Id, "Parameter");
        foreach my $SubProblemType (keys(%SubProblems))
        {
            my $New_Value = $SubProblems{$SubProblemType}{"New_Value"};
            my $Old_Value = $SubProblems{$SubProblemType}{"Old_Value"};
            my $Old_Value_Non_Const = $Old_Value;
            if($SubProblemType eq "Parameter_Type"
            and $Old_Value_Non_Const=~s/\Wconst(\W|\Z)/$1$2/g)
            {
                $Old_Value_Non_Const = correctName($Old_Value_Non_Const);
                if($Old_Value=~/\A\Q$New_Value\E const\Z/ or $Old_Value_Non_Const eq $New_Value)
                {# const -> non-const parameter
                    %{$SubProblems{"Parameter_Became_Non_Const"}} = %{$SubProblems{$SubProblemType}};
                    delete($SubProblems{$SubProblemType});
                    $SubProblemType = "Parameter_Became_Non_Const";
                }
            }
            elsif($SubProblemType eq "Parameter_Type_And_Size"
            or $SubProblemType eq "Parameter_Type")
            {
                my ($Arch1, $Arch2) = (getArch(1), getArch(2));
                if($Arch1 eq "unknown" or $Arch2 eq "unknown")
                { # if one of the architectures is unknown
                  # then set other arhitecture to unknown too
                    ($Arch1, $Arch2) = ("unknown", "unknown");
                }
                my ($Method1, $Passed1, $SizeOnStack1, $RegName1) = callingConvention($Interface, $ParamPos1, 1, $Arch1);
                my ($Method2, $Passed2, $SizeOnStack2, $RegName2) = callingConvention($Interface, $ParamPos2, 2, $Arch2);
                my $NewProblemType = $SubProblemType;
                if($Method1 eq $Method2)
                {
                    if($Method1 eq "stack" and $SizeOnStack1 ne $SizeOnStack2) {
                        $NewProblemType = "Parameter_Type_And_Stack";
                    }
                    elsif($Method1 eq "register" and $RegName1 ne $RegName2) {
                        $NewProblemType = "Parameter_Type_And_Register";
                    }
                }
                else
                {
                    if($Method1 eq "stack") {
                        $NewProblemType = "Parameter_Type_And_Pass_Through_Register";
                    }
                    elsif($Method1 eq "register") {
                        $NewProblemType = "Parameter_Type_And_Pass_Through_Stack";
                    }
                }
                if($NewProblemType ne $SubProblemType)
                {
                    %{$SubProblems{$NewProblemType}} = %{$SubProblems{$SubProblemType}};
                    delete($SubProblems{$SubProblemType});
                    $SubProblemType = $NewProblemType;
                    # add attributes
                    $SubProblems{$NewProblemType}{"Old_Reg"} = $RegName1;
                    $SubProblems{$NewProblemType}{"New_Reg"} = $RegName2;
                }
            }
            %{$CompatProblems{$Interface}{$SubProblemType}{$Parameter_Location}}=(
                "Target"=>$Parameter_Name,
                "Param_Pos"=>$ParamPos1  );
            @{$CompatProblems{$Interface}{$SubProblemType}{$Parameter_Location}}{keys(%{$SubProblems{$SubProblemType}})} = values %{$SubProblems{$SubProblemType}};
        }
    }
    @RecurTypes = ();
    # checking type definition changes
    my %SubProblems_Merge = mergeTypes($ParamType1_Id, $Tid_TDid{1}{$ParamType1_Id}, $ParamType2_Id, $Tid_TDid{2}{$ParamType2_Id});
    foreach my $SubProblemType (keys(%SubProblems_Merge))
    {
        foreach my $SubLocation (keys(%{$SubProblems_Merge{$SubProblemType}}))
        {
            my $NewProblemType = $SubProblemType;
            if($SubProblemType eq "DataType_Size")
            {
                my $InitialType_Type = $SubProblems_Merge{$SubProblemType}{$SubLocation}{"InitialType_Type"};
                if($InitialType_Type!~/\A(Pointer|Ref)\Z/ and $SubLocation!~/\-\>/)
                {# stack has been affected
                    $NewProblemType = "DataType_Size_And_Stack";
                }
            }
            my $NewLocation = ($SubLocation)?$Parameter_Location."->".$SubLocation:$Parameter_Location;
            %{$CompatProblems{$Interface}{$NewProblemType}{$NewLocation}}=(
                "Param_Type"=>get_TypeName($ParamType1_Id, 1),
                "Param_Pos"=>$ParamPos1,
                "Param_Name"=>$Parameter_Name  );
            @{$CompatProblems{$Interface}{$NewProblemType}{$NewLocation}}{keys(%{$SubProblems_Merge{$SubProblemType}{$SubLocation}})} = values %{$SubProblems_Merge{$SubProblemType}{$SubLocation}};
            if($SubLocation!~/\-\>/) {
                $CompatProblems{$Interface}{$NewProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($ParamType1_Id, 1);
            }
        }
    }
}

sub callingConvention($$$$)
{ # calling conventions for different compilers and operating systems
    my ($Interface, $ParamPos, $LibVersion, $Arch) = @_;
    my $ParamTypeId = $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$ParamPos}{"type"};
    my %Type = get_PureType($Tid_TDid{$LibVersion}{$ParamTypeId}, $ParamTypeId, $LibVersion);
    my ($Method, $Alignment, $Passed, $Register) = ("", 0, "", "");
    if($OSgroup=~/\A(linux|macos|freebsd)\Z/)
    {# GCC
        if($Arch eq "x86")
        { # System V ABI Intel386 ("Function Calling Sequence")
          # The stack is word aligned. Although the architecture does not require any
          # alignment of the stack, software convention and the operating system
          # requires that the stack be aligned on a word boundary.

          # Argument words are pushed onto the stack in reverse order (that is, the
          # rightmost argument in C call syntax has the highest address), preserving the
          # stack’s word alignment. All incoming arguments appear on the stack, residing
          # in the stack frame of the caller.

          # An argument’s size is increased, if necessary, to make it a multiple of words.
          # This may require tail padding, depending on the size of the argument.

          # Other areas depend on the compiler and the code being compiled. The stan-
          # dard calling sequence does not define a maximum stack frame size, nor does
          # it restrict how a language system uses the ‘‘unspecified’’ area of the stan-
          # dard stack frame.
            ($Method, $Alignment) = ("stack", 4);
        }
        elsif($Arch eq "x86_64")
        { # System V AMD64 ABI ("Function Calling Sequence")
            ($Method, $Alignment) = ("stack", 8);# eightbyte aligned
        }
        elsif($Arch eq "arm")
        { # Procedure Call Standard for the ARM Architecture
          # The stack must be double-word aligned
            ($Method, $Alignment) = ("stack", 8);# double-word
        }
    }
    elsif($OSgroup eq "windows")
    {# MS C++ Compiler
        if($Arch eq "x86")
        {
            if($ParamPos==0) {
                ($Method, $Register, $Passed) = ("register", "ecx", "value");
            }
            elsif($ParamPos==1) {
                ($Method, $Register, $Passed) = ("register", "edx", "value");
            }
            else {
                ($Method, $Alignment) = ("stack", 4);
            }
        }
        elsif($Arch eq "x86_64")
        {
            if($ParamPos<=3) {
                if($Type{"Name"}=~/\A(float|double|long double)\Z/) {
                    ($Method, $Passed) = ("xmm".$ParamPos, "value");
                }
                elsif($Type{"Name"}=~/\A(unsigned |)(short|int|long|long long)\Z/
                or $Type{"Type"}=~/\A(Struct|Union|Enum|Array)\Z/
                or $Type{"Name"}=~/\A(__m64|__m128)\Z/) {
                    if($ParamPos==0) {
                        ($Method, $Register, $Passed) = ("register", "rcx", "value");
                    }
                    elsif($ParamPos==1) {
                        ($Method, $Register, $Passed) = ("register", "rdx", "value");
                    }
                    elsif($ParamPos==2) {
                        ($Method, $Register, $Passed) = ("register", "r8", "value");
                    }
                    elsif($ParamPos==3) {
                        ($Method, $Register, $Passed) = ("register", "r9", "value");
                    }
                    if($Type{"Size"}>64
                    or $Type{"Type"} eq "Array") {
                        $Passed = "pointer";
                    }
                }
            }
            else {
                ($Method, $Alignment) = ("stack", 8);# word alignment
            }
        }
    }
    if($Method eq "register") {
        return ("register", $Passed, "", $Register);
    }
    else
    {# on the stack
        if(not $Alignment)
        {# default convention
            $Alignment = $WORD_SIZE{$LibVersion};
        }
        if(not $Passed)
        {# default convention
            $Passed = "value";
        }
        return ("stack", $Passed, (int(($Type{"Size"}+$Alignment)/$Alignment)*$Alignment), "");
    }
}

sub find_ParamPair_Pos_byName($$$)
{
    my ($Name, $Interface, $LibVersion) = @_;
    foreach my $ParamPos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$LibVersion}{$Interface}{"Param"}}))
    {
        next if(not defined $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$ParamPos});
        if($CompleteSignature{$LibVersion}{$Interface}{"Param"}{$ParamPos}{"name"} eq $Name)
        {
            return $ParamPos;
        }
    }
    return "lost";
}

sub find_ParamPair_Pos_byTypeAndPos($$$$$)
{
    my ($TypeName, $MediumPos, $Order, $Interface, $LibVersion) = @_;
    my @Positions = ();
    foreach my $ParamPos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$LibVersion}{$Interface}{"Param"}}))
    {
        next if($Order eq "backward" and $ParamPos>$MediumPos);
        next if($Order eq "forward" and $ParamPos<$MediumPos);
        next if(not defined $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$ParamPos});
        my $PTypeId = $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$ParamPos}{"type"};
        if(get_TypeName($PTypeId, $LibVersion) eq $TypeName) {
            push(@Positions, $ParamPos);
        }
    }
    return @Positions;
}

sub getTypeIdByName($$)
{
    my ($TypeName, $Version) = @_;
    return $TName_Tid{$Version}{correctName($TypeName)};
}

sub checkFormatChange($$)
{
    my ($Type1_Id, $Type2_Id) = @_;
    my $Type1_DId = $Tid_TDid{1}{$Type1_Id};
    my $Type2_DId = $Tid_TDid{2}{$Type2_Id};
    my %Type1_Pure = get_PureType($Type1_DId, $Type1_Id, 1);
    my %Type2_Pure = get_PureType($Type2_DId, $Type2_Id, 2);
    if($Type1_Pure{"Name"} eq $Type2_Pure{"Name"})
    {# equal types
        return 0;
    }
    if($Type1_Pure{"Name"}=~/\*/
    or $Type2_Pure{"Name"}=~/\*/)
    { # compared in detectTypeChange()
        return 0;
    }
    my %FloatType = map {$_=>1} (
        "float",
        "double",
        "long double"
    );
    if($Type1_Pure{"Type"} ne $Type2_Pure{"Type"})
    {# different types
        if($Type1_Pure{"Type"} eq "Intrinsic"
        and $Type2_Pure{"Type"} eq "Enum")
        { # int => enum
            return 0;
        }
        elsif($Type2_Pure{"Type"} eq "Intrinsic"
        and $Type1_Pure{"Type"} eq "Enum")
        { # enum => int
            return 0;
        }
        else {
            return 1;
        }
    }
    else
    {
        if($Type1_Pure{"Type"} eq "Intrinsic")
        {
            if($FloatType{$Type1_Pure{"Name"}}
            or $FloatType{$Type2_Pure{"Name"}})
            { # float <=> double
              # float <=> int
                return 1;
            }
        }
        elsif($Type1_Pure{"Type"}=~/Class|Struct|Union|Enum/)
        {
            my @Membs1 = keys(%{$Type1_Pure{"Memb"}});
            my @Membs2 = keys(%{$Type2_Pure{"Memb"}});
            if($#Membs1!=$#Membs2)
            { # different number of elements
                return 1;
            }
            if($Type1_Pure{"Type"} eq "Enum")
            {
                foreach my $Pos (@Membs1)
                { # compare elements by name and value
                    if($Type1_Pure{"Memb"}{$Pos}{"name"} ne $Type2_Pure{"Memb"}{$Pos}{"name"}
                    or $Type1_Pure{"Memb"}{$Pos}{"value"} ne $Type2_Pure{"Memb"}{$Pos}{"value"})
                    { # different names
                        return 1;
                    }
                }
            }
            else
            {
                foreach my $Pos (@Membs1)
                { # compare elements by type name
                    my $MT1 = get_TypeName($Type1_Pure{"Memb"}{$Pos}{"type"}, 1);
                    my $MT2 = get_TypeName($Type2_Pure{"Memb"}{$Pos}{"type"}, 2);
                    if($MT1 ne $MT2)
                    { # different types
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

sub detectTypeChange($$$)
{
    my ($Type1_Id, $Type2_Id, $Prefix) = @_;
    my %LocalProblems = ();
    my $Type1_DId = $Tid_TDid{1}{$Type1_Id};
    my $Type2_DId = $Tid_TDid{2}{$Type2_Id};
    my %Type1 = get_Type($Type1_DId, $Type1_Id, 1);
    my %Type2 = get_Type($Type2_DId, $Type2_Id, 2);
    my %Type1_Pure = get_PureType($Type1_DId, $Type1_Id, 1);
    my %Type2_Pure = get_PureType($Type2_DId, $Type2_Id, 2);
    my %Type1_Base = ($Type1_Pure{"Type"} eq "Array")?get_OneStep_BaseType($Tid_TDid{1}{$Type1_Pure{"Tid"}}, $Type1_Pure{"Tid"}, 1):get_BaseType($Type1_DId, $Type1_Id, 1);
    my %Type2_Base = ($Type2_Pure{"Type"} eq "Array")?get_OneStep_BaseType($Tid_TDid{2}{$Type2_Pure{"Tid"}}, $Type2_Pure{"Tid"}, 2):get_BaseType($Type2_DId, $Type2_Id, 2);
    my $Type1_PLevel = get_PointerLevel($Type1_DId, $Type1_Id, 1);
    my $Type2_PLevel = get_PointerLevel($Type2_DId, $Type2_Id, 2);
    return () if(not $Type1{"Name"} or not $Type2{"Name"});
    return () if(not $Type1_Base{"Name"} or not $Type2_Base{"Name"});
    return () if($Type1_PLevel eq "" or $Type2_PLevel eq "");
    if($Type1_Base{"Name"} ne $Type2_Base{"Name"}
    and ($Type1{"Name"} eq $Type2{"Name"} or ($Type1_PLevel>=1 and $Type1_PLevel==$Type2_PLevel
    and $Type1_Base{"Name"} ne "void" and $Type2_Base{"Name"} ne "void")))
    { # base type change
        if($Type1{"Type"} eq "Typedef" and $Type2{"Type"} eq "Typedef"
        and $Type1{"Name"} eq $Type2{"Name"})
        { # will be reported in mergeTypes() as typedef problem
            return ();
        }
        if($Type1_Base{"Name"}!~/anon\-/ and $Type2_Base{"Name"}!~/anon\-/)
        {
            if($Type1_Base{"Size"} ne $Type2_Base{"Size"}
            and $Type1_Base{"Size"} and $Type2_Base{"Size"})
            {
                %{$LocalProblems{$Prefix."_BaseType_And_Size"}}=(
                    "Old_Value"=>$Type1_Base{"Name"},
                    "New_Value"=>$Type2_Base{"Name"},
                    "Old_Size"=>$Type1_Base{"Size"}*$BYTE_SIZE,
                    "New_Size"=>$Type2_Base{"Size"}*$BYTE_SIZE,
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            else
            {
                if(checkFormatChange($Type1_Base{"Tid"}, $Type2_Base{"Tid"}))
                { # format change
                    %{$LocalProblems{$Prefix."_BaseType_Format"}}=(
                        "Old_Value"=>$Type1_Base{"Name"},
                        "New_Value"=>$Type2_Base{"Name"},
                        "InitialType_Type"=>$Type1_Pure{"Type"});
                }
                elsif(tNameLock($Type1_Base{"Tid"}, $Type2_Base{"Tid"}))
                {
                    %{$LocalProblems{$Prefix."_BaseType"}}=(
                        "Old_Value"=>$Type1_Base{"Name"},
                        "New_Value"=>$Type2_Base{"Name"},
                        "InitialType_Type"=>$Type1_Pure{"Type"});
                }
            }
        }
    }
    elsif($Type1{"Name"} ne $Type2{"Name"})
    { # type change
        if($Type1{"Name"}!~/anon\-/ and $Type2{"Name"}!~/anon\-/)
        {
            if($Prefix eq "Return" and $Type1{"Name"} eq "void"
            and $Type2_Pure{"Type"}=~/Intrinsic|Enum/) {
                # safe change
            }
            elsif($Prefix eq "Return"
            and $Type1_Pure{"Name"} eq "void")
            {
                %{$LocalProblems{"Return_Type_From_Void"}}=(
                    "New_Value"=>$Type2{"Name"},
                    "New_Size"=>$Type2{"Size"}*$BYTE_SIZE,
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            elsif($Prefix eq "Return" and $Type1_Pure{"Type"}=~/Intrinsic|Enum/
            and $Type2_Pure{"Type"}=~/Struct|Class|Union/)
            { # returns into hidden first parameter instead of a register
                
                # System V ABI Intel386 ("Function Calling Sequence")
                # A function that returns an integral or pointer value places its result in register %eax.

                # A floating-point return value appears on the top of the Intel387 register stack. The
                # caller then must remove the value from the Intel387 stack, even if it doesn’t use the
                # value.

                # If a function returns a structure or union, then the caller provides space for the
                # return value and places its address on the stack as argument word zero. In effect,
                # this address becomes a ‘‘hidden’’ first argument.
                
                %{$LocalProblems{"Return_Type_From_Register_To_Stack"}}=(
                    "Old_Value"=>$Type1{"Name"},
                    "New_Value"=>$Type2{"Name"},
                    "Old_Size"=>$Type1{"Size"}*$BYTE_SIZE,
                    "New_Size"=>$Type2{"Size"}*$BYTE_SIZE,
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            elsif($Prefix eq "Return"
            and $Type2_Pure{"Name"} eq "void")
            {
                %{$LocalProblems{"Return_Type_Became_Void"}}=(
                    "Old_Value"=>$Type1{"Name"},
                    "Old_Size"=>$Type1{"Size"}*$BYTE_SIZE,
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            elsif($Prefix eq "Return" and $Type2_Pure{"Type"}=~/Intrinsic|Enum/
            and $Type1_Pure{"Type"}=~/Struct|Class|Union/)
            { # returns in a register instead of a hidden first parameter
                %{$LocalProblems{"Return_Type_From_Stack_To_Register"}}=(
                    "Old_Value"=>$Type1{"Name"},
                    "New_Value"=>$Type2{"Name"},
                    "Old_Size"=>$Type1{"Size"}*$BYTE_SIZE,
                    "New_Size"=>$Type2{"Size"}*$BYTE_SIZE,
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            else
            {
                if($Type1{"Size"} ne $Type2{"Size"}
                and $Type1{"Size"} and $Type2{"Size"})
                {
                    %{$LocalProblems{$Prefix."_Type_And_Size"}}=(
                        "Old_Value"=>$Type1{"Name"},
                        "New_Value"=>$Type2{"Name"},
                        "Old_Size"=>(($Type1{"Type"} eq "Array")?$Type1{"Size"}*$Type1_Base{"Size"}:$Type1{"Size"})*$BYTE_SIZE,
                        "New_Size"=>(($Type2{"Type"} eq "Array")?$Type2{"Size"}*$Type2_Base{"Size"}:$Type2{"Size"})*$BYTE_SIZE,
                        "InitialType_Type"=>$Type1_Pure{"Type"});
                }
                else
                {
                    if(checkFormatChange($Type1_Id, $Type2_Id))
                    { # format change
                        %{$LocalProblems{$Prefix."_Type_Format"}}=(
                            "Old_Value"=>$Type1{"Name"},
                            "New_Value"=>$Type2{"Name"},
                            "InitialType_Type"=>$Type1_Pure{"Type"});
                    }
                    elsif(tNameLock($Type1_Id, $Type2_Id))
                    { # FIXME: correct this condition
                        %{$LocalProblems{$Prefix."_Type"}}=(
                            "Old_Value"=>$Type1{"Name"},
                            "New_Value"=>$Type2{"Name"},
                            "InitialType_Type"=>$Type1_Pure{"Type"});
                    }
                }
            }
        }
    }
    if($Type1_PLevel!=$Type2_PLevel)
    {
        if($Type1{"Name"} ne "void"
        and $Type2{"Name"} ne "void")
        {
            if($Type2_PLevel>$Type1_PLevel) {
                %{$LocalProblems{$Prefix."_PointerLevel_Increased"}}=(
                    "Old_Value"=>$Type1_PLevel,
                    "New_Value"=>$Type2_PLevel);
            }
            else {
                %{$LocalProblems{$Prefix."_PointerLevel_Decreased"}}=(
                    "Old_Value"=>$Type1_PLevel,
                    "New_Value"=>$Type2_PLevel);
            }
        }
    }
    if($Type1_Pure{"Type"} eq "Array")
    { # base_type[N] -> base_type[N]
      # base_type: older_structure -> typedef to newer_structure
        my %SubProblems = detectTypeChange($Type1_Base{"Tid"}, $Type2_Base{"Tid"}, $Prefix);
        foreach my $SubProblemType (keys(%SubProblems))
        {
            $SubProblemType=~s/_Type/_BaseType/g;
            next if(defined $LocalProblems{$SubProblemType});
            foreach my $Attr (keys(%{$SubProblems{$SubProblemType}})) {
                $LocalProblems{$SubProblemType}{$Attr} = $SubProblems{$SubProblemType}{$Attr};
            }
        }
    }
    return %LocalProblems;
}

sub tNameLock($$)
{
    my ($Tid1, $Tid2) = @_;
    return 1 if(not differentFmts());
    if($UseOldDumps)
    { # old dumps
        return 0;
    }
    my $T1 = get_TypeType($Tid1, 1);
    my $T2 = get_TypeType($Tid2, 2);
    my %Base1 = get_Type($Tid_TDid{1}{$Tid1}, $Tid1, 1);
    while($Base1{"Type"} eq "Typedef") {
        %Base1 = get_OneStep_BaseType($Base1{"TDid"}, $Base1{"Tid"}, 1);
    }
    my %Base2 = get_Type($Tid_TDid{2}{$Tid2}, $Tid2, 2);
    while($Base2{"Type"} eq "Typedef") {
        %Base2 = get_OneStep_BaseType($Base2{"TDid"}, $Base2{"Tid"}, 2);
    }
    if($T1 ne $T2
    and ($T1 eq "Typedef" or $T2 eq "Typedef")
    and ($Base1{"Name"} eq $Base2{"Name"})) {
        return 0;
    }
    return 1;
}

sub differentFmts()
{
    if(getGccVersion(1) ne getGccVersion(2))
    {# different GCC versions
        return 1;
    }
    if(cmpVersions(formatVersion($UsedDump{1}, 2),
    formatVersion($UsedDump{2}, 2))!=0)
    {# different dump versions (skip micro version)
        return 1;
    }
    return 0;
}

sub formatVersion($$)
{ # cut off version digits
    my ($Version, $Digits) = @_;
    my @Elems = split(/\./, $Version);
    return join(".", splice(@Elems, 0, $Digits));
} 

sub htmlSpecChars($)
{
    my $Str = $_[0];
    $Str=~s/\&([^#]|\Z)/&amp;$1/g;
    $Str=~s/</&lt;/g;
    $Str=~s/\-\>/&minus;&gt;/g;
    $Str=~s/>/&gt;/g;
    $Str=~s/([^ ])( )([^ ])/$1\@ALONE_SP\@$3/g;
    $Str=~s/ /&nbsp;/g;
    $Str=~s/\@ALONE_SP\@/ /g;
    $Str=~s/\n/<br\/>/g;
    return $Str;
}

sub black_name($)
{
    my $Name = $_[0];
    return "<span class='iname_b'>".highLight_Signature($Name)."</span>";
}

sub highLight_Signature($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 0, 0, 0);
}

sub highLight_Signature_Italic_Color($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 1, 1, 1);
}

sub separate_symbol($)
{
    my $Symbol = $_[0];
    my ($Name, $Spec, $Ver) = ($Symbol, "", "");
    if($Symbol=~/\A([^\@\$\?]+)([\@\$]+)([^\@\$]+)\Z/) {
        ($Name, $Spec, $Ver) = ($1, $2, $3);
    }
    return ($Name, $Spec, $Ver);
}

sub highLight_Signature_PPos_Italic($$$$$)
{
    my ($FullSignature, $Parameter_Position, $ItalicParams, $ColorParams, $ShowReturn) = @_;
    $ItalicParams=$ColorParams=0 if($CheckObjectsOnly);
    my ($Signature, $VersionSpec, $SymbolVersion) = separate_symbol($FullSignature);
    my $Return = "";
    if($ShowRetVal and $Signature=~s/([^:]):([^:].+?)\Z/$1/g) {
        $Return = $2;
    }
    my $SCenter = detect_center($Signature);
    if(not $SCenter)
    {# global data
        $Signature = htmlSpecChars($Signature);
        $Signature=~s!(\[data\])!<span style='color:Black;font-weight:normal;'>$1</span>!g;
        $Signature .= (($SymbolVersion)?"<span class='symver'>&nbsp;$VersionSpec&nbsp;$SymbolVersion</span>":"");
        if($Return and $ShowReturn) {
            $Signature .= "<span class='int_p nowrap'> &nbsp;<b>:</b>&nbsp;&nbsp;".htmlSpecChars($Return)."</span>";
        }
        return $Signature;
    }
    my ($Begin, $End) = (substr($Signature, 0, $SCenter), "");
    $Begin.=" " if($Begin!~/ \Z/);
    if($Signature=~/\)((| const)(| \[static\]))\Z/) {
        $End = $1;
    }
    my @Parts = ();
    my @SParts = get_signature_parts($Signature, 1);
    foreach my $Num (0 .. $#SParts)
    {
        my $Part = $SParts[$Num];
        $Part=~s/\A\s+|\s+\Z//g;
        my ($Part_Styled, $ParamName) = (htmlSpecChars($Part), "");
        if($Part=~/\([\*]+(\w+)\)/i) {
            $ParamName = $1;#func-ptr
        }
        elsif($Part=~/(\w+)[\,\)]*\Z/i) {
            $ParamName = $1;
        }
        if(not $ParamName) {
            push(@Parts, $Part_Styled);
            next;
        }
        if($ItalicParams and not $TName_Tid{1}{$Part}
        and not $TName_Tid{2}{$Part})
        {
            if($Parameter_Position ne ""
            and $Num==$Parameter_Position) {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span class='focus_p'>$ParamName</span>$2!ig;
            }
            elsif($ColorParams) {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span class='color_p'>$ParamName</span>$2!ig;
            }
            else {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span style='font-style:italic;'>$ParamName</span>$2!ig;
            }
        }
        $Part_Styled=~s/,(\w)/, $1/g;
        push(@Parts, $Part_Styled);
    }
    if(@Parts)
    {
        foreach my $Num (0 .. $#Parts)
        {
            if($Num==$#Parts)
            {# add ")" to the last parameter
                $Parts[$Num] = "<span class='nowrap'>".$Parts[$Num]." )</span>";
            }
            elsif(length($Parts[$Num])<=45) {
                $Parts[$Num] = "<span class='nowrap'>".$Parts[$Num]."</span>";
            }
        }
        $Signature = htmlSpecChars($Begin)."<span class='int_p'>(&nbsp;".join(" ", @Parts)."</span>".$End;
    }
    else {
        $Signature = htmlSpecChars($Begin)."<span class='int_p'>(&nbsp;)</span>".$End;
    }
    if($Return and $ShowReturn) {
        $Signature .= "<span class='int_p nowrap'> &nbsp;<b>:</b>&nbsp;&nbsp;".htmlSpecChars($Return)."</span>";
    }
    $Signature=~s!\[\]![<span style='padding-left:2px;'>]</span>!g;
    $Signature=~s!operator=!operator<span style='padding-left:2px'>=</span>!g;
    $Signature=~s!(\[in-charge\]|\[not-in-charge\]|\[in-charge-deleting\]|\[static\])!<span style='color:Black;font-weight:normal;'>$1</span>!g;
    return $Signature.(($SymbolVersion)?"<span class='symver'>&nbsp;$VersionSpec&nbsp;$SymbolVersion</span>":"");
}

sub get_signature_parts($$)
{
    my ($Signature, $Comma) = @_;
    my @Parts = ();
    my $Parameters = $Signature;
    my $ShortName = substr($Parameters, 0, detect_center($Parameters));
    $Parameters=~s/\A\Q$ShortName\E\(//g;
    $Parameters=~s/\)(| const)(| \[static\])\Z//g;
    return separate_params($Parameters, $Comma);
}

sub separate_params($$)
{
    my ($Params, $Comma) = @_;
    my @Parts = ();
    my ($Bracket_Num, $Bracket2_Num, $Part_Num) = (0, 0, 0);
    foreach my $Pos (0 .. length($Params) - 1)
    {
        my $Symbol = substr($Params, $Pos, 1);
        $Bracket_Num += 1 if($Symbol eq "(");
        $Bracket_Num -= 1 if($Symbol eq ")");
        $Bracket2_Num += 1 if($Symbol eq "<");
        $Bracket2_Num -= 1 if($Symbol eq ">");
        if($Symbol eq "," and $Bracket_Num==0 and $Bracket2_Num==0)
        {
            if($Comma) {# include comma
                $Parts[$Part_Num] .= $Symbol;
            }
            $Part_Num += 1;
        }
        else {
            $Parts[$Part_Num] .= $Symbol;
        }
    }
    return @Parts;
}

sub detect_center($)
{
    my $Signature = $_[0];
    my ($Bracket_Num, $Bracket2_Num) = (0, 0);
    my $Center = 0;
    if($Signature=~s/(operator([<>\-\=\*]+|\(\)))//g)
    {# operators: (),->,->*,<,<=,<<,<<=,>,>=,>>,>>=
        $Center+=length($1);
    }
    foreach my $Pos (0 .. length($Signature)-1)
    {
        my $Symbol = substr($Signature, $Pos, 1);
        $Bracket_Num += 1 if($Symbol eq "(");
        $Bracket_Num -= 1 if($Symbol eq ")");
        $Bracket2_Num += 1 if($Symbol eq "<");
        $Bracket2_Num -= 1 if($Symbol eq ">");
        if($Symbol eq "(" and
        $Bracket_Num==1 and $Bracket2_Num==0) {
            return $Center;
        }
        $Center+=1;
    }
    return 0;
}

sub testSystemCpp()
{
    print "verifying detectable C++ library changes\n";
    my ($HEADER1, $SOURCE1, $HEADER2, $SOURCE2) = ();
    my $DECL_SPEC = ($OSgroup eq "windows")?"__declspec( dllexport )":"";
    my $EXTERN = ($OSgroup eq "windows")?"extern ":"";# add "extern" for CL compiler
    
    # Begin namespace
    $HEADER1 .= "namespace TestNS {\n";
    $HEADER2 .= "namespace TestNS {\n";
    $SOURCE1 .= "namespace TestNS {\n";
    $SOURCE2 .= "namespace TestNS {\n";
    # Removed_Virtual_Method (inline)
    $HEADER1 .= "
        class $DECL_SPEC RemovedInlineVirtualFunction {
        public:
            virtual int removedMethod(int param) { return 0; }
            virtual int method(int param);
    };";
    $SOURCE1 .= "
        int RemovedInlineVirtualFunction::method(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC RemovedInlineVirtualFunction {
        public:
            virtual int method(int param);
        };";
    $SOURCE2 .= "
        int RemovedInlineVirtualFunction::method(int param) { return param; }";
    
    # MethodPtr
    $HEADER1 .= "
        class TestMethodPtr {
            public:
                typedef void (TestMethodPtr::*Method)(int*);
                Method _method;
                TestMethodPtr();
                void method();
        };";
    $SOURCE1 .= "
        TestMethodPtr::TestMethodPtr() { }
        void TestMethodPtr::method() { }";
    
    $HEADER2 .= "
        class TestMethodPtr {
            public:
                typedef void (TestMethodPtr::*Method)(int*, void*);
                Method _method;
                TestMethodPtr();
                void method();
        };";
    $SOURCE2 .= "
        TestMethodPtr::TestMethodPtr() { }
        void TestMethodPtr::method() { }";
    
    # FieldPtr
    $HEADER1 .= "
        class TestFieldPtr {
            public:
                typedef void* (TestFieldPtr::*Field);
                Field _field;
                TestFieldPtr();
                void method(void*);
        };";
    $SOURCE1 .= "
        TestFieldPtr::TestFieldPtr(){ }
        void TestFieldPtr::method(void*) { }";
    
    $HEADER2 .= "
        class TestFieldPtr {
            public:
                typedef int (TestFieldPtr::*Field);
                Field _field;
                TestFieldPtr();
                void method(void*);
        };";
    $SOURCE2 .= "
        TestFieldPtr::TestFieldPtr(){ }
        void TestFieldPtr::method(void*) { }";

    # Removed_Symbol (Template Specializations)
    $HEADER1 .= "
        template <unsigned int _TP, typename AAA>
        class Template {
            public:
                char const *field;
        };
        template <unsigned int _TP, typename AAA>
        class TestRemovedTemplate {
            public:
                char const *field;
                void method(int);
        };
        template <>
        class TestRemovedTemplate<7, char> {
            public:
                char const *field;
                void method(int);
        };";
    $SOURCE1 .= "
        void TestRemovedTemplate<7, char>::method(int){ }";

    # Removed_Symbol (Template Specializations)
    $HEADER1 .= "
        template <typename TName>
        int removedTemplateSpec(TName);

        template <> int removedTemplateSpec<char>(char);";
    $SOURCE1 .= "
        template <> int removedTemplateSpec<char>(char){return 0;}";
    
    # Global_Data_Value (int)
    $HEADER1 .= "
        $EXTERN $DECL_SPEC const int globalDataValue = 10;";
    
    $HEADER2 .= "
        $EXTERN $DECL_SPEC const int globalDataValue = 15;";
    
    # Global_Data_Became_Non_Const (int)
    $HEADER1 .= "
        $EXTERN $DECL_SPEC const int globalDataBecameNonConst = 10;";
    
    $HEADER2 .= "
        extern $DECL_SPEC int globalDataBecameNonConst;";
    $SOURCE2 .= "
        int globalDataBecameNonConst = 15;";

    # Global_Data_Became_Const (safe)
    $HEADER1 .= "
        extern $DECL_SPEC int globalDataBecameConst;";
    $SOURCE1 .= "
        int globalDataBecameConst = 10;";
    
    $HEADER2 .= "
        $EXTERN $DECL_SPEC const int globalDataBecameConst=15;";
    
    # Removed_Field (Ref)
    $HEADER1 .= "
        struct TestRefChange {
            int a, b, c;
        };
        $DECL_SPEC int paramRefChange(const TestRefChange & p1, int p2);";
    $SOURCE1 .= "
        int paramRefChange(const TestRefChange & p1, int p2) { return p2; }";
    
    $HEADER2 .= "
        struct TestRefChange {
            int a, b;
        };
        $DECL_SPEC int paramRefChange(const TestRefChange & p1, int p2);";
    $SOURCE2 .= "
        int paramRefChange(const TestRefChange & p1, int p2) { return p2; }";
    
    # Removed_Parameter
    $HEADER1 .= "
        $DECL_SPEC int removedParameter(int param, int removed_param);";
    $SOURCE1 .= "
        int removedParameter(int param, int removed_param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC int removedParameter(int param);";
    $SOURCE2 .= "
        int removedParameter(int param) { return 0; }";
    
    # Added_Parameter
    $HEADER1 .= "
        $DECL_SPEC int addedParameter(int param);";
    $SOURCE1 .= "
        int addedParameter(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC int addedParameter(int param, int added_param);";
    $SOURCE2 .= "
        int addedParameter(int param, int added_param) { return 0; }";
    
    # Added
    $HEADER2 .= "
        typedef int (*FUNCPTR_TYPE)(int a, int b);
        $DECL_SPEC int addedFunc(FUNCPTR_TYPE*const** f);";
    $SOURCE2 .= "
        int addedFunc(FUNCPTR_TYPE*const** f) { return 0; }";
    
    # Added_Virtual_Method
    $HEADER1 .= "
        class $DECL_SPEC AddedVirtualMethod {
        public:
            virtual int method(int param);
        };";
    $SOURCE1 .= "
        int AddedVirtualMethod::method(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC AddedVirtualMethod {
        public:
            virtual int addedMethod(int param);
            virtual int method(int param);
        };";
    $SOURCE2 .= "
        int AddedVirtualMethod::addedMethod(int param) {
            return param;
        }
        int AddedVirtualMethod::method(int param) { return param; }";

    # Added_Virtual_Method (added "virtual" attribute)
    $HEADER1 .= "
        class $DECL_SPEC BecameVirtualMethod {
        public:
            int becameVirtual(int param);
            virtual int method(int param);
        };";
    $SOURCE1 .= "
        int BecameVirtualMethod::becameVirtual(int param) { return param; }
        int BecameVirtualMethod::method(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC BecameVirtualMethod {
        public:
            virtual int becameVirtual(int param);
            virtual int method(int param);
        };";
    $SOURCE2 .= "
        int BecameVirtualMethod::becameVirtual(int param) { return param; }
        int BecameVirtualMethod::method(int param) { return param; }";

    # Added_Pure_Virtual_Method
    $HEADER1 .= "
        class $DECL_SPEC AddedPureVirtualMethod {
        public:
            virtual int method(int param);
            int otherMethod(int param);
        };";
    $SOURCE1 .= "
        int AddedPureVirtualMethod::method(int param) { return param; }
        int AddedPureVirtualMethod::otherMethod(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC AddedPureVirtualMethod {
        public:
            virtual int addedMethod(int param)=0;
            virtual int method(int param);
            int otherMethod(int param);
        };";
    $SOURCE2 .= "
        int AddedPureVirtualMethod::method(int param) { return param; }
        int AddedPureVirtualMethod::otherMethod(int param) { return param; }";

    # Added_Virtual_Method_At_End (Safe)
    $HEADER1 .= "
        class $DECL_SPEC AddedVirtualMethodAtEnd {
        public:
            AddedVirtualMethodAtEnd();
            int method1(int param);
            virtual int method2(int param);
        };";
    $SOURCE1 .= "
        AddedVirtualMethodAtEnd::AddedVirtualMethodAtEnd() { }
        int AddedVirtualMethodAtEnd::method1(int param) { return param; }
        int AddedVirtualMethodAtEnd::method2(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC AddedVirtualMethodAtEnd {
        public:
            AddedVirtualMethodAtEnd();
            int method1(int param);
            virtual int method2(int param);
            virtual int addedMethod(int param);
        };";
    $SOURCE2 .= "
        AddedVirtualMethodAtEnd::AddedVirtualMethodAtEnd() { }
        int AddedVirtualMethodAtEnd::method1(int param) { return param; }
        int AddedVirtualMethodAtEnd::method2(int param) { return param; }
        int AddedVirtualMethodAtEnd::addedMethod(int param) { return param; }";

    # Added_Virtual_Method_At_End (With Default Constructor)
    $HEADER1 .= "
        class $DECL_SPEC AddedVirtualMethodAtEnd_DefaultConstructor {
        public:
            int method1(int param);
            virtual int method2(int param);
        };";
    $SOURCE1 .= "
        int AddedVirtualMethodAtEnd_DefaultConstructor::method1(int param) { return param; }
        int AddedVirtualMethodAtEnd_DefaultConstructor::method2(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC AddedVirtualMethodAtEnd_DefaultConstructor {
        public:
            int method1(int param);
            virtual int method2(int param);
            virtual int addedMethod(int param);
        };";
    $SOURCE2 .= "
        int AddedVirtualMethodAtEnd_DefaultConstructor::method1(int param) { return param; }
        int AddedVirtualMethodAtEnd_DefaultConstructor::method2(int param) { return param; }
        int AddedVirtualMethodAtEnd_DefaultConstructor::addedMethod(int param) { return param; }";
    
    # Added_First_Virtual_Method
    $HEADER1 .= "
        class $DECL_SPEC AddedFirstVirtualMethod {
        public:
            int method(int param);
        };";
    $SOURCE1 .= "
        int AddedFirstVirtualMethod::method(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC AddedFirstVirtualMethod {
        public:
            int method(int param);
            virtual int addedMethod(int param);
        };";
    $SOURCE2 .= "
        int AddedFirstVirtualMethod::method(int param) { return param; }
        int AddedFirstVirtualMethod::addedMethod(int param) { return param; }";
    
    # Removed_Virtual_Method
    $HEADER1 .= "
        class $DECL_SPEC RemovedVirtualFunction {
        public:
            virtual int removedMethod(int param);
            virtual int vMethod(int param);
    };";
    $SOURCE1 .= "
        int RemovedVirtualFunction::removedMethod(int param) { return param; }
        int RemovedVirtualFunction::vMethod(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC RemovedVirtualFunction {
        public:
            int removedMethod(int param);
            virtual int vMethod(int param);
    };";
    $SOURCE2 .= "
        int RemovedVirtualFunction::removedMethod(int param) { return param; }
        int RemovedVirtualFunction::vMethod(int param) { return param; }";

    # Removed_Virtual_Method (Pure, From the End)
    $HEADER1 .= "
        class $DECL_SPEC RemovedPureVirtualMethodFromEnd {
        public:
            virtual int method(int param);
            virtual int removedMethod(int param)=0;
        };";
    $SOURCE1 .= "
        int RemovedPureVirtualMethodFromEnd::method(int param) { return param; }
        int RemovedPureVirtualMethodFromEnd::removedMethod(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC RemovedPureVirtualMethodFromEnd
        {
        public:
            virtual int method(int param);
            int removedMethod(int param);
        };";
    $SOURCE2 .= "
        int RemovedPureVirtualMethodFromEnd::method(int param) { return param; }
        int RemovedPureVirtualMethodFromEnd::removedMethod(int param) { return param; }";

    # Removed_Symbol (Pure with Implementation)
    $HEADER1 .= "
        class $DECL_SPEC RemovedPureSymbol {
        public:
            virtual int method(int param);
            virtual int removedMethod(int param)=0;
        };";
    $SOURCE1 .= "
        int RemovedPureSymbol::method(int param) { return param; }
        int RemovedPureSymbol::removedMethod(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC RemovedPureSymbol
        {
        public:
            virtual int method(int param);
        };";
    $SOURCE2 .= "
        int RemovedPureSymbol::method(int param) { return param; }";

    # Removed_Virtual_Method (From the End)
    $HEADER1 .= "
        class $DECL_SPEC RemovedVirtualMethodFromEnd {
        public:
            virtual int method(int param);
            virtual int removedMethod(int param);
        };";
    $SOURCE1 .= "
        int RemovedVirtualMethodFromEnd::method(int param) { return param; }
        int RemovedVirtualMethodFromEnd::removedMethod(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC RemovedVirtualMethodFromEnd
        {
        public:
            virtual int method(int param);
            int removedMethod(int param);
        };";
    $SOURCE2 .= "
        int RemovedVirtualMethodFromEnd::method(int param) { return param; }
        int RemovedVirtualMethodFromEnd::removedMethod(int param) { return param; }";

    # Removed_Last_Virtual_Method
    $HEADER1 .= "
        class $DECL_SPEC RemovedLastVirtualMethod
        {
        public:
            int method(int param);
            virtual int removedMethod(int param);
        };";
    $SOURCE1 .= "
        int RemovedLastVirtualMethod::method(int param) { return param; }";
    $SOURCE1 .= "
        int RemovedLastVirtualMethod::removedMethod(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC RemovedLastVirtualMethod
        {
        public:
            int method(int param);
            int removedMethod(int param);
        };";
    $SOURCE2 .= "
        int RemovedLastVirtualMethod::method(int param) { return param; }";
    $SOURCE2 .= "
        int RemovedLastVirtualMethod::removedMethod(int param) { return param; }";
    
    # Virtual_Table_Size
    $HEADER1 .= "
        class $DECL_SPEC VirtualTableSize
        {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };
        class $DECL_SPEC VirtualTableSize_SubClass: public VirtualTableSize
        {
        public:
            virtual int method3(int param);
            virtual int method4(int param);
        };";
    $SOURCE1 .= "
        int VirtualTableSize::method1(int param) { return param; }
        int VirtualTableSize::method2(int param) { return param; }
        int VirtualTableSize_SubClass::method3(int param) { return param; }
        int VirtualTableSize_SubClass::method4(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC VirtualTableSize
        {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
            virtual int addedMethod(int param);
        };
        class $DECL_SPEC VirtualTableSize_SubClass: public VirtualTableSize
        {
        public:
            virtual int method3(int param);
            virtual int method4(int param);
        };";
    $SOURCE2 .= "
        int VirtualTableSize::method1(int param) { return param; }
        int VirtualTableSize::method2(int param) { return param; }
        int VirtualTableSize::addedMethod(int param) { return param; }
        int VirtualTableSize_SubClass::method3(int param) { return param; }
        int VirtualTableSize_SubClass::method4(int param) { return param; }";
    
    # Virtual_Method_Position
    $HEADER1 .= "
        class $DECL_SPEC VirtualMethodPosition
        {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };";
    $SOURCE1 .= "
        int VirtualMethodPosition::method1(int param) { return param; }";
    $SOURCE1 .= "
        int VirtualMethodPosition::method2(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC VirtualMethodPosition
        {
        public:
            virtual int method2(int param);
            virtual int method1(int param);
        };";
    $SOURCE2 .= "
        int VirtualMethodPosition::method1(int param) { return param; }";
    $SOURCE2 .= "
        int VirtualMethodPosition::method2(int param) { return param; }";

    # Pure_Virtual_Method_Position
    $HEADER1 .= "
        class $DECL_SPEC PureVirtualFunctionPosition {
        public:
            virtual int method1(int param)=0;
            virtual int method2(int param)=0;
            int method3(int param);
        };";
    $SOURCE1 .= "
        int PureVirtualFunctionPosition::method3(int param) { return method1(7)+method2(7); }";
    
    $HEADER2 .= "
        class $DECL_SPEC PureVirtualFunctionPosition {
        public:
            virtual int method2(int param)=0;
            virtual int method1(int param)=0;
            int method3(int param);
        };";
    $SOURCE2 .= "
        int PureVirtualFunctionPosition::method3(int param) { return method1(7)+method2(7); }";

    # Virtual_Method_Position
    $HEADER1 .= "
        class $DECL_SPEC VirtualFunctionPosition {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };";
    $SOURCE1 .= "
        int VirtualFunctionPosition::method1(int param) { return 1; }
        int VirtualFunctionPosition::method2(int param) { return 2; }";
    
    $HEADER2 .= "
        class $DECL_SPEC VirtualFunctionPosition {
        public:
            virtual int method2(int param);
            virtual int method1(int param);
        };";
    $SOURCE2 .= "
        int VirtualFunctionPosition::method1(int param) { return 1; }
        int VirtualFunctionPosition::method2(int param) { return 2; }";
    
    # Virtual_Method_Position (safe)
    $HEADER1 .= "
        class $DECL_SPEC VirtualFunctionPositionSafe_Base {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };
        class $DECL_SPEC VirtualFunctionPositionSafe: public VirtualFunctionPositionSafe_Base {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };";
    $SOURCE1 .= "
        int VirtualFunctionPositionSafe_Base::method1(int param) { return param; }
        int VirtualFunctionPositionSafe_Base::method2(int param) { return param; }
        int VirtualFunctionPositionSafe::method1(int param) { return param; }
        int VirtualFunctionPositionSafe::method2(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC VirtualFunctionPositionSafe_Base {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };
        class $DECL_SPEC VirtualFunctionPositionSafe: public VirtualFunctionPositionSafe_Base {
        public:
            virtual int method2(int param);
            virtual int method1(int param);
        };";
    $SOURCE2 .= "
        int VirtualFunctionPositionSafe_Base::method1(int param) { return param; }
        int VirtualFunctionPositionSafe_Base::method2(int param) { return param; }
        int VirtualFunctionPositionSafe::method1(int param) { return param; }
        int VirtualFunctionPositionSafe::method2(int param) { return param; }";
    
    # Overridden_Virtual_Method
    $HEADER1 .= "
        class $DECL_SPEC OverriddenVirtualMethod_Base {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };
        class $DECL_SPEC OverriddenVirtualMethod: public OverriddenVirtualMethod_Base {
        public:
            OverriddenVirtualMethod();
            virtual int method3(int param);
        };";
    $SOURCE1 .= "
        int OverriddenVirtualMethod_Base::method1(int param) { return param; }
        int OverriddenVirtualMethod_Base::method2(int param) { return param; }
        OverriddenVirtualMethod::OverriddenVirtualMethod() {}
        int OverriddenVirtualMethod::method3(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC OverriddenVirtualMethod_Base {
        public:
            virtual int method1(int param);
            virtual int method2(int param);
        };
        class $DECL_SPEC OverriddenVirtualMethod:public OverriddenVirtualMethod_Base {
            OverriddenVirtualMethod();
            virtual int method2(int param);
            virtual int method3(int param);
        };";
    $SOURCE2 .= "
        int OverriddenVirtualMethod_Base::method1(int param) { return param; }
        int OverriddenVirtualMethod_Base::method2(int param) { return param; }
        OverriddenVirtualMethod::OverriddenVirtualMethod() {}
        int OverriddenVirtualMethod::method2(int param) { return param; }
        int OverriddenVirtualMethod::method3(int param) { return param; }";

    # Overridden_Virtual_Method_B (+ removed)
    $HEADER1 .= "
        
    class $DECL_SPEC OverriddenVirtualMethodB: public OverriddenVirtualMethod_Base {
        public:
            OverriddenVirtualMethodB();
            virtual int method2(int param);
            virtual int method3(int param);
    };";
    $SOURCE1 .= "
        OverriddenVirtualMethodB::OverriddenVirtualMethodB() {}
        int OverriddenVirtualMethodB::method2(int param) { return param; }
        int OverriddenVirtualMethodB::method3(int param) { return param; }";
    
    $HEADER2 .= "
        
    class $DECL_SPEC OverriddenVirtualMethodB:public OverriddenVirtualMethod_Base {
        public:
            OverriddenVirtualMethodB();
            virtual int method3(int param);
    };";
    $SOURCE2 .= "
        OverriddenVirtualMethodB::OverriddenVirtualMethodB() {}
        int OverriddenVirtualMethodB::method3(int param) { return param; }";
    
    # Size
    $HEADER1 .= "
        struct $DECL_SPEC TypeSize
        {
        public:
            TypeSize method(TypeSize param);
            int i[5];
            long j;
            double k;
            TypeSize* p;
        };";
    $SOURCE1 .= "
        TypeSize TypeSize::method(TypeSize param) { return param; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC TypeSize
        {
        public:
            TypeSize method(TypeSize param);
            int i[15];
            long j;
            double k;
            TypeSize* p;
            int added_member;
        };";
    $SOURCE2 .= "
        TypeSize TypeSize::method(TypeSize param) { return param; }";

    # Size_Of_Allocable_Class_Increased
    $HEADER1 .= "
        class $DECL_SPEC AllocableClassSize
        {
        public:
            AllocableClassSize();
            int method();
            double p[5];
        };";
    $SOURCE1 .= "
        AllocableClassSize::AllocableClassSize() { }";
    $SOURCE1 .= "
        int AllocableClassSize::method() { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AllocableClassSize
        {
        public:
            AllocableClassSize();
            int method();
            double p[15];
        };";
    $SOURCE2 .= "
        AllocableClassSize::AllocableClassSize() { }";
    $SOURCE2 .= "
        int AllocableClassSize::method() { return 0; }";
    
    # Size_Of_Allocable_Class_Decreased (decreased size, has derived class, has public members)
    $HEADER1 .= "
        class $DECL_SPEC DecreasedClassSize
        {
        public:
            DecreasedClassSize();
            int method();
            double p[15];
        };";
    $SOURCE1 .= "
        DecreasedClassSize::DecreasedClassSize() { }";
    $SOURCE1 .= "
        int DecreasedClassSize::method() { return 0; }";
    $HEADER1 .= "
        class $DECL_SPEC DecreasedClassSize_SubClass: public DecreasedClassSize
        {
        public:
            DecreasedClassSize_SubClass();
            int method();
            int f;
        };";
    $SOURCE1 .= "
        DecreasedClassSize_SubClass::DecreasedClassSize_SubClass() { f=7; }";
    $SOURCE1 .= "
        int DecreasedClassSize_SubClass::method() { return f; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC DecreasedClassSize
        {
        public:
            DecreasedClassSize();
            int method();
            double p[5];
        };";
    $SOURCE2 .= "
        DecreasedClassSize::DecreasedClassSize() { }";
    $SOURCE2 .= "
        int DecreasedClassSize::method() { return 0; }";
    $HEADER2 .= "
        class $DECL_SPEC DecreasedClassSize_SubClass: public DecreasedClassSize
        {
        public:
            DecreasedClassSize_SubClass();
            int method();
            int f;
        };";
    $SOURCE2 .= "
        DecreasedClassSize_SubClass::DecreasedClassSize_SubClass() { f=7; }";
    $SOURCE2 .= "
        int DecreasedClassSize_SubClass::method() { return f; }";

    # Size_Of_Copying_Class
    $HEADER1 .= "
        class $DECL_SPEC CopyingClassSize
        {
        public:
            int method();
            int p[5];
        };";
    $SOURCE1 .= "
        int CopyingClassSize::method() { return p[4]; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC CopyingClassSize
        {
        public:
            int method();
            int p[15];
        };";
    $SOURCE2 .= "
        int CopyingClassSize::method() { return p[10]; }";

    # Base_Class_Became_Virtually_Inherited
    $HEADER1 .= "
        class $DECL_SPEC BecameVirtualBase
        {
        public:
            BecameVirtualBase();
            int method();
            double p[5];
        };";
    $SOURCE1 .= "
        BecameVirtualBase::BecameVirtualBase() { }";
    $SOURCE1 .= "
        int BecameVirtualBase::method() { return 0; }";
    $HEADER1 .= "
        class $DECL_SPEC AddedVirtualBase1:public BecameVirtualBase
        {
        public:
            AddedVirtualBase1();
            int method();
        };";
    $SOURCE1 .= "
        AddedVirtualBase1::AddedVirtualBase1() { }";
    $SOURCE1 .= "
        int AddedVirtualBase1::method() { return 0; }";
    $HEADER1 .= "
        class $DECL_SPEC AddedVirtualBase2: public BecameVirtualBase
        {
        public:
            AddedVirtualBase2();
            int method();
        };";
    $SOURCE1 .= "
        AddedVirtualBase2::AddedVirtualBase2() { }";
    $SOURCE1 .= "
        int AddedVirtualBase2::method() { return 0; }";
    $HEADER1 .= "
        class $DECL_SPEC BaseClassBecameVirtuallyInherited:public AddedVirtualBase1, public AddedVirtualBase2
        {
        public:
            BaseClassBecameVirtuallyInherited();
        };";
    $SOURCE1 .= "
        BaseClassBecameVirtuallyInherited::BaseClassBecameVirtuallyInherited() { }";
    
    $HEADER2 .= "
        class $DECL_SPEC BecameVirtualBase
        {
        public:
            BecameVirtualBase();
            int method();
            double p[5];
        };";
    $SOURCE2 .= "
        BecameVirtualBase::BecameVirtualBase() { }";
    $SOURCE2 .= "
        int BecameVirtualBase::method() { return 0; }";
    $HEADER2 .= "
        class $DECL_SPEC AddedVirtualBase1:public virtual BecameVirtualBase
        {
        public:
            AddedVirtualBase1();
            int method();
        };";
    $SOURCE2 .= "
        AddedVirtualBase1::AddedVirtualBase1() { }";
    $SOURCE2 .= "
        int AddedVirtualBase1::method() { return 0; }";
    $HEADER2 .= "
        class $DECL_SPEC AddedVirtualBase2: public virtual BecameVirtualBase
        {
        public:
            AddedVirtualBase2();
            int method();
        };";
    $SOURCE2 .= "
        AddedVirtualBase2::AddedVirtualBase2() { }";
    $SOURCE2 .= "
        int AddedVirtualBase2::method() { return 0; }";
    $HEADER2 .= "
        class $DECL_SPEC BaseClassBecameVirtuallyInherited:public AddedVirtualBase1, public AddedVirtualBase2
        {
        public:
            BaseClassBecameVirtuallyInherited();
        };";
    $SOURCE2 .= "
        BaseClassBecameVirtuallyInherited::BaseClassBecameVirtuallyInherited() { }";

    # Added_Base_Class, Removed_Base_Class
    $HEADER1 .= "
        class $DECL_SPEC BaseClass
        {
        public:
            BaseClass();
            int method();
            double p[5];
        };
        class $DECL_SPEC RemovedBaseClass
        {
        public:
            RemovedBaseClass();
            int method();
        };
        class $DECL_SPEC ChangedBaseClass:public BaseClass, public RemovedBaseClass
        {
        public:
            ChangedBaseClass();
        };";
    $SOURCE1 .= "
        BaseClass::BaseClass() { }
        int BaseClass::method() { return 0; }
        RemovedBaseClass::RemovedBaseClass() { }
        int RemovedBaseClass::method() { return 0; }
        ChangedBaseClass::ChangedBaseClass() { }";
    
    $HEADER2 .= "
        class $DECL_SPEC BaseClass
        {
        public:
            BaseClass();
            int method();
            double p[5];
        };
        class $DECL_SPEC AddedBaseClass
        {
        public:
            AddedBaseClass();
            int method();
        };
        class $DECL_SPEC ChangedBaseClass:public BaseClass, public AddedBaseClass
        {
        public:
            ChangedBaseClass();
        };";
    $SOURCE2 .= "
        BaseClass::BaseClass() { }
        int BaseClass::method() { return 0; }
        AddedBaseClass::AddedBaseClass() { }
        int AddedBaseClass::method() { return 0; }
        ChangedBaseClass::ChangedBaseClass() { }";

    # Added_Base_Class_And_Shift, Removed_Base_Class_And_Shift
    $HEADER1 .= "
        struct $DECL_SPEC BaseClass2
        {
            BaseClass2();
            int method();
            double p[15];
        };
        class $DECL_SPEC ChangedBaseClassAndSize:public BaseClass
        {
        public:
            ChangedBaseClassAndSize();
        };";
    $SOURCE1 .= "
        BaseClass2::BaseClass2() { }
        int BaseClass2::method() { return 0; }
        ChangedBaseClassAndSize::ChangedBaseClassAndSize() { }";
    
    $HEADER2 .= "
        struct $DECL_SPEC BaseClass2
        {
            BaseClass2();
            int method();
            double p[15];
        };
        class $DECL_SPEC ChangedBaseClassAndSize:public BaseClass2
        {
        public:
            ChangedBaseClassAndSize();
        };";
    $SOURCE2 .= "
        BaseClass2::BaseClass2() { }
        int BaseClass2::method() { return 0; }
        ChangedBaseClassAndSize::ChangedBaseClassAndSize() { }";
    
    # Added_Field_And_Size
    $HEADER1 .= "
        struct $DECL_SPEC AddedFieldAndSize
        {
            int method(AddedFieldAndSize param);
            double i, j, k;
            AddedFieldAndSize* p;
        };";
    $SOURCE1 .= "
        int AddedFieldAndSize::method(AddedFieldAndSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedFieldAndSize
        {
            int method(AddedFieldAndSize param);
            double i, j, k;
            AddedFieldAndSize* p;
            int added_member1;
            long long added_member2;
        };";
    $SOURCE2 .= "
        int AddedFieldAndSize::method(AddedFieldAndSize param) { return 0; }";
    
    # Added_Field
    $HEADER1 .= "
        class $DECL_SPEC ObjectAddedMember
        {
        public:
            int method(int param);
            double i, j, k;
            AddedFieldAndSize* p;
        };";
    $SOURCE1 .= "
        int ObjectAddedMember::method(int param) { return param; }";
    
    $HEADER2 .= "
        class $DECL_SPEC ObjectAddedMember
        {
        public:
            int method(int param);
            double i, j, k;
            AddedFieldAndSize* p;
            int added_member1;
            long long added_member2;
        };";
    $SOURCE2 .= "
        int ObjectAddedMember::method(int param) { return param; }";
    
    # Added_Field (safe)
    $HEADER1 .= "
        struct $DECL_SPEC AddedBitfield
        {
            int method(AddedBitfield param);
            double i, j, k;
            int b1 : 32;
            int b2 : 31;
            AddedBitfield* p;
        };";
    $SOURCE1 .= "
        int AddedBitfield::method(AddedBitfield param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedBitfield
        {
            int method(AddedBitfield param);
            double i, j, k;
            int b1 : 32;
            int b2 : 31;
            int added_bitfield : 1;
            int added_bitfield2 : 1;
            AddedBitfield* p;
        };";
    $SOURCE2 .= "
        int AddedBitfield::method(AddedBitfield param) { return 0; }";
    
    # Bit_Field_Size
    $HEADER1 .= "
        struct $DECL_SPEC BitfieldSize
        {
            int method(BitfieldSize param);
            short changed_bitfield : 1;
        };";
    $SOURCE1 .= "
        int BitfieldSize::method(BitfieldSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC BitfieldSize
        {
            int method(BitfieldSize param);
            short changed_bitfield : 7;
        };";
    $SOURCE2 .= "
        int BitfieldSize::method(BitfieldSize param) { return 0; }";
    
    # Removed_Field
    $HEADER1 .= "
        struct $DECL_SPEC RemovedBitfield
        {
            int method(RemovedBitfield param);
            double i, j, k;
            int b1 : 32;
            int b2 : 31;
            int removed_bitfield : 1;
            RemovedBitfield* p;
        };";
    $SOURCE1 .= "
        int RemovedBitfield::method(RemovedBitfield param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC RemovedBitfield
        {
            int method(RemovedBitfield param);
            double i, j, k;
            int b1 : 32;
            int b2 : 31;
            RemovedBitfield* p;
        };";
    $SOURCE2 .= "
        int RemovedBitfield::method(RemovedBitfield param) { return 0; }";
        
    # Removed_Middle_Field
    $HEADER1 .= "
        struct $DECL_SPEC RemovedMiddleBitfield
        {
            int method(RemovedMiddleBitfield param);
            double i, j, k;
            int b1 : 32;
            int removed_middle_bitfield : 1;
            int b2 : 31;
            RemovedMiddleBitfield* p;
        };";
    $SOURCE1 .= "
        int RemovedMiddleBitfield::method(RemovedMiddleBitfield param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC RemovedMiddleBitfield
        {
            int method(RemovedMiddleBitfield param);
            double i, j, k;
            int b1 : 32;
            int b2 : 31;
            RemovedMiddleBitfield* p;
        };";
    $SOURCE2 .= "
        int RemovedMiddleBitfield::method(RemovedMiddleBitfield param) { return 0; }";
    
    # Added_Middle_Field_And_Size
    $HEADER1 .= "
        struct $DECL_SPEC AddedMiddleFieldAndSize
        {
            int method(AddedMiddleFieldAndSize param);
            int i;
            long j;
            double k;
            AddedMiddleFieldAndSize* p;
        };";
    $SOURCE1 .= "
        int AddedMiddleFieldAndSize::method(AddedMiddleFieldAndSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedMiddleFieldAndSize
        {
            int method(AddedMiddleFieldAndSize param);
            int i;
            int added_middle_member;
            long j;
            double k;
            AddedMiddleFieldAndSize* p;
        };";
    $SOURCE2 .= "
        int AddedMiddleFieldAndSize::method(AddedMiddleFieldAndSize param) { return 0; }";
        
    # Added_Field (padding)
    $HEADER1 .= "
        struct $DECL_SPEC AddedMiddlePaddedField
        {
            int method(int param);
            short i;
            long j;
            double k;
        };";
    $SOURCE1 .= "
        int AddedMiddlePaddedField::method(int param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedMiddlePaddedField
        {
            int method(int param);
            short i;
            short added_padded_field;
            long j;
            double k;
        };";
    $SOURCE2 .= "
        int AddedMiddlePaddedField::method(int param) { return 0; }";
        
    # Added_Field (tail padding)
    $HEADER1 .= "
        struct $DECL_SPEC AddedTailField
        {
            int method(int param);
            int i1, i2, i3, i4, i5, i6, i7;
            short s;
        };";
    $SOURCE1 .= "
        int AddedTailField::method(int param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedTailField
        {
            int method(int param);
            int i1, i2, i3, i4, i5, i6, i7;
            short s;
            short added_tail_field;
        };";
    $SOURCE2 .= "
        int AddedTailField::method(int param) { return 0; }";
        
    # Test Alignment
    $HEADER1 .= "
        struct $DECL_SPEC TestAlignment
        {
            int method(int param);
            short s:9;
            short   j:9;
            char  c;
            short t:9;
            short u:9;
            char d;
        };";
    $SOURCE1 .= "
        int TestAlignment::method(int param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC TestAlignment
        {
            int method(int param);
            short s:9;
            short   j:9;
            char  c;
            short t:9;
            short u:9;
            char d;
        };";
    $SOURCE2 .= "
        int TestAlignment::method(int param) { return 0; }";
    
    # Renamed_Field
    $HEADER1 .= "
        struct $DECL_SPEC RenamedField
        {
            int method(RenamedField param);
            long i;
            long j;
            double k;
            RenamedField* p;
        };";
    $SOURCE1 .= "
        int RenamedField::method(RenamedField param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC RenamedField
        {
            int method(RenamedField param);
            long renamed_member;
            long j;
            double k;
            RenamedField* p;
        };";
    $SOURCE2 .= "
        int RenamedField::method(RenamedField param) { return 0; }";
    
    # Removed_Field_And_Size
    $HEADER1 .= "
        struct $DECL_SPEC RemovedFieldAndSize
        {
            int method(RemovedFieldAndSize param);
            double i, j, k;
            RemovedFieldAndSize* p;
            int removed_member1;
            long removed_member2;
        };";
    $SOURCE1 .= "
        int RemovedFieldAndSize::method(RemovedFieldAndSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC RemovedFieldAndSize
        {
            int method(RemovedFieldAndSize param);
            double i, j, k;
            RemovedFieldAndSize* p;
        };";
    $SOURCE2 .= "
        int RemovedFieldAndSize::method(RemovedFieldAndSize param) { return 0; }";
        
    # Field Position
    $HEADER1 .= "
        struct $DECL_SPEC MovedField
        {
            int method(int param);
            double i;
            int j;
        };";
    $SOURCE1 .= "
        int MovedField::method(int param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC MovedField
        {
            int method(int param);
            int j;
            double i;
        };";
    $SOURCE2 .= "
        int MovedField::method(int param) { return 0; }";
    
    # Removed_Middle_Field_And_Size
    $HEADER1 .= "
        struct $DECL_SPEC RemovedMiddleFieldAndSize
        {
            int method(RemovedMiddleFieldAndSize param);
            int i;
            int removed_middle_member;
            long j;
            double k;
            RemovedMiddleFieldAndSize* p;
        };";
    $SOURCE1 .= "
        int RemovedMiddleFieldAndSize::method(RemovedMiddleFieldAndSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC RemovedMiddleFieldAndSize
        {
            int method(RemovedMiddleFieldAndSize param);
            int i;
            long j;
            double k;
            RemovedMiddleFieldAndSize* p;
        };";
    $SOURCE2 .= "
        int RemovedMiddleFieldAndSize::method(RemovedMiddleFieldAndSize param) { return 0; }";
    
    # Enum_Member_Value
    $HEADER1 .= "
        enum EnumMemberValue
        {
            MEMBER_1=1,
            MEMBER_2=2
        };";
    $HEADER1 .= "
        $DECL_SPEC int enumMemberValueChange(enum EnumMemberValue param);";
    $SOURCE1 .= "
        int enumMemberValueChange(enum EnumMemberValue param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    $HEADER2 .= "
        enum EnumMemberValue
        {
            MEMBER_1=2,
            MEMBER_2=1
        };";
    $HEADER2 .= "
        $DECL_SPEC int enumMemberValueChange(enum EnumMemberValue param);";
    $SOURCE2 .= "
        int enumMemberValueChange(enum EnumMemberValue param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    # Enum_Member_Name
    $HEADER1 .= "
        enum EnumMemberRename
        {
            BRANCH_1=1,
            BRANCH_2=2
        };";
    $HEADER1 .= "
        $DECL_SPEC int enumMemberRename(enum EnumMemberRename param);";
    $SOURCE1 .= "
        int enumMemberRename(enum EnumMemberRename param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    $HEADER2 .= "
        enum EnumMemberRename
        {
            BRANCH_FIRST=1,
            BRANCH_SECOND=2
        };";
    $HEADER2 .= "
        $DECL_SPEC int enumMemberRename(enum EnumMemberRename param);";
    $SOURCE2 .= "
        int enumMemberRename(enum EnumMemberRename param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    # Field_Type_And_Size
    $HEADER1 .= "
        struct $DECL_SPEC FieldTypeAndSize
        {
            int method(FieldTypeAndSize param);
            int i;
            long j;
            double k;
            FieldTypeAndSize* p;
        };";
    $SOURCE1 .= "
        int FieldTypeAndSize::method(FieldTypeAndSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC FieldTypeAndSize
        {
            int method(FieldTypeAndSize param);
            long long i;
            long j;
            double k;
            FieldTypeAndSize* p;
        };";
    $SOURCE2 .= "
        int FieldTypeAndSize::method(FieldTypeAndSize param) { return 0; }";
    
    # Member_Type
    $HEADER1 .= "
        struct $DECL_SPEC MemberType
        {
            int method(MemberType param);
            int i;
            long j;
            double k;
            MemberType* p;
        };";
    $SOURCE1 .= "
        int MemberType::method(MemberType param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC MemberType
        {
            int method(MemberType param);
            float i;
            long j;
            double k;
            MemberType* p;
        };";
    $SOURCE2 .= "
        int MemberType::method(MemberType param) { return 0; }";
    
    # Field_BaseType
    $HEADER1 .= "
        struct $DECL_SPEC FieldBaseType
        {
            int method(FieldBaseType param);
            int *i;
            long j;
            double k;
            FieldBaseType* p;
        };";
    $SOURCE1 .= "
        int FieldBaseType::method(FieldBaseType param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC FieldBaseType
        {
            int method(FieldBaseType param);
            long long *i;
            long j;
            double k;
            FieldBaseType* p;
        };";
    $SOURCE2 .= "
        int FieldBaseType::method(FieldBaseType param) { return 0; }";
    
    # Field_PointerLevel_Increased (and size)
    $HEADER1 .= "
        struct $DECL_SPEC FieldPointerLevelAndSize
        {
            int method(FieldPointerLevelAndSize param);
            long long i;
            long j;
            double k;
            FieldPointerLevelAndSize* p;
        };";
    $SOURCE1 .= "
        int FieldPointerLevelAndSize::method(FieldPointerLevelAndSize param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC FieldPointerLevelAndSize
        {
            int method(FieldPointerLevelAndSize param);
            long long *i;
            long j;
            double k;
            FieldPointerLevelAndSize* p;
        };";
    $SOURCE2 .= "
        int FieldPointerLevelAndSize::method(FieldPointerLevelAndSize param) { return 0; }";
    
    # Field_PointerLevel
    $HEADER1 .= "
        struct $DECL_SPEC FieldPointerLevel
        {
            int method(FieldPointerLevel param);
            int **i;
            long j;
            double k;
            FieldPointerLevel* p;
        };";
    $SOURCE1 .= "
        int FieldPointerLevel::method(FieldPointerLevel param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC FieldPointerLevel
        {
            int method(FieldPointerLevel param);
            int *i;
            long j;
            double k;
            FieldPointerLevel* p;
        };";
    $SOURCE2 .= "
        int FieldPointerLevel::method(FieldPointerLevel param) { return 0; }";
    
    # Added_Interface (method)
    $HEADER1 .= "
        struct $DECL_SPEC AddedInterface
        {
            int method(AddedInterface param);
            int i;
            long j;
            double k;
            AddedInterface* p;
        };";
    $SOURCE1 .= "
        int AddedInterface::method(AddedInterface param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedInterface
        {
            int method(AddedInterface param);
            int added_func(AddedInterface param);
            int i;
            long j;
            double k;
            AddedInterface* p;
        };";
    $SOURCE2 .= "
        int AddedInterface::method(AddedInterface param) { return 0; }";
    $SOURCE2 .= "
        int AddedInterface::added_func(AddedInterface param) { return 0; }";
    
    # Added_Interface (function)
    $HEADER2 .= "
        $DECL_SPEC int addedFunc2(void *** param);";
    $SOURCE2 .= "
        int addedFunc2(void *** param) { return 0; }";
    
    # Added_Interface (global variable)
    $HEADER1 .= "
        struct $DECL_SPEC AddedVariable
        {
            int method(AddedVariable param);
            int i1, i2;
            long j;
            double k;
            AddedVariable* p;
        };";
    $SOURCE1 .= "
        int AddedVariable::method(AddedVariable param) {
            return i1;
        }";
    
    $HEADER2 .= "
        struct $DECL_SPEC AddedVariable
        {
            int method(AddedVariable param);
            static int i1;
            static int i2;
            long j;
            double k;
            AddedVariable* p;
        };";
    $SOURCE2 .= "
        int AddedVariable::method(AddedVariable param) { return AddedVariable::i1; }";
    $SOURCE2 .= "
        int AddedVariable::i1=0;";
    $SOURCE2 .= "
        int AddedVariable::i2=0;";
    
    # Removed_Interface (method)
    $HEADER1 .= "
        struct $DECL_SPEC RemovedInterface
        {
            int method(RemovedInterface param);
            int removed_func(RemovedInterface param);
            int i;
            long j;
            double k;
            RemovedInterface* p;
        };";
    $SOURCE1 .= "
        int RemovedInterface::method(RemovedInterface param) { return 0; }";
    $SOURCE1 .= "
        int RemovedInterface::removed_func(RemovedInterface param) { return 0; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC RemovedInterface
        {
            int method(RemovedInterface param);
            int i;
            long j;
            double k;
            RemovedInterface* p;
        };";
    $SOURCE2 .= "
        int RemovedInterface::method(RemovedInterface param) { return 0; }";
    
    # Removed_Interface (function)
    $HEADER1 .= "
        $DECL_SPEC int removedFunc2(void *** param);";
    $SOURCE1 .= "
        int removedFunc2(void *** param) { return 0; }";
    
    # Method_Became_Static
    $HEADER1 .= "
        struct $DECL_SPEC MethodBecameStatic
        {
            MethodBecameStatic becameStatic(MethodBecameStatic param);
            int **i;
            long j;
            double k;
            MethodBecameStatic* p;
        };";
    $SOURCE1 .= "
        MethodBecameStatic MethodBecameStatic::becameStatic(MethodBecameStatic param) { return param; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC MethodBecameStatic
        {
            static MethodBecameStatic becameStatic(MethodBecameStatic param);
            int **i;
            long j;
            double k;
            MethodBecameStatic* p;
        };";
    $SOURCE2 .= "
        MethodBecameStatic MethodBecameStatic::becameStatic(MethodBecameStatic param) { return param; }";
    
    # Method_Became_NonStatic
    $HEADER1 .= "
        struct $DECL_SPEC MethodBecameNonStatic
        {
            static MethodBecameNonStatic becameNonStatic(MethodBecameNonStatic param);
            int **i;
            long j;
            double k;
            MethodBecameNonStatic* p;
        };";
    $SOURCE1 .= "
        MethodBecameNonStatic MethodBecameNonStatic::becameNonStatic(MethodBecameNonStatic param) { return param; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC MethodBecameNonStatic
        {
            MethodBecameNonStatic becameNonStatic(MethodBecameNonStatic param);
            int **i;
            long j;
            double k;
            MethodBecameNonStatic* p;
        };";
    $SOURCE2 .= "
        MethodBecameNonStatic MethodBecameNonStatic::becameNonStatic(MethodBecameNonStatic param) { return param; }";
    
    # Parameter_Type_And_Size
    $HEADER1 .= "
        $DECL_SPEC int funcParameterTypeAndSize(int param, int other_param);";
    $SOURCE1 .= "
        int funcParameterTypeAndSize(int param, int other_param) { return other_param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int funcParameterTypeAndSize(long long param, int other_param);";
    $SOURCE2 .= "
        int funcParameterTypeAndSize(long long param, int other_param) { return other_param; }";
    
    # Parameter_Type
    $HEADER1 .= "
        $DECL_SPEC int funcParameterType(int param, int other_param);";
    $SOURCE1 .= "
        int funcParameterType(int param, int other_param) { return other_param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int funcParameterType(float param, int other_param);";
    $SOURCE2 .= "
        int funcParameterType(float param, int other_param) { return other_param; }";
    
    # Parameter_BaseType
    $HEADER1 .= "
        $DECL_SPEC int funcParameterBaseType(int *param);";
    $SOURCE1 .= "
        int funcParameterBaseType(int *param) { return sizeof(*param); }";
    
    $HEADER2 .= "
        $DECL_SPEC int funcParameterBaseType(long long *param);";
    $SOURCE2 .= "
        int funcParameterBaseType(long long *param) { return sizeof(*param); }";
    
    # Parameter_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC long long funcParameterPointerLevelAndSize(long long param);";
    $SOURCE1 .= "
        long long funcParameterPointerLevelAndSize(long long param) { return param; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long funcParameterPointerLevelAndSize(long long *param);";
    $SOURCE2 .= "
        long long funcParameterPointerLevelAndSize(long long *param) { return param[5]; }";
    
    # Parameter_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC int funcParameterPointerLevel(int *param);";
    $SOURCE1 .= "
        int funcParameterPointerLevel(int *param) { return param[5]; }";
    
    $HEADER2 .= "
        $DECL_SPEC int funcParameterPointerLevel(int **param);";
    $SOURCE2 .= "
        int funcParameterPointerLevel(int **param) { return param[5][5]; }";
    
    # Return_Type_And_Size
    $HEADER1 .= "
        $DECL_SPEC int funcReturnTypeAndSize(int param);";
    $SOURCE1 .= "
        int funcReturnTypeAndSize(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long funcReturnTypeAndSize(int param);";
    $SOURCE2 .= "
        long long funcReturnTypeAndSize(int param) { return 0; }";
    
    # Return_Type
    $HEADER1 .= "
        $DECL_SPEC int funcReturnType(int param);";
    $SOURCE1 .= "
        int funcReturnType(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC float funcReturnType(int param);";
    $SOURCE2 .= "
        float funcReturnType(int param) { return 0.7; }";

    # Return_Type_Became_Void ("int" to "void")
    $HEADER1 .= "
        $DECL_SPEC int funcReturnTypeBecameVoid(int param);";
    $SOURCE1 .= "
        int funcReturnTypeBecameVoid(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC void funcReturnTypeBecameVoid(int param);";
    $SOURCE2 .= "
        void funcReturnTypeBecameVoid(int param) { return; }";
    
    # Return_BaseType
    $HEADER1 .= "
        $DECL_SPEC int* funcReturnBaseType(int param);";
    $SOURCE1 .= "
        int* funcReturnBaseType(int param) {
            int *x = new int[10];
            return x;
        }";
    
    $HEADER2 .= "
        $DECL_SPEC long long* funcReturnBaseType(int param);";
    $SOURCE2 .= "
        long long* funcReturnBaseType(int param) {
            long long *x = new long long[10];
            return x;
        }";
    
    # Return_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC long long funcReturnPointerLevelAndSize(int param);";
    $SOURCE1 .= "
        long long funcReturnPointerLevelAndSize(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long* funcReturnPointerLevelAndSize(int param);";
    $SOURCE2 .= "
        long long* funcReturnPointerLevelAndSize(int param) { return new long long[10]; }";
    
    # Return_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC int* funcReturnPointerLevel(int param);";
    $SOURCE1 .= "
        int* funcReturnPointerLevel(int param) { return new int[10]; }";
    
    $HEADER2 .= "
        $DECL_SPEC int** funcReturnPointerLevel(int param);";
    $SOURCE2 .= "
        int** funcReturnPointerLevel(int param) { return new int*[10]; }";
    
    # Size (anon type)
    $HEADER1 .= "
        typedef struct {
            int i;
            long j;
            double k;
        } AnonTypedef;
        $DECL_SPEC int funcAnonTypedef(AnonTypedef param);";
    $SOURCE1 .= "
        int funcAnonTypedef(AnonTypedef param) { return 0; }";
    
    $HEADER2 .= "
        typedef struct {
            int i;
            long j;
            double k;
            union {
                int dummy[256];
                struct {
                    char q_skiptable[256];
                    const char *p;
                    int l;
                } p;
            };
        } AnonTypedef;
        $DECL_SPEC int funcAnonTypedef(AnonTypedef param);";
    $SOURCE2 .= "
        int funcAnonTypedef(AnonTypedef param) { return 0; }";
    
    # Added_Field (safe: opaque)
    $HEADER1 .= "
        struct $DECL_SPEC OpaqueType
        {
        public:
            OpaqueType method(OpaqueType param);
            int i;
            long j;
            double k;
            OpaqueType* p;
        };";
    $SOURCE1 .= "
        OpaqueType OpaqueType::method(OpaqueType param) { return param; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC OpaqueType
        {
        public:
            OpaqueType method(OpaqueType param);
            int i;
            long j;
            double k;
            OpaqueType* p;
            int added_member;
        };";
    $SOURCE2 .= "
        OpaqueType OpaqueType::method(OpaqueType param) { return param; }";
    
    # Added_Field (safe: internal)
    $HEADER1 .= "
        struct $DECL_SPEC InternalType {
            InternalType method(InternalType param);
            int i;
            long j;
            double k;
            InternalType* p;
        };";
    $SOURCE1 .= "
        InternalType InternalType::method(InternalType param) { return param; }";
    
    $HEADER2 .= "
        struct $DECL_SPEC InternalType {
            InternalType method(InternalType param);
            int i;
            long j;
            double k;
            InternalType* p;
            int added_member;
        };";
    $SOURCE2 .= "
        InternalType InternalType::method(InternalType param) { return param; }";
    
    # Size (unnamed struct/union fields within structs/unions)
    $HEADER1 .= "
        typedef struct {
            int a;
            struct {
                int u1;
                float u2;
            };
            int d;
        } UnnamedTypeSize;
        $DECL_SPEC int unnamedTypeSize(UnnamedTypeSize param);";
    $SOURCE1 .= "
        int unnamedTypeSize(UnnamedTypeSize param) { return 0; }";
    
    $HEADER2 .= "
        typedef struct {
            int a;
            struct {
                long double u1;
                float u2;
            };
            int d;
        } UnnamedTypeSize;
        $DECL_SPEC int unnamedTypeSize(UnnamedTypeSize param);";
    $SOURCE2 .= "
        int unnamedTypeSize(UnnamedTypeSize param) { return 0; }";
    
    # constants
    $HEADER1 .= "
        #define PUBLIC_CONSTANT \"old_value\"";
    $HEADER2 .= "
        #define PUBLIC_CONSTANT \"new_value\"";
    
    $HEADER1 .= "
        #define PRIVATE_CONSTANT \"old_value\"
        #undef PRIVATE_CONSTANT";
    $HEADER2 .= "
        #define PRIVATE_CONSTANT \"new_value\"
        #undef PRIVATE_CONSTANT";
    
    # Added_Field (union)
    $HEADER1 .= "
        union UnionAddedField {
            int a;
            struct {
                int b;
                float c;
            };
            int d;
        };
        $DECL_SPEC int unionAddedField(UnionAddedField param);";
    $SOURCE1 .= "
        int unionAddedField(UnionAddedField param) { return 0; }";
    
    $HEADER2 .= "
        union UnionAddedField {
            int a;
            struct {
                long double x, y;
            } new_field;
            struct {
                int b;
                float c;
            };
            int d;
        };
        $DECL_SPEC int unionAddedField(UnionAddedField param);";
    $SOURCE2 .= "
        int unionAddedField(UnionAddedField param) { return 0; }";
    
    # Removed_Field (union)
    $HEADER1 .= "
        union UnionRemovedField {
            int a;
            struct {
                long double x, y;
            } removed_field;
            struct {
                int b;
                float c;
            };
            int d;
        };
        $DECL_SPEC int unionRemovedField(UnionRemovedField param);";
    $SOURCE1 .= "
        int unionRemovedField(UnionRemovedField param) { return 0; }";
    
    $HEADER2 .= "
        union UnionRemovedField {
            int a;
            struct {
                int b;
                float c;
            };
            int d;
        };
        $DECL_SPEC int unionRemovedField(UnionRemovedField param);";
    $SOURCE2 .= "
        int unionRemovedField(UnionRemovedField param) { return 0; }";

    # Added (typedef change)
    $HEADER1 .= "
        typedef float TYPEDEF_TYPE;
        $DECL_SPEC int parameterTypedefChange(TYPEDEF_TYPE param);";
    $SOURCE1 .= "
        int parameterTypedefChange(TYPEDEF_TYPE param) { return 1; }";
    
    $HEADER2 .= "
        typedef int TYPEDEF_TYPE;
        $DECL_SPEC int parameterTypedefChange(TYPEDEF_TYPE param);";
    $SOURCE2 .= "
        int parameterTypedefChange(TYPEDEF_TYPE param) { return 1; }";

    # Parameter_Default_Value_Changed
    $HEADER1 .= "
        $DECL_SPEC int parameterDefaultValueChanged(int param = 0xf00f); ";
    $SOURCE1 .= "
        int parameterDefaultValueChanged(int param) { return param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterDefaultValueChanged(int param = 0xf00b); ";
    $SOURCE2 .= "
        int parameterDefaultValueChanged(int param) { return param; }";

    # Parameter_Default_Value_Changed (char const *)
    $HEADER1 .= "
        $DECL_SPEC int parameterDefaultStringValueChanged(char const* param = \" str  1 \"); ";
    $SOURCE1 .= "
        int parameterDefaultStringValueChanged(char const* param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterDefaultStringValueChanged(char const* param = \" str  2 \"); ";
    $SOURCE2 .= "
        int parameterDefaultStringValueChanged(char const* param) { return 0; }";

    # Parameter_Default_Value_Removed
    $HEADER1 .= "
        $DECL_SPEC int parameterDefaultValueRemoved(int param = 0xf00f);
    ";
    $SOURCE1 .= "
        int parameterDefaultValueRemoved(int param) { return param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterDefaultValueRemoved(int param);";
    $SOURCE2 .= "
        int parameterDefaultValueRemoved(int param) { return param; }";
    
    # Field_Type (typedefs in member type)
    $HEADER1 .= "
        typedef float TYPEDEF_TYPE_2;
        struct $DECL_SPEC FieldTypedefChange{
        public:
            TYPEDEF_TYPE_2 m;
            TYPEDEF_TYPE_2 n;
        };
        $DECL_SPEC int fieldTypedefChange(FieldTypedefChange param);";
    $SOURCE1 .= "
        int fieldTypedefChange(FieldTypedefChange param) { return 1; }";
    
    $HEADER2 .= "
        typedef int TYPEDEF_TYPE_2;
        struct $DECL_SPEC FieldTypedefChange{
        public:
            TYPEDEF_TYPE_2 m;
            TYPEDEF_TYPE_2 n;
        };
        $DECL_SPEC int fieldTypedefChange(FieldTypedefChange param);";
    $SOURCE2 .= "
        int fieldTypedefChange(FieldTypedefChange param) { return 1; }";

    # Callback (testCallback symbol should be affected
    # instead of callback1 and callback2)
    $HEADER1 .= "
        class $DECL_SPEC Callback {
        public:
            virtual int callback1(int x, int y)=0;
            virtual int callback2(int x, int y)=0;
        };
        $DECL_SPEC int testCallback(Callback* p);";
    $SOURCE1 .= "
        int testCallback(Callback* p) {
            p->callback2(1, 2);
            return 0;
        }";

    $HEADER2 .= "
        class $DECL_SPEC Callback {
        public:
            virtual int callback1(int x, int y)=0;
            virtual int added_callback(int x, int y)=0;
            virtual int callback2(int x, int y)=0;
        };
        $DECL_SPEC int testCallback(Callback* p);";
    $SOURCE2 .= "
        int testCallback(Callback* p) {
            p->callback2(1, 2);
            return 0;
        }";
    
    # End namespace
    $HEADER1 .= "\n}\n";
    $HEADER2 .= "\n}\n";
    $SOURCE1 .= "\n}\n";
    $SOURCE2 .= "\n}\n";
    
    runTests("simple_lib_cpp", "C++", $HEADER1, $SOURCE1, $HEADER2, $SOURCE2, "TestNS::OpaqueType", "_ZN6TestNS12InternalType6methodES0_");
}

sub testSystemC()
{
    print "\nverifying detectable C library changes\n";
    my ($HEADER1, $SOURCE1, $HEADER2, $SOURCE2) = ();
    my $DECL_SPEC = ($OSgroup eq "windows")?"__declspec( dllexport )":"";
    my $EXTERN = ($OSgroup eq "windows")?"extern ":"";
    
    # Field_Type_And_Size
    $HEADER1 .= "
        struct FieldSizePadded
        {
            int i;
            char changed_field;
            // padding (3 bytes)
            int j;
        };
        $DECL_SPEC int fieldSizePadded(struct FieldSizePadded param);";
    $SOURCE1 .= "
        int fieldSizePadded(struct FieldSizePadded param) { return 0; }";
    
    $HEADER2 .= "
        struct FieldSizePadded
        {
            int i;
            int changed_field;
            int j;
        };
        $DECL_SPEC int fieldSizePadded(struct FieldSizePadded param);";
    $SOURCE2 .= "
        int fieldSizePadded(struct FieldSizePadded param) { return 0; }";
    
    # Parameter_Type_Format
    $HEADER1 .= "
        struct DType1
        {
            int i;
            double j;
        };
        $DECL_SPEC int parameterTypeFormat(struct DType1 param);";
    $SOURCE1 .= "
        int parameterTypeFormat(struct DType1 param) { return 0; }";
    
    $HEADER2 .= "
        struct DType2
        {
            double i;
            int j;
        };
        $DECL_SPEC int parameterTypeFormat(struct DType2 param);";
    $SOURCE2 .= "
        int parameterTypeFormat(struct DType2 param) { return 0; }";
    
    # Global_Data_Value (int)
    $HEADER1 .= "
        $EXTERN $DECL_SPEC const int globalDataValue = 10;";
    
    $HEADER2 .= "
        $EXTERN $DECL_SPEC const int globalDataValue = 15;";
    
    # Global_Data_Became_Non_Const (int)
    $HEADER1 .= "
        $EXTERN $DECL_SPEC const int globalDataBecameNonConst = 10;";
    
    $HEADER2 .= "
        extern $DECL_SPEC int globalDataBecameNonConst;";
    $SOURCE2 .= "
        int globalDataBecameNonConst = 15;";

    # Global_Data_Became_Const (safe)
    $HEADER1 .= "
        extern $DECL_SPEC int globalDataBecameConst;";
    $SOURCE1 .= "
        int globalDataBecameConst = 10;";
    
    $HEADER2 .= "
        $EXTERN $DECL_SPEC const int globalDataBecameConst=15;";
    
    # Global_Data_Became_Non_Const (struct)
    $HEADER1 .= "
        struct GlobalDataType{int a;int b;struct GlobalDataType* p;};
        $EXTERN $DECL_SPEC const struct GlobalDataType globalStructDataBecameConst = {1, 2, (struct GlobalDataType*)0};";
    
    $HEADER2 .= "
        struct GlobalDataType{int a;int b;struct GlobalDataType* p;};
        $EXTERN $DECL_SPEC struct GlobalDataType globalStructDataBecameConst = {1, 2, (struct GlobalDataType*)0};";
    
    # Removed_Parameter
    $HEADER1 .= "
        $DECL_SPEC int removedParameter(int param, int removed_param);";
    $SOURCE1 .= "
        int removedParameter(int param, int removed_param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC int removedParameter(int param);";
    $SOURCE2 .= "
        int removedParameter(int param) { return 0; }";
    
    # Added_Parameter
    $HEADER1 .= "
        $DECL_SPEC int addedParameter(int param);";
    $SOURCE1 .= "
        int addedParameter(int param) { return param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int addedParameter(int param, int added_param, int added_param2);";
    $SOURCE2 .= "
        int addedParameter(int param, int added_param, int added_param2) { return added_param2; }";
    
    # Added_Interface (typedef to funcptr parameter)
    $HEADER2 .= "
        typedef int (*FUNCPTR_TYPE)(int a, int b);
        $DECL_SPEC int addedFunc(FUNCPTR_TYPE*const** f);";
    $SOURCE2 .= "
        int addedFunc(FUNCPTR_TYPE*const** f) { return 0; }";
    
    # Added_Interface (funcptr parameter)
    $HEADER2 .= "
        $DECL_SPEC int addedFunc2(int(*func)(int, int));";
    $SOURCE2 .= "
        int addedFunc2(int(*func)(int, int)) { return 0; }";
    
    # Added_Interface (no limited parameters)
    $HEADER2 .= "
        $DECL_SPEC int addedFunc3(float p1, ...);";
    $SOURCE2 .= "
        int addedFunc3(float p1, ...) { return 0; }";
    
    # Size
    $HEADER1 .= "
        struct TypeSize
        {
            long long i[5];
            long j;
            double k;
            struct TypeSize* p;
        };
        $DECL_SPEC int testSize(struct TypeSize param, int param_2);";
    $SOURCE1 .= "
        int testSize(struct TypeSize param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct TypeSize
        {
            long long i[15];
            long long j;
            double k;
            struct TypeSize* p;
        };
        $DECL_SPEC int testSize(struct TypeSize param, int param_2);";
    $SOURCE2 .= "
        int testSize(struct TypeSize param, int param_2) { return param_2; }";
    
    # Added_Field_And_Size
    $HEADER1 .= "
        struct AddedFieldAndSize
        {
            int i;
            long j;
            double k;
            struct AddedFieldAndSize* p;
        };
        $DECL_SPEC int addedFieldAndSize(struct AddedFieldAndSize param, int param_2);";
    $SOURCE1 .= "
        int addedFieldAndSize(struct AddedFieldAndSize param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct AddedFieldAndSize
        {
            int i;
            long j;
            double k;
            struct AddedFieldAndSize* p;
            int added_member1;
            int added_member2;
        };
        $DECL_SPEC int addedFieldAndSize(struct AddedFieldAndSize param, int param_2);";
    $SOURCE2 .= "
        int addedFieldAndSize(struct AddedFieldAndSize param, int param_2) { return param_2; }";
    
    # Added_Middle_Field_And_Size
    $HEADER1 .= "
        struct AddedMiddleFieldAndSize
        {
            int i;
            long j;
            double k;
            struct AddedMiddleFieldAndSize* p;
        };
        $DECL_SPEC int addedMiddleFieldAndSize(struct AddedMiddleFieldAndSize param, int param_2);";
    $SOURCE1 .= "
        int addedMiddleFieldAndSize(struct AddedMiddleFieldAndSize param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct AddedMiddleFieldAndSize
        {
            int i;
            int added_middle_member;
            long j;
            double k;
            struct AddedMiddleFieldAndSize* p;
        };
        $DECL_SPEC int addedMiddleFieldAndSize(struct AddedMiddleFieldAndSize param, int param_2);";
    $SOURCE2 .= "
        int addedMiddleFieldAndSize(struct AddedMiddleFieldAndSize param, int param_2) { return param_2; }";

    # Added_Middle_Field
    $HEADER1 .= "
        struct AddedMiddleField
        {
            unsigned char field1;
            unsigned short field2;
        };
        $DECL_SPEC int addedMiddleField(struct AddedMiddleField param, int param_2);";
    $SOURCE1 .= "
        int addedMiddleField(struct AddedMiddleField param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct AddedMiddleField
        {
            unsigned char field1;
            unsigned char added_field;
            unsigned short field2;
        };
        $DECL_SPEC int addedMiddleField(struct AddedMiddleField param, int param_2);";
    $SOURCE2 .= "
        int addedMiddleField(struct AddedMiddleField param, int param_2) { return param_2; }";
    
    # Renamed_Field
    $HEADER1 .= "
        struct RenamedField
        {
            long i;
            long j;
            double k;
            struct RenamedField* p;
        };
        $DECL_SPEC int renamedField(struct RenamedField param, int param_2);";
    $SOURCE1 .= "
        int renamedField(struct RenamedField param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct RenamedField
        {
            long renamed_member;
            long j;
            double k;
            struct RenamedField* p;
        };
        $DECL_SPEC int renamedField(struct RenamedField param, int param_2);";
    $SOURCE2 .= "
        int renamedField(struct RenamedField param, int param_2) { return param_2; }";
        
    # Renamed_Field
    $HEADER1 .= "
        union RenamedUnionField
        {
            int renamed_from;
            double j;
        };
        $DECL_SPEC int renamedUnionField(union RenamedUnionField param);";
    $SOURCE1 .= "
        int renamedUnionField(union RenamedUnionField param) { return 0; }";
    
    $HEADER2 .= "
        union RenamedUnionField
        {
            int renamed_to;
            double j;
        };
        $DECL_SPEC int renamedUnionField(union RenamedUnionField param);";
    $SOURCE2 .= "
        int renamedUnionField(union RenamedUnionField param) { return 0; }";
    
    # Removed_Field_And_Size
    $HEADER1 .= "
        struct RemovedFieldAndSize
        {
            int i;
            long j;
            double k;
            struct RemovedFieldAndSize* p;
            int removed_member1;
            int removed_member2;
        };
        $DECL_SPEC int removedFieldAndSize(struct RemovedFieldAndSize param, int param_2);";
    $SOURCE1 .= "
        int removedFieldAndSize(struct RemovedFieldAndSize param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct RemovedFieldAndSize
        {
            int i;
            long j;
            double k;
            struct RemovedFieldAndSize* p;
        };
        $DECL_SPEC int removedFieldAndSize(struct RemovedFieldAndSize param, int param_2);";
    $SOURCE2 .= "
        int removedFieldAndSize(struct RemovedFieldAndSize param, int param_2) { return param_2; }";
    
    # Removed_Middle_Field
    $HEADER1 .= "
        struct RemovedMiddleField
        {
            int i;
            int removed_middle_member;
            long j;
            double k;
            struct RemovedMiddleField* p;
        };
        $DECL_SPEC int removedMiddleField(struct RemovedMiddleField param, int param_2);";
    $SOURCE1 .= "
        int removedMiddleField(struct RemovedMiddleField param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct RemovedMiddleField
        {
            int i;
            long j;
            double k;
            struct RemovedMiddleField* p;
        };
        $DECL_SPEC int removedMiddleField(struct RemovedMiddleField param, int param_2);";
    $SOURCE2 .= "
        int removedMiddleField(struct RemovedMiddleField param, int param_2) { return param_2; }";
    
    # Enum_Member_Value
    $HEADER1 .= "
        enum EnumMemberValue
        {
            MEMBER_1=1,
            MEMBER_2=2
        };
        $DECL_SPEC int enumMemberValue(enum EnumMemberValue param);";
    $SOURCE1 .= "
        int enumMemberValue(enum EnumMemberValue param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    $HEADER2 .= "
        enum EnumMemberValue
        {
            MEMBER_1=2,
            MEMBER_2=1
        };
        $DECL_SPEC int enumMemberValue(enum EnumMemberValue param);";
    $SOURCE2 .= "
        int enumMemberValue(enum EnumMemberValue param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";

    # Enum_Member_Removed
    $HEADER1 .= "
        enum EnumMemberRemoved
        {
            MEMBER=1,
            MEMBER_REMOVED=2
        };
        $DECL_SPEC int enumMemberRemoved(enum EnumMemberRemoved param);";
    $SOURCE1 .= "
        int enumMemberRemoved(enum EnumMemberRemoved param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    $HEADER2 .= "
        enum EnumMemberRemoved
        {
            MEMBER=1
        };
        $DECL_SPEC int enumMemberRemoved(enum EnumMemberRemoved param);";
    $SOURCE2 .= "
        int enumMemberRemoved(enum EnumMemberRemoved param)
        {
            switch(param)
            {
                case 1:
                    return 1;
            }
            return 0;
        }";
    
    # Enum_Member_Name
    $HEADER1 .= "
        enum EnumMemberName
        {
            BRANCH_1=1,
            BRANCH_2=2
        };
        $DECL_SPEC int enumMemberName(enum EnumMemberName param);";
    $SOURCE1 .= "
        int enumMemberName(enum EnumMemberName param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    $HEADER2 .= "
        enum EnumMemberName
        {
            BRANCH_FIRST=1,
            BRANCH_SECOND=2
        };
        $DECL_SPEC int enumMemberName(enum EnumMemberName param);";
    $SOURCE2 .= "
        int enumMemberName(enum EnumMemberName param)
        {
            switch(param)
            {
                case 1:
                    return 1;
                case 2:
                    return 2;
            }
            return 0;
        }";
    
    # Field_Type_And_Size
    $HEADER1 .= "
        struct FieldTypeAndSize
        {
            int i;
            long j;
            double k;
            struct FieldTypeAndSize* p;
        };
        $DECL_SPEC int fieldTypeAndSize(struct FieldTypeAndSize param, int param_2);";
    $SOURCE1 .= "
        int fieldTypeAndSize(struct FieldTypeAndSize param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct FieldTypeAndSize
        {
            int i;
            long long j;
            double k;
            struct FieldTypeAndSize* p;
        };
        $DECL_SPEC int fieldTypeAndSize(struct FieldTypeAndSize param, int param_2);";
    $SOURCE2 .= "
        int fieldTypeAndSize(struct FieldTypeAndSize param, int param_2) { return param_2; }";
    
    # Field_Type
    $HEADER1 .= "
        struct FieldType
        {
            int i;
            long j;
            double k;
            struct FieldType* p;
        };
        $DECL_SPEC int fieldType(struct FieldType param, int param_2);";
    $SOURCE1 .= "
        int fieldType(struct FieldType param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct FieldType
        {
            float i;
            long j;
            double k;
            struct FieldType* p;
        };
        $DECL_SPEC int fieldType(struct FieldType param, int param_2);";
    $SOURCE2 .= "
        int fieldType(struct FieldType param, int param_2) { return param_2; }";
    
    # Field_BaseType
    $HEADER1 .= "
        struct FieldBaseType
        {
            int i;
            long *j;
            double k;
            struct FieldBaseType* p;
        };
        $DECL_SPEC int fieldBaseType(struct FieldBaseType param, int param_2);";
    $SOURCE1 .= "
        int fieldBaseType(struct FieldBaseType param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct FieldBaseType
        {
            int i;
            long long *j;
            double k;
            struct FieldBaseType* p;
        };
        $DECL_SPEC int fieldBaseType(struct FieldBaseType param, int param_2);";
    $SOURCE2 .= "
        int fieldBaseType(struct FieldBaseType param, int param_2) { return param_2; }";
    
    # Field_PointerLevel (and Size)
    $HEADER1 .= "
        struct FieldPointerLevelAndSize
        {
            int i;
            long long j;
            double k;
            struct FieldPointerLevelAndSize* p;
        };
        $DECL_SPEC int fieldPointerLevelAndSize(struct FieldPointerLevelAndSize param, int param_2);";
    $SOURCE1 .= "
        int fieldPointerLevelAndSize(struct FieldPointerLevelAndSize param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct FieldPointerLevelAndSize
        {
            int i;
            long long *j;
            double k;
            struct FieldPointerLevelAndSize* p;
        };
        $DECL_SPEC int fieldPointerLevelAndSize(struct FieldPointerLevelAndSize param, int param_2);";
    $SOURCE2 .= "
        int fieldPointerLevelAndSize(struct FieldPointerLevelAndSize param, int param_2) { return param_2; }";
    
    # Field_PointerLevel
    $HEADER1 .= "
        struct FieldPointerLevel
        {
            int i;
            long *j;
            double k;
            struct FieldPointerLevel* p;
        };
        $DECL_SPEC int fieldPointerLevel(struct FieldPointerLevel param, int param_2);";
    $SOURCE1 .= "
        int fieldPointerLevel(struct FieldPointerLevel param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct FieldPointerLevel
        {
            int i;
            long **j;
            double k;
            struct FieldPointerLevel* p;
        };
        $DECL_SPEC int fieldPointerLevel(struct FieldPointerLevel param, int param_2);";
    $SOURCE2 .= "
        int fieldPointerLevel(struct FieldPointerLevel param, int param_2) { return param_2; }";
    
    # Added_Interface
    $HEADER2 .= "
        $DECL_SPEC int addedFunc4(int param);";
    $SOURCE2 .= "
        int addedFunc4(int param) { return param; }";
    
    # Removed_Interface
    $HEADER1 .= "
        $DECL_SPEC int removedFunc(int param);";
    $SOURCE1 .= "
        int removedFunc(int param) { return param; }";
    
    # Parameter_Type_And_Size
    $HEADER1 .= "
        $DECL_SPEC int parameterTypeAndSize(int param, int other_param);";
    $SOURCE1 .= "
        int parameterTypeAndSize(int param, int other_param) { return other_param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterTypeAndSize(long long param, int other_param);";
    $SOURCE2 .= "
        int parameterTypeAndSize(long long param, int other_param) { return other_param; }";

    # Parameter_Type_And_Size (test calling conventions)
    $HEADER1 .= "\n
        $DECL_SPEC int parameterCallingConvention(int p1, int p2, int p3);";
    $SOURCE1 .= "
        int parameterCallingConvention(int p1, int p2, int p3) { return 0; }";
    
    $HEADER2 .= "\n
        $DECL_SPEC float parameterCallingConvention(char p1, int p2, int p3);";
    $SOURCE2 .= "
        float parameterCallingConvention(char p1, int p2, int p3) { return 7.0f; }";
    
    # Parameter_Type
    $HEADER1 .= "
        $DECL_SPEC int parameterType(int param, int other_param);";
    $SOURCE1 .= "
        int parameterType(int param, int other_param) { return other_param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterType(float param, int other_param);";
    $SOURCE2 .= "
        int parameterType(float param, int other_param) { return other_param; }";
    
    # Parameter_Became_Non_Const
    $HEADER1 .= "
        $DECL_SPEC int parameterBecameNonConst(int const* param);";
    $SOURCE1 .= "
        int parameterBecameNonConst(int const* param) { return *param; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterBecameNonConst(int* param);";
    $SOURCE2 .= "
        int parameterBecameNonConst(int* param) {
            *param=10;
            return *param;
        }";
    
    # Parameter_BaseType (Typedef)
    $HEADER1 .= "
        typedef int* PARAM_TYPEDEF;
        $DECL_SPEC int parameterBaseTypedefChange(PARAM_TYPEDEF param);";
    $SOURCE1 .= "
        int parameterBaseTypedefChange(PARAM_TYPEDEF param) { return 0; }";
    
    $HEADER2 .= "
        typedef const int* PARAM_TYPEDEF;
        $DECL_SPEC int parameterBaseTypedefChange(PARAM_TYPEDEF param);";
    $SOURCE2 .= "
        int parameterBaseTypedefChange(PARAM_TYPEDEF param) { return 0; }";
    
    # Parameter_BaseType
    $HEADER1 .= "
        $DECL_SPEC int parameterBaseTypeChange(int *param);";
    $SOURCE1 .= "
        int parameterBaseTypeChange(int *param) { return sizeof(*param); }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterBaseTypeChange(long long *param);";
    $SOURCE2 .= "
        int parameterBaseTypeChange(long long *param) { return sizeof(*param); }";
    
    # Parameter_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC long long parameterPointerLevelAndSize(long long param);";
    $SOURCE1 .= "
        long long parameterPointerLevelAndSize(long long param) { return param; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long parameterPointerLevelAndSize(long long *param);";
    $SOURCE2 .= "
        long long parameterPointerLevelAndSize(long long *param) { return param[5]; }";
    
    # Parameter_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC int parameterPointerLevel(int *param);";
    $SOURCE1 .= "
        int parameterPointerLevel(int *param) { return param[5]; }";
    
    $HEADER2 .= "
        $DECL_SPEC int parameterPointerLevel(int **param);";
    $SOURCE2 .= "
        int parameterPointerLevel(int **param) { return param[5][5]; }";
    
    # Return_Type_And_Size
    $HEADER1 .= "
        $DECL_SPEC int returnTypeAndSize(int param);";
    $SOURCE1 .= "
        int returnTypeAndSize(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long returnTypeAndSize(int param);";
    $SOURCE2 .= "
        long long returnTypeAndSize(int param) { return 0; }";
    
    # Return_Type
    $HEADER1 .= "
        $DECL_SPEC int returnType(int param);";
    $SOURCE1 .= "
        int returnType(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC float returnType(int param);";
    $SOURCE2 .= "
        float returnType(int param) { return 0.7; }";

    # Return_Type_Became_Void ("int" to "void")
    $HEADER1 .= "
        $DECL_SPEC int returnTypeChangeToVoid(int param);";
    $SOURCE1 .= "
        int returnTypeChangeToVoid(int param) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC void returnTypeChangeToVoid(int param);";
    $SOURCE2 .= "
        void returnTypeChangeToVoid(int param) { return; }";

    # Return_Type ("struct" to "void*")
    $HEADER1 .= "
        struct SomeStruct {int A;long B;};
        $DECL_SPEC struct SomeStruct* returnTypeChangeToVoidPtr(int param);";
    $SOURCE1 .= "
        struct SomeStruct* returnTypeChangeToVoidPtr(int param) { return (struct SomeStruct*)0; }";
    
    $HEADER2 .= "
        struct SomeStruct {int A;int B;};
        $DECL_SPEC void* returnTypeChangeToVoidPtr(int param);";
    $SOURCE2 .= "
        void* returnTypeChangeToVoidPtr(int param) { return (void*)0; }";

    # Return_Type_From_Void_And_Stack_Layout ("void" to "struct")
    $HEADER1 .= "
        $DECL_SPEC void returnTypeChangeFromVoidToStruct(int param);";
    $SOURCE1 .= "
        void returnTypeChangeFromVoidToStruct(int param) { return; }";
    
    $HEADER2 .= "
        $DECL_SPEC struct SomeStruct returnTypeChangeFromVoidToStruct(int param);";
    $SOURCE2 .= "
        struct SomeStruct returnTypeChangeFromVoidToStruct(int param) {
            struct SomeStruct obj = {1,2};
            return obj;
        }";

    # Return_Type_Became_Void_And_Stack_Layout ("struct" to "void")
    $HEADER1 .= "
        $DECL_SPEC struct SomeStruct returnTypeChangeFromStructToVoid(int param);";
    $SOURCE1 .= "
        struct SomeStruct returnTypeChangeFromStructToVoid(int param) {
            struct SomeStruct obj = {1,2};
            return obj;
        }";
    
    $HEADER2 .= "
        $DECL_SPEC void returnTypeChangeFromStructToVoid(int param);";
    $SOURCE2 .= "
        void returnTypeChangeFromStructToVoid(int param) { return; }";
    
    # Return_Type_From_Void_And_Stack_Layout (safe, "void" to "long")
    $HEADER1 .= "
        $DECL_SPEC void returnTypeChangeFromVoidToLong(int param);";
    $SOURCE1 .= "
        void returnTypeChangeFromVoidToLong(int param) { return; }";
    
    $HEADER2 .= "
        $DECL_SPEC long returnTypeChangeFromVoidToLong(int param);";
    $SOURCE2 .= "
        long returnTypeChangeFromVoidToLong(int param) { return 0; }";
    
    # Return_Type_From_Register_To_Stack ("int" to "struct")
    $HEADER1 .= "
        $DECL_SPEC int returnTypeChangeFromIntToStruct(int param);";
    $SOURCE1 .= "
        int returnTypeChangeFromIntToStruct(int param) { return param; }";
    
    $HEADER2 .= "
        $DECL_SPEC struct SomeStruct returnTypeChangeFromIntToStruct(int param);";
    $SOURCE2 .= "
        struct SomeStruct returnTypeChangeFromIntToStruct(int param) {
            struct SomeStruct obj = {1,2};
            return obj;
        }";
    
    # Return_Type_From_Stack_To_Register (from struct to int)
    $HEADER1 .= "
        $DECL_SPEC struct SomeStruct returnTypeChangeFromStructToInt(int param);";
    $SOURCE1 .= "
        struct SomeStruct returnTypeChangeFromStructToInt(int param) {
            struct SomeStruct obj = {1,2};
            return obj;
        }";
    
    $HEADER2 .= "
        $DECL_SPEC int returnTypeChangeFromStructToInt(int param);";
    $SOURCE2 .= "
        int returnTypeChangeFromStructToInt(int param) { return param; }";

     # Return_Type_From_Stack_To_Register (from struct to int, without parameters)
    $HEADER1 .= "
        $DECL_SPEC struct SomeStruct returnTypeChangeFromStructToIntWithNoParams();";
    $SOURCE1 .= "
        struct SomeStruct returnTypeChangeFromStructToIntWithNoParams() {
            struct SomeStruct obj = {1,2};
            return obj;
        }";
    
    $HEADER2 .= "
        $DECL_SPEC int returnTypeChangeFromStructToIntWithNoParams();";
    $SOURCE2 .= "
        int returnTypeChangeFromStructToIntWithNoParams() { return 0; }";
    
    # Return_BaseType
    $HEADER1 .= "
        $DECL_SPEC int *returnBaseTypeChange(int param);";
    $SOURCE1 .= "
        int *returnBaseTypeChange(int param) { return (int*)0; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long *returnBaseTypeChange(int param);";
    $SOURCE2 .= "
        long long *returnBaseTypeChange(int param) { return (long long*)0; }";
    
    # Return_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC long long returnPointerLevelAndSize(int param);";
    $SOURCE1 .= "
        long long returnPointerLevelAndSize(int param) { return 100; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long *returnPointerLevelAndSize(int param);";
    $SOURCE2 .= "
        long long *returnPointerLevelAndSize(int param) { return (long long *)0; }";
    
    # Return_PointerLevel
    $HEADER1 .= "
        $DECL_SPEC long long *returnPointerLevel(int param);";
    $SOURCE1 .= "
        long long *returnPointerLevel(int param) { return (long long *)0; }";
    
    $HEADER2 .= "
        $DECL_SPEC long long **returnPointerLevel(int param);";
    $SOURCE2 .= "
        long long **returnPointerLevel(int param) { return (long long **)0; }";
    
    # Size (typedef to anon structure)
    $HEADER1 .= "
        typedef struct
        {
            int i;
            long j;
            double k;
        } AnonTypedef;
        $DECL_SPEC int anonTypedef(AnonTypedef param);";
    $SOURCE1 .= "
        int anonTypedef(AnonTypedef param) { return 0; }";
    
    $HEADER2 .= "
        typedef struct
        {
            int i;
            long j;
            double k;
            union {
                int dummy[256];
                struct {
                    char q_skiptable[256];
                    const char *p;
                    int l;
                } p;
            };
        } AnonTypedef;
        $DECL_SPEC int anonTypedef(AnonTypedef param);";
    $SOURCE2 .= "
        int anonTypedef(AnonTypedef param) { return 0; }";
    
    # Size (safe: opaque)
    $HEADER1 .= "
        struct OpaqueType
        {
            long long i[5];
            long j;
            double k;
            struct OpaqueType* p;
        };
        $DECL_SPEC int opaqueTypeUse(struct OpaqueType param, int param_2);";
    $SOURCE1 .= "
        int opaqueTypeUse(struct OpaqueType param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct OpaqueType
        {
            long long i[5];
            long long j;
            double k;
            struct OpaqueType* p;
        };
        $DECL_SPEC int opaqueTypeUse(struct OpaqueType param, int param_2);";
    $SOURCE2 .= "
        int opaqueTypeUse(struct OpaqueType param, int param_2) { return param_2; }";
    
    # Size (safe: internal)
    $HEADER1 .= "
        struct InternalType
        {
            long long i[5];
            long j;
            double k;
            struct InternalType* p;
        };
        $DECL_SPEC int internalTypeUse(struct InternalType param, int param_2);";
    $SOURCE1 .= "
        int internalTypeUse(struct InternalType param, int param_2) { return param_2; }";
    
    $HEADER2 .= "
        struct InternalType
        {
            long long i[5];
            long long j;
            double k;
            struct InternalType* p;
        };
        $DECL_SPEC int internalTypeUse(struct InternalType param, int param_2);";
    $SOURCE2 .= "
        int internalTypeUse(struct InternalType param, int param_2) { return param_2; }";
    
    if($OSgroup eq "linux")
    {# check versions
        # Changed version
        $HEADER1 .= "
            $DECL_SPEC int changedVersion(int param);
            $DECL_SPEC int changedDefaultVersion(int param);";
        $SOURCE1 .= "
            int changedVersion(int param) { return 0; }
            __asm__(\".symver changedVersion,changedVersion\@VERSION_2.0\");
            int changedDefaultVersion(int param) { return 0; }";
        
        $HEADER2 .= "
            $DECL_SPEC int changedVersion(int param);
            $DECL_SPEC int changedDefaultVersion(long param);";
        $SOURCE2 .= "
            int changedVersion(int param) { return 0; }
            __asm__(\".symver changedVersion,changedVersion\@VERSION_3.0\");
            int changedDefaultVersion(long param) { return 0; }";
        
        # Unchanged version
        $HEADER1 .= "
            $DECL_SPEC int unchangedVersion(int param);
            $DECL_SPEC int unchangedDefaultVersion(int param);";
        $SOURCE1 .= "
            int unchangedVersion(int param) { return 0; }
            __asm__(\".symver unchangedVersion,unchangedVersion\@VERSION_1.0\");
            int unchangedDefaultVersion(int param) { return 0; }";
        
        $HEADER2 .= "
            $DECL_SPEC int unchangedVersion(int param);
            $DECL_SPEC int unchangedDefaultVersion(int param);";
        $SOURCE2 .= "
            int unchangedVersion(int param) { return 0; }
            __asm__(\".symver unchangedVersion,unchangedVersion\@VERSION_1.0\");
            int unchangedDefaultVersion(int param) { return 0; }";
        
        # Non-Default to Default
        $HEADER1 .= "
            $DECL_SPEC int changedVersionToDefault(int param);";
        $SOURCE1 .= "
            int changedVersionToDefault(int param) { return 0; }
            __asm__(\".symver changedVersionToDefault,changedVersionToDefault\@VERSION_1.0\");";
        
        $HEADER2 .= "
            $DECL_SPEC int changedVersionToDefault(long param);";
        $SOURCE2 .= "
            int changedVersionToDefault(long param) { return 0; }";
        
        # Default to Non-Default
        $HEADER1 .= "
            $DECL_SPEC int changedVersionToNonDefault(int param);";
        $SOURCE1 .= "
            int changedVersionToNonDefault(int param) { return 0; }";
        
        $HEADER2 .= "
            $DECL_SPEC int changedVersionToNonDefault(long param);";
        $SOURCE2 .= "
            int changedVersionToNonDefault(long param) { return 0; }
            __asm__(\".symver changedVersionToNonDefault,changedVersionToNonDefault\@VERSION_3.0\");";
        
        # Added version
        $HEADER1 .= "
            $DECL_SPEC int addedVersion(int param);
            $DECL_SPEC int addedDefaultVersion(int param);";
        $SOURCE1 .= "
            int addedVersion(int param) { return 0; }
            int addedDefaultVersion(int param) { return 0; }";
        
        $HEADER2 .= "
            $DECL_SPEC int addedVersion(int param);
            $DECL_SPEC int addedDefaultVersion(int param);";
        $SOURCE2 .= "
            int addedVersion(int param) { return 0; }
            __asm__(\".symver addedVersion,addedVersion\@VERSION_2.0\");
            int addedDefaultVersion(int param) { return 0; }";

        # Removed version
        $HEADER1 .= "
            $DECL_SPEC int removedVersion(int param);
            $DECL_SPEC int removedVersion2(int param);
            $DECL_SPEC int removedDefaultVersion(int param);";
        $SOURCE1 .= "
            int removedVersion(int param) { return 0; }
            __asm__(\".symver removedVersion,removedVersion\@VERSION_1.0\");
            int removedVersion2(int param) { return 0; }
            __asm__(\".symver removedVersion2,removedVersion\@VERSION_3.0\");
            int removedDefaultVersion(int param) { return 0; }";
        
        $HEADER2 .= "
            $DECL_SPEC int removedVersion(int param);
            $DECL_SPEC int removedVersion2(int param);
            $DECL_SPEC int removedDefaultVersion(int param);";
        $SOURCE2 .= "
            int removedVersion(int param) { return 0; }
            int removedVersion2(int param) { return 0; }
            __asm__(\".symver removedVersion2,removedVersion\@VERSION_3.0\");
            int removedDefaultVersion(int param) { return 0; }";
        
        # Return_Type (good versioning)
        $HEADER1 .= "
            $DECL_SPEC int goodVersioning(int param);";
        $SOURCE1 .= "
            int goodVersioning(int param) { return 0; }
            __asm__(\".symver goodVersioning,goodVersioning\@VERSION_1.0\");";
        
        $HEADER2 .= "
            $DECL_SPEC int goodVersioningOld(int param);";
        $SOURCE2 .= "
            int goodVersioningOld(int param) { return 0; }
            __asm__(\".symver goodVersioningOld,goodVersioning\@VERSION_1.0\");";
        
        $HEADER2 .= "
            $DECL_SPEC float goodVersioning(int param);";
        $SOURCE2 .= "
            float goodVersioning(int param) { return 0.7; }
            __asm__(\".symver goodVersioning,goodVersioning\@VERSION_2.0\");";
        
        # Return_Type (bad versioning)
        $HEADER1 .= "
            $DECL_SPEC int badVersioning(int param);";
        $SOURCE1 .= "
            int badVersioning(int param) { return 0; }
            __asm__(\".symver badVersioning,badVersioning\@VERSION_1.0\");";
        
        $HEADER2 .= "
            $DECL_SPEC float badVersioningOld(int param);";
        $SOURCE2 .= "
            float badVersioningOld(int param) { return 0.7; }
            __asm__(\".symver badVersioningOld,badVersioning\@VERSION_1.0\");";
        
        $HEADER2 .= "
            $DECL_SPEC float badVersioning(int param);";
        $SOURCE2 .= "
            float badVersioning(int param) { return 0.7; }
            __asm__(\".symver badVersioning,badVersioning\@VERSION_2.0\");";
    }
    # unnamed struct/union fields within structs/unions
    $HEADER1 .= "
        typedef struct
        {
            int a;
            union {
                int b;
                float c;
            };
            int d;
        } UnnamedTypeSize;
        $DECL_SPEC int unnamedTypeSize(UnnamedTypeSize param);";
    $SOURCE1 .= "
        int unnamedTypeSize(UnnamedTypeSize param) { return 0; }";
    
    $HEADER2 .= "
        typedef struct
        {
            int a;
            union {
                long double b;
                float c;
            };
            int d;
        } UnnamedTypeSize;
        $DECL_SPEC int unnamedTypeSize(UnnamedTypeSize param);";
    $SOURCE2 .= "
        int unnamedTypeSize(UnnamedTypeSize param) { return 0; }";
    
    # Changed_Constant
    $HEADER1 .= "
        #define PUBLIC_CONSTANT \"old_value\"";
    $HEADER2 .= "
        #define PUBLIC_CONSTANT \"new_value\"";
    
    # Changed_Constant (Safe)
    $HEADER1 .= "
        #define INTEGER_CONSTANT 0x01";
    $HEADER2 .= "
        #define INTEGER_CONSTANT 1";
    
    # Changed_Constant (Safe)
    $HEADER1 .= "
        #define PRIVATE_CONSTANT \"old_value\"
        #undef PRIVATE_CONSTANT";
    $HEADER2 .= "
        #define PRIVATE_CONSTANT \"new_value\"
        #undef PRIVATE_CONSTANT";
    
    # Added_Field (union)
    $HEADER1 .= "
        union UnionTypeAddedField
        {
            int a;
            struct {
                int b;
                float c;
            };
            int d;
        };
        $DECL_SPEC int unionTypeAddedField(union UnionTypeAddedField param);";
    $SOURCE1 .= "
        int unionTypeAddedField(union UnionTypeAddedField param) { return 0; }";
    
    $HEADER2 .= "
        union UnionTypeAddedField
        {
            int a;
            struct {
                long double x, y;
            } new_field;
            struct {
                int b;
                float c;
            };
            int d;
        };
        $DECL_SPEC int unionTypeAddedField(union UnionTypeAddedField param);";
    $SOURCE2 .= "
        int unionTypeAddedField(union UnionTypeAddedField param) { return 0; }";
    
    # Prameter_BaseType (typedef)
    $HEADER1 .= "
        typedef float TYPEDEF_TYPE;
        $DECL_SPEC int parameterTypedefChange(TYPEDEF_TYPE param);";
    $SOURCE1 .= "
        int parameterTypedefChange(TYPEDEF_TYPE param) { return 1.0; }";
    
    $HEADER2 .= "
        typedef int TYPEDEF_TYPE;
        $DECL_SPEC int parameterTypedefChange(TYPEDEF_TYPE param);";
    $SOURCE2 .= "
        int parameterTypedefChange(TYPEDEF_TYPE param) { return 1; }";
    
    # Field_BaseType (typedef in member type)
    $HEADER1 .= "
        typedef float TYPEDEF_TYPE_2;
        struct FieldBaseTypedefChange{
        TYPEDEF_TYPE_2 m;};
        $DECL_SPEC int fieldBaseTypedefChange(struct FieldBaseTypedefChange param);";
    $SOURCE1 .= "
        int fieldBaseTypedefChange(struct FieldBaseTypedefChange param) { return 1; }";
    
    $HEADER2 .= "
        typedef int TYPEDEF_TYPE_2;
        struct FieldBaseTypedefChange{
        TYPEDEF_TYPE_2 m;};
        $DECL_SPEC int fieldBaseTypedefChange(struct FieldBaseTypedefChange param);";
    $SOURCE2 .= "
        int fieldBaseTypedefChange(struct FieldBaseTypedefChange param) { return 1; }";
    
    # "C++" keywords in "C" code
    $HEADER1 .= "
        $DECL_SPEC int explicit(int class, int virtual, int (*this)(int));";
    $SOURCE1 .= "
        $DECL_SPEC int explicit(int class, int virtual, int (*this)(int)) { return 0; }";
    
    $HEADER2 .= "
        $DECL_SPEC int explicit(int class, int virtual);";
    $SOURCE2 .= "
        $DECL_SPEC int explicit(int class, int virtual) { return 0; }";
    
    # Regression
    $HEADER1 .= "
        $DECL_SPEC int* testRegression(int *pointer, char const *name, ...);";
    $SOURCE1 .= "
        int* testRegression(int *pointer, char const *name, ...) { return 0; }";

    $HEADER2 .= "
        $DECL_SPEC int* testRegression(int *pointer, char const *name, ...);";
    $SOURCE2 .= "
        int* testRegression(int *pointer, char const *name, ...) { return 0; }";
    
    runTests("simple_lib_c", "C", $HEADER1, $SOURCE1, $HEADER2, $SOURCE2, "OpaqueType", "internalTypeUse");
}

sub runTests($$$$$$$$)
{
    my ($LibName, $Lang, $HEADER1, $SOURCE1, $HEADER2, $SOURCE2, $Opaque, $Private) = @_;
    my $Ext = ($Lang eq "C++")?"cpp":"c";
    rmtree($LibName);
    # creating test suite
    my $Path_v1 = "$LibName/simple_lib.v1";
    my $Path_v2 = "$LibName/simple_lib.v2";
    mkpath($Path_v1);
    mkpath($Path_v2);
    writeFile("$Path_v1/simple_lib.h", $HEADER1."\n");
    writeFile("$Path_v1/simple_lib.$Ext", "#include \"simple_lib.h\"\n".$SOURCE1."\n");
    writeFile("$LibName/v1.xml", "
        <version>
            1.0
        </version>
        
        <headers>
            ".get_abs_path($Path_v1)."
        </headers>
        
        <libs>
            ".get_abs_path($Path_v1)."
        </libs>
        
        <skip_types>
            $Opaque
        </skip_types>
        
        <skip_symbols>
            $Private
        </skip_symbols>
        
        <include_paths>
            ".get_abs_path($Path_v1)."
        </include_paths>\n");
    writeFile("$Path_v1/test.$Ext", "
        #include \"simple_lib.h\"
        int main()
        {
            return 0;
        }\n");
    
    writeFile("$Path_v2/simple_lib.h", $HEADER2."\n");
    writeFile("$Path_v2/simple_lib.$Ext", "#include \"simple_lib.h\"\n".$SOURCE2."\n");
    writeFile("$LibName/v2.xml", "
        <version>
            2.0
        </version>
        
        <headers>
            ".get_abs_path($Path_v2)."
        </headers>
        
        <libs>
            ".get_abs_path($Path_v2)."
        </libs>
        
        <skip_types>
            $Opaque
        </skip_types>
        
        <skip_symbols>
            $Private
        </skip_symbols>
        
        <include_paths>
            ".get_abs_path($Path_v2)."
        </include_paths>\n");
    writeFile("$Path_v2/test.$Ext", "
        #include \"simple_lib.h\"
        int main()
        {
            return 0;
        }\n");
    
    my ($BuildCmd, $BuildCmd_Test) = ("", "");
    if($OStarget eq "windows")
    {
        check_win32_env(); # to run MS VC++ compiler
        my $CL = get_CmdPath("cl");
        if(not $CL) {
            exitStatus("Not_Found", "can't find \"cl\" compiler");
        }
        $BuildCmd = "$CL /LD simple_lib.$Ext >build_log.txt 2>&1";
        $BuildCmd_Test = "$CL test.$Ext simple_lib.$LIB_EXT";
    }
    elsif($OStarget eq "linux")
    {
        if($Lang eq "C")
        {# tests for symbol versioning
            writeFile("$Path_v1/version", "
                VERSION_1.0 {
                    unchangedDefaultVersion;
                    removedDefaultVersion;
                };
                VERSION_2.0 {
                    changedDefaultVersion;
                };
                VERSION_3.0 {
                    changedVersionToNonDefault;
                };
            ");
            writeFile("$Path_v2/version", "
                VERSION_1.0 {
                    unchangedDefaultVersion;
                    changedVersionToDefault;
                };
                VERSION_2.0 {
                    addedDefaultVersion;
                };
                VERSION_3.0 {
                    changedDefaultVersion;
                };
            ");
            $BuildCmd = $GCC_PATH." -Wl,--version-script version -shared -fkeep-inline-functions simple_lib.$Ext -o simple_lib.$LIB_EXT";
            $BuildCmd_Test = $GCC_PATH." -Wl,--version-script version test.$Ext simple_lib.$LIB_EXT -o test";
        }
        else
        {
            $BuildCmd = $GCC_PATH." -shared -fkeep-inline-functions -x c++ simple_lib.$Ext -o simple_lib.$LIB_EXT";
            $BuildCmd_Test = $GCC_PATH." -x c++ test.$Ext simple_lib.$LIB_EXT -o test";
        }
        if(getArch(1)=~/\A(arm|x86_64)\Z/i)
        {# relocation R_ARM_MOVW_ABS_NC against `a local symbol' can not be used when making a shared object; recompile with -fPIC
            $BuildCmd .= " -fPIC";
            $BuildCmd_Test .= " -fPIC";
        }
    }
    elsif($OStarget eq "macos")
    {# using GCC -dynamiclib -x c++ -lstdc++
        $BuildCmd = $GCC_PATH." -dynamiclib -fkeep-inline-functions".($Lang eq "C++"?" -x c++ -lstdc++":"")." simple_lib.$Ext -o simple_lib.$LIB_EXT";
        $BuildCmd_Test = $GCC_PATH." ".($Lang eq "C++"?" -x c++ -lstdc++":"")." test.$Ext simple_lib.$LIB_EXT -o test";
    }
    else
    {# default unix-like
     # symbian target
        $BuildCmd = $GCC_PATH." -shared -fkeep-inline-functions".($Lang eq "C++"?" -x c++":"")." simple_lib.$Ext -o simple_lib.$LIB_EXT";
        $BuildCmd_Test = $GCC_PATH." ".($Lang eq "C++"?" -x c++":"")." test.$Ext simple_lib.$LIB_EXT -o test";
    }
    my $MkContent = "all:\n\t$BuildCmd\ntest:\n\t$BuildCmd_Test\n";
    writeFile("$Path_v1/Makefile", $MkContent);
    writeFile("$Path_v2/Makefile", $MkContent);
    system("cd $Path_v1 && $BuildCmd >build-log.txt 2>&1");
    if($?) {
        exitStatus("Error", "can't compile \'$Path_v1/simple_lib.$Ext\'");
    }
    system("cd $Path_v2 && $BuildCmd >build-log.txt 2>&1");
    if($?) {
        exitStatus("Error", "can't compile \'$Path_v2/simple_lib.$Ext\'");
    }
    # running the tool
    my $Cmd = "perl $0 -l $LibName -d1 $LibName/v1.xml -d2 $LibName/v2.xml";
    if($TestDump) {
        $Cmd .= " -use-dumps";
    }
    if($CrossGcc) {
        $Cmd .= " -cross-gcc \"$CrossGcc\"";
    }
    if($Debug)
    { # debug mode
        $Cmd .= " -debug";
        print "$Cmd\n";
    }
    system($Cmd);
    my $CReport = readAttributes("compat_reports/$LibName/1.0_to_2.0/abi_compat_report.html");
    my $NProblems = $CReport->{"type_problems_high"}+$CReport->{"type_problems_medium"};
    $NProblems += $CReport->{"interface_problems_high"}+$CReport->{"interface_problems_medium"};
    $NProblems += $CReport->{"removed"};
    if(($LibName eq "simple_lib_c" and $NProblems>30)
    or ($LibName eq "simple_lib_cpp" and $NProblems>60)) {
        print "result: SUCCESS ($NProblems problems found)\n\n";
    }
    else {
        print STDERR "result: FAILED ($NProblems problems found)\n\n";
    }
}

sub appendFile($$)
{
    my ($Path, $Content) = @_;
    return if(not $Path);
    if(my $Dir = get_dirname($Path)) {
        mkpath($Dir);
    }
    open(FILE, ">>".$Path) || die ("can't open file \'$Path\': $!\n");
    print FILE $Content;
    close(FILE);
}

sub writeFile($$)
{
    my ($Path, $Content) = @_;
    return if(not $Path);
    if(my $Dir = get_dirname($Path)) {
        mkpath($Dir);
    }
    open (FILE, ">".$Path) || die ("can't open file \'$Path\': $!\n");
    print FILE $Content;
    close(FILE);
}

sub readFile($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -f $Path);
    open (FILE, $Path);
    local $/ = undef;
    my $Content = <FILE>;
    close(FILE);
    if($Path!~/\.(tu|class)\Z/) {
        $Content=~s/\r/\n/g;
    }
    return $Content;
}

sub getGccVersion($)
{
    my $LibVersion = $_[0];
    if($GCC_VERSION{$LibVersion})
    { # dump version
        return $GCC_VERSION{$LibVersion};
    }
    elsif($UsedDump{$LibVersion})
    { # old-version dumps
        return "unknown";
    }
    my $GccVersion = get_dumpversion($GCC_PATH); # host version
    if(not $GccVersion) {
        return "unknown";
    }
    return $GccVersion;
}

sub showArch($)
{
    my $Arch = $_[0];
    if($Arch eq "arm"
    or $Arch eq "mips") {
        return uc($Arch);
    }
    return $Arch;
}

sub getArch($)
{
    my $LibVersion = $_[0];
    if($CPU_ARCH{$LibVersion})
    { # dump version
        return $CPU_ARCH{$LibVersion};
    }
    elsif($UsedDump{$LibVersion})
    { # old-version dumps
        return "unknown";
    }
    if(defined $Cache{"getArch"}{$LibVersion}) {
        return $Cache{"getArch"}{$LibVersion};
    }
    my $Arch = get_dumpmachine($GCC_PATH); # host version
    if(not $Arch) {
        return "unknown";
    }
    if($Arch=~/\A([\w]{3,})(-|\Z)/) {
        $Arch = $1;
    }
    $Arch = "x86" if($Arch=~/\Ai[3-7]86\Z/);
    if($OSgroup eq "windows") {
        $Arch = "x86" if($Arch=~/win32|mingw32/i);
        $Arch = "x86_64" if($Arch=~/win64|mingw64/i);
    }
    $Cache{"getArch"}{$LibVersion} = $Arch;
    return $Arch;
}

sub get_Report_Header()
{
    my $ArchInfo = " on <span style='color:Blue;'>".showArch(getArch(1))."</span>";
    if(getArch(1) ne getArch(2) or getArch(1) eq "unknown")
    { # don't show architecture in the header
        $ArchInfo="";
    }
    my $Report_Header = "<h1><span class='nowrap'>Binary compatibility report for the <span style='color:Blue;'>$TargetLibraryFName</span> $TargetComponent</span>";
    $Report_Header .= " <span class='nowrap'>&nbsp;between <span style='color:Red;'>".$Descriptor{1}{"Version"}."</span> and <span style='color:Red;'>".$Descriptor{2}{"Version"}."</span> versions$ArchInfo</span>";
    if($AppPath) {
        $Report_Header .= " <span class='nowrap'>&nbsp;(relating to the portability of application <span style='color:Blue;'>".get_filename($AppPath)."</span>)</span>";
    }
    $Report_Header .= "</h1>\n";
    return $Report_Header;
}

sub get_SourceInfo()
{
    my $CheckedHeaders = "<a name='Headers'></a><h2>Header Files (".keys(%{$Target_Headers{1}}).")</h2><hr/>\n";
    $CheckedHeaders .= "<div class='h_list'>\n";
    foreach my $Header_Path (sort {lc($Target_Headers{1}{$a}{"Identity"}) cmp lc($Target_Headers{1}{$b}{"Identity"})} keys(%{$Target_Headers{1}}))
    {
        my $Identity = $Target_Headers{1}{$Header_Path}{"Identity"};
        my $Header_Name = get_filename($Identity);
        my $Dest_Comment = ($Identity=~/[\/\\]/)?" ($Identity)":"";
        $CheckedHeaders .= "$Header_Name$Dest_Comment<br/>\n";
    }
    $CheckedHeaders .= "</div>\n";
    $CheckedHeaders .= "<br/><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    my $CheckedLibs = "<a name='Libs'></a><h2>".ucfirst($SLIB_TYPE)." Libraries (".keys(%{$Library_Symbol{1}}).")</h2><hr/>\n";
    $CheckedLibs .= "<div class='lib_list'>\n";
    foreach my $Library (sort {lc($a) cmp lc($b)}  keys(%{$Library_Symbol{1}}))
    {
        $Library.=" (.$LIB_EXT)" if($Library!~/\.\w+\Z/);
        $CheckedLibs .= "$Library<br/>\n";
    }
    $CheckedLibs .= "</div>\n";
    $CheckedLibs .= "<br/><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    if($CheckObjectsOnly) {
        $CheckedHeaders = "";
    }
    if($CheckHeadersOnly) {
        $CheckedLibs = "";
    }
    return $CheckedHeaders.$CheckedLibs;
}

sub get_TypeProblems_Count($$$)
{
    my ($TypeChanges, $TargetPriority, $Level) = @_;
    my $Type_Problems_Count = 0;
    foreach my $Type_Name (sort keys(%{$TypeChanges}))
    {
        my %Kinds_Target = ();
        foreach my $Kind (keys(%{$TypeChanges->{$Type_Name}}))
        {
            foreach my $Location (keys(%{$TypeChanges->{$Type_Name}{$Kind}}))
            {
                my $Target = $TypeChanges->{$Type_Name}{$Kind}{$Location}{"Target"};
                my $Priority = getProblemSeverity($Level, $Kind);
                next if($Priority ne $TargetPriority);
                if($Kinds_Target{$Kind}{$Target}) {
                    next;
                }
                if(cmp_priority($Type_MaxPriority{$Level}{$Type_Name}{$Kind}{$Target}, $Priority))
                {# select a problem with the highest priority
                    next;
                }
                $Kinds_Target{$Kind}{$Target} = 1;
                $Type_Problems_Count += 1;
            }
        }
    }
    return $Type_Problems_Count;
}

sub get_Summary($)
{
    my $Level = $_[0];# API or ABI
    my ($Added, $Removed, $I_Problems_High, $I_Problems_Medium, $I_Problems_Low,
    $T_Problems_High, $T_Problems_Medium, $T_Problems_Low) = (0,0,0,0,0,0,0,0);
    # check rules
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            if(not defined $CompatRules{$Level}{$Kind})
            { # unknown rule
                if(not $UnknownRules{$Level}{$Kind})
                { # only one warning
                    print "WARNING: unknown rule \"$Kind\" (\"$Level\")\n";
                    $UnknownRules{$Level}{$Kind}=1;
                }
                delete($CompatProblems{$Interface}{$Kind});
            }
        }
    }
    my %TotalAffected = ();
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            if($CompatRules{$Level}{$Kind}{"Kind"} eq "Symbols")
            {
                foreach my $Location (sort keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    my $Priority = getProblemSeverity($Level, $Kind);
                    if($Kind eq "Added_Interface") {
                        $Added += 1;
                    }
                    elsif($Kind eq "Removed_Interface") {
                        $Removed += 1;
                    }
                    else
                    {
                        if($Priority eq "Safe") {
                            next;
                        }
                        elsif($Priority eq "High") {
                            $I_Problems_High += 1;
                        }
                        elsif($Priority eq "Medium") {
                            $I_Problems_Medium += 1;
                        }
                        elsif($Priority eq "Low") {
                            $I_Problems_Low += 1;
                        }
                        if($Priority ne "Low" or $StrictCompat) {
                            $TotalAffected{$Interface} = 1;
                        }
                    }
                }
            }
        }
    }
    my %TypeChanges = ();
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            if($CompatRules{$Level}{$Kind}{"Kind"} eq "Types")
            {
                foreach my $Location (sort {cmp_locations($b, $a)} sort keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
                    my $Target = $CompatProblems{$Interface}{$Kind}{$Location}{"Target"};
                    my $Priority = getProblemSeverity($Level, $Kind);
                    if($Priority eq "Safe") {
                        next;
                    }
                    if($Priority ne "Low" or $StrictCompat) {
                        $TotalAffected{$Interface}=1;
                    }
                    if(cmp_priority($Type_MaxPriority{$Level}{$Type_Name}{$Kind}{$Target}, $Priority))
                    {# select a problem with the highest priority
                        next;
                    }
                    %{$TypeChanges{$Type_Name}{$Kind}{$Location}} = %{$CompatProblems{$Interface}{$Kind}{$Location}};
                    $Type_MaxPriority{$Level}{$Type_Name}{$Kind}{$Target} = max_priority($Type_MaxPriority{$Level}{$Type_Name}{$Kind}{$Target}, $Priority);
                }
            }
        }
    }
    $T_Problems_High = get_TypeProblems_Count(\%TypeChanges, "High", $Level);
    $T_Problems_Medium = get_TypeProblems_Count(\%TypeChanges, "Medium", $Level);
    $T_Problems_Low = get_TypeProblems_Count(\%TypeChanges, "Low", $Level);

    # test info
    my $TestInfo = "<h2>Test Info</h2><hr/>\n";
    $TestInfo .= "<table cellpadding='3' cellspacing='0' class='summary'>\n";
    $TestInfo .= "<tr><th>".ucfirst($TargetComponent)." Name</th><td>$TargetLibraryFName</td></tr>\n";
    my ($Arch1, $Arch2) = (getArch(1), getArch(2));
    my ($GccV1, $GccV2) = (getGccVersion(1), getGccVersion(2));
    my (@VInf1, @VInf2, $AddTestInfo) = ();
    if($Arch1 ne "unknown"
    and $Arch2 ne "unknown")
    { # CPU arch
        if($Arch1 eq $Arch2)
        { # go to the separate section
            $AddTestInfo .= "<tr><th>CPU Architecture</th><td>".showArch($Arch1)."</td></tr>\n";
        }
        else
        { # go to the version number
            push(@VInf1, showArch($Arch1));
            push(@VInf2, showArch($Arch2));
        }
    }
    if($GccV1 ne "unknown"
    and $GccV2 ne "unknown"
    and $OStarget ne "windows")
    { # GCC version
        if($GccV1 eq $GccV2)
        { # go to the separate section
            $AddTestInfo .= "<tr><th>GCC Version</th><td>$GccV1</td></tr>\n";
        }
        else
        { # go to the version number
            push(@VInf1, "gcc ".$GccV1);
            push(@VInf2, "gcc ".$GccV2);
        }
    }
    # show long version names with GCC version and CPU architecture name (if different)
    $TestInfo .= "<tr><th>Version #1</th><td>".$Descriptor{1}{"Version"}.(@VInf1?" (".join(", ", reverse(@VInf1)).")":"")."</td></tr>\n";
    $TestInfo .= "<tr><th>Version #2</th><td>".$Descriptor{2}{"Version"}.(@VInf2?" (".join(", ", reverse(@VInf2)).")":"")."</td></tr>\n";
    $TestInfo .= $AddTestInfo;
    $TestInfo .= "</table>\n";
    
    # test results
    my $Summary = "<h2>Test Results</h2><hr/>\n";
    $Summary .= "<table cellpadding='3' cellspacing='0' class='summary'>";
    
    my $Headers_Link = "0";
    $Headers_Link = "<a href='#Headers' style='color:Blue;'>".keys(%{$Target_Headers{1}})."</a>" if(keys(%{$Target_Headers{1}})>0);
    $Summary .= "<tr><th>Total Header Files</th><td>".($CheckObjectsOnly?"0&nbsp;(not&nbsp;analyzed)":$Headers_Link)."</td></tr>\n";
    
    my $Libs_Link = "0";
    $Libs_Link = "<a href='#Libs' style='color:Blue;'>".keys(%{$Library_Symbol{1}})."</a>" if(keys(%{$Library_Symbol{1}})>0);
    $Summary .= "<tr><th>Total ".ucfirst($SLIB_TYPE)." Libraries</th><td>".($CheckHeadersOnly?"0&nbsp;(not&nbsp;analyzed)":$Libs_Link)."</td></tr>\n";
    my $TotalTypes = keys(%CheckedTypes);
    if(not $TotalTypes) {# list all the types
        $TotalTypes = keys(%{$TName_Tid{1}});
    }
    $Summary .= "<tr><th>Total Symbols / Types</th><td>".keys(%CheckedSymbols)." / ".$TotalTypes."</td></tr>\n";
    
    my $Affected = 0;
    if($CheckObjectsOnly)
    { # only removed exported symbols
        $Affected = $Removed*100/keys(%{$Symbol_Library{1}});
    }
    else
    { # changed and removed public symbols
        if(keys(%CheckedSymbols)) {
            $Affected = (keys(%TotalAffected)+$Removed)*100/keys(%CheckedSymbols);
        }
        else {
            $Affected = 0;
        }
    }
    if(my $Num = cut_off_number($Affected, 3))
    {
        if($Num eq "0.00") {
            $Num = cut_off_number($Affected, 7);
        }
        $Affected = $Num;
    }
    if($Affected>=100) {
        $Affected = 100;
    }
    my $Verdict = "";
    $CHECKER_VERDICT = $Removed+$I_Problems_High+$T_Problems_High+$T_Problems_Medium+$I_Problems_Medium;
    if($StrictCompat) {
        $CHECKER_VERDICT+=$T_Problems_Low+$I_Problems_Low;
    }
    else {
        $CHECKER_WARNING+=$T_Problems_Low+$I_Problems_Low;
    }
    if($CHECKER_VERDICT) {
        $Verdict = "<span style='color:Red;'><b>Incompatible<br/>($Affected%)</b></span>";
    }
    else {
        $Verdict = "<span style='color:Green;'><b>Compatible</b></span>";
    }
    my $STAT_FIRST_LINE = $CHECKER_VERDICT?"verdict:incompatible;":"verdict:compatible;";
    
    $Summary .= "<tr><th>Verdict</th><td>$Verdict</td></tr>";
    
    $Summary .= "</table>\n";
    
    
    $STAT_FIRST_LINE .= "affected:$Affected;";# in percents
    # problem summary
    my $Problem_Summary = "<h2>Problem Summary</h2><hr/>\n";
    $Problem_Summary .= "<table cellpadding='3' cellspacing='0' class='summary'>";
    $Problem_Summary .= "<tr><th></th><th style='text-align:center;'>Severity</th><th style='text-align:center;'>Count</th></tr>";
    
    my $Added_Link = "0";
    $Added_Link = "<a href='#Added' style='color:Blue;'>$Added</a>" if($Added>0);
    #$Added_Link = "n/a" if($CheckHeadersOnly);
    $STAT_FIRST_LINE .= "added:$Added;";
    $Problem_Summary .= "<tr><th>Added Symbols</th><td>-</td><td>$Added_Link</td></tr>\n";
    
    my $Removed_Link = "0";
    $Removed_Link = "<a href='#Removed' style='color:Blue;'>$Removed</a>" if($Removed>0);
    #$Removed_Link = "n/a" if($CheckHeadersOnly);
    $STAT_FIRST_LINE .= "removed:$Removed;";
    $Problem_Summary .= "<tr><th>Removed Symbols</th><td style='color:Red;'>High</td><td>$Removed_Link</td></tr>\n";
    
    my $TH_Link = "0";
    $TH_Link = "<a href='#Type_Problems_High' style='color:Blue;'>$T_Problems_High</a>" if($T_Problems_High>0);
    $TH_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "type_problems_high:$T_Problems_High;";
    $Problem_Summary .= "<tr><th rowspan='3'>Problems with<br/>Data Types</th><td style='color:Red;'>High</td><td>$TH_Link</td></tr>\n";
    
    my $TM_Link = "0";
    $TM_Link = "<a href='#Type_Problems_Medium' style='color:Blue;'>$T_Problems_Medium</a>" if($T_Problems_Medium>0);
    $TM_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "type_problems_medium:$T_Problems_Medium;";
    $Problem_Summary .= "<tr><td>Medium</td><td>$TM_Link</td></tr>\n";
    
    my $TL_Link = "0";
    $TL_Link = "<a href='#Type_Problems_Low' style='color:Blue;'>$T_Problems_Low</a>" if($T_Problems_Low>0);
    $TL_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "type_problems_low:$T_Problems_Low;";
    $Problem_Summary .= "<tr><td>Low</td><td>$TL_Link</td></tr>\n";
    
    my $IH_Link = "0";
    $IH_Link = "<a href='#Interface_Problems_High' style='color:Blue;'>$I_Problems_High</a>" if($I_Problems_High>0);
    $IH_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "interface_problems_high:$I_Problems_High;";
    $Problem_Summary .= "<tr><th rowspan='3'>Problems with<br/>Symbols</th><td style='color:Red;'>High</td><td>$IH_Link</td></tr>\n";
    
    my $IM_Link = "0";
    $IM_Link = "<a href='#Interface_Problems_Medium' style='color:Blue;'>$I_Problems_Medium</a>" if($I_Problems_Medium>0);
    $IM_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "interface_problems_medium:$I_Problems_Medium;";
    $Problem_Summary .= "<tr><td>Medium</td><td>$IM_Link</td></tr>\n";
    
    my $IL_Link = "0";
    $IL_Link = "<a href='#Interface_Problems_Low' style='color:Blue;'>$I_Problems_Low</a>" if($I_Problems_Low>0);
    $IL_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "interface_problems_low:$I_Problems_Low;";
    $Problem_Summary .= "<tr><td>Low</td><td>$IL_Link</td></tr>\n";
    
    my $ChangedConstants_Link = "0";
    if(keys(%CheckedSymbols) and keys(%ProblemsWithConstants)>0) {
        $ChangedConstants_Link = "<a href='#Changed_Constants' style='color:Blue;'>".keys(%ProblemsWithConstants)."</a>";
    }
    $ChangedConstants_Link = "n/a" if($CheckObjectsOnly);
    $STAT_FIRST_LINE .= "changed_constants:".keys(%ProblemsWithConstants).";";
    $Problem_Summary .= "<tr><th>Problems with<br/>Constants</th><td>Low</td><td>$ChangedConstants_Link</td></tr>\n";
    $CHECKER_WARNING+=keys(%ProblemsWithConstants);
    
    if($CheckImplementation)
    {
        my $ChangedImplementation_Link = "0";
        $ChangedImplementation_Link = "<a href='#Changed_Implementation' style='color:Blue;'>".keys(%ImplProblems)."</a>" if(keys(%ImplProblems)>0);
        $ChangedImplementation_Link = "n/a" if($CheckHeadersOnly);
        $STAT_FIRST_LINE .= "changed_implementation:".keys(%ImplProblems).";";
        $Problem_Summary .= "<tr><th>Problems with<br/>Implementation</th><td>Low</td><td>$ChangedImplementation_Link</td></tr>\n";
        $CHECKER_WARNING+=keys(%ImplProblems);
    }
    $STAT_FIRST_LINE .= "tool_version:$ABI_COMPLIANCE_CHECKER_VERSION";
    $Problem_Summary .= "</table>\n";
    return ($TestInfo.$Summary.$Problem_Summary, $STAT_FIRST_LINE);
}

sub cut_off_number($$)
{
    my ($num, $digs_to_cut) = @_;
    if($num!~/\./)
    {
        $num .= ".";
        foreach (1 .. $digs_to_cut-1) {
            $num .= "0";
        }
    }
    elsif($num=~/\.(.+)\Z/ and length($1)<$digs_to_cut-1)
    {
        foreach (1 .. $digs_to_cut - 1 - length($1)) {
            $num .= "0";
        }
    }
    elsif($num=~/\d+\.(\d){$digs_to_cut,}/) {
      $num=sprintf("%.".($digs_to_cut-1)."f", $num);
    }
    return $num;
}

sub get_Report_ChangedConstants()
{
    my ($CHANGED_CONSTANTS, %HeaderConstant);
    foreach my $Constant (keys(%ProblemsWithConstants)) {
        $HeaderConstant{$Constants{1}{$Constant}{"Header"}}{$Constant} = 1;
    }
    my $Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%HeaderConstant))
    {
        $CHANGED_CONSTANTS .= "<span class='h_name'>$HeaderName</span><br/>\n";
        foreach my $Name (sort {lc($a) cmp lc($b)} keys(%{$HeaderConstant{$HeaderName}}))
        {
            $Number += 1;
            my $Change = applyMacroses("Binary", "Changed_Constant", $CompatRules{"Binary"}{"Changed_Constant"}{"Change"}, $ProblemsWithConstants{$Name});
            my $Effect = $CompatRules{"Binary"}{"Changed_Constant"}{"Effect"};
            my $Report = "<tr><th>1</th><td align='left' valign='top'>".$Change."</td><td align='left' valign='top'>$Effect</td></tr>\n";
            $Report = $ContentDivStart."<table cellpadding='3' cellspacing='0' class='problems_table'><tr><th width='2%'></th><th width='47%'>Change</th><th>Effect</th></tr>".$Report."</table><br/>$ContentDivEnd\n";
            $Report = $ContentSpanStart."<span class='extendable'>[+]</span> ".$Name.$ContentSpanEnd."<br/>\n".$Report;
            $CHANGED_CONSTANTS .= insertIDs($Report);
        }
        $CHANGED_CONSTANTS .= "<br/>\n";
    }
    if($CHANGED_CONSTANTS) {
        $CHANGED_CONSTANTS = "<a name='Changed_Constants'></a><h2>Problems with Constants ($Number)</h2><hr/>\n".$CHANGED_CONSTANTS."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $CHANGED_CONSTANTS;
}

sub get_Report_Implementation()
{
    my ($CHANGED_IMPLEMENTATION, %HeaderLibFunc);
    foreach my $Interface (sort keys(%ImplProblems))
    {
        my $HeaderName = $CompleteSignature{1}{$Interface}{"Header"};
        my $DyLib = $Symbol_Library{1}{$Interface};
        $HeaderLibFunc{$HeaderName}{$DyLib}{$Interface} = 1;
    }
    my $Changed_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%HeaderLibFunc))
    {
        foreach my $DyLib (sort {lc($a) cmp lc($b)} keys(%{$HeaderLibFunc{$HeaderName}}))
        {
            my $FDyLib=$DyLib.($DyLib!~/\.\w+\Z/?" (.$LIB_EXT)":"");
            if($HeaderName) {
                $CHANGED_IMPLEMENTATION .= "<span class='h_name'>$HeaderName</span>, <span class='lib_name'>$FDyLib</span><br/>\n";
            }
            else {
                $CHANGED_IMPLEMENTATION .= "<span class='lib_name'>$DyLib</span><br/>\n";
            }
            my %NameSpace_Interface = ();
            foreach my $Interface (keys(%{$HeaderLibFunc{$HeaderName}{$DyLib}})) {
                $NameSpace_Interface{get_IntNameSpace($Interface, 2)}{$Interface} = 1;
            }
            foreach my $NameSpace (sort keys(%NameSpace_Interface))
            {
                $CHANGED_IMPLEMENTATION .= ($NameSpace)?"<span class='ns_title'>namespace</span> <span class='ns'>$NameSpace</span>"."<br/>\n":"";
                my @SortedInterfaces = sort {lc(get_Signature($a, 1)) cmp lc(get_Signature($b, 1))} keys(%{$NameSpace_Interface{$NameSpace}});
                foreach my $Interface (@SortedInterfaces)
                {
                    $Changed_Number += 1;
                    my $Signature = get_Signature($Interface, 1);
                    if($NameSpace) {
                        $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                    }
                    $CHANGED_IMPLEMENTATION .= insertIDs($ContentSpanStart.highLight_Signature_Italic_Color($Signature).$ContentSpanEnd."<br/>\n".$ContentDivStart."<span class='mangled'>[ symbol: <b>$Interface</b> ]</span>".$ImplProblems{$Interface}{"Diff"}."<br/><br/>".$ContentDivEnd."\n");
                }
            }
            $CHANGED_IMPLEMENTATION .= "<br/>\n";
        }
    }
    if($CHANGED_IMPLEMENTATION) {
        $CHANGED_IMPLEMENTATION = "<a name='Changed_Implementation'></a><h2>Problems with Implementation ($Changed_Number)</h2><hr/>\n".$CHANGED_IMPLEMENTATION."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $CHANGED_IMPLEMENTATION;
}

sub get_Report_Added($)
{
    my $Level = $_[0];# API or ABI
    my ($ADDED_INTERFACES, %FuncAddedInHeaderLib);
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            if($Kind eq "Added_Interface") {
                my $HeaderName = $CompleteSignature{2}{$Interface}{"Header"};
                my $DyLib = $Symbol_Library{2}{$Interface};
                $FuncAddedInHeaderLib{$HeaderName}{$DyLib}{$Interface} = 1;
            }
        }
    }
    my $Added_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%FuncAddedInHeaderLib))
    {
        foreach my $DyLib (sort {lc($a) cmp lc($b)} keys(%{$FuncAddedInHeaderLib{$HeaderName}}))
        {
            my $FDyLib=$DyLib.($DyLib!~/\.\w+\Z/?" (.$LIB_EXT)":"");
            if($HeaderName and $DyLib) {
                $ADDED_INTERFACES .= "<span class='h_name'>$HeaderName</span>, <span class='lib_name'>$FDyLib</span><br/>\n";
            }
            elsif($DyLib) {
                $ADDED_INTERFACES .= "<span class='lib_name'>$FDyLib</span><br/>\n";
            }
            elsif($HeaderName) {
                $ADDED_INTERFACES .= "<span class='h_name'>$HeaderName</span><br/>\n";
            }
            my %NameSpace_Interface = ();
            foreach my $Interface (keys(%{$FuncAddedInHeaderLib{$HeaderName}{$DyLib}})) {
                $NameSpace_Interface{get_IntNameSpace($Interface, 2)}{$Interface} = 1;
            }
            foreach my $NameSpace (sort keys(%NameSpace_Interface))
            {
                $ADDED_INTERFACES .= ($NameSpace)?"<span class='ns_title'>namespace</span> <span class='ns'>$NameSpace</span>"."<br/>\n":"";
                my @SortedInterfaces = sort {lc(get_Signature($a, 2)) cmp lc(get_Signature($b, 2))} keys(%{$NameSpace_Interface{$NameSpace}});
                foreach my $Interface (@SortedInterfaces)
                {
                    $Added_Number += 1;
                    my $SubReport = "";
                    my $Signature = get_Signature($Interface, 2);
                    if($NameSpace) {
                        $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                    }
                    if($Interface=~/\A(_Z|\?)/) {
                        if($Signature) {
                            $SubReport = insertIDs($ContentSpanStart.highLight_Signature_Italic_Color($Signature).$ContentSpanEnd."<br/>\n".$ContentDivStart."<span class='mangled'>[ symbol: <b>$Interface</b> ]</span><br/><br/>".$ContentDivEnd."\n");
                        }
                        else {
                            $SubReport = "<span class=\"iname\">".$Interface."</span><br/>\n";
                        }
                    }
                    else {
                        if($Signature) {
                            $SubReport = "<span class=\"iname\">".highLight_Signature_Italic_Color($Signature)."</span><br/>\n";
                        }
                        else {
                            $SubReport = "<span class=\"iname\">".$Interface."</span><br/>\n";
                        }
                    }
                    $ADDED_INTERFACES .= $SubReport;
                }
            }
            $ADDED_INTERFACES .= "<br/>\n";
        }
    }
    if($ADDED_INTERFACES) {
        $ADDED_INTERFACES = "<a name='Added'></a><h2>Added Symbols ($Added_Number)</h2><hr/>\n".$ADDED_INTERFACES."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $ADDED_INTERFACES;
}

sub get_Report_Removed($)
{
    my $Level = $_[0];# API or ABI
    my ($REMOVED_INTERFACES, %FuncRemovedFromHeaderLib);
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            if($Kind eq "Removed_Interface")
            {
                my $HeaderName = $CompleteSignature{1}{$Interface}{"Header"};
                my $DyLib = $Symbol_Library{1}{$Interface};
                $FuncRemovedFromHeaderLib{$HeaderName}{$DyLib}{$Interface} = 1;
            }
        }
    }
    my $Removed_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%FuncRemovedFromHeaderLib))
    {
        foreach my $DyLib (sort {lc($a) cmp lc($b)} keys(%{$FuncRemovedFromHeaderLib{$HeaderName}}))
        {
            my $FDyLib=$DyLib.($DyLib!~/\.\w+\Z/?" (.$LIB_EXT)":"");
            if($HeaderName and $DyLib) {
                $REMOVED_INTERFACES .= "<span class='h_name'>$HeaderName</span>, <span class='lib_name'>$FDyLib</span><br/>\n";
            }
            elsif($DyLib) {
                $REMOVED_INTERFACES .= "<span class='lib_name'>$FDyLib</span><br/>\n";
            }
            elsif($HeaderName) {
                $REMOVED_INTERFACES .= "<span class='h_name'>$HeaderName</span><br/>\n";
            }
            my %NameSpace_Interface = ();
            foreach my $Interface (keys(%{$FuncRemovedFromHeaderLib{$HeaderName}{$DyLib}}))
            {
                $NameSpace_Interface{get_IntNameSpace($Interface, 1)}{$Interface} = 1;
            }
            foreach my $NameSpace (sort keys(%NameSpace_Interface))
            {
                $REMOVED_INTERFACES .= ($NameSpace)?"<span class='ns_title'>namespace</span> <span class='ns'>$NameSpace</span>"."<br/>\n":"";
                my @SortedInterfaces = sort {lc(get_Signature($a, 1)) cmp lc(get_Signature($b, 1))} keys(%{$NameSpace_Interface{$NameSpace}});
                foreach my $Interface (@SortedInterfaces)
                {
                    $Removed_Number += 1;
                    my $SubReport = "";
                    my $Signature = get_Signature($Interface, 1);
                    if($NameSpace) {
                        $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                    }
                    if($Interface=~/\A(_Z|\?)/) {
                        if($Signature) {
                            $SubReport = insertIDs($ContentSpanStart.highLight_Signature_Italic_Color($Signature).$ContentSpanEnd."<br/>\n".$ContentDivStart."<span class='mangled'>[ symbol: <b>$Interface</b> ]</span><br/><br/>".$ContentDivEnd."\n");
                        }
                        else {
                            $SubReport = "<span class=\"iname\">".$Interface."</span><br/>\n";
                        }
                    }
                    else {
                        if($Signature) {
                            $SubReport = "<span class=\"iname\">".highLight_Signature_Italic_Color($Signature)."</span><br/>\n";
                        }
                        else {
                            $SubReport = "<span class=\"iname\">".$Interface."</span><br/>\n";
                        }
                    }
                    $REMOVED_INTERFACES .= $SubReport;
                }
            }
            $REMOVED_INTERFACES .= "<br/>\n";
        }
    }
    if($REMOVED_INTERFACES) {
        $REMOVED_INTERFACES = "<a name='Removed'></a><a name='Withdrawn'></a><h2>Removed Symbols ($Removed_Number)</h2><hr/>\n".$REMOVED_INTERFACES."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $REMOVED_INTERFACES;
}

sub applyMacroses($$$$)
{
    my ($Level, $Kind, $Content, $Problem) = @_;
    return "" if(not $Content or not $Problem);
    foreach my $Attr (sort {$b cmp $a} keys(%{$Problem}))
    {
        my $Macro = "\@".uc($Attr);
        my $Value = $Problem->{$Attr};
        if($Value=~/\s\(/)
        {# functions
            $Value = black_name($Value);
        }
        elsif($Value=~/\s/) {
            $Value = "<span class='value'>".htmlSpecChars($Value)."</span>";
        }
        elsif($Value=~/\A\d+\Z/
        and ($Attr eq "Old_Size" or $Attr eq "New_Size"))
        {# bits to bytes
            if($Value % $BYTE_SIZE)
            { # bits
                if($Value==1) {
                    $Value = "<b>".$Value."</b> bit";
                }
                else {
                    $Value = "<b>".$Value."</b> bits";
                }
            }
            else
            { # bytes
                $Value /= $BYTE_SIZE;
                if($Value==1) {
                    $Value = "<b>".$Value."</b> byte";
                }
                else {
                    $Value = "<b>".$Value."</b> bytes";
                }
            }
        }
        else {
            $Value = "<b>".htmlSpecChars($Value)."</b>";
        }
        $Content=~s/\Q$Macro\E/$Value/g;
    }
    if($Content=~/\@(OLD_VALUE|OLD_SIZE)/)
    {
        if(not $IncompleteRules{$Level}{$Kind})
        { # only one warning
            print "WARNING: incomplete rule \"$Kind\" (\"$Level\")\n";
            $IncompleteRules{$Level}{$Kind} = 1;
        }
    }
    $Content=~s!<nowrap>(.+?)</nowrap>!<span class='nowrap'>$1</span>!g;
    return $Content;
}

sub get_Report_InterfaceProblems($$)
{
    my ($TargetPriority, $Level) = @_;
    my ($INTERFACE_PROBLEMS, %FuncHeaderLib, %SymbolChanges);
    foreach my $Interface (sort keys(%CompatProblems))
    {
        next if($Interface=~/\A([^\@\$\?]+)[\@\$]+/ and defined $CompatProblems{$1});
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            if($CompatRules{$Level}{$Kind}{"Kind"} eq "Symbols"
            and $Kind ne "Added_Interface" and $Kind ne "Removed_Interface")
            {
                my $HeaderName = $CompleteSignature{1}{$Interface}{"Header"};
                my $DyLib = $Symbol_Library{1}{$Interface};
                $FuncHeaderLib{$HeaderName}{$DyLib}{$Interface} = 1;
                %{$SymbolChanges{$Interface}{$Kind}} = %{$CompatProblems{$Interface}{$Kind}};
            }
        }
    }
    my $Problems_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%FuncHeaderLib))
    {
        foreach my $DyLib (sort {lc($a) cmp lc($b)} keys(%{$FuncHeaderLib{$HeaderName}}))
        {
            my ($HEADER_LIB_REPORT, %NameSpace_Interface, %NewSignature) = ();
            foreach my $Interface (keys(%{$FuncHeaderLib{$HeaderName}{$DyLib}})) {
                $NameSpace_Interface{get_IntNameSpace($Interface, 1)}{$Interface} = 1;
            }
            foreach my $NameSpace (sort keys(%NameSpace_Interface))
            {
                my $NAMESPACE_REPORT = "";
                my @SortedInterfaces = sort {lc($tr_name{$a}) cmp lc($tr_name{$b})} keys(%{$NameSpace_Interface{$NameSpace}});
                foreach my $Interface (@SortedInterfaces)
                {
                    my $Signature = get_Signature($Interface, 1);
                    my $InterfaceProblemsReport = "";
                    my $ProblemNum = 1;
                    foreach my $Kind (keys(%{$SymbolChanges{$Interface}}))
                    {
                        foreach my $Location (sort keys(%{$SymbolChanges{$Interface}{$Kind}}))
                        {
                            my $Priority = getProblemSeverity($Level, $Kind);
                            if($Priority ne $TargetPriority) {
                                next;
                            }
                            my %Problem = %{$SymbolChanges{$Interface}{$Kind}{$Location}};
                            $Problem{"Param_Pos"} = numToStr($Problem{"Param_Pos"} + 1);
                            $Problem{"PSize"} = $WORD_SIZE{2};
                            $NewSignature{$Interface} = $Problem{"New_Signature"};
                            
                            if(my $Change = applyMacroses($Level, $Kind, $CompatRules{$Level}{$Kind}{"Change"}, \%Problem))
                            {
                                my $Effect = applyMacroses($Level, $Kind, $CompatRules{$Level}{$Kind}{"Effect"}, \%Problem);
                                $InterfaceProblemsReport .= "<tr><th>$ProblemNum</th><td align='left' valign='top'>".$Change."</td><td align='left' valign='top'>".$Effect."</td></tr>\n";
                                $ProblemNum += 1;
                                $Problems_Number += 1;
                            }
                        }
                    }
                    $ProblemNum -= 1;
                    if($InterfaceProblemsReport)
                    {
                        if($Signature) {
                            $NAMESPACE_REPORT .= $ContentSpanStart."<span class='extendable'>[+]</span> ".highLight_Signature_Italic_Color($Signature)." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart\n";
                            if($Interface=~/\A(_Z|\?)/
                            and not $NewSignature{$Interface}) {
                                $NAMESPACE_REPORT .= "<span class='mangled'>&nbsp;&nbsp;[ symbol: <b>$Interface</b> ]</span><br/>\n";
                            }
                        }
                        else {
                            $NAMESPACE_REPORT .= $ContentSpanStart."<span class='extendable'>[+]</span> ".$Interface." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart\n";
                        }
                        if($NewSignature{$Interface}) {# argument list changed to
                            $NAMESPACE_REPORT .= "\n<span class='new_signature_label'>changed to:</span><br/><span class='new_signature'>".highLight_Signature_Italic_Color($NewSignature{$Interface})."</span>\n";
                        }
                        $NAMESPACE_REPORT .= "<table cellpadding='3' cellspacing='0' class='problems_table'><tr><th width='2%'></th><th width='47%'>Change</th><th>Effect</th></tr>$InterfaceProblemsReport</table><br/>$ContentDivEnd\n";
                        $NAMESPACE_REPORT = insertIDs($NAMESPACE_REPORT);
                        if($NameSpace) {
                            $NAMESPACE_REPORT=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                        }
                    }
                }
                if($NAMESPACE_REPORT) {
                    $HEADER_LIB_REPORT .= (($NameSpace)?"<span class='ns_title'>namespace</span> <span class='ns'>$NameSpace</span>"."<br/>\n":"").$NAMESPACE_REPORT;
                }
            }
            if($HEADER_LIB_REPORT)
            {
                my $FDyLib=$DyLib.($DyLib!~/\.\w+\Z/?" (.$LIB_EXT)":"");
                if($HeaderName and $DyLib) {
                    $INTERFACE_PROBLEMS .= "<span class='h_name'>$HeaderName</span>, <span class='lib_name'>$FDyLib</span><br/>\n".$HEADER_LIB_REPORT."<br/>";
                }
                elsif($HeaderName) {
                    $INTERFACE_PROBLEMS .= "<span class='h_name'>$HeaderName</span><br/>\n".$HEADER_LIB_REPORT."<br/>";
                }
                elsif($DyLib) {
                    $INTERFACE_PROBLEMS .= "<span class='lib_name'>$FDyLib</span><br/>\n".$HEADER_LIB_REPORT."<br/>";
                }
                else {
                    $INTERFACE_PROBLEMS .= $HEADER_LIB_REPORT."<br/>";
                }
            }
        }
    }
    if($INTERFACE_PROBLEMS) {
        $INTERFACE_PROBLEMS = "<a name=\'Interface_Problems_$TargetPriority\'></a>\n<h2>Problems with Symbols, $TargetPriority Severity ($Problems_Number)</h2><hr/>\n".$INTERFACE_PROBLEMS."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $INTERFACE_PROBLEMS;
}

sub get_Report_TypeProblems($$)
{
    my ($TargetPriority, $Level) = @_;
    my ($TYPE_PROBLEMS, %TypeHeader, %TypeChanges, %TypeType) = ();
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            if($CompatRules{$Level}{$Kind}{"Kind"} eq "Types")
            {
                foreach my $Location (sort {cmp_locations($b, $a)} sort keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
                    my $Target = $CompatProblems{$Interface}{$Kind}{$Location}{"Target"};
                    my $Priority = getProblemSeverity($Level, $Kind);
                    if($Priority eq "Safe") {
                        next;
                    }
                    if(not $TypeType{$Type_Name}
                    or $TypeType{$Type_Name} eq "Struct")
                    {# register type of the type, select "class" if type has "class"- and "struct"-type changes
                        $TypeType{$Type_Name} = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Type"};
                    }
                    if(cmp_priority($Type_MaxPriority{$Level}{$Type_Name}{$Kind}{$Target}, $Priority))
                    {# select a problem with the highest priority
                        next;
                    }
                    %{$TypeChanges{$Type_Name}{$Kind}{$Location}} = %{$CompatProblems{$Interface}{$Kind}{$Location}};
                    my $HeaderName = get_TypeHeader($TName_Tid{1}{$Type_Name}, 1);
                    $TypeHeader{$HeaderName}{$Type_Name} = 1;
                }
            }
        }
    }
    my $Problems_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%TypeHeader))
    {
        my ($HEADER_REPORT, %NameSpace_Type) = ();
        foreach my $TypeName (keys(%{$TypeHeader{$HeaderName}})) {
            $NameSpace_Type{get_TypeNameSpace($TypeName, 1)}{$TypeName} = 1;
        }
        foreach my $NameSpace (sort keys(%NameSpace_Type))
        {
            my $NAMESPACE_REPORT = "";
            my @SortedTypes = sort {lc($TypeType{$a}." ".$a) cmp lc($TypeType{$b}." ".$b)} keys(%{$NameSpace_Type{$NameSpace}});
            foreach my $TypeName (@SortedTypes)
            {
                my $ProblemNum = 1;
                my ($TypeProblemsReport, %Kinds_Locations, %Kinds_Target) = ();
                foreach my $Kind (sort {$b=~/Size/ <=> $a=~/Size/} sort keys(%{$TypeChanges{$TypeName}}))
                {
                    foreach my $Location (sort {cmp_locations($b, $a)} sort keys(%{$TypeChanges{$TypeName}{$Kind}}))
                    {
                        my $Priority = getProblemSeverity($Level, $Kind);
                        if($Priority ne $TargetPriority) {
                            next;
                        }
                        $Kinds_Locations{$Kind}{$Location} = 1;
                        my %Problem = %{$TypeChanges{$TypeName}{$Kind}{$Location}};
                        my $Target = $Problem{"Target"};
                        next if($Kinds_Target{$Kind}{$Target});
                        $Kinds_Target{$Kind}{$Target} = 1;
                        $Problem{"PSize"} = $WORD_SIZE{2};
                        if(my $Change = applyMacroses($Level, $Kind, $CompatRules{$Level}{$Kind}{"Change"}, \%Problem))
                        {
                            my $Effect = applyMacroses($Level, $Kind, $CompatRules{$Level}{$Kind}{"Effect"}, \%Problem);
                            $TypeProblemsReport .= "<tr><th>$ProblemNum</th><td align='left' valign='top'>".$Change."</td><td align='left' valign='top'>$Effect</td></tr>\n";
                            $ProblemNum += 1;
                            $Problems_Number += 1;
                            $Kinds_Locations{$Kind}{$Location} = 1;
                        }
                    }
                }
                $ProblemNum -= 1;
                if($TypeProblemsReport)
                {
                    my $Affected_Interfaces = getAffectedInterfaces($TypeName, \%Kinds_Locations, $Level);
                    my $ShowVTables = "";
                    if(grep {$_=~/Virtual|Base_Class/} keys(%Kinds_Locations)) {
                        $ShowVTables = showVTables($TypeName);
                    }
                    $NAMESPACE_REPORT .= $ContentSpanStart."<span class='extendable'>[+]</span> <span class='ttype'>".lc($TypeType{$TypeName})."</span> ".htmlSpecChars($TypeName)." ($ProblemNum)".$ContentSpanEnd;
                    $NAMESPACE_REPORT .= "<br/>\n$ContentDivStart<table cellpadding='3' cellspacing='0' class='problems_table'><tr>\n";
                    $NAMESPACE_REPORT .= "<th width='2%'></th><th width='47%'>Change</th>\n";
                    $NAMESPACE_REPORT .= "<th>Effect</th></tr>$TypeProblemsReport</table>\n";
                    $NAMESPACE_REPORT .= "$ShowVTables$Affected_Interfaces<br/><br/>$ContentDivEnd\n";
                    $NAMESPACE_REPORT = insertIDs($NAMESPACE_REPORT);
                    if($NameSpace) {
                        $NAMESPACE_REPORT=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                    }
                }
            }
            if($NAMESPACE_REPORT) {
                $HEADER_REPORT .= (($NameSpace)?"<span class='ns_title'>namespace</span> <span class='ns'>$NameSpace</span>"."<br/>\n":"").$NAMESPACE_REPORT;
            }
        }
        if($HEADER_REPORT) {
            $TYPE_PROBLEMS .= "<span class='h_name'>$HeaderName</span><br/>\n".$HEADER_REPORT."<br/>";
        }
    }
    if($TYPE_PROBLEMS) {
        $TYPE_PROBLEMS = "<a name=\'Type_Problems_$TargetPriority\'></a>\n<h2>Problems with Data Types, $TargetPriority Severity ($Problems_Number)</h2><hr/>\n".$TYPE_PROBLEMS."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $TYPE_PROBLEMS;
}

sub showVTables($)
{
    my $TypeName = $_[0];
    my $TypeId1 = $TName_Tid{1}{$TypeName};
    my %Type1 = get_Type($Tid_TDid{1}{$TypeId1}, $TypeId1, 1);
    if(defined $Type1{"VTable"}
    and keys(%{$Type1{"VTable"}}))
    {
        my $TypeId2 = $TName_Tid{2}{$TypeName};
        my %Type2 = get_Type($Tid_TDid{2}{$TypeId2}, $TypeId2, 2);
        if(defined $Type2{"VTable"}
        and keys(%{$Type2{"VTable"}}))
        {
            my $VTABLES = "<table cellpadding='3' cellspacing='0' class='virtual_table'>";
            $VTABLES .= "<tr><th width='2%'>Offset</th>";
            $VTABLES .= "<th width='45%'>Virtual Table (Old) - ".(keys(%{$Type1{"VTable"}}))." entries</th>";
            $VTABLES .= "<th>Virtual Table (New) - ".(keys(%{$Type2{"VTable"}}))." entries</th></tr>";
            my %Indexes = map {$_=>1} (keys(%{$Type1{"VTable"}}), keys(%{$Type2{"VTable"}}));
            foreach (sort {int($a)<=>int($b)} (keys(%Indexes)))
            {
                my ($VL1, $VL2) = ($Type1{"VTable"}{$_}, $Type2{"VTable"}{$_});
                my ($SetColor1, $SetColor2) = ("", "");
                if($VL1 ne $VL2) {
                    if($VL1) {
                        $SetColor1 = " class='vtable_red'";
                        $SetColor2 = " class='vtable_red'";
                    }
                    else {
                        $SetColor2 = " class='vtable_yellow'";
                    }
                }
                else {
                    $VL1 = showVEntry($VL1);
                    $VL2 = showVEntry($VL2);
                }
                $VL1 = htmlSpecChars($VL1);
                $VL2 = htmlSpecChars($VL2);
                $VTABLES .= "<tr><th>$_</th>";
                $VTABLES .= "<td$SetColor1>$VL1</td>";
                $VTABLES .= "<td$SetColor2>$VL2</td></tr>\n";
            }
            $VTABLES .= "</table><br/>";
            my $SHOW_VTABLES = "<span style='padding-left:15px;'>".$ContentSpanStart_Info."[+] show v-table (old and new)".$ContentSpanEnd."</span><br/>\n";
            $SHOW_VTABLES .= $ContentDivStart.$VTABLES.$ContentDivEnd;
            return $SHOW_VTABLES;
        }
    }
    return "";
}

sub showVEntry($)
{
    my $VEntry = $_[0];
    $VEntry=~s/_ZTI\w+/typeinfo/g;
    if($VEntry=~s/ \[with (\w+) = (.+?)(, \w+ = .+|])\Z//g)
    {# std::basic_streambuf<_CharT, _Traits>::imbue [with _CharT = char, _Traits = std::char_traits<char>]
        # become std::basic_streambuf<char, ...>::imbue
        my ($Pname, $Pval) = ($1, $2);
        if($Pname eq "_CharT" and $VEntry=~/\Astd::/)
        {# stdc++ typedefs
            $VEntry=~s/<$Pname(, [^<>]+|)>/<$Pval>/g;
            # FIXME: simplify names using stdcxx typedefs (StdCxxTypedef)
            # The typedef info should be added to ABI dumps
        }
        else
        {
            $VEntry=~s/<$Pname>/<$Pval>/g;
            $VEntry=~s/<$Pname, [^<>]+>/<$Pval, ...>/g;
        }
    }
    if($VEntry=~/\A(.+)::(_ZThn.+)\Z/) {
        $VEntry = "non-virtual thunk";
    }
    if($VEntry=~/\A(.+)::\~(.+)\Z/ and $1 eq $2) {
        $VEntry = "~".$1;
    }
    return $VEntry;
}

sub getAffectedInterfaces($$$)
{
    my ($Target_TypeName, $Kinds_Locations, $Level) = @_;
    my ($Affected_Interfaces_Header, $Affected_Interfaces, %FunctionNumber, %FunctionProblems) = ();
    foreach my $Interface (sort {lc($tr_name{$a}?$tr_name{$a}:$a) cmp lc($tr_name{$b}?$tr_name{$b}:$b)} keys(%CompatProblems))
    {
        last if(keys(%FunctionNumber)>1000);
        if(($Interface=~/C2E|D2E|D0E/)) {
            # duplicated problems for C2 constructors, D2 and D0 destructors
            next;
        }
        my ($MinPath_Length, $ProblemLocation_Last) = ();
        my $MaxPriority = 0;
        my $Signature = get_Signature($Interface, 1);
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            foreach my $Location (keys(%{$CompatProblems{$Interface}{$Kind}}))
            {
                if(not defined $Kinds_Locations->{$Kind}
                or not $Kinds_Locations->{$Kind}{$Location}) {
                    next;
                }
                if($Interface=~/\A([^\@\$\?]+)[\@\$]+/ and defined $CompatProblems{$1}
                and defined $CompatProblems{$1}{$Kind}{$Location})
                {# duplicated problems for versioned symbols
                    next;
                }
                my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
                next if($Type_Name ne $Target_TypeName);
                my $ProblemLocation = clear_location($Location, $Type_Name);
                my $Parameter_Position = $CompatProblems{$Interface}{$Kind}{$Location}{"Param_Pos"};
                my $Priority = $CompatProblems{$Interface}{$Kind}{$Location}{"Priority"};
                $FunctionNumber{$Interface} = 1;
                my $Path_Length = 0;
                while($ProblemLocation=~/\-\>/g){$Path_Length += 1;}
                if($MinPath_Length eq "" or ($Path_Length<=$MinPath_Length and $Priority_Value{$Priority}>$MaxPriority)
                or (cmp_locations($ProblemLocation, $ProblemLocation_Last) and $Priority_Value{$Priority}==$MaxPriority))
                {
                    $MinPath_Length = $Path_Length;
                    $MaxPriority = $Priority_Value{$Priority};
                    $ProblemLocation_Last = $ProblemLocation;
                    my $Description = getAffectDescription($Interface, $Kind, $Location, $Level);
                    $FunctionProblems{$Interface}{"Issue"} = "<span class='iname_b'>".highLight_Signature_PPos_Italic($Signature, $Parameter_Position, 1, 0, 0)."</span><br/>"."<div class='affect'>$Description</div>\n";
                    $FunctionProblems{$Interface}{"Priority"} = $MaxPriority;
                }
            }
        }
    }
    foreach my $Interface (sort {$FunctionProblems{$b}{"Priority"}<=>$FunctionProblems{$a}{"Priority"}}
    sort {lc($tr_name{$a}?$tr_name{$a}:$a) cmp lc($tr_name{$b}?$tr_name{$b}:$b)} keys(%FunctionProblems)) {
        $Affected_Interfaces .= $FunctionProblems{$Interface}{"Issue"};
    }
    $Affected_Interfaces = "<div class='affected'>".$Affected_Interfaces."</div>";
    if(keys(%FunctionNumber)>1000) {
        $Affected_Interfaces .= "and others ...<br/>";
    }
    if($Affected_Interfaces) {
        $Affected_Interfaces_Header = $ContentSpanStart_Affected."[+] affected symbols (".(keys(%FunctionNumber)>1000?"more than 1000":keys(%FunctionNumber)).")".$ContentSpanEnd;
        $Affected_Interfaces =  $ContentDivStart.$Affected_Interfaces.$ContentDivEnd;
    }
    return "<span style='padding-left:15px'>".$Affected_Interfaces_Header."</span>".$Affected_Interfaces;
}

sub cmp_locations($$)
{
    my ($Location1, $Location2) = @_;
    if($Location2=~/(\A|\W)(RetVal|this)(\W|\Z)/
    and $Location1!~/(\A|\W)(RetVal|this)(\W|\Z)/ and $Location1!~/\-\>/) {
        return 1;
    }
    if($Location2=~/(\A|\W)(RetVal|this)(\W|\Z)/ and $Location2=~/\-\>/
    and $Location1!~/(\A|\W)(RetVal|this)(\W|\Z)/ and $Location1=~/\-\>/) {
        return 1;
    }
    return 0;
}

sub clear_location($$)
{
    my ($Location, $TypeName) = @_;
    $Location=~s/->\Q$TypeName\E\Z//g if($TypeName);
    return $Location;
}

sub getAffectDescription($$$$)
{
    my ($Interface, $Kind, $Location, $Level) = @_;
    my %Affect = %{$CompatProblems{$Interface}{$Kind}{$Location}};
    my $Signature = get_Signature($Interface, 1);
    my $Old_Value = $Affect{"Old_Value"};
    my $New_Value = $Affect{"New_Value"};
    my $Type_Name = $Affect{"Type_Name"};
    my $Parameter_Name = $Affect{"Param_Name"};
    my $Start_Type_Name = $Affect{"Start_Type_Name"};
    my $InitialType_Type = $Affect{"InitialType_Type"};
    my $Parameter_Position = numToStr($Affect{"Param_Pos"} + 1);
    my @Sentence_Parts = ();
    my $Location_To_Type = $Location;
    $Location_To_Type=~s/\A(.*)\-\>.+?\Z/$1/;
    if($Kind=~/(\A|_)Virtual(_|\Z)/)
    {
        if($Kind eq "Overridden_Virtual_Method"
        or $Kind eq "Overridden_Virtual_Method_B") {
            push(@Sentence_Parts, "The method '".highLight_Signature($New_Value)."' will be called instead of this method.");
        }
        elsif($Kind eq "Added_Virtual_Method_At_End") {
            push(@Sentence_Parts, "Call of this method may result in crash or incorrect behavior of applications.");
        }
        elsif($Kind eq "Pure_Virtual_Replacement")
        {
            if($Old_Value eq $Signature) {
                push(@Sentence_Parts, "Call of this method may result in crash or incorrect behavior of applications because it has been replaced by other virtual method.");
            }
            else {
                push(@Sentence_Parts, "Call of this method may result in crash or incorrect behavior of applications because some pure virtual method in v-table has been replaced by other.");
            }
        }
        elsif($Kind eq "Virtual_Replacement") {
            push(@Sentence_Parts, "Call of this method may result in crash or incorrect behavior of applications because it has been replaced by other virtual method.");
        }
        elsif($Kind eq "Virtual_Method_Position"
        or $Kind eq "Pure_Virtual_Method_Position") {
            push(@Sentence_Parts, "Call of this pure virtual method implementation may result in crash or incorrect behavior of applications because the layout of virtual table has been changed.");
        }
        else {
            push(@Sentence_Parts, "Call of this virtual method may result in crash or incorrect behavior of applications because the layout of virtual table has been changed.");
        }
    }
    elsif($CompatRules{$Level}{$Kind}{"Kind"} eq "Types")
    {
        if($Location_To_Type eq "this")
        {
            my $METHOD_TYPE = $CompleteSignature{1}{$Interface}{"Constructor"}?"constructor":"method";
            my $ClassName = get_TypeName($CompleteSignature{1}{$Interface}{"Class"}, 1);
            if($ClassName eq $Type_Name) {
                return "This $METHOD_TYPE is from \'".htmlSpecChars($Type_Name)."\' class.";
            }
            else {
                return "This $METHOD_TYPE is from derived class \'".htmlSpecChars($ClassName)."\'.";
            }
        }
        if($Location_To_Type=~/RetVal/)
        {# return value
            if($Location_To_Type=~/\-\>/) {
                push(@Sentence_Parts, "Field \'".htmlSpecChars($Location_To_Type)."\' in return value");
            }
            else {
                push(@Sentence_Parts, "Return value");
            }
            if($InitialType_Type eq "Pointer") {
                push(@Sentence_Parts, "(pointer)");
            }
            elsif($InitialType_Type eq "Ref") {
                push(@Sentence_Parts, "(reference)");
            }
        }
        elsif($Location_To_Type=~/this/)
        {# "this" pointer
            if($Location_To_Type=~/\-\>/) {
                push(@Sentence_Parts, "Field \'".htmlSpecChars($Location_To_Type)."\' in the object of this method");
            }
            else {
                push(@Sentence_Parts, "\'this\' pointer");
            }
        }
        else
        {# parameters
            if($Location_To_Type=~/\-\>/) {
                push(@Sentence_Parts, "Field \'".htmlSpecChars($Location_To_Type)."\' in $Parameter_Position parameter");
            }
            else {
                push(@Sentence_Parts, "$Parameter_Position parameter");
            }
            if($Parameter_Name) {
                push(@Sentence_Parts, "\'$Parameter_Name\'");
            }
            if($InitialType_Type eq "Pointer") {
                push(@Sentence_Parts, "(pointer)");
            }
            elsif($InitialType_Type eq "Ref") {
                push(@Sentence_Parts, "(reference)");
            }
        }
        if($Location_To_Type eq "this") {
            push(@Sentence_Parts, "has base type \'".htmlSpecChars($Type_Name)."\'.");
        }
        elsif($Start_Type_Name eq $Type_Name) {
            push(@Sentence_Parts, "has type \'".htmlSpecChars($Type_Name)."\'.");
        }
        else {
            push(@Sentence_Parts, "has base type \'".htmlSpecChars($Type_Name)."\'.");
        }
    }
    return join(" ", @Sentence_Parts);
}

sub createHtmlReport()
{
    my $CssStyles = "
    <style type=\"text/css\">
    body {
        font-family:Arial;
        color:Black;
        font-size:14px;
    }
    hr {
        color:Black;
        background-color:Black;
        height:1px;
        border:0;
    }
    h1 {
        margin-bottom:0px;
        padding-bottom:0px;
        font-size:26px;
    }
    h2 {
        margin-bottom:0px;
        padding-bottom:0px;
        font-size:20px;
        white-space:nowrap;
    }
    span.section {
        font-weight:bold;
        cursor:pointer;
        margin-left:7px;
        font-size:16px;
        color:#003E69;
        white-space:nowrap;
    }
    span.new_signature {
        font-weight:bold;
        margin-left:28px;
        font-size:16px;
        color:#003E69;
    }
    span.new_signature_label {
        margin-left:28px;
        font-size:14px;
        color:Black;
    }
    span:hover.section {
        color:#336699;
    }
    span.section_affected {
        cursor:pointer;
        margin-left:7px;
        font-size:14px;
        color:#cc3300;
    }
    span.section_info {
        cursor:pointer;
        margin-left:7px;
        font-size:14px;
        color:Black;
    }
    span.extendable {
        font-weight:100;
        font-size:16px;
    }
    span.h_name {
        color:#cc3300;
        font-size:14px;
        font-weight:bold;
    }
    div.h_list {
        padding-left:10px;
        color:#333333;
        font-size:15px;
    }
    span.ns_title {
        margin-left:2px;
        color:#408080;
        font-size:13px;
    }
    span.ns {
        color:#408080;
        font-size:13px;
        font-weight:bold;
    }
    div.lib_list {
        padding-left:10px;
        color:#333333;
        font-size:15px;
    }
    span.lib_name {
        color:Green;
        font-size:14px;
        font-weight:bold;
    }
    span.iname {
        font-weight:bold;
        font-size:16px;
        color:#003E69;
        margin-left:7px;
    }
    span.iname_b {
        font-weight:bold;
        font-size:15px;
        color:#333333;
    }
    span.int_p {
        font-weight:normal;
        white-space:normal;
    }
    div.affect {
        padding-left:15px;
        padding-bottom:4px;
        font-size:14px;
        font-style:italic;
        line-height:13px;
    }
    div.affected {
        padding-left:30px;
        padding-top:5px;
    }
    table.problems_table {
        border-collapse:collapse;
        border:1px outset black;
        line-height:16px;
        margin-left:15px;
        margin-top:3px;
        margin-bottom:3px;
        width:900px;
    }
    table.problems_table td {
        border:1px solid Gray;
    }
    table.problems_table th {
        background-color:#eeeeee;
        color:#333333;
        font-weight:bold;
        font-size:13px;
        font-family:Verdana;
        border:1px solid Gray;
        text-align:center;
        vertical-align:top;
        white-space:nowrap;
    }
    table.virtual_table {
        border-collapse:collapse;
        border:1px outset black;
        line-height:16px;
        margin-left:30px;
        margin-top:3px;
        width:100px;
    }
    table.virtual_table td {
        border:1px solid Gray;
        white-space:nowrap;
        border:1px solid Gray;
    }
    table.virtual_table th {
        background-color:#eeeeee;
        color:#333333;
        font-weight:bold;
        font-size:13px;
        font-family:Verdana;
        border:1px solid Gray;
        text-align:center;
        vertical-align:top;
        white-space:nowrap;
    }
    td.vtable_red {
        background-color:#FFCCCC;
    }
    td.vtable_yellow {
        background-color:#FFFFCC;
    }
    table.summary {
        border-collapse:collapse;
        border:1px outset black;
    }
    table.summary th {
        background-color:#eeeeee;
        font-weight:100;
        text-align:left;
        font-size:15px;
        white-space:nowrap;
        border:1px inset gray;
    }
    table.summary td {
        padding-left:10px;
        padding-right:5px;
        text-align:right;
        font-size:16px;
        white-space:nowrap;
        border:1px inset gray;
    }
    table.code_view {
        cursor:text;
        margin-top:7px;
        width:50%;
        margin-left:20px;
        font-family:Monaco, \"Courier New\", Courier;
        font-size:14px;
        padding:10px;
        border:1px solid #e0e8e5;
        color:#444444;
        background-color:#eff3f2;
        overflow:auto;
    }
    table.code_view td {
        padding-left:15px;
        text-align:left;
        white-space:nowrap;
    }
    span.mangled {
        padding-left:15px;
        font-size:13px;
        cursor:text;
        color:#444444;
    }
    span.symver {
        color:#333333;
        font-size:14px;
        white-space:nowrap;
    }
    span.color_p {
        font-style:italic;
        color:Brown;
    }
    span.focus_p {
        font-style:italic;
        color:Red;
    }
    span.ttype {
        font-weight:100;
    }
    span.nowrap {
        white-space:nowrap;
    }
    span.value {
        white-space:nowrap;
        font-weight:bold;
    }
    </style>";
    my ($Summary, $StatLine) = get_Summary("Binary");
    my $Title = "$TargetLibraryFName: ".$Descriptor{1}{"Version"}." to ".$Descriptor{2}{"Version"}." binary compatibility report";
    my $Keywords = "$TargetLibraryFName, binary compatibility, API, report";
    my $Description = "Binary compatibility report for the $TargetLibraryFName $TargetComponent between ".$Descriptor{1}{"Version"}." and ".$Descriptor{2}{"Version"}." versions";
    if(getArch(1) eq getArch(2)
    and getArch(1) ne "unknown") {
        $Description .= " on ".showArch(getArch(1));
    }
    writeFile($REPORT_PATH, "<!-\- $StatLine -\->\n".composeHTML_Head($Title, $Keywords, $Description, $CssStyles."\n".$JScripts)."\n<body>
    <div><a name='Top'></a>\n".get_Report_Header()."\n$Summary\n".get_Report_Added("Binary").get_Report_Removed("Binary").get_Report_Problems("High", "Binary").get_Report_Problems("Medium", "Binary").get_Report_Problems("Low", "Binary").get_SourceInfo()."</div>
    <br/><br/><br/><hr/>
    ".getReportFooter($TargetLibraryFName)."
    <div style='height:999px;'></div>\n</body></html>");
}

sub getReportFooter($)
{
    my $LibName = $_[0];
    my $Footer = "<div style='width:100%;font-size:11px;' align='right'><i>Generated on ".(localtime time); # report date
    $Footer .= " for <span style='font-weight:bold'>$LibName</span>"; # tested library/system name
    $Footer .= " by <a href='http://ispras.linux-foundation.org/index.php/ABI_compliance_checker'>ABI Compliance Checker</a>"; # tool name
    my $ToolSummary = "<br/>A tool for checking backward binary compatibility of a shared C/C++ library API&nbsp;&nbsp;";
    $Footer .= " $ABI_COMPLIANCE_CHECKER_VERSION &nbsp;$ToolSummary</i></div>"; # tool version
    return $Footer;
}

sub get_Report_Problems($$)
{
    my ($Priority, $Level) = @_;
    my $Problems = get_Report_TypeProblems($Priority, $Level).get_Report_InterfaceProblems($Priority, $Level);
    if($Priority eq "Low") {
        $Problems .= get_Report_ChangedConstants();
        if($CheckImplementation and $Level eq "Binary") {
            $Problems .= get_Report_Implementation();
        }
    }
    if($Problems) {
        $Problems = "<a name=\'".$Priority."_Risk_Problems\'></a>".$Problems;
    }
    return $Problems;
}

sub composeHTML_Head($$$$)
{
    my ($Title, $Keywords, $Description, $OtherInHead) = @_;
    return "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n<head>
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />
    <meta name=\"keywords\" content=\"$Keywords\" />
    <meta name=\"description\" content=\"$Description\" />
    <title>\n        $Title\n    </title>\n$OtherInHead\n</head>";
}

sub insertIDs($)
{
    my $Text = $_[0];
    while($Text=~/CONTENT_ID/)
    {
        if(int($Content_Counter)%2) {
            $ContentID -= 1;
        }
        $Text=~s/CONTENT_ID/c_$ContentID/;
        $ContentID += 1;
        $Content_Counter += 1;
    }
    return $Text;
}

sub checkPreprocessedUnit($)
{
    my $Path = $_[0];
    my $CurHeader = "";
    open(PREPROC, "$Path") || die ("can't open file \'$Path\': $!\n");
    while(<PREPROC>)
    {# detecting public and private constants
        next if(not /\A#/);
        chomp($_);
        if(/#[ \t]+\d+[ \t]+\"(.+)\"/) {
            $CurHeader=path_format($1, $OSgroup);
        }
        if(not $Include_Neighbors{$Version}{get_filename($CurHeader)}
        and not $Target_Headers{$Version}{$CurHeader})
        { # not a target
            next;
        }
        if($TargetHeader
        and get_filename($CurHeader) ne $TargetHeader)
        { # user-defined header
            next;
        }
        if(/\#[ \t]*define[ \t]+([_A-Z0-9]+)[ \t]+(.+)[ \t]*\Z/)
        {
            my ($Name, $Value) = ($1, $2);
            if(not $Constants{$Version}{$Name}{"Access"})
            {
                $Constants{$Version}{$Name}{"Access"} = "public";
                $Constants{$Version}{$Name}{"Value"} = $Value;
                $Constants{$Version}{$Name}{"Header"} = get_filename($CurHeader);
            }
        }
        elsif(/\#[ \t]*undef[ \t]+([_A-Z]+)[ \t]*/) {
            $Constants{$Version}{$1}{"Access"} = "private";
        }
    }
    close(PREPROC);
    foreach my $Constant (keys(%{$Constants{$Version}}))
    {
        if($Constants{$Version}{$Constant}{"Access"} eq "private" or $Constant=~/_h\Z/i
        or $Constants{$Version}{$Constant}{"Header"}=~/\<built\-in\>|\<internal\>|\A\./)
        { # skip private constants
            delete($Constants{$Version}{$Constant});
        }
        else {
            delete($Constants{$Version}{$Constant}{"Access"});
        }
    }
}

my %IgnoreConstant=(
    "VERSION"=>1,
    "VERSIONCODE"=>1,
    "VERNUM"=>1,
    "VERS_INFO"=>1,
    "PATCHLEVEL"=>1,
    "INSTALLPREFIX"=>1,
    "VBUILD"=>1,
    "VPATCH"=>1,
    "VMINOR"=>1,
    "BUILD_STRING"=>1,
    "BUILD_TIME"=>1,
    "PACKAGE_STRING"=>1,
    "PRODUCTION"=>1,
    "CONFIGURE_COMMAND"=>1,
    "INSTALLDIR"=>1,
    "BINDIR"=>1,
    "CONFIG_FILE_PATH"=>1,
    "DATADIR"=>1,
    "EXTENSION_DIR"=>1,
    "INCLUDE_PATH"=>1,
    "LIBDIR"=>1,
    "LOCALSTATEDIR"=>1,
    "SBINDIR"=>1,
    "SYSCONFDIR"=>1,
    "RELEASE"=>1,
    "SOURCE_ID"=>1,
    "SUBMINOR"=>1,
    "MINOR"=>1,
    "MINNOR"=>1,
    "MINORVERSION"=>1,
    "MAJOR"=>1,
    "MAJORVERSION"=>1,
    "MICRO"=>1,
    "MICROVERSION"=>1,
    "BINARY_AGE"=>1,
    "INTERFACE_AGE"=>1,
    "CORE_ABI"=>1,
    "PATCH"=>1,
    "COPYRIGHT"=>1,
    "TIMESTAMP"=>1,
    "REVISION"=>1,
    "PACKAGE_TAG"=>1,
    "PACKAGEDATE"=>1,
    "NUMVERSION"=>1
);

sub mergeConstants()
{
    foreach my $Constant (keys(%{$Constants{1}}))
    {
        if($SkipConstants{1}{$Constant})
        { # skipped by the user
            next;
        }
        if($Constants{2}{$Constant}{"Value"} eq "")
        { # empty value
            next;
        }
        if($TargetHeader
        and $Constants{1}{$Constant}{"Header"} ne $TargetHeader)
        { # user-defined header
            next;
        }
        my ($Old_Value, $New_Value, $Old_Value_Pure, $New_Value_Pure);
        $Old_Value = $Old_Value_Pure = uncover_constant(1, $Constant);
        $New_Value = $New_Value_Pure = uncover_constant(2, $Constant);
        $Old_Value_Pure=~s/(\W)\s+/$1/g;
        $Old_Value_Pure=~s/\s+(\W)/$1/g;
        $New_Value_Pure=~s/(\W)\s+/$1/g;
        $New_Value_Pure=~s/\s+(\W)/$1/g;
        next if($New_Value_Pure eq "" or $Old_Value_Pure eq "");
        if($New_Value_Pure ne $Old_Value_Pure)
        { # different values
            if(grep {$Constant=~/(\A|_)$_(_|\Z)/} keys(%IgnoreConstant))
            { # ignore library version
                next;
            }
            if($Constant=~/(\A|_)(lib|open|)$TargetLibraryShortName(_|)(VERSION|VER|DATE|API|PREFIX)(_|\Z)/i)
            { # ignore library version
                next;
            }
            if($Old_Value=~/\A('|"|)[\/\\]\w+([\/\\]|:|('|"|)\Z)/ or $Old_Value=~/[\/\\]\w+[\/\\]\w+/)
            { # ignoring path defines:
              #  /lib64:/usr/lib64:/lib:/usr/lib:/usr/X11R6/lib/Xaw3d ...
                next;
            }
            if($Old_Value=~/\A\(*\w+(\s+|\|)/)
            { # ignore source defines:
              #  static int gcry_pth_init ( void) { return ...
              #  (RE_BACKSLASH_ESCAPE_IN_LISTS | RE...
                next;
            }
            if(convert_integer($Old_Value) eq convert_integer($New_Value))
            { # 0x0001 and 0x1, 0x1 and 1 equal constants
                next;
            }
            if($Old_Value eq "0" and $New_Value eq "NULL")
            { # 0 => NULL
                next;
            }
            if($Old_Value eq "NULL" and $New_Value eq "0")
            { # NULL => 0
                next;
            }
            %{$ProblemsWithConstants{$Constant}} = (
                "Target"=>$Constant,
                "Old_Value"=>$Old_Value,
                "New_Value"=>$New_Value  );
        }
    }
}

sub convert_integer($)
{
    my $Value = $_[0];
    if($Value=~/\A0x[a-f0-9]+\Z/)
    {# hexadecimal
        return hex($Value);
    }
    elsif($Value=~/\A0[0-7]+\Z/)
    {# octal
        return oct($Value);
    }
    elsif($Value=~/\A0b[0-1]+\Z/)
    {# binary
        return oct($Value);
    }
    else {
        return $Value;
    }
}

sub uncover_constant($$)
{
    my ($LibVersion, $Constant) = @_;
    return "" if(not $LibVersion or not $Constant);
    return $Constant if(isCyclical(\@RecurConstant, $Constant));
    if($Cache{"uncover_constant"}{$LibVersion}{$Constant} ne "") {
        return $Cache{"uncover_constant"}{$LibVersion}{$Constant};
    }
    my $Value = $Constants{$LibVersion}{$Constant}{"Value"};
    if($Value=~/\A[A-Z0-9_]+\Z/ and $Value=~/[A-Z]/)
    {
        push(@RecurConstant, $Constant);
        if((my $Uncovered = uncover_constant($LibVersion, $Value)) ne "") {
            $Value = $Uncovered;
        }
        pop(@RecurConstant);
    }
    # FIXME: uncover $Value using all the enum constants
    # USECASE: change of define NC_LONG from NC_INT (enum value) to NC_INT (define)
    $Cache{"uncover_constant"}{$LibVersion}{$Constant} = $Value;
    return $Value;
}

sub getSymbols($)
{
    my $LibVersion = $_[0];
    my @DyLibPaths = getSoPaths($LibVersion);
    if($#DyLibPaths==-1 and not $CheckHeadersOnly)
    {
        if($LibVersion==1) {
            $CheckHeadersOnly = 1;
        }
        else {
            exitStatus("Error", "$SLIB_TYPE libraries are not found in the ".$Descriptor{$LibVersion}{"Version"});
        }
    }
    my %GroupNames = map {parse_libname(get_filename($_), "name+ext")=>1} @DyLibPaths;
    foreach my $DyLibPath (sort {length($a)<=>length($b)} @DyLibPaths) {
        getSymbols_Lib($LibVersion, $DyLibPath, 0, \%GroupNames, "+Weak");
    }
}

sub get_VTableSymbolSize($$)
{
    my ($ClassName, $LibVersion) = @_;
    return 0 if(not $ClassName);
    my $Symbol = $ClassVTable{$ClassName};
    if(defined $Symbol_Library{$LibVersion}{$Symbol}
    and my $DyLib = $Symbol_Library{$LibVersion}{$Symbol})
    {# bind class name and v-table size
        if(defined $Library_Symbol{$LibVersion}{$DyLib}{$Symbol}
        and my $Size = -$Library_Symbol{$LibVersion}{$DyLib}{$Symbol})
        {# size from the shared library
            if($Size>=12) {
#               0     (int (*)(...))0
#               4     (int (*)(...))(& _ZTIN7mysqlpp8DateTimeE)
#               8     mysqlpp::DateTime::~DateTime
                return $Size;
            }
            else {
                return 0;
            }
        }
    }
}

sub canonifyName($)
{# make TIFFStreamOpen(char const*, std::basic_ostream<char, std::char_traits<char> >*)
# to be TIFFStreamOpen(char const*, std::basic_ostream<char>*)
    my $Name = $_[0];
    $Name=~s/,\s*std::(allocator|less|char_traits|regex_traits)<\w+> //g;
    return $Name;
}

sub translateSymbols(@_$)
{
    my $LibVersion = pop(@_);
    my (@MnglNames1, @MnglNames2, @UnMnglNames) = ();
    foreach my $Interface (sort @_)
    {
        if($Interface=~/\A_Z/)
        {
            next if($tr_name{$Interface});
            $Interface=~s/[\@\$]+(.*)\Z//;
            push(@MnglNames1, $Interface);
        }
        elsif($Interface=~/\A\?/) {
            push(@MnglNames2, $Interface);
        }
        else
        {# not mangled
            $tr_name{$Interface} = $Interface;
            $mangled_name_gcc{$Interface} = $Interface;
            $mangled_name{$LibVersion}{$Interface} = $Interface;
        }
    }
    if($#MnglNames1 > -1)
    {# gcc names
        @UnMnglNames = reverse(unmangleArray(@MnglNames1));
        foreach my $MnglName (@MnglNames1)
        {
            my $Unmangled = $tr_name{$MnglName} = correctName(canonifyName(pop(@UnMnglNames)));
            if(not $mangled_name_gcc{$Unmangled}) {
                $mangled_name_gcc{$Unmangled} = $MnglName;
            }
            if($MnglName=~/\A_ZTV/ and $Unmangled=~/vtable for (.+)/)
            {# bind class name and v-table symbol
                $ClassVTable{$1} = $MnglName;
                $VTableClass{$MnglName} = $1;
            }
        }
    }
    if($#MnglNames2 > -1)
    {# cl names
        @UnMnglNames = reverse(unmangleArray(@MnglNames2));
        foreach my $MnglName (@MnglNames2)
        {
            $tr_name{$MnglName} = correctName(pop(@UnMnglNames));
            $mangled_name{$LibVersion}{$tr_name{$MnglName}} = $MnglName;
        }
    }
}

sub link_symbol($$$)
{
    my ($Symbol, $RunWith, $Deps) = @_;
    if(link_symbol_internal($Symbol, $RunWith, \%Symbol_Library)) {
        return 1;
    }
    if($Deps eq "+Deps")
    { # check the dependencies
        if(link_symbol_internal($Symbol, $RunWith, \%DepSymbols)) {
            return 1;
        }
    }
    return 0;
}

sub link_symbol_internal($$$)
{
    my ($Symbol, $RunWith, $Where) = @_;
    return 0 if(not $Where or not $Symbol);
    if($Where->{$RunWith}{$Symbol})
    { # the exact match by symbol name
        return 1;
    }
    if(my $VSym = $SymVer{$RunWith}{$Symbol})
    { # indirect symbol version, i.e.
      # foo_old and its symlink foo@v (or foo@@v)
      # foo_old may be in .symtab table
        if($Where->{$RunWith}{$VSym}) {
            return 1;
        }
    }
    my ($Sym, $Del, $Ver) = separate_symbol($Symbol);
    if($Sym and $Ver)
    { # search for the symbol with the same version
      # or without version
        if($Where->{$RunWith}{$Sym})
        { # old: foo@v|foo@@v
          # new: foo
            return 1;
        }
        if($Where->{$RunWith}{$Sym."\@".$Ver})
        { # old: foo|foo@@v
          # new: foo@v
            return 1;
        }
        if($Where->{$RunWith}{$Sym."\@\@".$Ver})
        { # old: foo|foo@v
          # new: foo@@v
            return 1;
        }
    }
    return 0;
}

sub getSymbols_App($)
{
    my $Path = $_[0];
    return () if(not $Path or not -f $Path);
    my @Imported = ();
    if($OSgroup eq "macos")
    {
        my $OtoolCmd = get_CmdPath("otool");
        if(not $OtoolCmd) {
            exitStatus("Not_Found", "can't find \"otool\"");
        }
        open(APP, "$OtoolCmd -IV $Path 2>$TMP_DIR/null |");
        while(<APP>) {
            if(/[^_]+\s+_?([\w\$]+)\s*\Z/) {
                push(@Imported, $1);
            }
        }
        close(APP);
    }
    elsif($OSgroup eq "windows")
    {
        my $DumpBinCmd = get_CmdPath("dumpbin");
        if(not $DumpBinCmd) {
            exitStatus("Not_Found", "can't find \"dumpbin.exe\"");
        }
        open(APP, "$DumpBinCmd /IMPORTS $Path 2>$TMP_DIR/null |");
        while(<APP>) {
            if(/\s*\w+\s+\w+\s+\w+\s+([\w\?\@]+)\s*/) {
                push(@Imported, $1);
            }
        }
        close(APP);
    }
    else
    {
        my $ReadelfCmd = get_CmdPath("readelf");
        if(not $ReadelfCmd) {
            exitStatus("Not_Found", "can't find \"readelf\"");
        }
        open(APP, "$ReadelfCmd -WhlSsdA $Path 2>$TMP_DIR/null |");
        my $symtab=0; # indicates that we are processing 'symtab' section of 'readelf' output
        while(<APP>)
        {
            if( /'.dynsym'/ ) {
                $symtab=0;
            }
            elsif($symtab == 1) {
                # do nothing with symtab (but there are some plans for the future)
                next;
            }
            elsif( /'.symtab'/ ) {
                $symtab=1;
            }
            elsif(my ($fullname, $idx, $Ndx, $type, $size, $bind) = readline_ELF($_))
            {
                if( $Ndx eq "UND" ) {
                    #only imported symbols
                    push(@Imported, $fullname);
                }
            }
        }
        close(APP);
    }
    return @Imported;
}

sub readline_ELF($)
{
    if($_[0]=~/\s*\d+:\s+(\w*)\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s([^\s]+)/)
    { # the line of 'readelf' output corresponding to the interface
      # symbian-style: _ZN12CCTTokenType4NewLE4TUid3RFs@@ctfinder{000a0000}[102020e5].dll
        my ($value, $size, $type, $bind,
        $vis, $Ndx, $fullname)=($1, $2, $3, $4, $5, $6, $7);
        if($bind!~/\A(WEAK|GLOBAL)\Z/) {
            return ();
        }
        if($type!~/\A(FUNC|IFUNC|OBJECT|COMMON)\Z/) {
            return ();
        }
        if($vis!~/\A(DEFAULT|PROTECTED)\Z/) {
            return ();
        }
        if($Ndx eq "ABS" and $value!~/\D|1|2|3|4|5|6|7|8|9/) {
            return ();
        }
        if($OStarget eq "symbian")
        {
            if($fullname=~/_\._\.absent_export_\d+/)
            {# "_._.absent_export_111"@@libstdcpp{00010001}[10282872].dll
                return ();
            }
            my @Elems = separate_symbol($fullname);
            $fullname = $Elems[0];# remove internal version, {00020001}[10011235].dll
        }
        return ($fullname, $value, $Ndx, $type, $size, $bind);
    }
    else {
        return ();
    }
}

sub getSymbols_Lib($$$$$)
{
    my ($LibVersion, $Lib_Path, $IsNeededLib, $GroupNames, $Weak) = @_;
    return if(not $Lib_Path or not -f $Lib_Path);
    my ($Lib_Dir, $Lib_Name) = separate_path(resolve_symlink($Lib_Path));
    return if($CheckedDyLib{$LibVersion}{$Lib_Name} and $IsNeededLib);
    return if(isCyclical(\@RecurLib, $Lib_Name) or $#RecurLib>=1);
    $CheckedDyLib{$LibVersion}{$Lib_Name} = 1;
    getImplementations($LibVersion, $Lib_Path) if($CheckImplementation and not $IsNeededLib);
    push(@RecurLib, $Lib_Name);
    my (%Value_Interface, %Interface_Value, %NeededLib) = ();
    if(not $IsNeededLib)
    { # libstdc++ and libc are always used by other libs
      # if you test one of these libs then you not need
      # to find them in the system for reusing
        if(parse_libname($Lib_Name, "short") eq "libstdc++")
        { # libstdc++.so.6
            $STDCXX_TESTING = 1;
        }
        if(parse_libname($Lib_Name, "short") eq "libc")
        { # libc-2.11.3.so
            $GLIBC_TESTING = 1;
        }
    }
    if($OStarget eq "macos")
    {# Mac OS X: *.dylib, *.a
        my $OtoolCmd = get_CmdPath("otool");
        if(not $OtoolCmd) {
            exitStatus("Not_Found", "can't find \"otool\"");
        }
        open(LIB, "$OtoolCmd -TV $Lib_Path 2>$TMP_DIR/null |");
        while(<LIB>)
        {
            if(/[^_]+\s+_([\w\$]+)\s*\Z/)
            {
                my $realname = $1;
                if($IsNeededLib and $GroupNames
                and not $GroupNames->{parse_libname($Lib_Name, "name+ext")}) {
                    $DepSymbols{$LibVersion}{$realname} = 1;
                }
                if(not $IsNeededLib)
                {
                    $Symbol_Library{$LibVersion}{$realname} = $Lib_Name;
                    $Library_Symbol{$LibVersion}{$Lib_Name}{$realname} = 1;
                    if(not $Language{$LibVersion}{$Lib_Name}) {
                        if($realname =~/\A(_Z|\?)/) {
                            $Language{$LibVersion}{$Lib_Name} = "C++";
                            if(not $IsNeededLib) {
                                $COMMON_LANGUAGE{$LibVersion} = "C++";
                            }
                        }
                    }
                    if($CheckObjectsOnly
                    and $LibVersion==1) {
                        $CheckedSymbols{$realname} = 1;
                    }
                }
            }
        }
        close(LIB);
        if($LIB_TYPE eq "dynamic")
        { # dependencies
            open(LIB, "$OtoolCmd -L $Lib_Path 2>$TMP_DIR/null |");
            while(<LIB>) {
                if(/\s*([\/\\].+\.$LIB_EXT)\s*/
                and $1 ne $Lib_Path) {
                    $NeededLib{$1} = 1;
                }
            }
            close(LIB);
        }
    }
    elsif($OStarget eq "windows")
    { # Windows *.dll, *.lib
        my $DumpBinCmd = get_CmdPath("dumpbin");
        if(not $DumpBinCmd) {
            exitStatus("Not_Found", "can't find \"dumpbin\"");
        }
        open(LIB, "$DumpBinCmd /EXPORTS \"$Lib_Path\" 2>$TMP_DIR/null |");
        while(<LIB>)
        { # 1197 4AC 0000A620 SetThreadStackGuarantee
          # 1198 4AD          SetThreadToken (forwarded to ...)
          # 3368 _o2i_ECPublicKey
            if(/\A\s*\d+\s+[a-f\d]+\s+[a-f\d]+\s+([\w\?\@]+)\s*\Z/i
            or /\A\s*\d+\s+[a-f\d]+\s+([\w\?\@]+)\s*\(\s*forwarded\s+/
            or /\A\s*\d+\s+_([\w\?\@]+)\s*\Z/)
            { # dynamic, static and forwarded symbols
                my $realname = $1;
                if($IsNeededLib and not $GroupNames->{parse_libname($Lib_Name, "name+ext")}) {
                    $DepSymbols{$LibVersion}{$realname} = 1;
                }
                if(not $IsNeededLib)
                {
                    $Symbol_Library{$LibVersion}{$realname} = $Lib_Name;
                    $Library_Symbol{$LibVersion}{$Lib_Name}{$realname} = 1;
                    if(not $Language{$LibVersion}{$Lib_Name})
                    {
                        if($realname =~/\A(_Z|\?)/) {
                            $Language{$LibVersion}{$Lib_Name} = "C++";
                            if(not $IsNeededLib) {
                                $COMMON_LANGUAGE{$LibVersion} = "C++";
                            }
                        }
                    }
                    if($CheckObjectsOnly
                    and $LibVersion==1) {
                        $CheckedSymbols{$realname} = 1;
                    }
                }
            }
        }
        close(LIB);
        if($LIB_TYPE eq "dynamic")
        { # dependencies
            open(LIB, "$DumpBinCmd /DEPENDENTS $Lib_Path 2>$TMP_DIR/null |");
            while(<LIB>) {
                if(/\s*([^\s]+?\.$LIB_EXT)\s*/i
                and $1 ne $Lib_Path) {
                    $NeededLib{path_format($1, $OSgroup)} = 1;
                }
            }
            close(LIB);
        }
    }
    else
    { # Unix; *.so, *.a
      # Symbian: *.dso, *.lib
        my $ReadelfCmd = get_CmdPath("readelf");
        if(not $ReadelfCmd) {
            exitStatus("Not_Found", "can't find \"readelf\"");
        }
        open(LIB, "$ReadelfCmd -WhlSsdA $Lib_Path 2>$TMP_DIR/null |");
        my $symtab=0; # indicates that we are processing 'symtab' section of 'readelf' output
        while(<LIB>)
        {
            if($LIB_TYPE eq "dynamic")
            { # dynamic library specifics
                if(/NEEDED.+\[([^\[\]]+)\]/)
                { # dependencies
                    $NeededLib{$1} = 1;
                    next;
                }
                if(/'.dynsym'/)
                { # dynamic table
                    $symtab=0;
                    next;
                }
                if($symtab == 1)
                { # do nothing with symtab
                    next;
                }
                if(/'.symtab'/)
                { # symbol table
                    $symtab=1;
                    next;
                }
            }
            if(my ($fullname, $idx, $Ndx, $type, $size, $bind) = readline_ELF($_))
            { # read ELF entry
                if( $Ndx eq "UND" )
                { # ignore interfaces that are imported from somewhere else
                    next;
                }
                if($bind eq "WEAK"
                and $Weak eq "-Weak")
                { # skip WEAK symbols
                    next;
                }
                my ($realname, $version_spec, $version) = separate_symbol($fullname);
                if($type eq "OBJECT")
                { # global data
                    $CompleteSignature{$LibVersion}{$fullname}{"Object"} = 1;
                    $CompleteSignature{$LibVersion}{$realname}{"Object"} = 1;
                }
                if($IsNeededLib and not $GroupNames->{parse_libname($Lib_Name, "name+ext")}) {
                    $DepSymbols{$LibVersion}{$fullname} = 1;
                }
                if(not $IsNeededLib)
                {
                    $Symbol_Library{$LibVersion}{$fullname} = $Lib_Name;
                    $Library_Symbol{$LibVersion}{$Lib_Name}{$fullname} = ($type eq "OBJECT")?-$size:1;
                    if($LIB_EXT eq "so")
                    { # value
                        $Interface_Value{$LibVersion}{$fullname} = $idx;
                        $Value_Interface{$LibVersion}{$idx}{$fullname} = 1;
                    }
                    if(not $Language{$LibVersion}{$Lib_Name})
                    {
                        if($fullname=~/\A(_Z|\?)/) {
                            $Language{$LibVersion}{$Lib_Name} = "C++";
                            if(not $IsNeededLib) {
                                $COMMON_LANGUAGE{$LibVersion} = "C++";
                            }
                        }
                    }
                    if($CheckObjectsOnly
                    and $LibVersion==1) {
                        $CheckedSymbols{$fullname} = 1;
                    }
                }
            }
        }
        close(LIB);
    }
    if(not $IsNeededLib and $LIB_EXT eq "so")
    { # get symbol versions
        foreach my $Symbol (keys(%{$Symbol_Library{$LibVersion}}))
        {
            next if($Symbol!~/\@/);
            my $Interface_SymName = "";
            foreach my $Symbol_SameValue (keys(%{$Value_Interface{$LibVersion}{$Interface_Value{$LibVersion}{$Symbol}}}))
            {
                if($Symbol_SameValue ne $Symbol
                and $Symbol_SameValue!~/\@/)
                {
                    $SymVer{$LibVersion}{$Symbol_SameValue} = $Symbol;
                    $Interface_SymName = $Symbol_SameValue;
                    last;
                }
            }
            if(not $Interface_SymName)
            {
                if($Symbol=~/\A([^\@\$\?]*)[\@\$]+([^\@\$]*)\Z/
                and not $SymVer{$LibVersion}{$1}) {
                    $SymVer{$LibVersion}{$1} = $Symbol;
                }
            }
        }
    }
    foreach my $DyLib (sort keys(%NeededLib))
    {
        my $DepPath = find_lib_path($LibVersion, $DyLib);
        if($DepPath and -f $DepPath) {
            getSymbols_Lib($LibVersion, $DepPath, 1, $GroupNames, "+Weak");
        }
    }
    pop(@RecurLib);
}

sub get_path_prefixes($)
{
    my $Path = $_[0];
    my ($Dir, $Name) = separate_path($Path);
    my %Prefixes = ();
    foreach my $Prefix (reverse(split(/[\/\\]+/, $Dir)))
    {
        $Prefixes{$Name} = 1;
        $Name = joinPath($Prefix, $Name);
        last if(keys(%Prefixes)>5 or $Prefix eq "include");
    }
    return keys(%Prefixes);
}

sub detectSystemHeaders()
{
    foreach my $DevelPath (keys(%{$SystemPaths{"include"}}))
    {
        next if(not -d $DevelPath);
        foreach my $Path (cmd_find($DevelPath,"f","",""))
        { # search for all header files in the /usr/include
          # with or without extension (ncurses.h, QtCore, ...)
            foreach my $Part (get_path_prefixes($Path)) {
                $SystemHeaders{$Part}{$Path}=1;
            }
        }
    }
    foreach my $DevelPath (keys(%{$SystemPaths{"lib"}}))
    {
        next if(not -d $DevelPath);
        foreach my $Path (cmd_find($DevelPath,"f","*.h",""))
        { # search for config headers in the /usr/lib
            foreach my $Part (get_path_prefixes($Path)) {
                $SystemHeaders{$Part}{$Path}=1;
            }
        }
    }
}

sub detectSystemObjects()
{
    foreach my $DevelPath (keys(%{$SystemPaths{"lib"}}))
    {
        next if(not -d $DevelPath);
        foreach my $Path (find_libs($DevelPath,"",""))
        { # search for shared libraries in the /usr/lib
            $SystemObjects{parse_libname(get_filename($Path), "name+ext")}{$Path}=1;
        }
    }
}

sub find_lib_path($$)
{
    my ($LibVersion, $DyLib) = @_;
    return "" if(not $DyLib or not $LibVersion);
    return $DyLib if(is_abs($DyLib));
    if(defined $Cache{"find_lib_path"}{$LibVersion}{$DyLib}) {
        return $Cache{"find_lib_path"}{$LibVersion}{$DyLib};
    }
    if(my @Paths = sort keys(%{$InputObject_Paths{$LibVersion}{$DyLib}})) {
        return ($Cache{"find_lib_path"}{$LibVersion}{$DyLib} = $Paths[0]);
    }
    elsif(my $DefaultPath = $DyLib_DefaultPath{$DyLib}) {
        return ($Cache{"find_lib_path"}{$LibVersion}{$DyLib} = $DefaultPath);
    }
    else
    {
        foreach my $Dir (sort keys(%DefaultLibPaths), sort keys(%{$SystemPaths{"lib"}}))
        { # search in default linker paths and then in all system paths
            if(-f $Dir."/".$DyLib) {
                return ($Cache{"find_lib_path"}{$LibVersion}{$DyLib} = joinPath($Dir,$DyLib));
            }
        }
        detectSystemObjects() if(not keys(%SystemObjects));
        if(my @AllObjects = keys(%{$SystemObjects{$DyLib}})) {
            return ($Cache{"find_lib_path"}{$LibVersion}{$DyLib} = $AllObjects[0]);
        }
        my $ShortName = parse_libname($DyLib, "name+ext");
        if($ShortName ne $DyLib
        and my $Path = find_lib_path($ShortName))
        { # FIXME: check this case
            return ($Cache{"find_lib_path"}{$LibVersion}{$DyLib} = $Path);
        }
        return ($Cache{"find_lib_path"}{$LibVersion}{$DyLib} = "");
    }
}

sub symbols_Preparation($)
{ # recreate environment using info from *.abi file
    my $LibVersion = $_[0];
    foreach my $Lib_Name (keys(%{$Library_Symbol{$LibVersion}}))
    {
        foreach my $Interface (keys(%{$Library_Symbol{$LibVersion}{$Lib_Name}}))
        {
            $Symbol_Library{$LibVersion}{$Interface} = $Lib_Name;
            if($Library_Symbol{$LibVersion}{$Lib_Name}{$Interface}<=-1)
            { # data marked as -size in the dump
                $CompleteSignature{$LibVersion}{$Interface}{"Object"} = 1;
            }
            if(not $Language{$LibVersion}{$Lib_Name}) {
                if($Interface=~/\A(_Z|\?)/) {
                    $Language{$LibVersion}{$Lib_Name} = "C++";
                }
            }
        }
    }
    my @VFunc = ();
    foreach my $FuncInfoId (keys(%{$SymbolInfo{$LibVersion}}))
    {
        my $MnglName = $SymbolInfo{$LibVersion}{$FuncInfoId}{"MnglName"};
        if(not $MnglName)
        { # C-functions
            next;
        }
        if(not $Symbol_Library{$LibVersion}{$MnglName}
        and not $DepSymbols{$LibVersion}{$MnglName}) {
            push(@VFunc, $MnglName);
        }
    }
    translateSymbols(@VFunc, $LibVersion);
}

sub getSoPaths($)
{
    my $LibVersion = $_[0];
    my @SoPaths = ();
    foreach my $Dest (split(/\s*\n\s*/, $Descriptor{$LibVersion}{"Libs"}))
    {
        if(not -e $Dest) {
            exitStatus("Access_Error", "can't access \'$Dest\'");
        }
        my @SoPaths_Dest = getSOPaths_Dest($Dest, $LibVersion);
        foreach (@SoPaths_Dest) {
            push(@SoPaths, $_);
        }
    }
    return @SoPaths;
}

sub skip_lib($$)
{
    my ($Path, $LibVersion) = @_;
    return 1 if(not $Path or not $LibVersion);
    my $LibName = get_filename($Path);
    return 1 if($SkipLibs{$LibVersion}{"Name"}{$LibName});
    my $ShortName = parse_libname($LibName, "name+ext");
    return 1 if($SkipLibs{$LibVersion}{"Name"}{$ShortName});
    foreach my $Dir (keys(%{$SkipLibs{$LibVersion}{"Path"}}))
    {
        if($Path=~/\Q$Dir\E/) {
            return 1;
        }
    }
    foreach my $Pattern (keys(%{$SkipLibs{$LibVersion}{"Pattern"}}))
    {
        return 1 if($LibName=~/$Pattern/);
        return 1 if($Pattern=~/[\/\\]/ and $Path=~/$Pattern/);
    }
    return 0;
}

sub skip_header($$)
{
    my ($Path, $LibVersion) = @_;
    return 1 if(not $Path or not $LibVersion);
    my $HeaderName = get_filename($Path);
    return 1 if($SkipHeaders{$LibVersion}{"Name"}{$HeaderName});
    foreach my $Dir (keys(%{$SkipHeaders{$LibVersion}{"Path"}}))
    {
        if($Path=~/\Q$Dir\E/) {
            return 1;
        }
    }
    foreach my $Pattern (keys(%{$SkipHeaders{$LibVersion}{"Pattern"}}))
    {
        if($HeaderName=~/$Pattern/) {
            return 1;
        }
        if($Pattern=~/[\/\\]/ and $Path=~/$Pattern/) {
            return 1;
        }
    }
    return 0;
}

sub register_objects($$)
{
    my ($Dir, $LibVersion) = @_;
    if($SystemPaths{"lib"}{$Dir})
    { # system directory
        return;
    }
    if($RegisteredObjDirs{$LibVersion}{$Dir})
    { # already registered
        return;
    }
    foreach my $Path (find_libs($Dir,"",1))
    {
        next if(ignore_path($Path));
        next if(skip_lib($Path, $LibVersion));
        $InputObject_Paths{$LibVersion}{get_filename($Path)}{$Path} = 1;
    }
    $RegisteredObjDirs{$LibVersion}{$Dir} = 1;
}

sub getSOPaths_Dest($$)
{
    my ($Dest, $LibVersion) = @_;
    if(skip_lib($Dest, $LibVersion)) {
        return ();
    }
    if(-f $Dest)
    {
        if(not parse_libname($Dest, "name")) {
            exitStatus("Error", "incorrect format of library (should be *.$LIB_EXT): \'$Dest\'");
        }
        $InputObject_Paths{$LibVersion}{get_filename($Dest)}{$Dest} = 1;
        register_objects(get_dirname($Dest), $LibVersion);
        return ($Dest);
    }
    elsif(-d $Dest)
    {
        $Dest=~s/[\/\\]+\Z//g;
        my @AllObjects = ();
        if($SystemPaths{"lib"}{$Dest})
        { # you have specified /usr/lib as the search directory (<libs>) in the XML descriptor
          # and the real name of the library by -l option (bz2, stdc++, Xaw, ...)
            foreach my $Path (cmd_find($Dest,"","*".esc($TargetLibraryName)."*\.$LIB_EXT*",2))
            { # all files and symlinks that match the name of a library
                if(get_filename($Path)=~/\A(|lib)\Q$TargetLibraryName\E[\d\-]*\.$LIB_EXT[\d\.]*\Z/i)
                {
                    $InputObject_Paths{$LibVersion}{get_filename($Path)}{$Path} = 1;
                    push(@AllObjects, resolve_symlink($Path));
                }
            }
        }
        else
        { # search for all files and symlinks
            foreach my $Path (find_libs($Dest,"",""))
            {
                next if(ignore_path($Path));
                next if(skip_lib($Path, $LibVersion));
                $InputObject_Paths{$LibVersion}{get_filename($Path)}{$Path} = 1;
                push(@AllObjects, resolve_symlink($Path));
            }
            if($OSgroup eq "macos")
            { # shared libraries on MacOS X may have no extension
                foreach my $Path (cmd_find($Dest,"f","",""))
                {
                    next if(ignore_path($Path));
                    next if(skip_lib($Path, $LibVersion));
                    if(get_filename($Path)!~/\./
                    and cmd_file($Path)=~/(shared|dynamic)\s+library/i) {
                        $InputObject_Paths{$LibVersion}{get_filename($Path)}{$Path} = 1;
                        push(@AllObjects, resolve_symlink($Path));
                    }
                }
            }
        }
        return @AllObjects;
    }
    else {
        return ();
    }
}

sub isCyclical($$) {
    return (grep {$_ eq $_[1]} @{$_[0]});
}

sub read_symlink($)
{
    my $Path = $_[0];
    return "" if(not $Path);
    return "" if(not -f $Path and not -l $Path);
    if(defined $Cache{"read_symlink"}{$Path}) {
        return $Cache{"read_symlink"}{$Path};
    }
    if(my $Res = readlink($Path)) {
        return ($Cache{"read_symlink"}{$Path} = $Res);
    }
    elsif(my $ReadlinkCmd = get_CmdPath("readlink")) {
        return ($Cache{"read_symlink"}{$Path} = `$ReadlinkCmd -n $Path`);
    }
    elsif(my $FileCmd = get_CmdPath("file"))
    {
        my $Info = `$FileCmd $Path`;
        if($Info=~/symbolic\s+link\s+to\s+['`"]*([\w\d\.\-\/\\]+)['`"]*/i) {
            return ($Cache{"read_symlink"}{$Path} = $1);
        }
    }
    return ($Cache{"read_symlink"}{$Path} = "");
}

sub resolve_symlink($)
{
    my $Path = $_[0];
    return "" if(not $Path);
    return "" if(not -f $Path and not -l $Path);
    if(defined $Cache{"resolve_symlink"}{$Path}) {
        return $Cache{"resolve_symlink"}{$Path};
    }
    return $Path if(isCyclical(\@RecurSymlink, $Path));
    push(@RecurSymlink, $Path);
    if(-l $Path and my $Redirect=read_symlink($Path))
    {
        if(is_abs($Redirect))
        { # absolute path
            if($SystemRoot and $SystemRoot ne "/"
            and $Path=~/\A\Q$SystemRoot\E\//
            and (-f $SystemRoot.$Redirect or -l $SystemRoot.$Redirect))
            { # symbolic links from the sysroot
              # should be corrected to point to
              # the files inside sysroot
                $Redirect = $SystemRoot.$Redirect;
            }
            my $Res = resolve_symlink($Redirect);
            pop(@RecurSymlink);
            return ($Cache{"resolve_symlink"}{$Path} = $Res);
        }
        elsif($Redirect=~/\.\.[\/\\]/)
        { # relative path
            $Redirect = joinPath(get_dirname($Path),$Redirect);
            while($Redirect=~s&(/|\\)[^\/\\]+(\/|\\)\.\.(\/|\\)&$1&){};
            my $Res = resolve_symlink($Redirect);
            pop(@RecurSymlink);
            return ($Cache{"resolve_symlink"}{$Path} = $Res);
        }
        elsif(-f get_dirname($Path)."/".$Redirect)
        { # file name in the same directory
            my $Res = resolve_symlink(joinPath(get_dirname($Path),$Redirect));
            pop(@RecurSymlink);
            return ($Cache{"resolve_symlink"}{$Path} = $Res);
        }
        else
        { # broken link
            pop(@RecurSymlink);
            return ($Cache{"resolve_symlink"}{$Path} = "");
        }
    }
    pop(@RecurSymlink);
    return ($Cache{"resolve_symlink"}{$Path} = $Path);
}

sub genDescriptorTemplate()
{
    writeFile("VERSION.xml", $Descriptor_Template."\n");
    print "XML-descriptor template ./VERSION.xml has been generated\n";
}

sub detectWordSize()
{
    return "" if(not $GCC_PATH);
    if($Cache{"detectWordSize"}) {
        return $Cache{"detectWordSize"};
    }
    writeFile("$TMP_DIR/empty.h", "");
    my $Defines = `$GCC_PATH -E -dD $TMP_DIR/empty.h`;
    unlink("$TMP_DIR/empty.h");
    my $WSize = 0;
    if($Defines=~/ __SIZEOF_POINTER__\s+(\d+)/)
    {# GCC 4
        $WSize = $1;
    }
    elsif($Defines=~/ __PTRDIFF_TYPE__\s+(\w+)/)
    {# GCC 3
        my $PTRDIFF = $1;
        if($PTRDIFF=~/long/) {
            $WSize = 8;
        }
        else {
            $WSize = 4;
        }
    }
    if(not int($WSize)) {
        exitStatus("Error", "can't check WORD size");
    }
    return ($Cache{"detectWordSize"} = $WSize);
}

sub majorVersion($)
{
    my $Version = $_[0];
    return 0 if(not $Version);
    my @VParts = split(/\./, $Version);
    return $VParts[0];
}

sub cmpVersions($$)
{# compare two versions in dotted-numeric format
    my ($V1, $V2) = @_;
    return 0 if($V1 eq $V2);
    return undef if($V1!~/\A\d+[\.\d+]*\Z/);
    return undef if($V2!~/\A\d+[\.\d+]*\Z/);
    my @V1Parts = split(/\./, $V1);
    my @V2Parts = split(/\./, $V2);
    for (my $i = 0; $i <= $#V1Parts && $i <= $#V2Parts; $i++) {
        return -1 if(int($V1Parts[$i]) < int($V2Parts[$i]));
        return 1 if(int($V1Parts[$i]) > int($V2Parts[$i]));
    }
    return -1 if($#V1Parts < $#V2Parts);
    return 1 if($#V1Parts > $#V2Parts);
    return 0;
}

sub read_ABI_Dump($$)
{
    my ($LibVersion, $Path) = @_;
    return if(not $LibVersion or not -e $Path);
    my $FilePath = unpackDump($Path);
    if($FilePath!~/\.abi\Z/) {
        exitStatus("Invalid_Dump", "specified ABI dump \'$Path\' is not valid, try to recreate it");
    }
    my $Content = readFile($FilePath);
    unlink($FilePath);
    if($Content!~/};\s*\Z/) {
        exitStatus("Invalid_Dump", "specified ABI dump \'$Path\' is not valid, try to recreate it");
    }
    my $LibraryABI = eval($Content);
    if(not $LibraryABI) {
        exitStatus("Error", "internal error - eval() procedure seem to not working correctly, try to remove 'use strict' and try again");
    }
    # new dumps (>=1.22) have a personal versioning
    my $DumpVersion = $LibraryABI->{"ABI_DUMP_VERSION"};
    my $ToolVersion = $LibraryABI->{"ABI_COMPLIANCE_CHECKER_VERSION"};
    if(not $DumpVersion)
    { # old dumps (<=1.21.6) have been marked by the tool version
        $DumpVersion = $ToolVersion;
    }
    $UsedDump{$LibVersion} = $DumpVersion;
    if(majorVersion($DumpVersion) ne $ABI_DUMP_MAJOR)
    { # should be compatible with dumps of the same major version
        if(cmpVersions($DumpVersion, $ABI_DUMP_VERSION)>0)
        { # Don't know how to parse future dump formats
            exitStatus("Dump_Version", "incompatible version $DumpVersion of specified ABI dump (newer than $ABI_DUMP_VERSION)");
        }
        elsif(cmpVersions($DumpVersion, $ABI_COMPLIANCE_CHECKER_VERSION)>0 and not $LibraryABI->{"ABI_DUMP_VERSION"})
        { # Don't know how to parse future dump formats
            exitStatus("Dump_Version", "incompatible version $DumpVersion of specified ABI dump (newer than $ABI_COMPLIANCE_CHECKER_VERSION)");
        }
        if($UseOldDumps) {
            if(cmpVersions($DumpVersion, $OLDEST_SUPPORTED_VERSION)<0) {
                exitStatus("Dump_Version", "incompatible version $DumpVersion of specified ABI dump (older than $OLDEST_SUPPORTED_VERSION)");
            }
        }
        else
        {
            my $Msg = "incompatible version $DumpVersion of specified ABI dump (allowed only $ABI_DUMP_MAJOR.0<=V<=$ABI_DUMP_MAJOR.9)";
            if(cmpVersions($DumpVersion, $OLDEST_SUPPORTED_VERSION)>=0) {
                $Msg .= "\nUse -old-dumps option to use old-version dumps ($OLDEST_SUPPORTED_VERSION<=V<$ABI_DUMP_MAJOR.0)";
            }
            exitStatus("Dump_Version", $Msg);
        }
    }
    $TypeInfo{$LibVersion} = $LibraryABI->{"TypeInfo"};
    if(not $TypeInfo{$LibVersion})
    { # support for old dumps
        $TypeInfo{$LibVersion} = $LibraryABI->{"TypeDescr"};
    }
    if(cmpVersions($DumpVersion, "2.0")<0)
    { # support for old dumps: type names corrected in ACC 1.22 (dump 2.0 format)
        foreach my $TypeDeclId (keys(%{$TypeInfo{$LibVersion}})) {
            foreach my $TypeId (keys(%{$TypeInfo{$LibVersion}{$TypeDeclId}}))
            {
                my $TName = $TypeInfo{$LibVersion}{$TypeDeclId}{$TypeId}{"Name"};
                if($TName=~/\A(\w+)::(\w+)/) {
                    my ($Part1, $Part2) = ($1, $2);
                    if($Part1 eq $Part2) {
                        $TName=~s/\A$Part1:\:$Part1(\W)/$Part1$1/;
                    }
                    else {
                        $TName=~s/\A(\w+:\:)$Part2:\:$Part2(\W)/$1$Part2$2/;
                    }
                }
                $TypeInfo{$LibVersion}{$TypeDeclId}{$TypeId}{"Name"} = $TName;
            }
        }
    }
    foreach my $TypeDeclId (sort {int($a)<=>int($b)} keys(%{$TypeInfo{$LibVersion}}))
    {
        foreach my $TypeId (sort {int($a)<=>int($b)} keys(%{$TypeInfo{$LibVersion}{$TypeDeclId}}))
        {
            my %TInfo = %{$TypeInfo{$LibVersion}{$TypeDeclId}{$TypeId}};
            if(defined $TInfo{"Base"})
            {
                foreach (keys(%{$TInfo{"Base"}})) {
                    $Class_SubClasses{$LibVersion}{$_}{$TypeId}=1;
                }
            }
            if($TInfo{"Type"} eq "Typedef")
            {
                my ($BTDid, $BTid) = ($TInfo{"BaseType"}{"TDid"}, $TInfo{"BaseType"}{"Tid"});
                $Typedef_BaseName{$LibVersion}{$TInfo{"Name"}} = $TypeInfo{$LibVersion}{$BTDid}{$BTid}{"Name"};
            }
            if(not $TName_Tid{$LibVersion}{$TInfo{"Name"}})
            { # classes: class (id1), typedef (artificial, id2 > id1)
                $TName_Tid{$LibVersion}{$TInfo{"Name"}} = $TypeId;
            }
        }
    }
    $SymbolInfo{$LibVersion} = $LibraryABI->{"SymbolInfo"};
    if(not $SymbolInfo{$LibVersion})
    { # support for old dumps
        $SymbolInfo{$LibVersion} = $LibraryABI->{"FuncDescr"};
    }
    if(not keys(%{$SymbolInfo{$LibVersion}}))
    { # validation of old-version dumps
        exitStatus("Invalid_Dump", "the input dump d$LibVersion is invalid");
    }
    $Library_Symbol{$LibVersion} = $LibraryABI->{"Symbols"};
    if(not $Library_Symbol{$LibVersion})
    { # support for old dumps
        $Library_Symbol{$LibVersion} = $LibraryABI->{"Interfaces"};
    }
    $DepSymbols{$LibVersion} = $LibraryABI->{"DepSymbols"};
    if(not $DepSymbols{$LibVersion})
    { # support for old dumps
        $DepSymbols{$LibVersion} = $LibraryABI->{"DepInterfaces"};
    }
    if(not $DepSymbols{$LibVersion})
    { # support for old dumps
      # Cannot reconstruct DepSymbols. This may result in false
      # positives if the old dump is for library 2. Not a problem if
      # old dumps are only from old libraries.
        $DepSymbols{$LibVersion} = {};
    }
    $SymVer{$LibVersion} = $LibraryABI->{"SymbolVersion"};
    $Tid_TDid{$LibVersion} = $LibraryABI->{"Tid_TDid"};
    $Descriptor{$LibVersion}{"Version"} = $LibraryABI->{"LibraryVersion"};
    $SkipTypes{$LibVersion} = $LibraryABI->{"SkipTypes"};
    if(not $SkipTypes{$LibVersion})
    { # support for old dumps
        $SkipTypes{$LibVersion} = $LibraryABI->{"OpaqueTypes"};
    }
    $SkipSymbols{$LibVersion} = $LibraryABI->{"SkipSymbols"};
    if(not $SkipSymbols{$LibVersion})
    { # support for old dumps
        $SkipSymbols{$LibVersion} = $LibraryABI->{"SkipInterfaces"};
    }
    if(not $SkipSymbols{$LibVersion})
    { # support for old dumps
        $SkipSymbols{$LibVersion} = $LibraryABI->{"InternalInterfaces"};
    }
    read_Headers_DumpInfo($LibraryABI, $LibVersion);
    read_Libs_DumpInfo($LibraryABI, $LibVersion);
    $Constants{$LibVersion} = $LibraryABI->{"Constants"};
    $NestedNameSpaces{$LibVersion} = $LibraryABI->{"NameSpaces"};
    if(not $NestedNameSpaces{$LibVersion})
    { # support for old dumps
      # Cannot reconstruct NameSpaces. This may affect design
      # of the compatibility report.
        $NestedNameSpaces{$LibVersion} = {};
    }
    # target system type
    # needed to adopt HTML report
    if(not $DumpSystem{"Name"})
    { # to use in createSymbolsList(...)
        $OStarget = $LibraryABI->{"Target"};
    }
    read_Machine_DumpInfo($LibraryABI, $LibVersion);
    symbols_Preparation($LibVersion);
    $Descriptor{$LibVersion}{"Dump"} = 1;
}

sub read_Machine_DumpInfo($$)
{
    my ($LibraryABI, $LibVersion) = @_;
    if($LibraryABI->{"Arch"}) {
        $CPU_ARCH{$LibVersion} = $LibraryABI->{"Arch"};
    }
    if($LibraryABI->{"WordSize"}) {
        $WORD_SIZE{$LibVersion} = $LibraryABI->{"WordSize"};
    }
    if(not $WORD_SIZE{$LibVersion}
    and $LibraryABI->{"SizeOfPointer"})
    {# support for old dumps
        $WORD_SIZE{$LibVersion} = $LibraryABI->{"SizeOfPointer"};
    }
    if($LibraryABI->{"GccVersion"}) {
        $GCC_VERSION{$LibVersion} = $LibraryABI->{"GccVersion"};
    }
}

sub read_Libs_DumpInfo($$)
{
    my ($LibraryABI, $LibVersion) = @_;
    if(keys(%{$Library_Symbol{$LibVersion}})
    and not $DumpAPI) {
        $Descriptor{$LibVersion}{"Libs"} = "OK";
    }
}

sub read_Headers_DumpInfo($$)
{
    my ($LibraryABI, $LibVersion) = @_;
    if(keys(%{$LibraryABI->{"Headers"}})
    and not $DumpAPI) {
        $Descriptor{$LibVersion}{"Headers"} = "OK";
    }
    foreach my $Identity (keys(%{$LibraryABI->{"Headers"}}))
    {# headers info is stored in the old dumps in the different way
        if($UseOldDumps
        and my $Name = $LibraryABI->{"Headers"}{$Identity}{"Name"})
        {# support for old dumps: headers info corrected in 1.22
            $Identity = $Name;
        }
        $Target_Headers{$LibVersion}{$Identity}{"Identity"} = $Identity;
    }
}

sub find_libs($$$)
{
    my ($Path, $Type, $MaxDepth) = @_;
    # FIXME: correct the search pattern
    return cmd_find($Path, $Type, ".*\\.$LIB_EXT\[0-9.]*", $MaxDepth);
}

sub createDescriptor_Rpm_Deb($$)
{
    my ($LibVersion, $VirtualPath) = @_;
    return if(not $LibVersion or not $VirtualPath or not -e $VirtualPath);
    my $IncDir = "none";
    $IncDir = $VirtualPath."/usr/include/" if(-d $VirtualPath."/usr/include/");
    $IncDir = $VirtualPath."/include/" if(-d $VirtualPath."/include/");
    my $LibDir = "none";
    $LibDir = $VirtualPath."/lib64/" if(-d $VirtualPath."/lib64/");
    $LibDir = $VirtualPath."/usr/lib64/" if(-d $VirtualPath."/usr/lib64/");
    $LibDir = $VirtualPath."/lib/" if(-d $VirtualPath."/lib/");
    $LibDir = $VirtualPath."/usr/lib/" if(-d $VirtualPath."/usr/lib/");
    if($IncDir ne "none")
    {
        my @Objects = ();
        foreach my $Path (find_libs($LibDir,"f","")) {
            push(@Objects, $Path);
        }
        $LibDir = "none" if($#Objects==-1);
    }
    if($IncDir eq "none" and $LibDir eq "none") {
        exitStatus("Error", "can't find headers and $SLIB_TYPE libraries in the package d$LibVersion");
    }
    readDescriptor($LibVersion,"
      <version>
          ".$TargetVersion{$LibVersion}."
      </version>

      <headers>
          $IncDir
      </headers>

      <libs>
          $LibDir
      </libs>");
}

sub createDescriptor($$)
{
    my ($LibVersion, $Path) = @_;
    return if(not $LibVersion or not $Path or not -e $Path);
    if($Path=~/\.abi(\Q.tar.gz\E|\Q.zip\E)\Z/)
    {# read an ABI dump (*.zip or *.tar.gz)
        read_ABI_Dump($LibVersion, $Path);
    }
    else
    {
        if($Path=~/\.deb\Z/i)
        {# Deb "devel" package
            my $ArCmd = get_CmdPath("ar");
            if(not $ArCmd) {
                exitStatus("Not_Found", "can't find \"ar\"");
            }
            my $VirtualPath = "$TMP_DIR/package.v$LibVersion/";
            mkpath($VirtualPath);
            system("cd $VirtualPath && $ArCmd p ".get_abs_path($Path)." data.tar.gz | tar zx");
            if($?) {
                exitStatus("Error", "can't extract package d$LibVersion");
            }
            if(not $TargetVersion{$LibVersion})
            {# auto detect version of Deb package
                system("cd $VirtualPath && $ArCmd p ".get_abs_path($Path)." control.tar.gz | tar zx");
                if(-f "$VirtualPath/control") {
                    my $ControlInfo = `grep Version $VirtualPath/control`;
                    if($ControlInfo=~/Version\s*:\s*([\d\.\-\_]*\d)/) {
                        $TargetVersion{$LibVersion} = $1;
                    }
                }
            }
            if(not $TargetVersion{$LibVersion})
            {
                if($Path=~/_([\d\.\-\_]*\d)(.+?)\.deb\Z/i)
                {# auto detect version of Deb package
                    $TargetVersion{$LibVersion} = $1;
                }
            }
            if(not $TargetVersion{$LibVersion}) {
                exitStatus("Error", "specify ".($LibVersion==1?"1st":"2nd")." version number (-v$LibVersion <num> option)");
            }
            createDescriptor_Rpm_Deb($LibVersion, $VirtualPath);
        }
        elsif($Path=~/\.rpm\Z/i)
        {# RPM "devel" package
            my $Rpm2CpioCmd = get_CmdPath("rpm2cpio");
            if(not $Rpm2CpioCmd) {
                exitStatus("Not_Found", "can't find \"rpm2cpio\"");
            }
            my $CpioCmd = get_CmdPath("cpio");
            if(not $CpioCmd) {
                exitStatus("Not_Found", "can't find \"cpio\"");
            }
            my $VirtualPath = "$TMP_DIR/package.v$LibVersion/";
            mkpath($VirtualPath);
            system("cd $VirtualPath && $Rpm2CpioCmd ".get_abs_path($Path)." | cpio -id --quiet");
            if($?) {
                exitStatus("Error", "can't extract package d$LibVersion");
            }
            my $RpmCmd = get_CmdPath("rpm");
            if(not $RpmCmd) {
                exitStatus("Not_Found", "can't find \"rpm\"");
            }
            if(not $TargetVersion{$LibVersion}) {
                $TargetVersion{$LibVersion} = `$RpmCmd -qp --queryformat \%{version} $Path 2>$TMP_DIR/null`;
            }
            if(not $TargetVersion{$LibVersion})
            {
                if($Path=~/-([\d\.\-\_]*\d)(.+?)\.rpm\Z/i)
                {# auto detect version of Rpm package
                    $TargetVersion{$LibVersion} = $1;
                }
            }
            if(not $TargetVersion{$LibVersion}) {
                exitStatus("Error", "specify ".($LibVersion==1?"1st":"2nd")." version number (-v$LibVersion <num> option)");
            }
            createDescriptor_Rpm_Deb($LibVersion, $VirtualPath);
        }
        elsif(-d $Path)
        {# directory with headers files and shared objects
            readDescriptor($LibVersion,"
              <version>
                  ".$TargetVersion{$LibVersion}."
              </version>

              <headers>
                  $Path
              </headers>

              <libs>
                  $Path
              </libs>");
        }
        else
        {# files
            if($Path=~/\.xml\Z/i)
            {# standard XML-descriptor
                readDescriptor($LibVersion, readFile($Path));
            }
            elsif(is_header($Path, 1, $LibVersion))
            {# header file
                readDescriptor($LibVersion,"
                  <version>
                      ".$TargetVersion{$LibVersion}."
                  </version>

                  <headers>
                      $Path
                  </headers>

                  <libs>
                      none
                  </libs>");
            }
            elsif(parse_libname($Path, "name"))
            {# shared object
                readDescriptor($LibVersion,"
                  <version>
                      ".$TargetVersion{$LibVersion}."
                  </version>

                  <headers>
                      none
                  </headers>

                  <libs>
                      $Path
                  </libs>");
            }
            else
            {# standard XML-descriptor
                readDescriptor($LibVersion, readFile($Path));
            }
        }
        # create log directory
        my ($LOG_DIR, $LOG_FILE) = ("logs/$TargetLibraryName/".$Descriptor{$LibVersion}{"Version"}, "log.txt");
        if($OutputLogPath{$LibVersion}) {# used defined by -log-path option
            ($LOG_DIR, $LOG_FILE) = separate_path($OutputLogPath{$LibVersion});
        }
        mkpath($LOG_DIR);
        $LOG_PATH{$LibVersion} = get_abs_path($LOG_DIR)."/".$LOG_FILE;
        unlink($LOG_PATH{$LibVersion});# remove an old log
        if($Debug) {
            $DEBUG_PATH{$LibVersion} = "debug/$TargetLibraryName/".$Descriptor{$LibVersion}{"Version"};
            rmtree($DEBUG_PATH{$LibVersion});
        }
    }
}

sub detect_lib_default_paths()
{
    my %LPaths = ();
    if($OSgroup eq "bsd")
    {
        if(my $LdConfig = get_CmdPath("ldconfig")) {
            foreach my $Line (split(/\n/, `$LdConfig -r 2>$TMP_DIR/null`)) {
                if($Line=~/\A[ \t]*\d+:\-l(.+) \=\> (.+)\Z/) {
                    $LPaths{"lib".$1} = $2;
                }
            }
        }
        else {
            print "WARNING: can't find ldconfig\n";
        }
    }
    else
    {
        if(my $LdConfig = get_CmdPath("ldconfig"))
        {
            if($SystemRoot and $Config{"osname"} eq "linux")
            {# use host (x86) ldconfig with the target (arm) ld.so.conf
                if(-e $SystemRoot."/etc/ld.so.conf") {
                    $LdConfig .= " -f ".$SystemRoot."/etc/ld.so.conf";
                }
            }
            foreach my $Line (split(/\n/, `$LdConfig -p 2>$TMP_DIR/null`)) {
                if($Line=~/\A[ \t]*([^ \t]+) .* \=\> (.+)\Z/) {
                    $LPaths{$1} = $2;
                }
            }
        }
        elsif($Config{"osname"}=~/linux/i) {
            print "WARNING: can't find ldconfig\n";
        }
    }
    return \%LPaths;
}

sub detect_bin_default_paths()
{
    my $EnvPaths = $ENV{"PATH"};
    if($OSgroup eq "beos") {
        $EnvPaths.=":".$ENV{"BETOOLS"};
    }
    my $Sep = ($OSgroup eq "windows")?";":":|;";
    foreach my $Path (sort {length($a)<=>length($b)} split(/$Sep/, $EnvPaths))
    {
        $Path = path_format($Path, $OSgroup);
        $Path=~s/[\/\\]+\Z//g;
        next if(not $Path);
        if($SystemRoot
        and $Path=~/\A\Q$SystemRoot\E\//)
        { # do NOT use binaries from target system
            next;
        }
        $DefaultBinPaths{$Path} = 1;
    }
}

sub detect_inc_default_paths()
{
    return () if(not $GCC_PATH);
    my %DPaths = ("Cpp"=>{},"Gcc"=>{},"Inc"=>{});
    writeFile("$TMP_DIR/empty.h", "");
    foreach my $Line (split(/\n/, `$GCC_PATH -v -x c++ -E "$TMP_DIR/empty.h" 2>&1`))
    {# detecting gcc default include paths
        if($Line=~/\A[ \t]*((\/|\w+:\\).+)[ \t]*\Z/)
        {
            my $Path = simplify_path($1);
            $Path=~s/[\/\\]+\Z//g;
            $Path = path_format($Path, $OSgroup);
            if($Path=~/c\+\+|\/g\+\+\//)
            {
                $DPaths{"Cpp"}{$Path}=1;
                if(not defined $MAIN_CPP_DIR
                or get_depth($MAIN_CPP_DIR)>get_depth($Path)) {
                    $MAIN_CPP_DIR = $Path;
                }
            }
            elsif($Path=~/gcc/) {
                $DPaths{"Gcc"}{$Path}=1;
            }
            else
            {
                next if($Path=~/local[\/\\]+include/);
                if($SystemRoot
                and $Path!~/\A\Q$SystemRoot\E(\/|\Z)/)
                { # The GCC include path for user headers is not a part of the system root
                  # The reason: you are not specified the --cross-gcc option or selected a wrong compiler
                  # or it is the internal cross-GCC path like arm-linux-gnueabi/include
                    next;
                }
                $DPaths{"Inc"}{$Path}=1;
            }
        }
    }
    unlink("$TMP_DIR/empty.h");
    return %DPaths;
}

sub detect_default_paths($)
{
    my ($HSearch, $LSearch, $BSearch, $GSearch) = (1, 1, 1, 1);
    my $Search = $_[0];
    if($Search!~/inc/) {
        $HSearch = 0;
    }
    if($Search!~/lib/) {
        $LSearch = 0;
    }
    if($Search!~/bin/) {
        $BSearch = 0;
    }
    if($Search!~/gcc/) {
        $GSearch = 0;
    }
    if(keys(%{$SystemPaths{"include"}}))
    { # <search_headers> section of the XML descriptor
      # do NOT search for systems headers
        $HSearch = 0;
    }
    if(keys(%{$SystemPaths{"lib"}}))
    { # <search_headers> section of the XML descriptor
      # do NOT search for systems headers
        $LSearch = 0;
    }
    foreach my $Type (keys(%{$OS_AddPath{$OSgroup}}))
    {# additional search paths
        next if($Type eq "include" and not $HSearch);
        next if($Type eq "lib" and not $LSearch);
        next if($Type eq "bin" and not $BSearch);
        foreach my $Path (keys(%{$OS_AddPath{$OSgroup}{$Type}}))
        {
            next if(not -d $Path);
            $SystemPaths{$Type}{$Path} = $OS_AddPath{$OSgroup}{$Type}{$Path};
        }
    }
    if($OSgroup ne "windows")
    {# unix-like
        foreach my $Type ("include", "lib", "bin")
        {# automatic detection of system "devel" directories
            next if($Type eq "include" and not $HSearch);
            next if($Type eq "lib" and not $LSearch);
            next if($Type eq "bin" and not $BSearch);
            my ($UsrDir, $RootDir) = ("/usr", "/");
            if($SystemRoot and $Type ne "bin")
            {# 1. search for target headers and libraries
             # 2. use host commands: ldconfig, readelf, etc.
                ($UsrDir, $RootDir) = ("$SystemRoot/usr", $SystemRoot);
            }
            foreach my $Path (cmd_find($RootDir,"d","*$Type*",1)) {
                $SystemPaths{$Type}{$Path} = 1;
            }
            if(-d $RootDir."/".$Type)
            {# if "/lib" is symbolic link
                if($RootDir eq "/") {
                    $SystemPaths{$Type}{"/".$Type} = 1;
                }
                else {
                    $SystemPaths{$Type}{$RootDir."/".$Type} = 1;
                }
            }
            if(-d $UsrDir) {
                foreach my $Path (cmd_find($UsrDir,"d","*$Type*",1)) {
                    $SystemPaths{$Type}{$Path} = 1;
                }
                if(-d $UsrDir."/".$Type)
                {# if "/usr/lib" is symbolic link
                    $SystemPaths{$Type}{$UsrDir."/".$Type} = 1;
                }
            }
        }
    }
    if($BSearch)
    {
        detect_bin_default_paths();
        foreach my $Path (keys(%DefaultBinPaths)) {
            $SystemPaths{"bin"}{$Path} = $DefaultBinPaths{$Path};
        }
    }
    # check environment variables
    if($OSgroup eq "beos")
    {
        if($HSearch)
        {
            foreach my $Path (split(/:|;/, $ENV{"BEINCLUDES"}))
            {
                if(is_abs($Path)) {
                    $DefaultIncPaths{$Path} = 1;
                }
            }
        }
        if($LSearch)
        {
            foreach my $Path (split(/:|;/, $ENV{"BELIBRARIES"}), split(/:|;/, $ENV{"LIBRARY_PATH"}))
            {
                if(is_abs($Path)) {
                    $DefaultLibPaths{$Path} = 1;
                }
            }
        }
    }
    if($LSearch)
    { # using linker to get system paths
        if(my $LPaths = detect_lib_default_paths())
        {# unix-like
            foreach my $Name (keys(%{$LPaths}))
            {
                if($SystemRoot
                and $LPaths->{$Name}!~/\A\Q$SystemRoot\E\//)
                { # wrong ldconfig configuration
                  # check your <sysroot>/etc/ld.so.conf
                    next;
                }
                $DyLib_DefaultPath{$Name} = $LPaths->{$Name};
                $DefaultLibPaths{get_dirname($LPaths->{$Name})} = 1;
            }
        }
        foreach my $Path (keys(%DefaultLibPaths)) {
            $SystemPaths{"lib"}{$Path} = $DefaultLibPaths{$Path};
        }
    }
    if($GSearch)
    { # GCC path and default include dirs
        if($CrossGcc)
        { # -cross-gcc arm-linux-gcc
            if(-e $CrossGcc)
            { # absolute or relative path
                $GCC_PATH = get_abs_path($CrossGcc);
            }
            elsif($CrossGcc!~/\// and get_CmdPath($CrossGcc))
            { # command name
                $GCC_PATH = $CrossGcc;
            }
            else {
                exitStatus("Access_Error", "can't access \'$CrossGcc\'");
            }
            if($GCC_PATH=~/\s/) {
                $GCC_PATH = "\"".$GCC_PATH."\"";
            }
        }
        else {
            $GCC_PATH = get_CmdPath("gcc");
        }
        if(not $GCC_PATH) {
            exitStatus("Not_Found", "can't find GCC>=3.0 in PATH");
        }
        if(not $CheckObjectsOnly_Opt)
        {
            if(my $GCC_Ver = get_dumpversion($GCC_PATH))
            {
                my $GccTarget = get_dumpmachine($GCC_PATH);
                print "Using GCC $GCC_Ver ($GccTarget)\n";
                if($GccTarget=~/symbian/)
                {
                    $OStarget = "symbian";
                    $LIB_EXT = $OS_LibExt{$LIB_TYPE}{$OStarget};
                }
            }
            else {
                exitStatus("Error", "something is going wrong with the GCC compiler");
            }
        }
        if(not $NoStdInc)
        { # do NOT search in GCC standard paths
            my %DPaths = detect_inc_default_paths();
            %DefaultCppPaths = %{$DPaths{"Cpp"}};
            %DefaultGccPaths = %{$DPaths{"Gcc"}};
            %DefaultIncPaths = %{$DPaths{"Inc"}};
            foreach my $Path (keys(%DefaultIncPaths)) {
                $SystemPaths{"include"}{$Path} = $DefaultIncPaths{$Path};
            }
        }
    }
    if($HSearch)
    { # user include paths
        if(-d $SystemRoot."/usr/include") {
            $UserIncPath{$SystemRoot."/usr/include"}=1;
        }
    }
}

sub get_dumpversion($)
{
    my $Cmd = $_[0];
    return "" if(not $Cmd);
    if($Cache{"get_dumpversion"}{$Cmd}) {
        return $Cache{"get_dumpversion"}{$Cmd};
    }
    my $Version = `$Cmd -dumpversion 2>$TMP_DIR/null`;
    chomp($Version);
    return ($Cache{"get_dumpversion"}{$Cmd} = $Version);
}

sub get_dumpmachine($)
{
    my $Cmd = $_[0];
    return "" if(not $Cmd);
    if($Cache{"get_dumpmachine"}{$Cmd}) {
        return $Cache{"get_dumpmachine"}{$Cmd};
    }
    my $Machine = `$Cmd -dumpmachine 2>$TMP_DIR/null`;
    chomp($Machine);
    return ($Cache{"get_dumpmachine"}{$Cmd} = $Machine);
}

sub check_command($)
{
    my $Cmd = $_[0];
    return "" if(not $Cmd);
    my @Options = (
        "--version",
        "-help"
    );
    foreach my $Opt (@Options)
    {
        my $Info = `$Cmd $Opt 2>$TMP_DIR/null`;
        if($Info) {
            return 1;
        }
    }
    return 0;
}

sub check_gcc_version($$)
{
    my ($Cmd, $Req) = @_;
    return 0 if(not $Cmd or not $Req);
    my $GccVersion = get_dumpversion($Cmd);
    if(cmpVersions($GccVersion, $Req)>=0) {
        return $Cmd;
    }
    return "";
}

sub get_depth($)
{
    if(defined $Cache{"get_depth"}{$_[0]}) {
        return $Cache{"get_depth"}{$_[0]}
    }
    return ($Cache{"get_depth"}{$_[0]} = ($_[0]=~tr![\/\\]|\:\:!!));
}

sub find_gcc_cxx_headers($)
{
    my $LibVersion = $_[0];
    return if($Cache{"find_gcc_cxx_headers"});# this function should be called once
    # detecting system header paths
    foreach my $Path (sort {get_depth($b) <=> get_depth($a)} keys(%DefaultGccPaths))
    {
        foreach my $HeaderPath (sort {get_depth($a) <=> get_depth($b)} cmd_find($Path,"f","",""))
        {
            my $FileName = get_filename($HeaderPath);
            next if($DefaultGccHeader{$FileName});
            $DefaultGccHeader{$FileName} = $HeaderPath;
        }
    }
    if(($COMMON_LANGUAGE{$LibVersion} eq "C++" or $CheckHeadersOnly) and not $STDCXX_TESTING)
    {
        foreach my $CppDir (sort {get_depth($b)<=>get_depth($a)} keys(%DefaultCppPaths))
        {
            my @AllCppHeaders = cmd_find($CppDir,"f","","");
            foreach my $Path (sort {get_depth($a)<=>get_depth($b)} @AllCppHeaders)
            {
                my $FileName = get_filename($Path);
                next if($DefaultCppHeader{$FileName});
                $DefaultCppHeader{$FileName} = $Path;
            }
        }
    }
    $Cache{"find_gcc_cxx_headers"} = 1;
}

sub parse_libname($$)
{
    my ($Name, $Type) = @_;
    if($OStarget eq "symbian") {
        return parse_libname_symbian($Name, $Type);
    }
    elsif($OStarget eq "windows") {
        return parse_libname_windows($Name, $Type);
    }
    if($Name=~/((((lib|).+?)([\-\_][\d\-\.\_]+|))\.$LIB_EXT)(\.(.+)|)\Z/)
    { # libSDL-1.2.so.0.7.1
      # libwbxml2.so.0.0.18
        if($Type eq "name")
        { # libSDL-1.2
          # libwbxml2
            return $2;
        }
        elsif($Type eq "name+ext")
        { # libSDL-1.2.so
          # libwbxml2.so
            return $1;
        }
        elsif($Type eq "version")
        {
            if($7 ne "")
            { # 0.7.1
                return $7;
            }
            else
            { # libc-2.5.so (=>2.5 version)
                my $MV = $5;
                $MV=~s/\A[\-\_]+//g;
                return $MV;
            }
        }
        elsif($Type eq "short")
        { # libSDL
          # libwbxml2
            return $3;
        }
        elsif($Type eq "shortest")
        { # SDL
          # wbxml
            return shortest_name($3);
        }
    }
    return "";# error
}

sub parse_libname_symbian($$)
{
    my ($Name, $Type) = @_;
    if($Name=~/(((.+?)(\{.+\}|))\.$LIB_EXT)\Z/)
    { # libpthread{00010001}.dso
        if($Type eq "name")
        { # libpthread{00010001}
            return $2;
        }
        elsif($Type eq "name+ext")
        { # libpthread{00010001}.dso
            return $1;
        }
        elsif($Type eq "version")
        { # 00010001
            my $V = $4;
            $V=~s/\{(.+)\}/$1/;
            return $V;
        }
        elsif($Type eq "short")
        { # libpthread
            return $3;
        }
        elsif($Type eq "shortest")
        { # pthread
            return shortest_name($3);
        }
    }
    return "";# error
}

sub parse_libname_windows($$)
{
    my ($Name, $Type) = @_;
    if($Name=~/((.+?)\.$LIB_EXT)\Z/)
    { # netapi32.dll
        if($Type eq "name")
        { # netapi32
            return $2;
        }
        elsif($Type eq "name+ext")
        { # netapi32.dll
            return $1;
        }
        elsif($Type eq "version")
        { # DLL version embedded
          # at binary-level
            return "";
        }
        elsif($Type eq "short")
        { # netapi32
            return $2;
        }
        elsif($Type eq "shortest")
        { # netapi
            return shortest_name($2);
        }
    }
    return "";# error
}

sub shortest_name($)
{
    my $Name = $_[0];
    # remove prefix
    $Name=~s/\A(lib|open)//;
    # remove suffix
    $Name=~s/[\W\d_]+\Z//i;
    $Name=~s/([a-z]{2,})(lib)\Z/$1/i;
    return $Name;
}

sub getPrefix($)
{
    my $Str = $_[0];
    if($Str=~/\A(Get|get|Set|set)([A-Z]|_)/)
    {# GetError
        return "";
    }
    if($Str=~/\A([_]*[A-Z][a-z]{1,5})[A-Z]/)
    {# XmuValidArea: Xmu
        return $1;
    }
    elsif($Str=~/\A([_]*[a-z]+)[A-Z]/)
    {# snfReadFont: snf
        return $1;
    }
    elsif($Str=~/\A([_]*[A-Z]{2,})[A-Z][a-z]+([A-Z][a-z]+|\Z)/)
    {# XRRTimes: XRR
        return $1;
    }
    elsif($Str=~/\A([_]*[a-z0-9]{2,}_)[a-z]+/i)
    {# alarm_event_add: alarm_
        return $1;
    }
    elsif($Str=~/\A(([a-z])\2{1,})/i)
    {# ffopen
        return $1;
    }
    else {
        return "";
    }
}

sub problem_title($)
{
    if($_[0]==1)  {
        return "1 problem";
    }
    else  {
        return $_[0]." problems";
    }
}

sub warning_title($)
{
    if($_[0]==1)  {
        return "1 warning";
    }
    else  {
        return $_[0]." warnings";
    }
}

sub readLineNum($$)
{
    my ($Path, $Num) = @_;
    return "" if(not $Path or not -f $Path);
    open (FILE, $Path);
    foreach (1 ... $Num) {
        <FILE>;
    }
    my $Line = <FILE>;
    close(FILE);
    return $Line;
}

sub readAttributes($)
{
    my $Path = $_[0];
    return () if(not $Path or not -f $Path);
    my %Attributes = ();
    if(readLineNum($Path, 0)=~/<!--\s+(.+)\s+-->/) {
        foreach my $AttrVal (split(/;/, $1)) {
            if($AttrVal=~/(.+):(.+)/)
            {
                my ($Name, $Value) = ($1, $2);
                $Attributes{$Name} = $Value;
            }
        }
    }
    return \%Attributes;
}

sub createSymbolsList($$$$$)
{
    my ($DPath, $SaveTo, $LName, $LVersion, $ArchName) = @_;
    read_ABI_Dump(1, $DPath);
    prepareInterfaces(1);
    my %SymbolHeaderLib = ();
    my $Total = 0;
    # Get List
    foreach my $Symbol (sort keys(%{$CompleteSignature{1}}))
    {
        if(not link_symbol($Symbol, 1, "-Deps"))
        {# skip src only and all external functions
            next;
        }
        if(not symbolFilter($Symbol, 1, "Public"))
        { # skip other symbols
            next;
        }
        my $HeaderName = $CompleteSignature{1}{$Symbol}{"Header"};
        if(not $HeaderName)
        {# skip src only and all external functions
            next;
        }
        my $DyLib = $Symbol_Library{1}{$Symbol};
        if(not $DyLib)
        {# skip src only and all external functions
            next;
        }
        $SymbolHeaderLib{$HeaderName}{$DyLib}{$Symbol} = 1;
        $Total+=1;
    }
    # Draw List
    my $SYMBOLS_LIST = "<h1>Public symbols in <span style='color:Blue;'>$LName</span> (<span style='color:Red;'>$LVersion</span>)";
    $SYMBOLS_LIST .= " on <span style='color:Blue;'>".showArch($ArchName)."</span><br/>Total: $Total</h1><br/>";
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%SymbolHeaderLib))
    {
        foreach my $DyLib (sort {lc($a) cmp lc($b)} keys(%{$SymbolHeaderLib{$HeaderName}}))
        {
            if($HeaderName and $DyLib) {
                $SYMBOLS_LIST .= "<span class='h_name'>$HeaderName</span>, <span class='lib_name'>$DyLib</span><br/>\n";
            }
            elsif($DyLib) {
                $SYMBOLS_LIST .= "<span class='lib_name'>$DyLib</span><br/>\n";
            }
            elsif($HeaderName) {
                $SYMBOLS_LIST .= "<span class='h_name'>$HeaderName</span><br/>\n";
            }
            my %NS_Symbol = ();
            foreach my $Symbol (keys(%{$SymbolHeaderLib{$HeaderName}{$DyLib}})) {
                $NS_Symbol{get_IntNameSpace($Symbol, 1)}{$Symbol} = 1;
            }
            foreach my $NameSpace (sort keys(%NS_Symbol))
            {
                $SYMBOLS_LIST .= ($NameSpace)?"<span class='ns_title'>namespace</span> <span class='ns'>$NameSpace</span>"."<br/>\n":"";
                my @SortedInterfaces = sort {lc(get_Signature($a, 1)) cmp lc(get_Signature($b, 1))} keys(%{$NS_Symbol{$NameSpace}});
                foreach my $Symbol (@SortedInterfaces)
                {
                    my $SubReport = "";
                    my $Signature = get_Signature($Symbol, 1);
                    if($NameSpace) {
                        $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                    }
                    if($Symbol=~/\A(_Z|\?)/)
                    {
                        if($Signature) {
                            $SubReport = insertIDs($ContentSpanStart.highLight_Signature_Italic_Color($Signature).$ContentSpanEnd."<br/>\n".$ContentDivStart."<span class='mangled'>[ symbol: <b>$Symbol</b> ]</span><br/><br/>".$ContentDivEnd."\n");
                        }# report_added
                        else {
                            $SubReport = "<span class='iname'>".$Symbol."</span><br/>\n";
                        }
                    }
                    else
                    {
                        if($Signature) {
                            $SubReport = "<span class='iname'>".highLight_Signature_Italic_Color($Signature)."</span><br/>\n";
                        }
                        else {
                            $SubReport = "<span class='iname'>".$Symbol."</span><br/>\n";
                        }
                    }
                    $SYMBOLS_LIST .= $SubReport;
                }
            }
            $SYMBOLS_LIST .= "<br/>\n";
        }
    }
    # Clear Info
    (%TypeInfo, %SymbolInfo, %Library_Symbol,
    %DepSymbols, %SymVer, %Tid_TDid, %SkipTypes,
    %SkipSymbols, %NestedNameSpaces, %ClassMethods,
    %AllocableClass, %ClassToId, %CompleteSignature,
    %Symbol_Library) = ();
    ($Content_Counter, $ContentID) = (0, 0);
    # Print Report
    my $CssStyles = "
    <style type=\"text/css\">
    body {
        font-family:Arial;
        color:Black;
        font-size:14px;
    }
    hr {
        color:Black;
        background-color:Black;
        height:1px;
        border:0;
    }
    h1 {
        font-size:26px;
    }
    span.iname {
        font-weight:bold;
        font-size:16px;
        color:#003E69;
        margin-left:7px;
    }
    span.section {
        font-weight:bold;
        cursor:pointer;
        margin-left:7px;
        font-size:16px;
        color:#003E69;
        white-space:nowrap;
    }
    span:hover.section {
        color:#336699;
    }
    span.h_name {
        color:#cc3300;
        font-size:14px;
        font-weight:bold;
    }
    span.ns_title {
        margin-left:2px;
        color:#408080;
        font-size:13px;
    }
    span.ns {
        color:#408080;
        font-size:13px;
        font-weight:bold;
    }
    span.lib_name {
        color:Green;
        font-size:14px;
        font-weight:bold;
    }
    span.int_p {
        font-weight:normal;
        white-space:normal;
    }
    span.mangled {
        padding-left:15px;
        font-size:13px;
        cursor:text;
        color:#444444;
    }
    span.symver {
        color:#333333;
        font-size:14px;
        white-space:nowrap;
    }
    span.color_p {
        font-style:italic;
        color:Brown;
    }
    span.nowrap {
        white-space:nowrap;
    }
    </style>";
    $SYMBOLS_LIST = "<a name='Top'></a>".$SYMBOLS_LIST."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    my $Title = "$LName: public symbols";
    my $Keywords = "$LName, API, symbols";
    my $Description = "List of symbols in $LName ($LVersion) on ".showArch($ArchName);
    $SYMBOLS_LIST = composeHTML_Head($Title, $Keywords, $Description, $CssStyles."\n".$JScripts)."
    <body><div>\n$SYMBOLS_LIST</div>
    <br/><br/><hr/>\n".getReportFooter($LName)."
    <div style='height:999px;'></div></body></html>";
    writeFile($SaveTo, $SYMBOLS_LIST);
}

sub cmpSystems()
{ # -cmp-systems option handler
  # should be used with -d1 and -d2 options
    if(not $Descriptor{1}{"Path"}) {
        exitStatus("Error", "the option -d1 should be specified");
    }
    elsif(not -d $Descriptor{1}{"Path"}) {
        exitStatus("Access_Error", "can't access directory \'".$Descriptor{1}{"Path"}."\'");
    }
    elsif(not -d $Descriptor{1}{"Path"}."/abi_dumps") {
        exitStatus("Access_Error", "can't access directory \'".$Descriptor{1}{"Path"}."/abi_dumps\'");
    }
    if(not $Descriptor{2}{"Path"}) {
        exitStatus("Error", "the option -d2 should be specified");
    }
    elsif(not -d $Descriptor{2}{"Path"}) {
        exitStatus("Access_Error", "can't access directory \'".$Descriptor{2}{"Path"}."\'");
    }
    elsif(not -d $Descriptor{2}{"Path"}."/abi_dumps") {
        exitStatus("Access_Error", "can't access directory \'".$Descriptor{2}{"Path"}."/abi_dumps\'");
    }
    # sys_dumps/<System>/<Arch>/...
    my $SystemName1 = get_filename(get_dirname($Descriptor{1}{"Path"}));
    my $SystemName2 = get_filename(get_dirname($Descriptor{2}{"Path"}));
    # sys_dumps/<System>/<Arch>/...
    my $ArchName = get_filename($Descriptor{1}{"Path"});
    if($ArchName ne get_filename($Descriptor{2}{"Path"})) {
        exitStatus("Error", "can't compare systems of different CPU architecture");
    }
    if(my $OStarget_Dump = readFile($Descriptor{1}{"Path"}."/target.txt"))
    {# change target
        $OStarget = $OStarget_Dump;
        $LIB_EXT = $OS_LibExt{$LIB_TYPE}{$OStarget};
        if(not $LIB_EXT) {
            $LIB_EXT = $OS_LibExt{$LIB_TYPE}{"default"};
        }
    }
    if(my $Mode = readFile($Descriptor{1}{"Path"}."/mode.txt"))
    { # change mode
        if($Mode eq "headers-only")
        { # -headers-only mode
            $CheckHeadersOnly = 1;
            $GroupByHeaders = 1;
        }
        if($Mode eq "group-by-headers") {
            $GroupByHeaders = 1;
        }
    }
    my $SYS_REPORT_PATH = "sys_compat_reports/".$SystemName1."_to_".$SystemName2."/$ArchName";
    rmtree($SYS_REPORT_PATH);
    my (%LibSoname1, %LibSoname2) = ();
    foreach (split(/\n/, readFile($Descriptor{1}{"Path"}."/sonames.txt"))) {
        if(my ($LFName, $Soname) = split(/;/, $_))
        {
            if($OStarget eq "symbian") {
                $Soname=~s/\{.+\}//;
            }
            $LibSoname1{$LFName} = $Soname;
        }
    }
    foreach (split(/\n/, readFile($Descriptor{2}{"Path"}."/sonames.txt"))) {
        if(my ($LFName, $Soname) = split(/;/, $_))
        {
            if($OStarget eq "symbian") {
                $Soname=~s/\{.+\}//;
            }
            $LibSoname2{$LFName} = $Soname;
        }
    }
    my (%LibV1, %LibV2) = ();
    foreach (split(/\n/, readFile($Descriptor{1}{"Path"}."/versions.txt"))) {
        if(my ($LFName, $V) = split(/;/, $_)) {
            $LibV1{$LFName} = $V;
        }
    }
    foreach (split(/\n/, readFile($Descriptor{2}{"Path"}."/versions.txt"))) {
        if(my ($LFName, $V) = split(/;/, $_)) {
            $LibV2{$LFName} = $V;
        }
    }
    my @Dumps1 = cmd_find($Descriptor{1}{"Path"}."/abi_dumps","f","*.tar.gz",1);
    if(not @Dumps1)
    { # zip-based dump
        @Dumps1 = cmd_find($Descriptor{1}{"Path"}."/abi_dumps","f","*.zip",1);
    }
    my @Dumps2 = cmd_find($Descriptor{2}{"Path"}."/abi_dumps","f","*.tar.gz",1);
    if(not @Dumps2)
    { # zip-based dump
        @Dumps2 = cmd_find($Descriptor{2}{"Path"}."/abi_dumps","f","*.zip",1);
    }
    my (%LibVers1, %LibVers2) = ();
    foreach my $DPath (@Dumps1)
    {
        if(get_filename($DPath)=~/\A(.+)\.abi(\Q.tar.gz\E|\Q.zip\E)\Z/)
        {
            my ($Soname, $V) = ($LibSoname1{$1}, $LibV1{$1});
            if(not $V) {
                $V = parse_libname($1, "version");
            }
            if($GroupByHeaders) {
                $Soname = $1;
            }
            $LibVers1{$Soname}{$V} = $DPath;
        }
    }
    foreach my $DPath (@Dumps2)
    {
        if(get_filename($DPath)=~/\A(.+)\.abi(\Q.tar.gz\E|\Q.zip\E)\Z/)
        {
            my ($Soname, $V) = ($LibSoname2{$1}, $LibV2{$1});
            if(not $V) {
                $V = parse_libname($1, "version");
            }
            if($GroupByHeaders) {
                $Soname = $1;
            }
            $LibVers2{$Soname}{$V} = $DPath;
        }
    }
    my (%Added, %Removed) = ();
    my (%ChangedSoname, %TestResults, %SONAME_Changed);
    if(not $GroupByHeaders)
    {
        my %ChangedSoname_Safe = ();
        foreach my $LName (sort keys(%LibSoname2))
        { # libcurl.so.3 -> libcurl.so.4 (search for SONAME by the file name)
          # OS #1 => OS #2
            if(defined $LibVers2{$LName})
            { # already registered
                next;
            }
            my $Soname = $LibSoname2{$LName};
            if(defined $LibVers2{$Soname}
            and defined $LibVers1{$LName})
            {
                $LibVers2{$LName} = $LibVers2{$Soname};
                $ChangedSoname_Safe{$Soname}=$LName;
            }
        }
        foreach my $LName (sort keys(%LibSoname1))
        { # libcurl.so.3 -> libcurl.so.4 (search for SONAME by the file name)
          # OS #1 <= OS #2
            if(defined $LibVers1{$LName})
            { # already registered
                next;
            }
            my $Soname = $LibSoname1{$LName};
            if(defined $LibVers1{$Soname}
            and defined $LibVers2{$LName}) {
                $LibVers1{$LName} = $LibVers1{$Soname};
            }
        }
        my (%AddedShort, %RemovedShort) = ();
        if(not $GroupByHeaders) {
            print "Checking added/removed libs\n";
        }
        foreach my $LName (sort {lc($a) cmp lc($b)} keys(%LibVers1))
        { # removed libs
            if(not is_target_lib($LName)) {
                next;
            }
            if(not defined $LibVers1{$LName}) {
                next;
            }
            my @Versions1 = keys(%{$LibVers1{$LName}});
            if($#Versions1>=1)
            { # should be only one version
                next;
            }
            if(not defined $LibVers2{$LName}
            or not keys(%{$LibVers2{$LName}}))
            { # removed library
                if(not $LibSoname2{$LName})
                {
                    $RemovedShort{parse_libname($LName, "name+ext")}{$LName}=1;
                    $Removed{$LName}{"version"}=$Versions1[0];
                    my $ListPath = "info/$LName/symbols.html";
                    createSymbolsList($LibVers1{$LName}{$Versions1[0]},
                    $SYS_REPORT_PATH."/".$ListPath, $LName, $Versions1[0]."-".$SystemName1, $ArchName);
                    $Removed{$LName}{"list"} = $ListPath;
                }
            }
        }
        foreach my $LName (sort {lc($a) cmp lc($b)} keys(%LibVers2))
        { # added libs
            if(not is_target_lib($LName)) {
                next;
            }
            if(not defined $LibVers2{$LName}) {
                next;
            }
            my @Versions2 = keys(%{$LibVers2{$LName}});
            if($#Versions2>=1)
            { # should be only one version
                next;
            }
            if($ChangedSoname_Safe{$LName})
            { # changed soname but added the symbolic link for old-version library
                next;
            }
            if(not defined $LibVers1{$LName}
            or not keys(%{$LibVers1{$LName}}))
            { # added library
                if(not $LibSoname1{$LName})
                {
                    $AddedShort{parse_libname($LName, "name+ext")}{$LName}=1;
                    $Added{$LName}{"version"}=$Versions2[0];
                    my $ListPath = "info/$LName/symbols.html";
                    createSymbolsList($LibVers2{$LName}{$Versions2[0]},
                    $SYS_REPORT_PATH."/".$ListPath, $LName, $Versions2[0]."-".$SystemName2, $ArchName);
                    $Added{$LName}{"list"} = $ListPath;
                }
            }
        }
        foreach my $LSName (keys(%AddedShort))
        { # changed SONAME
            my @AddedSonames = keys(%{$AddedShort{$LSName}});
            next if(length(@AddedSonames)!=1);
            my @RemovedSonames = keys(%{$RemovedShort{$LSName}});
            next if(length(@RemovedSonames)!=1);
            $ChangedSoname{$AddedSonames[0]}=$RemovedSonames[0];
            $ChangedSoname{$RemovedSonames[0]}=$AddedSonames[0];
        }
    }
    foreach my $LName (sort {lc($a) cmp lc($b)} keys(%LibVers1))
    {
        if(not is_target_lib($LName)) {
            next;
        }
        my @Versions1 = keys(%{$LibVers1{$LName}});
        if(not @Versions1 or $#Versions1>=1)
        { # should be only one version
            next;
        }
        my $LV1 = $Versions1[0];
        my $DPath1 = $LibVers1{$LName}{$LV1};
        my @Versions2 = keys(%{$LibVers2{$LName}});
        if($#Versions2>=1)
        { # should be only one version
            next;
        }
        my ($LV2, $LName2, $DPath2) = ();
        if(@Versions2)
        {
            $LV2 = $Versions2[0];
            $DPath2 = $LibVers2{$LName}{$LV2};
        }
        elsif($LName2 = $ChangedSoname{$LName})
        { # changed SONAME
            @Versions2 = keys(%{$LibVers2{$LName2}});
            if(not @Versions2 or $#Versions2>=1) {
                next;
            }
            $LV2 = $Versions2[0];
            $DPath2 = $LibVers2{$LName2}{$LV2};
            $LName = parse_libname($LName, "name+ext");
            $SONAME_Changed{$LName} = 1;
        }
        else
        { # removed
            next;
        }
        my ($FV1, $FV2) = ($LV1."-".$SystemName1, $LV2."-".$SystemName2);
        my $ACC_compare = "perl $0 -l $LName -d1 \"$DPath1\" -d2 \"$DPath2\"";
        my $LReportPath = "compat_reports/$LName/abi_compat_report.html";
        my $LReportPath_Full = $SYS_REPORT_PATH."/".$LReportPath;
        $ACC_compare .= " -report-path \"$LReportPath_Full\"";
        if($CheckHeadersOnly) {
            $ACC_compare .= " -headers-only";
        }
        if($GroupByHeaders) {
            $ACC_compare .= " -component header";
        }
        if($Debug)
        { # debug mode
            print "$ACC_compare\n";
        }
        print "Checking $LName: ";
        system($ACC_compare." 1>$TMP_DIR/null 2>$TMP_DIR/$LName.stderr");
        if(-s "$TMP_DIR/$LName.stderr")
        {
            my $ErrorLog = readFile("$TMP_DIR/$LName.stderr");
            chomp($ErrorLog);
            print "Failed ($ErrorLog)\n";
        }
        else
        {
            print "Ok\n";
            $TestResults{$LName} = readAttributes($LReportPath_Full);
            $TestResults{$LName}{"v1"} = $LV1;
            $TestResults{$LName}{"v2"} = $LV2;
            $TestResults{$LName}{"path"} = $LReportPath;
        }
    }
    my $SYS_REPORT_STYLES = "
    <style type=\"text/css\">
    body {
        font-family:Arial;
        font-size:14px;
        background:#ffffff;
        color:Black;
        padding-left:15px;
    }
    hr {
        color:Black;
        background-color:Black;
        height:1px;
        border:0;
    }
    h1 {
        margin-bottom:3px;
        padding-bottom:3px;
        font-size:26px;
        white-space:nowrap;
    }
    table.wikitable {
        border-collapse:collapse;
        margin:1em 1em 1em 0;
        background:#f9f9f9;
        border:1px #aaaaaa solid;
    }
    table.wikitable td, th {
        border:1px #aaaaaa solid;
        text-align:center;
        padding:0.2em;
        padding-left:5px;
        padding-right:5px;
        white-space:nowrap;
    }
    table.wikitable th {
        background:#f2f2f2;
    }
    table.wikitable td.left {
        text-align:left;
        background-color:#f2f2f2;
    }
    td.passed {
        background-color:#CCFFCC;
    }
    td.warning {
        background-color:#F4F4AF;
    }
    td.failed {
        background-color:#FFC3CE;
    }
    td.new {
        background-color:#C6DEFF;
    }
    a.default {
        color:#336699;
    }
    th.severity {
        width:85px;
        color:Black;
    }
    sup {
        font-size:10px;
    }
    </style>";
    my $SONAME_Title = "SONAME";
    if($OStarget eq "windows") {
        $SONAME_Title = "DLL";
    }
    elsif($OStarget eq "symbian") {
        $SONAME_Title = "DSO";
    }
    if($GroupByHeaders)
    { # show the list of headers
        $SONAME_Title = "Header File";
    }
    my $SYS_REPORT = "<h1>Binary compatibility between <span style='color:Blue;'>$SystemName1</span> and <span style='color:Blue;'>$SystemName2</span> on <span style='color:Blue;'>".showArch($ArchName)."</span></h1>\n";
    # legend
    $SYS_REPORT .= "<table>
    <tr><td class='new' width='80px'>Added</td><td class='passed' width='80px'>Compatible</td></tr>
    <tr><td class='warning'>Warning</td><td class='failed'>Incompatible</td></tr>
    </table>";
    $SYS_REPORT .= "<table class='wikitable'>
    <tr><th rowspan='2'>$SONAME_Title<sup>".(keys(%TestResults) + keys(%Added) + keys(%Removed) - keys(%SONAME_Changed))."</sup></th>";
    if(not $GroupByHeaders) {
        $SYS_REPORT .= "<th colspan='2'>VERSION</th>";
    }
    $SYS_REPORT .= "<th rowspan='2'>Compatibility</th>
    <th rowspan='2'>Added<br/>Symbols</th>
    <th rowspan='2'>Removed<br/>Symbols</th>
    <th colspan='3' style='white-space:nowrap;'>Backward Binary Compatibility Problems</th></tr>";
    if(not $GroupByHeaders) {
        $SYS_REPORT .= "<tr><th>$SystemName1</th><th>$SystemName2</th>";
    }
    $SYS_REPORT .= "<th class='severity'>High</th><th class='severity'>Medium</th><th class='severity'>Low</th></tr>\n";
    my %RegisteredPairs = ();
    foreach my $LName (sort {lc($a) cmp lc($b)} (keys(%TestResults), keys(%Added), keys(%Removed)))
    {
        next if($SONAME_Changed{$LName});
        my $CompatReport = $TestResults{$LName}{"path"};
        my $Anchor = $LName;
        $Anchor=~s/\+/p/g;# anchors to libFLAC++ is libFLACpp
        $Anchor=~s/\~//g;# libqttracker.so.1~6
        $SYS_REPORT .= "<tr>\n<td class='left'>$LName<a name=\'$Anchor\'></a></td>\n";
        if(defined $Removed{$LName}) {
            $SYS_REPORT .= "<td class='failed'>".$Removed{$LName}{"version"}."</td>\n";
        }
        elsif(defined $Added{$LName}) {
            $SYS_REPORT .= "<td class='new'><a href='".$Added{$LName}{"list"}."'>added</a></td>\n";
        }
        elsif(not $GroupByHeaders) {
            $SYS_REPORT .= "<td>".$TestResults{$LName}{"v1"}."</td>\n";
        }
        if(defined $Added{$LName})
        {# added library
            $SYS_REPORT .= "<td class='new'>".$Added{$LName}{"version"}."</td>\n";
            $SYS_REPORT .= "<td class='passed'>100%</td>\n";
            if($RegisteredPairs{$LName}) {
                # do nothing
            }
            elsif(my $To = $ChangedSoname{$LName})
            {
                $RegisteredPairs{$To}=1;
                $SYS_REPORT .= "<td colspan='5' rowspan='2'>SONAME has changed, see <a href='".$TestResults{parse_libname($LName, "name+ext")}{"path"}."'>ABI differences</a></td>\n";
            }
            else {
                foreach (1 .. 5) {
                    $SYS_REPORT .= "<td>n/a</td>\n"; # colspan='5'
                }
            }
            $SYS_REPORT .= "</tr>\n";
            next;
        }
        elsif(defined $Removed{$LName})
        {# removed library
            $SYS_REPORT .= "<td class='failed'><a href='".$Removed{$LName}{"list"}."'>removed</a></td>\n";
            $SYS_REPORT .= "<td class='failed'>0%</td>\n";
            if($RegisteredPairs{$LName}) {
                # do nothing
            }
            elsif(my $To = $ChangedSoname{$LName})
            {
                $RegisteredPairs{$To}=1;
                $SYS_REPORT .= "<td colspan='5' rowspan='2'>SONAME has changed, see <a href='".$TestResults{parse_libname($LName, "name+ext")}{"path"}."'>ABI differences</a></td>\n";
            }
            else {
                foreach (1 .. 5) {
                    $SYS_REPORT .= "<td>n/a</td>\n"; # colspan='5'
                }
            }
            $SYS_REPORT .= "</tr>\n";
            next;
        }
        elsif(not $GroupByHeaders) {
            $SYS_REPORT .= "<td>".$TestResults{$LName}{"v2"}."</td>\n";
        }
        if($TestResults{$LName}{"verdict"} eq "compatible") {
            $SYS_REPORT .= "<td class='passed'><a href=\'$CompatReport\'>100%</a></td>\n";
        }
        else
        {
            my $Compatible = 100 - $TestResults{$LName}{"affected"};
            $SYS_REPORT .= "<td class='failed'><a href=\'$CompatReport\'>$Compatible%</a></td>\n";
        }
        my $AddedSym="";
        if(my $Count = $TestResults{$LName}{"added"}) {
            $AddedSym="<a href='$CompatReport\#Added'>$Count new</a>";
        }
        if($AddedSym) {
            $SYS_REPORT.="<td class='new'>$AddedSym</td>";
        }
        else {
            $SYS_REPORT.="<td class='passed'>0</td>";
        }
        my $RemovedSym="";
        if(my $Count = $TestResults{$LName}{"removed"}) {
            $RemovedSym="<a href='$CompatReport\#Removed'>$Count removed</a>";
        }
        if($RemovedSym) {
            $SYS_REPORT.="<td class='failed'>$RemovedSym</td>";
        }
        else {
            $SYS_REPORT.="<td class='passed'>0</td>";
        }
        my $High="";
        if(my $Count = $TestResults{$LName}{"type_problems_high"}+$TestResults{$LName}{"interface_problems_high"}) {
            $High="<a href='$CompatReport\#High_Risk_Problems'>".problem_title($Count)."</a>";
        }
        if($High) {
            $SYS_REPORT.="<td class='failed'>$High</td>";
        }
        else {
            $SYS_REPORT.="<td class='passed'>0</td>";
        }
        my $Medium="";
        if(my $Count = $TestResults{$LName}{"type_problems_medium"}+$TestResults{$LName}{"interface_problems_medium"}) {
            $Medium="<a href='$CompatReport\#Medium_Risk_Problems'>".problem_title($Count)."</a>";
        }
        if($Medium) {
            $SYS_REPORT.="<td class='failed'>$Medium</td>";
        }
        else {
            $SYS_REPORT.="<td class='passed'>0</td>";
        }
        my $Low="";
        if(my $Count = $TestResults{$LName}{"type_problems_low"}+$TestResults{$LName}{"interface_problems_low"}+$TestResults{$LName}{"changed_constants"}) {
            $Low="<a href='$CompatReport\#Low_Risk_Problems'>".warning_title($Count)."</a>";
        }
        if($Low) {
            $SYS_REPORT.="<td class='warning'>$Low</td>";
        }
        else {
            $SYS_REPORT.="<td class='passed'>0</td>";
        }
        $SYS_REPORT .= "</tr>\n";
    }
    # bottom header
    $SYS_REPORT .= "<tr><th rowspan='2'>$SONAME_Title</th>";
    if(not $GroupByHeaders) {
        $SYS_REPORT .= "<th>$SystemName1</th><th>$SystemName2</th>";
    }
    $SYS_REPORT .= "<th rowspan='2'>Compatibility</th>
    <th rowspan='2'>Added<br/>Symbols</th>
    <th rowspan='2'>Removed<br/>Symbols</th>
    <th class='severity'>High</th><th class='severity'>Medium</th><th class='severity'>Low</th></tr>";
    if(not $GroupByHeaders) {
        $SYS_REPORT .= "<tr><th colspan='2'>VERSION</th>";
    }
    $SYS_REPORT .= "<th colspan='3' style='white-space:nowrap;'>Backward Binary Compatibility Problems</th></tr>\n";
    $SYS_REPORT .= "</table>";
    my $Title = "$SystemName1 to $SystemName2 binary compatibility report";
    my $Keywords = "compatibility, $SystemName1, $SystemName2, API";
    my $Description = "Binary compatibility between $SystemName1 and $SystemName2 on ".showArch($ArchName);
    writeFile($SYS_REPORT_PATH."/abi_compat_report.html", composeHTML_Head($Title, $Keywords, $Description, $SYS_REPORT_STYLES)."\n<body>
    <div>$SYS_REPORT</div>
    <br/><br/><br/><hr/>
    ".getReportFooter($SystemName2)."
    <div style='height:999px;'></div>\n</body></html>");
    print "see detailed report:\n  $SYS_REPORT_PATH/abi_compat_report.html\n";
}

sub check_list($$)
{
    my ($Item, $Skip) = @_;
    return 0 if(not $Skip);
    my @Patterns = @{$Skip};
    foreach my $Pattern (@Patterns)
    {
        if($Pattern=~s/\*/.*/g)
        { # wildcards
            if($Item=~/$Pattern/) {
                return 1;
            }
        }
        elsif($Pattern=~/[\/\\]/)
        { # directory
            if($Item=~/\Q$Pattern\E/) {
                return 1;
            }
        }
        elsif($Item eq $Pattern
        or get_filename($Item) eq $Pattern)
        { # by name
            return 1;
        }
    }
    return 0;
}

sub is_target_lib($)
{
    my $LName = $_[0];
    if($TargetLibraryName
    and $LName!~/\Q$TargetLibraryName\E/) {
        return 0;
    }
    if($TargetLibsPath
    and not $TargetLibs{$LName}
    and not $TargetLibs{parse_libname($LName, "name+ext")}) {
        return 0;
    }
    return 1;
}

sub filter_format($)
{
    my $FiltRef = $_[0];
    foreach my $Entry (keys(%{$FiltRef}))
    {
        foreach my $Filt (@{$FiltRef->{$Entry}})
        {
            if($Filt=~/[\/\\]/) {
                $Filt = path_format($Filt, $OSgroup);
            }
        }
    }
}

sub read_sys_descriptor($)
{
    my $Path = $_[0];
    my $Content = readFile($Path);
    my %Tags = (
        "headers" => "mf",
        "skip_headers" => "mf",
        "skip_libs" => "mf",
        "include_preamble" => "mf",
        "non_self_compiled" => "mf",
        "add_include_paths" => "mf",
        "gcc_options" => "m",
        "skip_symbols" => "m",
        "skip_types" => "m",
        "ignore_symbols" => "h",
        "defines" => "s"
    );
    my %DInfo = ();
    foreach my $Tag (keys(%Tags))
    {
        if(my $TContent = parseTag(\$Content, $Tag))
        {
            if($Tags{$Tag}=~/m/)
            { # multi-line (+order)
                my @Items = split(/\s*\n\s*/, $TContent);
                $DInfo{$Tag} = [];
                foreach my $Item (@Items)
                {
                    if($Tags{$Tag}=~/f/) {
                        $Item = path_format($Item, $OSgroup);
                    }
                    push(@{$DInfo{$Tag}}, $Item);
                }
            
            }
            elsif($Tags{$Tag}=~/s/)
            { # single element
                $DInfo{$Tag} = $TContent;
            }
            else
            { # hash array
                my @Items = split(/\s*\n\s*/, $TContent);
                foreach my $Item (@Items) {
                    $DInfo{$Tag}{$Item}=1;
                }
            }
        }
    }
    return \%DInfo;
}

sub read_sys_info($)
{
    my $Target = $_[0];
    my $SYS_INFO_PATH = $TOOL_DIR."/modules/SysInfo";
    if(-d $SYS_INFO_PATH."/".$Target)
    { # symbian, windows
        $SYS_INFO_PATH .= "/".$Target;
    }
    else
    { # default
        $SYS_INFO_PATH .= "/unix";
    }
    if($TargetSysInfo)
    { # user-defined target
        $SYS_INFO_PATH = $TargetSysInfo;
    }
    if(not -d $SYS_INFO_PATH) {
        exitStatus("Module_Error", "can't access \'$SYS_INFO_PATH\'");
    }
    # Library Specific Info
    my %SysInfo = ();
    if(not -d $SYS_INFO_PATH."/descriptors/") {
        exitStatus("Module_Error", "can't access \'$SYS_INFO_PATH/descriptors\'");
    }
    foreach my $DPath (cmd_find($SYS_INFO_PATH."/descriptors/","f","",1))
    {
        my $LSName = get_filename($DPath);
        $LSName=~s/\.xml\Z//;
        $SysInfo{$LSName} = read_sys_descriptor($DPath);
    }
    # Exceptions
    if(check_gcc_version($GCC_PATH, "4.4"))
    { # exception for libstdc++
        $SysInfo{"libstdc++"}{"gcc_options"} = ["-std=c++0x"];
    }
    if($OStarget eq "symbian")
    { # exception for libstdcpp
        $SysInfo{"libstdcpp"}{"defines"} = "namespace std { struct nothrow_t {}; }";
    }
    if($DumpSystem{"Name"}=~/maemo/i)
    { # GL/gl.h: No such file
        $SysInfo{"libSDL"}{"skip_headers"}=["SDL_opengl.h"];
    }
    if($OStarget eq "linux") {
        $SysInfo{"libboost_"}{"headers"} = ["/boost/", "/asio/"];
    }
    # Common Info
    if(not -f $SYS_INFO_PATH."/common.xml") {
        exitStatus("Module_Error", "can't access \'$SYS_INFO_PATH/common.xml\'");
    }
    my $SysCInfo = read_sys_descriptor($SYS_INFO_PATH."/common.xml");
    my @CompilerOpts = ();
    if($DumpSystem{"Name"}=~/maemo|meego/i) {
        push(@CompilerOpts, "-DMAEMO_CHANGES", "-DM_APPLICATION_NAME=\\\"app\\\"");
    }
    if(my @Opts = keys(%{$DumpSystem{"GccOpts"}})) {
        push(@CompilerOpts, @Opts);
    }
    if(@CompilerOpts)
    {
        if(not $SysCInfo->{"gcc_options"}) {
            $SysCInfo->{"gcc_options"} = [];
        }
        push(@{$SysCInfo->{"gcc_options"}}, @CompilerOpts);
    }
    foreach my $Name (keys(%SysInfo))
    { # strict headers that should be
      # matched for only one library
        if($SysInfo{$Name}{"headers"}) {
            $SysCInfo->{"sheaders"}{$Name} = $SysInfo{$Name}{"headers"};
        }
    }
    return (\%SysInfo, $SysCInfo);
}

sub optimize_set(@)
{
    my %Included = ();
    foreach my $Path (@_)
    {
        detect_header_includes($Path, 1);
        foreach my $Include (keys(%{$Header_Includes{1}{$Path}})) {
            $Included{get_filename($Include)}{$Include}=1;
        }
    }
    my @Res = ();
    foreach my $Path (@_)
    {
        my $Add = 1;
        foreach my $Inc (keys(%{$Included{get_filename($Path)}}))
        {
            if($Path=~/\/\Q$Inc\E\Z/)
            {
                $Add = 0;
                last;
            }
        }
        if($Add) {
            push(@Res, $Path);
        }
    }
    return @Res;
}

sub dumpSystem()
{ # -dump-system option handler
  # should be used with -sysroot and -cross-gcc options
    my $SYS_DUMP_PATH = "sys_dumps/".$DumpSystem{"Name"}."/".getArch(1);
    if(not $TargetLibraryName) {
        rmtree($SYS_DUMP_PATH);
    }
    my (@SystemLibs, @SysHeaders) = ();
    foreach my $Path (keys(%{$DumpSystem{"Libs"}}))
    {
        if(not -e $Path) {
            exitStatus("Access_Error", "can't access \'$Path\'");
        }
        if(-d $Path)
        {
            if(my @SubLibs = find_libs($Path,"",1)) {
                push(@SystemLibs, @SubLibs);
            }
            $DumpSystem{"SearchLibs"}{$Path}=1;
        }
        else
        { # single file
            push(@SystemLibs, $Path);
            $DumpSystem{"SearchLibs"}{get_dirname($Path)}=1;
        }
    }
    foreach my $Path (keys(%{$DumpSystem{"Headers"}}))
    {
        if(not -e $Path) {
            exitStatus("Access_Error", "can't access \'$Path\'");
        }
        if(-d $Path)
        {
            if(my @SubHeaders = cmd_find($Path,"f","","")) {
                push(@SysHeaders, @SubHeaders);
            }
            $DumpSystem{"SearchHeaders"}{$Path}=1;
        }
        else
        { # single file
            push(@SysHeaders, $Path);
            $DumpSystem{"SearchHeaders"}{get_dirname($Path)}=1;
        }
    }
    if($CheckHeadersOnly)
    { # -headers-only
        $GroupByHeaders = 1;
        # @SysHeaders = optimize_set(@SysHeaders);
    }
    elsif($DumpSystem{"Image"})
    { # one big image
        $GroupByHeaders = 1;
        @SystemLibs = ($DumpSystem{"Image"});
    }
    writeFile($SYS_DUMP_PATH."/target.txt", $OStarget);
    my (%SysLib_Symbols, %SymbolGroup, %Symbol_SysHeaders,
    %SysHeader_Symbols, %SysLib_SysHeaders, %MatchByName) = ();
    my (%Skipped, %Failed, %Success) = ();
    my (%SysHeaderDir_SysLibs, %SysHeaderDir_SysHeaders) = ();
    my (%LibPrefixes, %SymbolCounter, %TotalLibs) = ();
    my %Glibc = map {$_=>1} (
        "libc",
        "libpthread"
    );
    my ($SysInfo, $SysCInfo) = read_sys_info($OStarget);
    if(not $GroupByHeaders) {
        print "Indexing sonames ...\n";
    }
    my (%LibSoname, %SysLibVersion) = ();
    my %DevelPaths = map {$_=>1} @SystemLibs;
    foreach my $Path (sort keys(%{$DumpSystem{"SearchLibs"}})) {
        foreach my $LPath (find_libs($Path,"",1)) {
            $DevelPaths{$LPath}=1;
        }
    }
    foreach my $LPath (keys(%DevelPaths))
    { # register SONAMEs
        my $LName = get_filename($LPath);
        my $LRelPath = cut_path_prefix($LPath, $SystemRoot);
        if(not is_target_lib($LName)) {
            next;
        }
        if($OSgroup=~/\A(linux|macos|freebsd)\Z/
        and $LName!~/\Alib/) {
            next;
        }
        if(my $Soname = get_soname($LPath))
        {
            if($OStarget eq "symbian")
            {
                if($Soname=~/[\/\\]/)
                { # L://epoc32/release/armv5/lib/gfxtrans{000a0000}.dso
                    $Soname = get_filename($Soname);
                }
                $Soname = lc($Soname);
            }
            if(not defined $LibSoname{$LName}) {
                $LibSoname{$LName}=$Soname;
            }
            if(-l $LPath and my $Path = resolve_symlink($LPath))
            {
                my $Name = get_filename($Path);
                if(not defined $LibSoname{$Name}) {
                    $LibSoname{$Name}=$Soname;
                }
            }
        }
        else
        { # windows and others
            $LibSoname{$LName}=$LName;
        }
    }
    my $SONAMES = "";
    foreach (sort {lc($a) cmp lc($b)} keys(%LibSoname)) {
        $SONAMES .= $_.";".$LibSoname{$_}."\n";
    }
    if(not $GroupByHeaders) {
        writeFile($SYS_DUMP_PATH."/sonames.txt", $SONAMES);
    }
    foreach my $LPath (sort keys(%DevelPaths))
    {# register VERSIONs
        my $LName = get_filename($LPath);
        if(not is_target_lib($LName)
        and not is_target_lib($LibSoname{$LName})) {
            next;
        }
        if(my $V = get_binversion($LPath))
        { # binary version
            $SysLibVersion{$LName} = $V;
        }
        elsif(my $V = parse_libname($LName, "version"))
        { # source version
            $SysLibVersion{$LName} = $V;
        }
        elsif($LName=~/([\d\.\-\_]+)\.$LIB_EXT\Z/)
        { # libfreebl3.so
            if($1 ne 32 and $1 ne 64) {
                $SysLibVersion{$LName} = $1;
            }
        }
    }
    my $VERSIONS = "";
    foreach (sort {lc($a) cmp lc($b)} keys(%SysLibVersion)) {
        $VERSIONS .= $_.";".$SysLibVersion{$_}."\n";
    }
    if(not $GroupByHeaders) {
        writeFile($SYS_DUMP_PATH."/versions.txt", $VERSIONS);
    }
    my %SysLibs = ();
    foreach my $LPath (sort @SystemLibs)
    {
        my $LName = get_filename($LPath);
        my $LSName = parse_libname($LName, "short");
        my $LRelPath = cut_path_prefix($LPath, $SystemRoot);
        if(not is_target_lib($LName)) {
            next;
        }
        if($OSgroup=~/\A(linux|macos|freebsd)\Z/
        and $LName!~/\Alib/) {
            next;
        }
        if($OStarget eq "symbian")
        {
            if(my $V = parse_libname($LName, "version"))
            { # skip qtcore.dso
              # register qtcore{00040604}.dso
                delete($SysLibs{get_dirname($LPath)."\\".$LSName.".".$LIB_EXT});
                my $MV = parse_libname($LibSoname{$LSName.".".$LIB_EXT}, "version");
                if($MV and $V ne $MV)
                { # skip other versions:
                  #  qtcore{00040700}.dso
                  #  qtcore{00040702}.dso
                    next;
                }
            }
        }
        if(my $Skip = $SysCInfo->{"skip_libs"})
        { # do NOT check some libs
            if(check_list($LRelPath, $Skip)) {
                next;
            }
        }
        if(-l $LPath)
        { # symlinks
            if(my $Path = resolve_symlink($LPath)) {
                $SysLibs{$Path} = 1;
            }
        }
        elsif(-f $LPath)
        {
            if($Glibc{$LSName}
            and cmd_file($LPath)=~/ASCII/)
            {# GNU ld scripts (libc.so, libpthread.so)
                my @Candidates = cmd_find($SystemRoot."/lib","",$LSName.".".$LIB_EXT."*","1");
                if(@Candidates)
                {
                    my $Candidate = $Candidates[0];
                    if(-l $Candidate
                    and my $Path = resolve_symlink($Candidate)) {
                        $Candidate = $Path;
                    }
                    $SysLibs{$Candidate} = 1;
                }
            }
            else {
                $SysLibs{$LPath} = 1;
            }
        }
    }
    @SystemLibs = ();# clear memory
    if(not $CheckHeadersOnly)
    {
        if($DumpSystem{"Image"}) {
            print "Reading symbols from image ...\n";
        }
        else {
            print "Reading symbols from libraries ...\n";
        }
    }
    foreach my $LPath (sort keys(%SysLibs))
    {
        my $LRelPath = cut_path_prefix($LPath, $SystemRoot);
        my $LName = get_filename($LPath);
        getSymbols_Lib(1, $LPath, 0, (), "-Weak");
        my @AllSymbols = keys(%{$Library_Symbol{1}{$LName}});
        translateSymbols(@AllSymbols, 1);
        foreach my $Symbol (@AllSymbols)
        {
            $Symbol=~s/[\@\$]+(.*)\Z//g;
            if($Symbol=~/\A(_Z|\?)/)
            {
                if(skipGlobalData($Symbol)) {
                    next;
                }
                if($Symbol=~/(C1|C2|D0|D1|D2)E/)
                { # do NOT analyze constructors
                  # and destructors
                    next;
                }
                my $Unmangled = $tr_name{$Symbol};
                $Unmangled=~s/<.+>//g;
                if($Unmangled=~/\A([\w:]+)/)
                { # cut out the parameters
                    my @Elems = split(/::/, $1);
                    my ($Class, $Short) = ();
                    $Short = $Elems[$#Elems];
                    if($#Elems>=1) {
                        $Class = $Elems[$#Elems-1];
                        pop(@Elems);
                    }
                    # the short and class name should be
                    # matched in one header file
                    $SymbolGroup{$LRelPath}{$Class} = $Short;
                    foreach my $Sym (@Elems)
                    {
                        if($SysCInfo->{"ignore_symbols"}{$Symbol})
                        { # do NOT match this symbol
                            next;
                        }
                        $SysLib_Symbols{$LPath}{$Sym}=1;
                        if(my $Prefix = getPrefix($Sym)) {
                            $LibPrefixes{$Prefix}{$LName}+=1;
                        }
                        $SymbolCounter{$Sym}{$LName}=1;
                    }
                }
            }
            else
            {
                if($SysCInfo->{"ignore_symbols"}{$Symbol})
                { # do NOT match this symbol
                    next;
                }
                $SysLib_Symbols{$LPath}{$Symbol}=1;
                if(my $Prefix = getPrefix($Symbol)) {
                    $LibPrefixes{$Prefix}{$LName}+=1;
                }
                $SymbolCounter{$Symbol}{$LName}=1;
            }
        }
    }
    if(not $CheckHeadersOnly) {
        writeFile($SYS_DUMP_PATH."/symbols.txt", Dumper(\%SysLib_Symbols));
    }
    my %DupLibs = ();
    foreach my $LPath1 (keys(%SysLib_Symbols))
    { # match duplicated libs
      # libmenu contains all symbols from libmenuw
        foreach my $LPath2 (keys(%SysLib_Symbols))
        {
            next if($LPath1 eq $LPath2);
            my $Duplicate=1;
            foreach (keys(%{$SysLib_Symbols{$LPath1}}))
            {
                if(not $SysLib_Symbols{$LPath2}{$_}) {
                    $Duplicate=0;
                    last;
                }
            }
            if($Duplicate) {
                $DupLibs{$LPath1}{$LPath2}=1;
            }
        }
    }
    foreach my $Prefix (keys(%LibPrefixes))
    {
        my @Libs = keys(%{$LibPrefixes{$Prefix}});
        @Libs = sort {$LibPrefixes{$Prefix}{$b}<=>$LibPrefixes{$Prefix}{$a}} @Libs;
        $LibPrefixes{$Prefix}=$Libs[0];
    }
    print "Reading symbols from headers ...\n";
    foreach my $HPath (@SysHeaders)
    {
        $HPath = path_format($HPath, $OSgroup);
        my $HRelPath = cut_path_prefix($HPath, $SystemRoot);
        my ($HDir, $HName) = separate_path($HRelPath);
        if(is_not_header($HName))
        { # have a wrong extension: .gch, .in
            next;
        }
        if($HName=~/~\Z/)
        { # reserved copy
            next;
        }
        if($HRelPath=~/[\/\\]_gen/)
        { # telepathy-1.0/telepathy-glib/_gen
          # telepathy-1.0/libtelepathy/_gen-tp-constants-deprecated.h
            next;
        }
        if($HRelPath=~/include[\/\\]linux[\/\\]/)
        { # kernel-space headers
            next;
        }
        if($HRelPath=~/include[\/\\]asm[\/\\]/)
        { # asm headers
            next;
        }
        if($HRelPath=~/[\/\\]microb-engine[\/\\]/)
        { # MicroB engine (Maemo 4)
            next;
        }
        if($HRelPath=~/\Wprivate(\W|\Z)/)
        { # private directories (include/tcl-private, ...)
            next;
        }
        my $Content = readFile($HPath);
        $Content=~s/\/\*(.|\n)+?\*\///g;
        $Content=~s/\/\/.*?\n//g;# remove comments
        $Content=~s/#\s*define[^\n\\]*(\\\n[^\n\\]*)+\n*//g;# remove defines
        $Content=~s/#[^\n]*?\n//g;# remove directives
        $Content=~s/(\A|\n)class\s+\w+;\n//g;# remove forward declarations
        # FIXME: try to add preprocessing stage
        foreach my $Symbol (split(/\W+/, $Content))
        {
            $Symbol_SysHeaders{$Symbol}{$HRelPath} = 1;
            $SysHeader_Symbols{$HRelPath}{$Symbol} = 1;
        }
        $SysHeaderDir_SysHeaders{$HDir}{$HName} = 1;
        my $HShort = $HName;
        $HShort=~s/\.\w+\Z//g;
        if($HShort=~/\Alib/) {
            $MatchByName{$HShort} = $HRelPath;
        }
        elsif(get_filename(get_dirname($HRelPath))=~/\Alib(.+)\Z/)
        { # libical/ical.h
            if($HShort=~/\Q$1\E/) {
                $MatchByName{"lib".$HShort} = $HRelPath;
            }
        }
        elsif($OStarget eq "windows"
        and $HShort=~/api\Z/) {
            $MatchByName{$HShort} = $HRelPath;
        }
    }
    my %SkipDHeaders = (
    # header files, that should be in the <skip_headers> section
    # but should be matched in the algorithm
        # MeeGo 1.2 Harmattan
        "libtelepathy-qt4" => ["TelepathyQt4/_gen", "client.h",
                        "TelepathyQt4/*-*", "debug.h", "global.h",
                        "properties.h", "Channel", "channel.h", "message.h"],
    );
    filter_format(\%SkipDHeaders);
    if(not $GroupByHeaders) {
        print "Matching symbols ...\n";
    }
    foreach my $LPath (sort keys(%SysLibs))
    { # matching
        my $LName = get_filename($LPath);
        my $LNameSE = parse_libname($LName, "name+ext");
        my $LRelPath = cut_path_prefix($LPath, $SystemRoot);
        my $LSName = parse_libname($LName, "short");
        my $SName = parse_libname($LName, "shortest");
        if($LRelPath=~/\/debug\//)
        { # debug libs
            $Skipped{$LRelPath}=1;
            next;
        }
        $TotalLibs{$LRelPath}=1;
        $SysLib_SysHeaders{$LRelPath} = ();
        if(keys(%{$SysLib_Symbols{$LPath}}))
        { # try to match by name of the header
            if(my $Path = $MatchByName{$LSName}) {
                $SysLib_SysHeaders{$LRelPath}{$Path}="exact match ($LSName)";
            }
            if(length($SName)>=3
            and my $Path = $MatchByName{"lib".$SName}) {
                $SysLib_SysHeaders{$LRelPath}{$Path}="exact match (lib$SName)";
            }
        }
        my (%SymbolDirs, %SymbolFiles) = ();
        foreach my $Symbol (sort {length($b) cmp length($a)}
        sort keys(%{$SysLib_Symbols{$LPath}}))
        {
            if($SysCInfo->{"ignore_symbols"}{$Symbol}) {
                next;
            }
            if(not $DupLibs{$LPath}
            and keys(%{$SymbolCounter{$Symbol}})>=2
            and my $Prefix = getPrefix($Symbol))
            { # duplicated symbols
                if($LibPrefixes{$Prefix}
                and $LibPrefixes{$Prefix} ne $LName
                and not $Glibc{$LSName}) {
                    next;
                }
            }
            if(length($Symbol)<=2) {
                next;
            }
            if($Symbol!~/[A-Z_0-9]/
            and length($Symbol)<10
            and keys(%{$Symbol_SysHeaders{$Symbol}})>=3)
            { # undistinguished symbols
              # FIXME: improve this filter
              # signalfd (libc.so)
              # regcomp (libpcreposix.so)
                next;
            }
            if($Symbol=~/\A(_M_|_Rb_|_S_)/)
            { # _M_insert, _Rb_tree, _S_destroy_c_locale
                next;
            }
            if($Symbol=~/\A[A-Z][a-z]+\Z/)
            { # Clone, Initialize, Skip, Unlock, Terminate, Chunk
                next;
            }
            if($Symbol=~/\A[A-Z][A-Z]\Z/)
            { #  BC, PC, UP, SP
                next;
            }
            if($Symbol=~/_t\Z/)
            { # pthread_mutex_t, wchar_t
                next;
            }
            my @SymHeaders = keys(%{$Symbol_SysHeaders{$Symbol}});
            @SymHeaders = sort {lc($a) cmp lc($b)} @SymHeaders;# sort by name
            @SymHeaders = sort {length(get_dirname($a))<=>length(get_dirname($b))} @SymHeaders;# sort by length
            if(length($SName)>=3)
            { # sort candidate headers by name
                @SymHeaders = sort {$b=~/\Q$SName\E/i<=>$a=~/\Q$SName\E/i} @SymHeaders;
            }
            else
            { # libz, libX11
                @SymHeaders = sort {$b=~/lib\Q$SName\E/i<=>$a=~/lib\Q$SName\E/i} @SymHeaders;
                @SymHeaders = sort {$b=~/\Q$SName\Elib/i<=>$a=~/\Q$SName\Elib/i} @SymHeaders;
            }
            @SymHeaders = sort {$b=~/\Q$LSName\E/i<=>$a=~/\Q$LSName\E/i} @SymHeaders;
            @SymHeaders = sort {$SymbolDirs{get_dirname($b)}<=>$SymbolDirs{get_dirname($a)}} @SymHeaders;
            @SymHeaders = sort {$SymbolFiles{get_filename($b)}<=>$SymbolFiles{get_filename($a)}} @SymHeaders;
            foreach my $HRelPath (@SymHeaders)
            {
                if(my $Group = $SymbolGroup{$LRelPath}{$Symbol}) {
                    if(not $SysHeader_Symbols{$HRelPath}{$Group}) {
                        next;
                    }
                }
                if(my $Search = $SysInfo->{$LSName}{"headers"})
                { # search for specified headers
                    if(not check_list($HRelPath, $Search)) {
                        next;
                    }
                }
                if(my $Skip = $SysInfo->{$LSName}{"skip_headers"})
                { # do NOT search for some headers
                    if(check_list($HRelPath, $Skip)) {
                        next;
                    }
                }
                if(my $Skip = $SysCInfo->{"skip_headers"})
                { # do NOT search for some headers
                    if(check_list($HRelPath, $Skip)) {
                        next;
                    }
                }
                if(my $Skip = $SysInfo->{$LSName}{"non_self_compiled"})
                { # do NOT search for some headers
                    if(check_list($HRelPath, $Skip)) {
                        $SymbolDirs{get_dirname($HRelPath)}+=1;
                        $SymbolFiles{get_filename($HRelPath)}+=1;
                        next;
                    }
                }
                my $Continue = 1;
                foreach my $Name (keys(%{$SysCInfo->{"sheaders"}}))
                {
                    if($LSName!~/\Q$Name\E/
                    and check_list($HRelPath, $SysCInfo->{"sheaders"}{$Name}))
                    { # restriction to search for C++ or Boost headers
                      # in the boost/ and c++/ directories only
                        $Continue=0;
                        last;
                    }
                }
                if(not $Continue) {
                    next;
                }
                $SysLib_SysHeaders{$LRelPath}{$HRelPath}=$Symbol;
                $SysHeaderDir_SysLibs{get_dirname($HRelPath)}{$LNameSE}=1;
                $SysHeaderDir_SysLibs{get_dirname(get_dirname($HRelPath))}{$LNameSE}=1;
                $SymbolDirs{get_dirname($HRelPath)}+=1;
                $SymbolFiles{get_filename($HRelPath)}+=1;
                last;# select one header for one symbol
            }
        }
        if(keys(%{$SysLib_SysHeaders{$LRelPath}})) {
            $Success{$LRelPath}=1;
        }
        else {
            $Failed{$LRelPath}=1;
        }
    }
    if(not $GroupByHeaders)
    { # matching results
        writeFile($SYS_DUMP_PATH."/match.txt", Dumper(\%SysLib_SysHeaders));
        writeFile($SYS_DUMP_PATH."/skipped.txt", join("\n", sort keys(%Skipped)));
        writeFile($SYS_DUMP_PATH."/failed.txt", join("\n", sort keys(%Failed)));
    }
    (%SysLib_Symbols, %SymbolGroup, %Symbol_SysHeaders, %SysHeader_Symbols) = ();# free memory
    if($GroupByHeaders)
    {
        if($DumpSystem{"Image"} and not $CheckHeadersOnly) {
            @SysHeaders = keys(%{$SysLib_SysHeaders{$DumpSystem{"Image"}}});
        }
        %SysLib_SysHeaders = ();
        foreach my $Path (@SysHeaders)
        {
            if(my $Skip = $SysCInfo->{"skip_headers"})
            { # do NOT search for some headers
                if(check_list($Path, $Skip)) {
                    next;
                }
            }
            $SysLib_SysHeaders{$Path}{$Path} = 1;
        }
        if($CheckHeadersOnly) {
            writeFile($SYS_DUMP_PATH."/mode.txt", "headers-only");
        }
        else {
            writeFile($SYS_DUMP_PATH."/mode.txt", "group-by-headers");
        }
    }
    @SysHeaders = ();# clear memory
    my (%Skipped, %Failed, %Success) = ();
    print "Generating XML descriptors ...\n";
    foreach my $LRelPath (keys(%SysLib_SysHeaders))
    {
        my $LName = get_filename($LRelPath);
        my $DPath = $SYS_DUMP_PATH."/descriptors/$LName.xml";
        unlink($DPath);
        if(my @LibHeaders = keys(%{$SysLib_SysHeaders{$LRelPath}}))
        {
            my $LSName = parse_libname($LName, "short");
            if($GroupByHeaders)
            { # header short name
                $LSName = $LName;
                $LSName=~s/\.(.+?)\Z//;
            }
            my (%DirsHeaders, %Includes) = ();
            foreach my $HRelPath (@LibHeaders) {
                $DirsHeaders{get_dirname($HRelPath)}{$HRelPath}=1;
            }
            foreach my $Dir (keys(%DirsHeaders))
            {
                my $DirPart = 0;
                my $TotalHeaders = keys(%{$SysHeaderDir_SysHeaders{$Dir}});
                if($TotalHeaders) {
                    $DirPart = (keys(%{$DirsHeaders{$Dir}})*100)/$TotalHeaders;
                }
                my $Neighbourhoods = keys(%{$SysHeaderDir_SysLibs{$Dir}});
                if($Neighbourhoods==1)
                { # one lib in this directory
                    if(get_filename($Dir) ne "include"
                    and $DirPart>=5)
                    { # complete directory
                        $Includes{$Dir} = 1;
                    }
                    else
                    { # list of headers
                        @Includes{keys(%{$DirsHeaders{$Dir}})}=values(%{$DirsHeaders{$Dir}});
                    }
                }
                elsif((keys(%{$DirsHeaders{$Dir}})*100)/($#LibHeaders+1)>5)
                {# remove 5% divergence
                    if(get_filename($Dir) ne "include"
                    and $DirPart>=50)
                    { # complete directory
                        $Includes{$Dir} = 1;
                    }
                    else
                    { # list of headers
                        @Includes{keys(%{$DirsHeaders{$Dir}})}=values(%{$DirsHeaders{$Dir}});
                    }
                }
            }
            if($GroupByHeaders)
            { # one header in one ABI dump
                %Includes = ($LibHeaders[0] => 1);
            }
            my $LVersion = $SysLibVersion{$LName};
            if($LVersion)
            {# append by system name
                $LVersion .= "-".$DumpSystem{"Name"};
            }
            else {
                $LVersion = $DumpSystem{"Name"};
            }
            my @Content = ("<version>\n    $LVersion\n</version>");
            my @IncHeaders = sort {natural_sorting($a, $b)} keys(%Includes);
            sort_by_word(\@IncHeaders, parse_libname($LName, "shortest"));
            if(is_abs($IncHeaders[0])) {
                push(@Content, "<headers>\n    ".join("\n    ", @IncHeaders)."\n</headers>");
            }
            else {
                push(@Content, "<headers>\n    {RELPATH}/".join("\n    {RELPATH}/", @IncHeaders)."\n</headers>");
            }
            if($GroupByHeaders)
            {
                if($DumpSystem{"Image"}) {
                    push(@Content, "<libs>\n    ".$DumpSystem{"Image"}."\n</libs>");
                }
            }
            else
            {
                if(is_abs($LRelPath) or -f $LRelPath) {
                    push(@Content, "<libs>\n    $LRelPath\n</libs>");
                }
                else {
                    push(@Content, "<libs>\n    {RELPATH}/$LRelPath\n</libs>");
                }
            }
            if(my @SearchHeaders = keys(%{$DumpSystem{"SearchHeaders"}})) {
                push(@Content, "<search_headers>\n    ".join("\n    ", @SearchHeaders)."\n</search_headers>");
            }
            if(my @SearchLibs = keys(%{$DumpSystem{"SearchLibs"}})) {
                push(@Content, "<search_libs>\n    ".join("\n    ", @SearchLibs)."\n</search_libs>");
            }
            if(my @Tools = keys(%{$DumpSystem{"Tools"}})) {
                push(@Content, "<tools>\n    ".join("\n    ", @Tools)."\n</tools>");
            }
            my @Skip = ();
            if($SysInfo->{$LSName}{"skip_headers"}) {
                @Skip = (@Skip, @{$SysInfo->{$LSName}{"skip_headers"}});
            }
            if($SysCInfo->{"skip_headers"}) {
                @Skip = (@Skip, @{$SysCInfo->{"skip_headers"}});
            }
            if($SysInfo->{$LSName}{"non_self_compiled"}) {
                @Skip = (@Skip, @{$SysInfo->{$LSName}{"non_self_compiled"}});
            }
            if($SkipDHeaders{$LSName}) {
                @Skip = (@Skip, @{$SkipDHeaders{$LSName}});
            }
            if(@Skip) {
                push(@Content, "<skip_headers>\n    ".join("\n    ", @Skip)."\n</skip_headers>");
            }
            if($SysInfo->{$LSName}{"add_include_paths"}) {
                push(@Content, "<add_include_paths>\n    ".join("\n    ", @{$SysInfo->{$LSName}{"add_include_paths"}})."\n</add_include_paths>");
            }
            if($SysInfo->{$LSName}{"skip_types"}) {
                push(@Content, "<skip_types>\n    ".join("\n    ", @{$SysInfo->{$LSName}{"skip_types"}})."\n</skip_types>");
            }
            my @SkipSymb = ();
            if($SysInfo->{$LSName}{"skip_symbols"}) {
                push(@SkipSymb, @{$SysInfo->{$LSName}{"skip_symbols"}});
            }
            if($SysCInfo->{"skip_symbols"}) {
                push(@SkipSymb, @{$SysCInfo->{"skip_symbols"}});
            }
            if(@SkipSymb) {
                push(@Content, "<skip_symbols>\n    ".join("\n    ", @SkipSymb)."\n</skip_symbols>");
            }
            my @Preamble = ();
            if($SysCInfo->{"include_preamble"}) {
                push(@Preamble, @{$SysCInfo->{"include_preamble"}});
            }
            if($LSName=~/\AlibX\w+\Z/)
            { # add Xlib.h for libXt, libXaw, libXext and others
                push(@Preamble, "Xlib.h", "X11/Intrinsic.h");
            }
            if($SysInfo->{$LSName}{"include_preamble"}) {
                push(@Preamble, @{$SysInfo->{$LSName}{"include_preamble"}});
            }
            if(@Preamble) {
                push(@Content, "<include_preamble>\n    ".join("\n    ", @Preamble)."\n</include_preamble>");
            }
            my @Defines = ();
            if($SysCInfo->{"defines"}) {
                push(@Defines, $SysCInfo->{"defines"});
            }
            if($SysInfo->{$LSName}{"defines"}) {
                push(@Defines, $SysInfo->{$LSName}{"defines"});
            }
            if($DumpSystem{"Defines"}) {
                push(@Defines, $DumpSystem{"Defines"});
            }
            if(@Defines) {
                push(@Content, "<defines>\n    ".join("\n    ", @Defines)."\n</defines>");
            }
            my @CompilerOpts = ();
            if($SysCInfo->{"gcc_options"}) {
                push(@CompilerOpts, @{$SysCInfo->{"gcc_options"}});
            }
            if($SysInfo->{$LSName}{"gcc_options"}) {
                push(@CompilerOpts, @{$SysInfo->{$LSName}{"gcc_options"}});
            }
            if(@CompilerOpts) {
                push(@Content, "<gcc_options>\n    ".join("\n    ", @CompilerOpts)."\n</gcc_options>");
            }
            if($DumpSystem{"CrossPrefix"}) {
                push(@Content, "<cross_prefix>\n    ".$DumpSystem{"CrossPrefix"}."\n</cross_prefix>");
            }
            writeFile($DPath, join("\n\n", @Content));
            $Success{$LRelPath}=1;
        }
    }
    print "Dumping ABIs:\n";
    my %DumpSuccess = ();
    my @Descriptors = cmd_find($SYS_DUMP_PATH."/descriptors","f","*.xml","1");
    foreach my $DPath (sort {lc($a) cmp lc($b)} @Descriptors)
    {
        my $DName = get_filename($DPath);
        my $LName = "";
        if($DName=~/\A(.+).xml\Z/) {
            $LName = $1;
        }
        else {
            next;
        }
        if(not is_target_lib($LName)
        and not is_target_lib($LibSoname{$LName})) {
            next;
        }
        $DPath = cut_path_prefix($DPath, $ORIG_DIR);
        my $ACC_dump = "perl $0";
        if($GroupByHeaders)
        { # header name is going here
            $ACC_dump .= " -l $LName";
        }
        else {
            $ACC_dump .= " -l ".parse_libname($LName, "name");
        }
        $ACC_dump .= " -dump \"$DPath\"";
        if($SystemRoot)
        {
            $ACC_dump .= " -relpath \"$SystemRoot\"";
            $ACC_dump .= " -sysroot \"$SystemRoot\"";
        }
        my $DumpPath = "$SYS_DUMP_PATH/abi_dumps/$LName.abi.$AR_EXT";
        $ACC_dump .= " -dump-path \"$DumpPath\"";
        my $LogPath = "$SYS_DUMP_PATH/logs/$LName.txt";
        unlink($LogPath);
        $ACC_dump .= " -log-path \"$LogPath\"";
        if($CrossGcc) {
            $ACC_dump .= " -cross-gcc \"$CrossGcc\"";
        }
        if($CheckHeadersOnly) {
            $ACC_dump .= " -headers-only";
        }
        if($UseStaticLibs) {
            $ACC_dump .= " -static-libs";
        }
        if($GroupByHeaders) {
            $ACC_dump .= " -header $LName";
        }
        if($NoStdInc
        or $OStarget=~/windows|symbian/)
        { # 1. user-defined
          # 2. windows/minGW
          # 3. symbian/GCC
            $ACC_dump .= " -nostdinc";
        }
        if($Debug)
        { # debug mode
            $ACC_dump .= " -debug";
            print "$ACC_dump\n";
        }
        print "Dumping $LName: ";
        system($ACC_dump." 1>$TMP_DIR/null 2>$TMP_DIR/$LName.stderr");
        appendFile("$SYS_DUMP_PATH/logs/$LName.txt", "The ACC parameters:\n  $ACC_dump\n");
        if(-s "$TMP_DIR/$LName.stderr")
        {
            appendFile("$SYS_DUMP_PATH/logs/$LName.txt", readFile("$TMP_DIR/$LName.stderr"));
            if($CODE_ERROR{$?>>8} eq "Invalid_Dump") {
                print "Empty\n";
            }
            else {
                print "Failed (\'$SYS_DUMP_PATH/logs/$LName.txt\')\n";
            }
        }
        elsif(not -f $DumpPath) {
            print "Failed (\'$SYS_DUMP_PATH/logs/$LName.txt\')\n";
        }
        else
        {
            $DumpSuccess{$LName}=1;
            print "Ok\n";
        }
    }
    if(not $GroupByHeaders)
    { # general mode
        print "Total libraries:         ".keys(%TotalLibs)."\n";
        print "Skipped libraries:       ".keys(%Skipped)." ($SYS_DUMP_PATH/skipped.txt)\n";
        print "Failed to find headers:  ".keys(%Failed)." ($SYS_DUMP_PATH/failed.txt)\n";
    }
    print "Created descriptors:     ".keys(%Success)." ($SYS_DUMP_PATH/descriptors/)\n";
    print "Dumped ABIs:             ".keys(%DumpSuccess)." ($SYS_DUMP_PATH/abi_dumps/)\n";
    print "The ".$DumpSystem{"Name"}." system ABI has been dumped to:\n  $SYS_DUMP_PATH\n";
}

sub is_abs($) {
    return ($_[0]=~/\A(\/|\w+:[\/\\])/);
}

sub get_binversion($)
{
    my $Path = $_[0];
    if($OStarget eq "windows"
    and $LIB_EXT eq "dll")
    { # get version of DLL using "sigcheck"
        my $SigcheckCmd = get_CmdPath("sigcheck");
        if(not $SigcheckCmd) {
            return "";
        }
        my $VInfo = `$SigcheckCmd -n $Path 2>$TMP_DIR/null`;
        $VInfo=~s/\s*\(.*\)\s*//;
        chomp($VInfo);
        return $VInfo;
    }
    return "";
}

sub get_soname($)
{
    my $Path = $_[0];
    return if(not $Path or not -e $Path);
    if(defined $Cache{"get_soname"}{$Path}) {
        return $Cache{"get_soname"}{$Path};
    }
    my $ObjdumpCmd = get_CmdPath("objdump");
    if(not $ObjdumpCmd) {
        exitStatus("Not_Found", "can't find \"objdump\"");
    }
    my $SonameCmd = "$ObjdumpCmd -x $Path 2>$TMP_DIR/null";
    if($OSgroup eq "windows") {
        $SonameCmd .= " | find \"SONAME\"";
    }
    else {
        $SonameCmd .= " | grep SONAME";
    }
    if(my $SonameInfo = `$SonameCmd`) {
        if($SonameInfo=~/SONAME\s+([^\s]+)/) {
            return ($Cache{"get_soname"}{$Path} = $1);
        }
    }
    return ($Cache{"get_soname"}{$Path}="");
}

sub checkVersionNum($$)
{
    my ($LibVersion, $Path) = @_;
    if(my $VerNum = $TargetVersion{$LibVersion}) {
        return $VerNum;
    }
    my $UsedAltDescr = 0;
    foreach my $Part (split(/\s*,\s*/, $Path))
    {# try to get version string from file path
        next if($Part=~/\.xml\Z/i);
        next if($Part=~/\.abi(\Q.tar.gz\E|\Q.zip\E)\Z/);
        if(parse_libname($Part, "version")
        or is_header($Part, 1, $LibVersion) or -d $Part) {
            $UsedAltDescr = 1;
            if(my $VerNum = readStringVersion($Part)) {
                $TargetVersion{$LibVersion} = $VerNum;
                if($DumpAPI) {
                    print "WARNING: setting version number to $VerNum (use -vnum <num> option to change it)\n";
                }
                else {
                    print "WARNING: setting ".($LibVersion==1?"1st":"2nd")." version number to \"$VerNum\" (use -v$LibVersion <num> option to change it)\n";
                }
                return $TargetVersion{$LibVersion};
            }
        }
    }
    if($UsedAltDescr)
    {
        if($DumpAPI) {
            exitStatus("Error", "version number is not set (use -vnum <num> option)");
        }
        else {
            exitStatus("Error", ($LibVersion==1?"1st":"2nd")." version number is not set (use -v$LibVersion <num> option)");
        }
    }
}

sub readStringVersion($)
{
    my $Str = $_[0];
    return "" if(not $Str);
    $Str=~s/\Q$TargetLibraryName\E//g;
    if($Str=~/(\/|\\|\w|\A)[\-\_]*(\d+[\d\.\-]+\d+|\d+)/)
    {# .../libssh-0.4.0/...
        return $2;
    }
    elsif(my $V = parse_libname($Str, "version")) {
        return $V;
    }
    return "";
}

sub scan_libs_and_headers($)
{
    my $LibVersion = $_[0];
    if(not $CheckHeadersOnly)
    {
        if($OStarget eq "windows")
        { # dumpbin.exe will crash
          # without VS Environment
            check_win32_env();
        }
        getSymbols($LibVersion);
    }
    if(not $CheckObjectsOnly) {
        searchForHeaders($LibVersion);
    }
}

sub get_OSgroup()
{
    if($Config{"osname"}=~/macos|darwin|rhapsody/i) {
        return "macos";
    }
    elsif($Config{"osname"}=~/freebsd|openbsd|netbsd/i) {
        return "bsd";
    }
    elsif($Config{"osname"}=~/haiku|beos/i) {
        return "beos";
    }
    elsif($Config{"osname"}=~/symbian|epoc/i) {
        return "symbian";
    }
    elsif($Config{"osname"}=~/win/i) {
        return "windows";
    }
    else {
        return $Config{"osname"};
    }
}

sub dump_sorting($)
{
    my $hash = $_[0];
    return [] if(not $hash or not keys(%{$hash}));
    if((keys(%{$hash}))[0]=~/\A\d+\Z/) {
        return [sort {int($a) <=> int($b)} keys(%{$hash})];
    }
    else {
        return [sort {$a cmp $b} keys(%{$hash})];
    }
}

sub exitStatus($$)
{
    my ($Code, $Msg) = @_;
    print STDERR "ERROR: ". $Msg."\n";
    exit($ERROR_CODE{$Code});
}

sub exitReport()
{# the tool has run without any errors
    printReport();
    if($COMPILE_ERRORS)
    {# errors in headers may add false positives/negatives
        exit($ERROR_CODE{"Compile_Error"});
    }
    if($CHECKER_VERDICT) {
        exit($ERROR_CODE{"Incompatible"});
    }
    else {
        exit($ERROR_CODE{"Compatible"});
    }
}

sub readRules($)
{
    my $Kind = $_[0];
    if(not -f $RULES_PATH{$Kind}) {
        exitStatus("Module_Error", "can't access \'".$RULES_PATH{$Kind}."\'");
    }
    my $Content = readFile($RULES_PATH{$Kind});
    while(my $Rule = parseTag(\$Content, "rule"))
    {
        my $RId = parseTag(\$Rule, "id");
        my @Properties = ("Severity", "Change", "Effect", "Overcome", "Kind");
        foreach my $Prop (@Properties) {
            if(my $Value = parseTag(\$Rule, lc($Prop))) {
                $CompatRules{$Kind}{$RId}{$Prop} = $Value;
            }
        }
        if($CompatRules{$Kind}{$RId}{"Kind"}=~/\A(Symbols|Parameters)\Z/) {
            $CompatRules{$Kind}{$RId}{"Kind"} = "Symbols";
        }
        else {
            $CompatRules{$Kind}{$RId}{"Kind"} = "Types";
        }
    }
}

sub printReport()
{
    print "creating compatibility report ...\n";
    createHtmlReport();
    if($CHECKER_VERDICT) {
        print "result: INCOMPATIBLE (total problems: $CHECKER_VERDICT, warnings: $CHECKER_WARNING)\n";
    }
    else {
        print "result: COMPATIBLE (total problems: $CHECKER_VERDICT, warnings: $CHECKER_WARNING)\n";
    }
    print "see detailed report:\n  $REPORT_PATH\n";
}

sub check_win32_env()
{
    if(not $ENV{"DevEnvDir"}
    or not $ENV{"LIB"}) {
        exitStatus("Error", "can't start without VS environment (vsvars32.bat)");
    }
}

sub createDump()
{
    if(not -e $DumpAPI) {
        exitStatus("Access_Error", "can't access \'$DumpAPI\'");
    }
    # check the archive utilities
    if($OSgroup eq "windows")
    {# using zip
        my $ZipCmd = get_CmdPath("zip");
        if(not $ZipCmd) {
            exitStatus("Not_Found", "can't find \"zip\"");
        }
    }
    else
    { # using tar and gzip
        my $TarCmd = get_CmdPath("tar");
        if(not $TarCmd) {
            exitStatus("Not_Found", "can't find \"tar\"");
        }
        my $GzipCmd = get_CmdPath("gzip");
        if(not $GzipCmd) {
            exitStatus("Not_Found", "can't find \"gzip\"");
        }
    }
    my @DParts = split(/\s*,\s*/, $DumpAPI);
    foreach my $Part (@DParts)
    {
        if(not -e $Part) {
            exitStatus("Access_Error", "can't access \'$Part\'");
        }
    }
    checkVersionNum(1, $DumpAPI);
    foreach my $Part (@DParts) {
        createDescriptor(1, $Part);
    }
    detect_default_paths("inc|lib|bin|gcc"); # complete analysis
    my $DumpPath = "abi_dumps/$TargetLibraryName/".$TargetLibraryName."_".$Descriptor{1}{"Version"}.".abi.".$AR_EXT;
    if($OutputDumpPath)
    { # user defined path
        $DumpPath = $OutputDumpPath;
    }
    if(not $DumpPath=~s/\Q.$AR_EXT\E\Z//g) {
        exitStatus("Error", "the dump path (-dump-path option) should be the path to a *.$AR_EXT file");
    }
    scan_libs_and_headers(1);
    $WORD_SIZE{1} = detectWordSize();
    translateSymbols(keys(%{$Symbol_Library{1}}), 1);
    translateSymbols(keys(%{$DepSymbols{1}}), 1);
    if($Descriptor{1}{"Headers"}
    and not $Descriptor{1}{"Dump"}) {
        readHeaders(1);
    }
    formatDump(1);
    if(not keys(%{$SymbolInfo{1}}) and not $CheckObjectsOnly)
    {
        if($CheckHeadersOnly) {
            exitStatus("Invalid_Dump", "the symbol set is empty");
        }
        else {
            exitStatus("Invalid_Dump", "the symbol sets from headers and libraries have empty intersection");
        }
    }
    my %HeadersInfo = ();
    foreach my $HPath (keys(%{$Target_Headers{1}}))
    { # headers info stored without paths in the dump
        $HeadersInfo{$Target_Headers{1}{$HPath}{"Identity"}} = $Target_Headers{1}{$HPath}{"Position"};
    }
    print "creating library ABI dump ...\n";
    my %LibraryABI = (
        "TypeInfo" => $TypeInfo{1},
        "SymbolInfo" => $SymbolInfo{1},
        "Symbols" => $Library_Symbol{1},
        "DepSymbols" => $DepSymbols{1},
        "SymbolVersion" => $SymVer{1},
        "LibraryVersion" => $Descriptor{1}{"Version"},
        "LibraryName" => $TargetLibraryName,
        "Tid_TDid" => $Tid_TDid{1},
        "SkipTypes" => $SkipTypes{1},
        "SkipSymbols" => $SkipSymbols{1},
        "Headers" => \%HeadersInfo,
        "Constants" => $Constants{1},
        "NameSpaces" => $NestedNameSpaces{1},
        "Target" => $OStarget,
        "Arch" => getArch(1),
        "WordSize" => $WORD_SIZE{1},
        "GccVersion" => get_dumpversion($GCC_PATH),
        "ABI_DUMP_VERSION" => $ABI_DUMP_VERSION,
        "ABI_COMPLIANCE_CHECKER_VERSION" => $ABI_COMPLIANCE_CHECKER_VERSION,
    );
    
    writeFile($DumpPath, Dumper(\%LibraryABI));
    if(not -s $DumpPath) {
        unlink($DumpPath);
        exitStatus("Error", "can't create ABI dump because something is going wrong with the Data::Dumper module");
    }
    my $Package = createArchive($DumpPath);
    print "library ABI has been dumped to:\n  ./$Package\n";
    print "you can transfer this dump everywhere and use instead of the ".$Descriptor{1}{"Version"}." version descriptor\n";
}

sub quickEmptyReports()
{ # Quick "empty" reports
  # 4 times faster than merging equal dumps
  # NOTE: the dump contains the "LibraryVersion" attribute
  # if you change the version, then your dump will be different
  # OVERCOME: use -v1 and v2 options for comparing dumps
  # and don't change version in the XML descriptor (and dumps)
  # OVERCOME 2: separate meta info from the dumps in ACC 2.0
    if(-s $Descriptor{1}{"Path"} == -s $Descriptor{2}{"Path"})
    {
        my $FilePath1 = unpackDump($Descriptor{1}{"Path"});
        my $FilePath2 = unpackDump($Descriptor{2}{"Path"});
        if($FilePath1 and $FilePath2)
        {
            my $Content = readFile($FilePath1);
            if($Content eq readFile($FilePath2))
            {
                # read a number of headers, libs, symbols and types
                my $ABIdump = eval($Content);
                if(not $ABIdump) {
                    exitStatus("Error", "internal error");
                }
                if(not $ABIdump->{"TypeInfo"})
                {# support for old dumps
                    $ABIdump->{"TypeInfo"} = $ABIdump->{"TypeDescr"};
                }
                if(not $ABIdump->{"SymbolInfo"})
                {# support for old dumps
                    $ABIdump->{"SymbolInfo"} = $ABIdump->{"FuncDescr"};
                }
                read_Headers_DumpInfo($ABIdump, 1);
                read_Libs_DumpInfo($ABIdump, 1);
                read_Machine_DumpInfo($ABIdump, 1);
                read_Machine_DumpInfo($ABIdump, 2);
                %CheckedTypes = %{$ABIdump->{"TypeInfo"}};
                %CheckedSymbols = %{$ABIdump->{"SymbolInfo"}};
                $Descriptor{1}{"Version"} = $TargetVersion{1}?$TargetVersion{1}:$ABIdump->{"LibraryVersion"};
                $Descriptor{2}{"Version"} = $TargetVersion{2}?$TargetVersion{2}:$ABIdump->{"LibraryVersion"};
                if(not $OutputReportPath) {
                    $REPORT_PATH = "compat_reports/$TargetLibraryName/".$Descriptor{1}{"Version"}."_to_".$Descriptor{2}{"Version"}."/abi_compat_report.html";
                }
                exitReport();
            }
        }
    }
}

sub compareAPIs()
{
    # read input XML descriptors or ABI dumps
    if(not $Descriptor{1}{"Path"}) {
        exitStatus("Error", "-d1 option is not specified");
    }
    my @DParts1 = split(/\s*,\s*/, $Descriptor{1}{"Path"});
    foreach my $Part (@DParts1)
    {
        if(not -e $Part) {
            exitStatus("Access_Error", "can't access \'$Part\'");
        }
    }
    if(not $Descriptor{2}{"Path"}) {
        exitStatus("Error", "-d2 option is not specified");
    }
    my @DParts2 = split(/\s*,\s*/, $Descriptor{2}{"Path"});
    foreach my $Part (@DParts2)
    {
        if(not -e $Part) {
            exitStatus("Access_Error", "can't access \'$Part\'");
        }
    }
    detect_default_paths("bin"); # to extract dumps
    if($#DParts1==0 and $#DParts2==0
    and $Descriptor{1}{"Path"}=~/\.abi(\Q.tar.gz\E|\Q.zip\E)\Z/
    and $Descriptor{2}{"Path"}=~/\.abi(\Q.tar.gz\E|\Q.zip\E)\Z/)
    {# optimization: equal ABI dumps
        quickEmptyReports();
    }
    checkVersionNum(1, $Descriptor{1}{"Path"});
    checkVersionNum(2, $Descriptor{2}{"Path"});
    print "preparation, please wait ...\n";
    foreach my $Part (@DParts1) {
        createDescriptor(1, $Part);
    }
    foreach my $Part (@DParts2) {
        createDescriptor(2, $Part);
    }
    # check consistency
    if(not $Descriptor{1}{"Headers"}
    and not $Descriptor{1}{"Libs"}) {
        exitStatus("Error", "descriptor d1 does not contain both header files and libraries info");
    }
    if(not $Descriptor{2}{"Headers"}
    and not $Descriptor{2}{"Libs"}) {
        exitStatus("Error", "descriptor d2 does not contain both header files and libraries info");
    }
    if($Descriptor{1}{"Headers"} and not $Descriptor{1}{"Libs"}
    and not $Descriptor{2}{"Headers"} and $Descriptor{2}{"Libs"}) {
        exitStatus("Error", "can't compare headers with $SLIB_TYPE libraries");
    }
    elsif(not $Descriptor{1}{"Headers"} and $Descriptor{1}{"Libs"}
    and $Descriptor{2}{"Headers"} and not $Descriptor{2}{"Libs"}) {
        exitStatus("Error", "can't compare $SLIB_TYPE libraries with headers");
    }
    if(not $Descriptor{1}{"Headers"}) {
        if($CheckHeadersOnly_Opt) {
            exitStatus("Error", "can't find header files info in descriptor d1");
        }
    }
    if(not $Descriptor{2}{"Headers"}) {
        if($CheckHeadersOnly_Opt) {
            exitStatus("Error", "can't find header files info in descriptor d2");
        }
    }
    if(not $Descriptor{1}{"Headers"}
    or not $Descriptor{2}{"Headers"}) {
        if(not $CheckObjectsOnly_Opt) {
            print "WARNING: comparing $SLIB_TYPE libraries only\n";
            $CheckObjectsOnly = 1;
        }
    }
    if(not $Descriptor{1}{"Libs"}) {
        if($CheckObjectsOnly_Opt) {
            exitStatus("Error", "can't find $SLIB_TYPE libraries info in descriptor d1");
        }
    }
    if(not $Descriptor{2}{"Libs"}) {
        if($CheckObjectsOnly_Opt) {
            exitStatus("Error", "can't find $SLIB_TYPE libraries info in descriptor d2");
        }
    }
    if(not $Descriptor{1}{"Libs"}
    or not $Descriptor{2}{"Libs"})
    {
        if(not $CheckHeadersOnly_Opt) {
            print "WARNING: comparing headers only\n";
            $CheckHeadersOnly = 1;
        }
    }
    if($UseDumps)
    { # --use-dumps
      # parallel processing
        my $pid = fork();
        if($pid)
        {# dump on two CPU cores
            my @PARAMS = ("-dump", $Descriptor{1}{"Path"}, "-l", $TargetLibraryName);
            if($RelativeDirectory{1}) {
                @PARAMS = (@PARAMS, "-relpath", $RelativeDirectory{1});
            }
            if($OutputLogPath{1}) {
                @PARAMS = (@PARAMS, "-log-path", $OutputLogPath{1});
            }
            if($CrossGcc) {
                @PARAMS = (@PARAMS, "-cross-gcc", $CrossGcc);
            }
            if($Debug) {
                @PARAMS = (@PARAMS, "-debug");
            }
            system("perl", "$0", @PARAMS);
            if($?) {
                exit(1);
            }
        }
        else
        {# child
            my @PARAMS = ("-dump", $Descriptor{2}{"Path"}, "-l", $TargetLibraryName);
            if($RelativeDirectory{2}) {
                @PARAMS = (@PARAMS, "-relpath", $RelativeDirectory{2});
            }
            if($OutputLogPath{2}) {
                @PARAMS = (@PARAMS, "-log-path", $OutputLogPath{2});
            }
            if($CrossGcc) {
                @PARAMS = (@PARAMS, "-cross-gcc", $CrossGcc);
            }
            if($Debug) {
                @PARAMS = (@PARAMS, "-debug");
            }
            system("perl", "$0", @PARAMS);
            if($?) {
                exit(1);
            }
            else {
                exit(0);
            }
        }
        waitpid($pid, 0);
        my @CMP_PARAMS = ("-l", $TargetLibraryName);
        @CMP_PARAMS = (@CMP_PARAMS, "-d1", "abi_dumps/$TargetLibraryName/".$TargetLibraryName."_".$Descriptor{1}{"Version"}.".abi.$AR_EXT");
        @CMP_PARAMS = (@CMP_PARAMS, "-d2", "abi_dumps/$TargetLibraryName/".$TargetLibraryName."_".$Descriptor{2}{"Version"}.".abi.$AR_EXT");
        if($TargetLibraryFName ne $TargetLibraryName) {
            @CMP_PARAMS = (@CMP_PARAMS, "-l-full", $TargetLibraryFName);
        }
        if($ShowRetVal) {
            @CMP_PARAMS = (@CMP_PARAMS, "-show-retval");
        }
        if($CrossGcc) {
            @CMP_PARAMS = (@CMP_PARAMS, "-cross-gcc", $CrossGcc);
        }
        system("perl", "$0", @CMP_PARAMS);
        exit($?>>8);
    }
    if(not $Descriptor{1}{"Dump"}
    or not $Descriptor{2}{"Dump"})
    { # need GCC toolchain to analyze
      # header files and libraries
        detect_default_paths("inc|lib|gcc");
    }
    if(not $Descriptor{1}{"Dump"})
    {
        scan_libs_and_headers(1);
        $WORD_SIZE{1} = detectWordSize();
    }
    if(not $Descriptor{2}{"Dump"})
    {
        scan_libs_and_headers(2);
        $WORD_SIZE{2} = detectWordSize();
    }
    if($AppPath and not keys(%{$Symbol_Library{1}})) {
        print "WARNING: the application ".get_filename($AppPath)." has no symbols imported from the $SLIB_TYPE libraries\n";
    }
    if(not $OutputReportPath) {
        $REPORT_PATH = "compat_reports/$TargetLibraryName/".$Descriptor{1}{"Version"}."_to_".$Descriptor{2}{"Version"}."/abi_compat_report.html";
    }
    mkpath(get_dirname($REPORT_PATH));
    unlink($REPORT_PATH);
    # started to process input data
    readRules("Binary");
    readRules("Source");
    translateSymbols(keys(%{$Symbol_Library{1}}), 1);
    translateSymbols(keys(%{$DepSymbols{1}}), 1);
    translateSymbols(keys(%{$Symbol_Library{2}}), 2);
    translateSymbols(keys(%{$DepSymbols{2}}), 2);
    if(not $CheckHeadersOnly)
    {# added/removed in libs
        detectAdded();
        detectRemoved();
    }
    if(not $CheckObjectsOnly)
    {
        if($Descriptor{1}{"Headers"}
        and not $Descriptor{1}{"Dump"}) {
            readHeaders(1);
        }
        if($Descriptor{2}{"Headers"}
        and not $Descriptor{2}{"Dump"}) {
            readHeaders(2);
        }
        print "comparing headers ...\n";
        prepareInterfaces(1);
        prepareInterfaces(2);
        %SymbolInfo=();
        if($CheckHeadersOnly)
        { # added/removed in headers
            detectAdded_H();
            detectRemoved_H();
        }
        mergeSignatures();
        if(keys(%CheckedSymbols)) {
            mergeConstants();
        }
    }
    if($CheckHeadersOnly)
    { # added/removed in headers
        mergeHeaders();
    }
    else
    {
        print "comparing libraries ...\n";
        mergeLibs();
        if($CheckImplementation) {
            mergeImplementations();
        }
    }
}

sub readSystemDescriptor($)
{
    my $Content = $_[0];
    $Content=~s/\/\*(.|\n)+?\*\///g;
    $Content=~s/<\!--(.|\n)+?-->//g;
    $DumpSystem{"Name"} = parseTag(\$Content, "name");
    if(not $DumpSystem{"Name"}) {
        exitStatus("Error", "system name is not specified (<name> section)");
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "libs")))
    { # target libs
        if(not -e $Path) {
            exitStatus("Access_Error", "can't access \'$Path\'");
        }
        $Path = get_abs_path($Path);
        $DumpSystem{"Libs"}{clean_path($Path)} = 1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "search_libs")))
    { # target libs
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = get_abs_path($Path);
        $DumpSystem{"SearchLibs"}{clean_path($Path)} = 1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "headers")))
    {
        if(not -e $Path) {
            exitStatus("Access_Error", "can't access \'$Path\'");
        }
        $Path = get_abs_path($Path);
        $DumpSystem{"Headers"}{clean_path($Path)} = 1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "search_headers")))
    {
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = get_abs_path($Path);
        $DumpSystem{"SearchHeaders"}{clean_path($Path)} = 1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "tools")))
    {
        if(not -d $Path) {
            exitStatus("Access_Error", "can't access directory \'$Path\'");
        }
        $Path = get_abs_path($Path);
        $Path = clean_path($Path);
        $DumpSystem{"Tools"}{$Path} = 1;
        $SystemPaths{"bin"}{$Path} = 1;
        $TargetTools{$Path}=1;
    }
    foreach my $Path (split(/\s*\n\s*/, parseTag(\$Content, "gcc_options"))) {
        $DumpSystem{"GccOpts"}{clean_path($Path)} = 1;
    }
    if($DumpSystem{"CrossPrefix"} = parseTag(\$Content, "cross_prefix"))
    { # <cross_prefix> section of XML descriptor
        $CrossPrefix = $DumpSystem{"CrossPrefix"};
    }
    elsif($CrossPrefix)
    { # -cross-prefix tool option
        $DumpSystem{"CrossPrefix"} = $CrossPrefix;
    }
    $DumpSystem{"Defines"} = parseTag(\$Content, "defines");
    if($DumpSystem{"Image"} = parseTag(\$Content, "image"))
    { # <image>
      # FIXME: isn't implemented yet
        if(not -f $DumpSystem{"Image"}) {
            exitStatus("Access_Error", "can't access \'".$DumpSystem{"Image"}."\'");
        }
    }
}

sub scenario()
{
    if($Help) {
        HELP_MESSAGE();
        exit(0);
    }
    if($InfoMsg) {
        INFO_MESSAGE();
        exit(0);
    }
    if($ShowVersion) {
        print "ABI Compliance Checker (ACC) $ABI_COMPLIANCE_CHECKER_VERSION\nCopyright (C) 2011 Institute for System Programming, RAS\nLicense: LGPLv2 or GPLv2 <http://www.gnu.org/licenses/>\nThis program is free software: you can redistribute it and/or modify it.\n\nWritten by Andrey Ponomarenko.\n";
        exit(0);
    }
    if($DumpVersion) {
        print "$ABI_COMPLIANCE_CHECKER_VERSION\n";
        exit(0);
    }
    if($SystemRoot_Opt)
    {# user defined root
        if(not -e $SystemRoot_Opt) {
            exitStatus("Access_Error", "can't access \'$SystemRoot\'");
        }
        $SystemRoot = $SystemRoot_Opt;
        $SystemRoot=~s/[\/]+\Z//g;
        if($SystemRoot) {
            $SystemRoot = get_abs_path($SystemRoot);
        }
    }
    $Data::Dumper::Sortkeys = 1;
    # FIXME: can't pass \&dump_sorting - cause a segfault sometimes
    # $Data::Dumper::Sortkeys = \&dump_sorting;
    if($TargetLibsPath)
    {
        if(not -f $TargetLibsPath) {
            exitStatus("Access_Error", "can't access file \'$TargetLibsPath\'");
        }
        foreach my $Lib (split(/\s*\n\s*/, readFile($TargetLibsPath))) {
            $TargetLibs{$Lib} = 1;
        }
    }
    if($TestTool
    or $TestDump)
    {# --test, --test-dump
        detect_default_paths("bin|gcc"); # to compile libs
        testSystemC();
        testSystemCpp();
        exit(0);
    }
    if($DumpSystem{"Name"})
    {# --dump-system
        if($DumpSystem{"Name"}=~/\.xml\Z/)
        { # system XML descriptor
            if(not -f $DumpSystem{"Name"}) {
                exitStatus("Access_Error", "can't access file '".$DumpSystem{"Name"}."'");
            }
            readSystemDescriptor(readFile($DumpSystem{"Name"}));
        }
        elsif($SystemRoot_Opt)
        { # -sysroot "/" option
          # default target: /usr/lib, /usr/include
          # search libs: /usr/lib and /lib
            if(not -e $SystemRoot."/usr/lib") {
                exitStatus("Access_Error", "can't access '".$SystemRoot."/usr/lib'");
            }
            if(not -e $SystemRoot."/lib") {
                exitStatus("Access_Error", "can't access '".$SystemRoot."/lib'");
            }
            if(not -e $SystemRoot."/usr/include") {
                exitStatus("Access_Error", "can't access '".$SystemRoot."/usr/include'");
            }
            readSystemDescriptor("
                <name>
                    ".$DumpSystem{"Name"}."
                </name>
                <headers>
                    $SystemRoot/usr/include
                </headers>
                <libs>
                    $SystemRoot/usr/lib
                </libs>
                <search_libs>
                    $SystemRoot/lib
                </search_libs>");
        }
        else {
            exitStatus("Error", "-sysroot <dirpath> option should be specified, usually it's \"/\"");
        }
        detect_default_paths("bin|gcc"); # to check symbols
        if($OStarget eq "windows")
        { # to run dumpbin.exe
          # and undname.exe
            check_win32_env();
        }
        dumpSystem();
        exit(0);
    }
    if($CmpSystems)
    {# --cmp-systems
        detect_default_paths("bin"); # to extract dumps
        cmpSystems();
        exit(0);
    }
    if($GenDescriptor) {
        genDescriptorTemplate();
        exit(0);
    }
    if(not $TargetLibraryName) {
        exitStatus("Error", "library name is not selected (option -l <name>)");
    }
    else
    { # validate library name
        if($TargetLibraryName=~/[\*\/\\]/) {
            exitStatus("Error", "\"\\\", \"\/\" and \"*\" symbols are not allowed in the library name");
        }
    }
    if(not $TargetLibraryFName) {
        $TargetLibraryFName = $TargetLibraryName;
    }
    if($CheckHeadersOnly_Opt and $CheckObjectsOnly_Opt) {
        exitStatus("Error", "you can't specify both -headers-only and -objects-only options at the same time");
    }
    if($SymbolsListPath)
    {
        if(not -f $SymbolsListPath) {
            exitStatus("Access_Error", "can't access file \'$SymbolsListPath\'");
        }
        foreach my $Interface (split(/\s*\n\s*/, readFile($SymbolsListPath))) {
            $SymbolsList{$Interface} = 1;
        }
    }
    if($ParamNamesPath)
    {
        if(not -f $ParamNamesPath) {
            exitStatus("Access_Error", "can't access file \'$ParamNamesPath\'");
        }
        foreach my $Line (split(/\n/, readFile($ParamNamesPath)))
        {
            if($Line=~s/\A(\w+)\;//) {
                my $Interface = $1;
                if($Line=~/;(\d+);/) {
                    while($Line=~s/(\d+);(\w+)//) {
                        $AddIntParams{$Interface}{$1}=$2;
                    }
                }
                else {
                    my $Num = 0;
                    foreach my $Name (split(/;/, $Line)) {
                        $AddIntParams{$Interface}{$Num++}=$Name;
                    }
                }
            }
        }
    }
    if($AppPath)
    {
        if(not -f $AppPath) {
            exitStatus("Access_Error", "can't access file \'$AppPath\'");
        }
        foreach my $Interface (getSymbols_App($AppPath)) {
            $SymbolsList_App{$Interface} = 1;
        }
    }
    if($DumpAPI)
    { # --dump-abi
      # make an API dump
        createDump();
        exit($COMPILE_ERRORS);
    }
    # default: compare APIs
    #  -d1 <path(s)>
    #  -d2 <path(s)>
    compareAPIs();
    exitReport();
}

scenario();
