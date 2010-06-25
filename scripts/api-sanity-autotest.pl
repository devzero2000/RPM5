#!/usr/bin/perl
########################################################################
# API Sanity Autotest 1.10, unit test generator for C/C++ library API
# Supported platforms: Linux and Unix (FreeBSD, Haiku ...).
# Copyright (C) The Linux Foundation
# Copyright (C) Institute for System Programming, RAS
# Author: Andrey Ponomarenko
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
########################################################################
use Getopt::Long;
Getopt::Long::Configure ("posix_default", "no_ignore_case");
use POSIX qw(setsid);
use File::Path qw(mkpath rmtree);
use File::Copy qw(copy);
use Cwd qw(abs_path);
use Config;

my $API_SANITY_AUTOTEST_VERSION = "1.10";
my ($Help, $TargetLibraryName, $GenerateTests, $TargetInterfaceName, $BuildTests, $RunTests, $CleanTests,
$DisableReuse, $LongVarNames, $Descriptor, $UseXvfb, $TestSystem, $MinimumCode, $TestDataPath, $MaximumCode,
$RandomCode, $GenerateDescriptorTemplate, $GenerateSpecTypeTemplate, $InterfacesListPath, $SpecTypes_PackagePath,
$CheckReturn, $DisableDefaultValues, $CheckStdCxxInterfaces, $DisableIncludeOptimization, $ShowReturnTypes,
$ShowExpendTime, $NoLibs, $Template2Code, $Standalone, $ShowVersion, $MakeIsolated, $ParameterNamesFilePath,
$CleanSources, $SplintAnnotations, $DumpVersion, $TargetHeaderName, $RelativeDirectory);

my @INPUT_OPTIONS = @ARGV;
my $CmdName = get_FileName($0);
GetOptions("h|help!" => \$Help,
  "v|version!" => \$ShowVersion,
  "dumpversion!" => \$DumpVersion,
#general options
  "l|library=s" => \$TargetLibraryName,
  "d|descriptor=s" => \$Descriptor,
  "gen|generate!" => \$GenerateTests,
  "build|make!" => \$BuildTests,
  "run!" => \$RunTests,
  "clean!" => \$CleanTests,
  "f|function|i|interface|s|symbol=s" => \$TargetInterfaceName,
  "functions-list|interfaces-list|symbols-list=s" => \$InterfacesListPath,
  "header=s" => \$TargetHeaderName,
  "xvfb!" => \$UseXvfb,
  "t2c|template2code" => \$Template2Code,
  "splint-specs" => \$SplintAnnotations,
#extra options
  "d-tmpl|descriptor-template!" =>\$GenerateDescriptorTemplate,
  "s-tmpl|specialized-type-template!" =>\$GenerateSpecTypeTemplate,
  "r|random!" =>\$RandomCode,
  "min!" =>\$MinimumCode,
  "max!" =>\$MaximumCode,
  "show-retval!" => \$ShowReturnTypes,
  "check-retval!" => \$CheckReturn,
  "st|specialized-types=s" => \$SpecTypes_PackagePath,
  "td|test-data=s" => \$TestDataPath,
  "without-shared-objects!" => \$NoLibs,
  "isolated!" => \$MakeIsolated,
  "view-only!" => \$CleanSources,
  "disable-default-values!" => \$DisableDefaultValues,
  "p|params=s" => \$ParameterNamesFilePath,
#other options
  "test!" => \$TestSystem,
  "time!" => \$ShowExpendTime,
  "check-stdcxx-symbols!" => \$CheckStdCxxInterfaces,
  "disable-variable-reuse!" => \$DisableReuse,
  "long-variable-names!" => \$LongVarNames,
  "relpath|reldir=s" => \$RelativeDirectory
) or exit(1);

sub HELP_MESSAGE()
{
    print STDERR <<"EOM"

NAME:
  $CmdName - generate basic unit tests for C/C++ library API

DESCRIPTION:
  Automatic generator of basic unit tests for shared C/C++ library. It helps
  to quickly generate simple ("sanity" or "shallow"-quality) tests for all
  functions from the target library API using its signatures and data type
  definitions straight from the library header files. The quality of generated
  tests allows to check absence of critical errors in simple use cases and can
  be improved by involving of highly reusable specialized types for the library.

  API Sanity Autotest can execute generated tests and detect all kinds of
  emitted signals, early program exits, program hanging and specified
  requirement failures. API Sanity Autotest can be considered as a tool for
  out-of-box low-cost sanity checking of library API or as a test development
  framework for initial generation of templates for advanced tests. Also it
  supports universal Template2Code format of tests, splint specifications,
  random test generation mode and other useful features.

  This tool is free software: you can redistribute it and/or modify it under
  the terms of the GNU GPL or GNU LGPL.

USAGE:
  $CmdName [options]

EXAMPLE OF USE:
  $CmdName -l <library_name> -d <descriptor_path> -gen -build -run

INFORMATION OPTIONS:
  -h|-help
      Print this help.

  -v|-version
      Print version information.

  -dumpversion
      Print the tool version ($API_SANITY_AUTOTEST_VERSION) and don't do anything else.

GENERAL OPTIONS:
  -l|-library <name>
      Library name (without version). 
      It affects only on the path and the title of the reports.

  -d|-descriptor <path>
      Path to the library descriptor.
      For more information, please see:
        http://ispras.linux-foundation.org/index.php/Library_Descriptor

  -gen|-generate
      Generate test(s). Options -l and -d should be specified.
      To generate test for the particular function use it with -f option.

  -build|-make
      Build test(s). Options -l and -d should be specified.
      To build test for the particular function use it with -f option.

  -run
      Run test(s), create test report. Options -l and -d should be specified.
      To run test for the particular function use it with -f option.

  -clean
      Clean test(s). Options -l and -d should be specified.
      To clean test for the particular function use it with -f option.

  -f|-function|-s|-symbol|-i|-interface <name>
      Generate/Build/Run test for the specified function (mangled/symbol name in C++).

  -functions-list|-symbols-list|-interfaces-list <path>
      This option allows to specify a file with a list of functions (one per line,
      mangled/symbol name in C++) that should be tested, other library functions
      will not be tested.

  -header <name>
      This option allows to restrict a list of functions that should be tested
      by providing a header file name in which they are declared. This option
      was introduced for step-by-step tests development.

  -xvfb
      Use Xvfb-server instead of current X-server (by default) for running tests.
      For more information, please see:
        http://en.wikipedia.org/wiki/Xvfb

  -t2c|-template2code
      Generate tests in the universal Template2Code format.
      For more information, please see:
        http://sourceforge.net/projects/template2code/

  -splint-specs
     Read splint specifications (annotations) in the header files.
     For more information, please see:
        http://www.splint.org/manual/manual.html

EXTRA OPTIONS:
  -d-tmpl|-descriptor-template
      Create library descriptor template 'lib_ver.xml' in the current directory.

  -s-tmpl|specialized-type-template
      Create specialized type template 'spectypes.xml' in the current directory.

  -r|-random
      Random test generation mode.

  -min
      Generate minimun code, call functions with minimum number of parameters
      for initializing parameters of other functions.

  -max
      Generate maximum code, call functions with maximum number of parameters
      for initializing parameters of other functions.

  -show-retval
      Show the function return type in the report.

  -check-retval
      Insert requirements on return values (retval!=NULL) for each called function.

  -st|-specialized-types <path>
      Path to the file with the collection of specialized types.
      For more information, please see:
        http://ispras.linux-foundation.org/index.php/Specialized_Type

  -td|-test-data <path>
      Path to the directory with the test data files.
      For more information, please see:
        http://ispras.linux-foundation.org/index.php/Specialized_Type#Using_test_data

  -without-shared-objects
      If the library haven't shared objects this option allows to generate tests
      without it.

  -isolated
      Allow to restrict functions usage by the lists specified by the -functions-list
      option or by the group devision in the descriptor.

  -view-only
      Remove all files from the test suite except *.html files. This option allows to
      create a lightweight html-index for all tests in the test suite.

  -disable-default-values
      Disable usage of default values for function parameters.
  
  -p|-params <path>
      Path to file with the function parameter names. It can be used for improving
      generated tests if the library header files don't contain parameter names.
      File format:
            func1;param1;param2;param3 ...
            func2;param1;param2;param3 ...
            ...

  -relpath|-reldir <path>
      Replace {RELPATH} in the library descriptor to <path>.

OTHER OPTIONS:
  -test
      Run internal tests, create simple library and run API-Sanity-Autotest on it.
      This option allows to check if the tool works correctly on the system.

  -time
      Show elapsed time for generating, building and running tests.

  -check-stdcxx-symbols
      Enable checking of the libstdc++ impurity functions.

  -disable-variable-reuse
      Disable reusing of previously created variables in the test.

  -long-variable-names
      Enable long (complex) variable names instead of short names.

DESCRIPTOR EXAMPLE:
  <version>
     1.30.0
  </version>

  <headers>
     /usr/local/atk/atk-1.30.0/include/
  </headers>

  <libs>
     /usr/local/atk/atk-1.30.0/lib/
  </libs>


Report bugs to <api-sanity-autotest\@linuxtesting.org>
For more information, please see: http://ispras.linux-foundation.org/index.php/API_Sanity_Autotest
EOM
      ;
}

my $Descriptor_Template = "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<descriptor>

<!-- Template for the Library Descriptor -->

<!--
     Necessary Sections
                        -->

<version>
    <!-- Version of the library -->
</version>

<headers>
    <!-- The list of paths to header files and/or
         directories with header files, one per line -->
</headers>

<libs>
    <!-- The list of paths to shared objects and/or
         directories with shared objects, one per line -->
</libs>

<!--
     Additional Sections
                         -->

<include_paths>
    <!-- The list of paths to be searched for header files
         needed for compiling of library headers, one per line.
         NOTE: If you define this section then the tool
         will not automatically detect include paths -->
</include_paths>

<add_include_paths>
    <!-- The list of include paths that should be added
         to the automatically detected include paths, one per line -->
</add_include_paths>

<gcc_options>
    <!-- Additional gcc options, one per line -->
</gcc_options>

<include_preamble>
    <!-- The list of header files that should be included before other headers, one per line.
         For example, it is a tree.h for libxml2 and ft2build.h for freetype2 -->
</include_preamble>

<libs_depend>
    <!-- The list of paths to shared objects that should be provided to gcc
         for resolving undefined symbols (if NEEDED elf section doesn't include it) -->
</libs_depend>

<opaque_types>
    <!-- The list of opaque types, one per line -->
</opaque_types>

<skip_interfaces>
    <!-- The list of functions (mangled/symbol names in C++)
         that should not be called in the tests, one per line -->
</skip_interfaces>

<skip_headers>
    <!-- The list of headers that should not be processed, one name per line -->
</skip_headers>

<libgroup>
    <name>
        <!-- Name of the libgroup -->
    </name>

    <interfaces>
        <!-- The list of functions (mangled/symbol names in C++)
             in the group that should be tested, one per line -->
    </interfaces>
</libgroup>

<out_params>
    <!-- Associating of out(returned)-parameters
         with interfaces, one entry per line:
           function_name:param_name
                 or
           function_name:param_number
         Examples:
           dbus_parse_address:entry
           dbus_parse_address:2       -->
</out_params>

<skip_warnings>
    <!-- The list of warnings that should not be shown in the report, one pattern per line -->
</skip_warnings>

</descriptor>";

my $SpecType_Template="<?xml version=\"1.0\" encoding=\"utf-8\"?>
<collection>

<lib_version>
    <!-- Constraint on the library version
         to which this collection will be applied.
         Select it from the following list:
           x.y.z
           >=x.y.z
           <=x.y.z -->
</lib_version>

<!--
 C/C++ language extensions in the code:
  \$(type) - instruction initializing an instance of data type
  \$[interface] - instruction for interface call with properly initialized parameters
  \$0 - an instance of the specialized type
  \$1, \$2, ... - references to 1st, 2nd and other interface parameters
  \$obj - reference to the object that current method calls on (C++ only)
  For more information, please see:
    http://ispras.linux-foundation.org/index.php/Specialized_Type
-->

<spec_type>
    <kind>
        <!-- Kind of the specialized type.
             Select it from the following list:
               normal
               common_param
               common_retval
               env
               common_env -->
    </kind>

    <data_type>
        <!-- Name of the corresponding real data type.
             You can specify several data types if kind is 'common_param'
             or 'common_retval', one per line.
             This section is not used if kind is 'env' or 'common_env' -->
    </data_type>

    <value>
        <!-- Value for initialization (true, 1.0, \"string\", ...) -->
    </value>

    <pre_condition>
        <!-- Precondition on associated function parameter.
               Example: \$0!=NULL -->
    </pre_condition>

    <post_condition>
        <!-- Postcondition on associated function return value or parameter.
               Example: \$0!=NULL && \$obj.style()==Qt::DotLine -->
    </post_condition>

    <init_code>
        <!-- Code that should be invoked before function call.
               Example: \$0->start(); -->
    </init_code>

    <final_code>
        <!-- Code that should be invoked after function call
               Example: \$0->end(); -->
    </final_code>

    <global_code>
        <!-- Declarations of auxiliary functions and global variables,
              header includes -->
    </global_code>

    <associating>
        <!-- Several \"associating\" sections
              are allowed simultaneously -->
        
        <interfaces>
            <!-- List of interfaces (mangled/symbol names in C++)
                  that will be associated with the specialized type, one per line -->
        </interfaces>

        <except>
            <!-- List of interfaces (mangled/symbol names in C++)
                 that will not be associated with the specialized type, one per line.
                 This section is used if kind is 'common_env', 'common_param'
                 or 'common_return' -->
        </except>

        <links>
            <!-- Associations with the return value, parameters
                 or/and object, one per line:
                   param1
                   param2
                   param3
                    ...
                   object
                   retval -->
        </links>
    </associating>
    
    <name>
        <!-- Name of the specialized type -->
    </name>

    <libs>
        <!-- External shared objects, one per line.
             If spectype contains call of the functions from
             some external shared objects then these objects
             should be listed here. Corresponding external
             header files should be included in global_code -->
    </libs>

    <lib_version>
        <!-- Constraint on the library version
             to which this spectype will be applied.
             Select it from the following list:
               x.y.z
               >=x.y.z
               <=x.y.z -->
    </lib_version>
</spec_type>

<spec_type>
    <!-- Other specialized type -->
</spec_type>

</collection>";

#Constants
my $BUFF_SIZE = 256;
my $DEFAULT_ARRAY_AMOUNT = 4;
my $MAX_PARAMS_INLINE = 3;
my $MAX_PARAMS_LENGTH_INLINE = 60;
my $POINTER_SIZE;
my $HANGED_EXECUTION_TIME = 7;
my $TOOL_SIGNATURE = "<hr/><div style='width:100%;font-family:Arial;font-size:11px;' align='right'><i>Generated on ".(localtime time)." for <span style='font-weight:bold'>$TargetLibraryName</span> by <a href='http://ispras.linux-foundation.org/index.php/API_Sanity_Autotest'>API-Sanity-Autotest</a> $API_SANITY_AUTOTEST_VERSION &nbsp;</i></div>";
my $MIN_PARAMS_MATRIX = 8;
my $MATRIX_WIDTH = 4;
my $MATRIX_MAX_ELEM_LENGTH = 7;
my $MAX_FILE_NAME_LENGTH = 255;
my $MAX_COMMAND_LINE_ARGUMENTS = 4096;

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
"compound" => ","
);

my %IsKeyword=(
"delete"=>1,
"if"=>1,
"else"=>1,
"for"=>1,
"public"=>1,
"private"=>1,
"new"=>1,
"protected"=>1,
"main"=>1,
"sizeof"=>1,
"malloc"=>1,
"return"=>1,
"include"=>1,
"true"=>1,
"false"=>1,
"const"=>1,
"int"=>1,
"long"=>1,
"void"=>1,
"short"=>1,
"float"=>1,
"unsigned"=>1,
"char"=>1,
"double"=>1,
"class"=>1,
"struct"=>1,
"union"=>1,
"enum"=>1,
"volatile"=>1,
"restrict"=>1
);

my %GlibcHeader=(
"aliases.h"=>1,
"argp.h"=>1,
"argz.h"=>1,
"assert.h"=>1,
"cpio.h"=>1,
"ctype.h"=>1,
"dirent.h"=>1,
"envz.h"=>1,
"errno.h"=>1,
"error.h"=>1,
"execinfo.h"=>1,
"fcntl.h"=>1,
"fstab.h"=>1,
"ftw.h"=>1,
"glob.h"=>1,
"grp.h"=>1,
"iconv.h"=>1,
"ifaddrs.h"=>1,
"inttypes.h"=>1,
"langinfo.h"=>1,
"limits.h"=>1,
"link.h"=>1,
"locale.h"=>1,
"malloc.h"=>1,
"mntent.h"=>1,
"monetary.h"=>1,
"nl_types.h"=>1,
"obstack.h"=>1,
"printf.h"=>1,
"pwd.h"=>1,
"regex.h"=>1,
"sched.h"=>1,
"search.h"=>1,
"setjmp.h"=>1,
"shadow.h"=>1,
"signal.h"=>1,
"spawn.h"=>1,
"stdint.h"=>1,
"stdio.h"=>1,
"stdlib.h"=>1,
"string.h"=>1,
"tar.h"=>1,
"termios.h"=>1,
"time.h"=>1,
"ulimit.h"=>1,
"unistd.h"=>1,
"utime.h"=>1,
"wchar.h"=>1,
"wctype.h"=>1,
"wordexp.h"=>1
);

my %GlibcDir=(
"sys"=>1,
"linux"=>1,
"bits"=>1,
"gnu"=>1,
"netinet"=>1,
"rpc"=>1
);

my %OperatingSystemAddPaths=(
# this data needed if tool can't determine paths automatically
"default"=>{
    "include"=>{"/usr/include"=>1,"/usr/lib"=>1},
    "lib"=>{"/usr/lib"=>1,"/lib"=>1},
    "bin"=>{"/usr/bin"=>1,"/bin"=>1,"/sbin"=>1,"/usr/sbin"=>1},
    "pkgconfig"=>{"/usr/lib/pkgconfig"=>1}},
"haiku"=>{
    "include"=>{"/boot/common"=>1,"/boot/develop"=>1},
    "lib"=>{"/boot/common/lib"=>1,"/boot/system/lib"=>1,"/boot/apps"=>1},
    "bin"=>{"/boot/common/bin"=>1,"/boot/system/bin"=>1},
    "pkgconfig"=>{"/boot/common/lib/pkgconfig"=>1},
    "gcc"=>{"/boot/develop/abi"=>1}}
    # Haiku has gcc-2.95.3 by default, try to find >= 3.0.0 in these paths
);

#Global variables
my $ST_ID=0;
my $REPORT_PATH;
my $TEST_SUITE_PATH;
my $LOG_PATH;
my %Interface_TestDir;
my %CompilerOptions_Libs;
my $CompilerOptions_Headers;
my %Language;
my %LibInfo;
my %Cache;
my %Descriptor;
my $TestedInterface;
my $COMMON_LANGUAGE="C";
my $STDCXX_TESTING;
my $GLIBC_TESTING;
my $MAIN_CPP_DIR;
my %SubClass_Created;
my $ConstantsSrc;
my %Constants;
my %AllDefines;
my $MaxTypeId_Start;
my $STAT_FIRST_LINE = "";

#Types
my %TypeDescr;
my %TemplateInstance;
my %TemplateInstance_Func;
my %TemplateHeader;
my %TypedefToAnon;
my %OpaqueTypes;
my %Tid_TDid;
my %TName_Tid;
my %StructUnionPName_Tid;
my %Class_Constructors;
my %Class_Destructors;
my %ReturnTypeId_Interface;
my %BaseType_PLevel_Return;
my %OutParam_Interface;
my %BaseType_PLevel_OutParam;
my %Interface_OutParam;
my %Interface_OutParam_NoUsing;
my %OutParamInterface_Pos;
my %OutParamInterface_Pos_NoUsing;
my %Class_SubClasses;
my %Class_BaseClasses;
my %Type_Typedef;
my %Typedef_BaseName;
my %StdCxxTypedef;
my %NameSpaces;
my %NestedNameSpaces;
my %EnumMembers;
my %SubClass_Instance;
my %SubClass_ObjInstance;
my %ClassInTargetLibrary;
my %TemplateNotInst;
my %EnumMembName_Id;
my %ExplicitTypedef;
my %StructIDs;
my %BaseType_PLevel_Type;
my %Struct_SubClasses;
my %Struct_Parent;
my %Library_Prefixes;
my %Member_Struct;
my %IgnoreTmplInst;

#Interfaces
my %FuncDescr;
my %CompleteSignature;
my %SkipInterfaces;
my %SkipInterfaces_Pattern;
my %tr_name;
my %mangled_name;
my %Interface_Library;
my %InterfaceVersion_Library;
my %NeededInterface_Library;
my %NeededInterfaceVersion_Library;
my %Class_PureVirtFunc;
my %Class_Method;
my %Interface_Overloads;
my %OverloadedInterface;
my %InlineTemplateFunction;
my %InterfacesList;
my %MethodNames;
my %FuncNames;
my %GlobVarNames;
my %InternalInterfaces;
my %Func_TypeId;
my %Header_Interface;
my %SoLib_IntPrefix;
my $NodeInterface;
my %LibGroups;
my %Interface_LibGroup;
my %AddIntParams;
my %Func_ShortName_MangledName;
my %UserDefinedOutParam;
my %MangledNames;
my $LibraryMallocFunc;

#Headers
my %Include_Preamble;
my %Headers;
my %Header_Dependency;
my %Include_Paths;
my %Add_Include_Paths;
my %DependencyHeaders_All;
my %DependencyHeaders_All_FullPath;
my %Header_ErrorRedirect;
my %Header_Includes;
my %Header_ShouldNotBeUsed;
my $Header_MaxIncludes;
my $IsHeaderListSpecified = 1;
my %Header_TopHeader;
my %Header_Include_Prefix;
my %Header_Prefix;
my %RecursiveIncludes;
my %RecursiveIncludes_Inverse;
my %RegisteredHeaders;
my %RegisteredHeaders_Short;
my %RegisteredDirs;
my %SpecTypeHeaders;
my %SkipHeaders;
my %SkipHeaders_Pattern;
my %SkipWarnings;
my %SkipWarnings_Pattern;
my %Header_NameSpaces;
my %Include_Order;
my %Include_RevOrder;

# Binaries
my %DefaultBinPaths;
my ($GCC_PATH, $GPP_PATH, $CPP_FILT) = ("gcc", "g++", "c++filt");

#Shared objects
my %SharedObjects;
my %SharedObject_Path;
my %SharedObject_UndefinedSymbols;
my %SoLib_DefaultPath;
my %CheckedSoLib;

#System shared objects
my %SystemObjects;
my %DefaultLibPaths;
my %SystemObjects_Needed;

#System header files
my %SystemHeaders;
my %DefaultCppPaths;
my %DefaultGccPaths;
my %DefaultIncPaths;
my %DefaultCppHeader;
my %DefaultGccHeader;

#Test results
my %GenResult;
my %BuildResult;
my %RunResult;
my %ResultCounter;

#Signals
my %SigNo;
my %SigName;

#Recursion locks
my @RecurTypeId;
my @RecurInterface;
my @RecurSpecType;
my @RecurHeader;
my @RecurLib;
my @RecurInclude;
my @RecurSymlink;

#System
my %SystemPaths;

#Global state
my (%ValueCollection, %Block_Variable, %UseVarEveryWhere, %SpecEnv, %Block_InsNum, $MaxTypeId, %Wrappers,
%Wrappers_SubClasses, %IntSubClass, %IntrinsicNum, %AuxType, %AuxFunc, %UsedConstructors,
%ConstraintNum, %RequirementsCatalog, %UsedProtectedMethods, %Create_SubClass, %SpecCode,
%SpecLibs, %UsedInterfaces, %OpenStreams, %IntSpecType, %Block_Param, %Class_SubClassTypedef, %AuxHeaders,
%Template2Code_Defines, %TraceFunc);

#Block initialization
my $CurrentBlock;

#Special types
my %SpecType;
my %InterfaceSpecType;
my %InterfaceSupplement;
my %InterfaceSupplement_Lib;
my %Common_SpecEnv;
my %Common_SpecType_Exceptions;

#Annotations
my %Interface_PreCondition;
my %Interface_PostCondition;

#Report
my $ContentSpanStart = "<span class=\"section\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\"><span class='ext' style='padding-right:2px'>[+]</span>\n";
my $ContentSpanStart_Title = "<span class=\"section_title\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\"><span class='ext_title' style='padding-right:2px'>[+]</span>\n";
my $ContentSpanEnd = "</span>\n";
my $ContentDivStart = "<div id=\"CONTENT_ID\" style=\"display:none;\">\n";
my $ContentDivEnd = "</div>\n";
my $ContentID = 1;
my $Content_Counter = 0;

sub get_CmdPath($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    return $Cache{"get_CmdPath"}{$Name} if(defined $Cache{"get_CmdPath"}{$Name});
    if(my $DefaultPath = get_CmdPath_Default($Name))
    {
        $Cache{"get_CmdPath"}{$Name} = $DefaultPath;
        return $DefaultPath;
    }
    foreach my $Path (sort {length($a)<=>length($b)} keys(%{$SystemPaths{"bin"}}))
    {
        if(-f $Path."/".$Name)
        {
            $Cache{"get_CmdPath"}{$Name} = $Path."/".$Name;
            return $Path."/".$Name;
        }
    }
    $Cache{"get_CmdPath"}{$Name} = "";
    return "";
}

sub get_CmdPath_Default($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    return $Cache{"get_CmdPath_Default"}{$Name} if(defined $Cache{"get_CmdPath_Default"}{$Name});
    if($Name eq "c++filt" and $CPP_FILT ne "c++filt")
    {
        $Cache{"get_CmdPath_Default"}{$Name} = $CPP_FILT;
        return $CPP_FILT;
    }
    if(`$Name --version 2>/dev/null`)
    {
        $Cache{"get_CmdPath_Default"}{$Name} = $Name;
        return $Name;
    }
    foreach my $Path (sort {length($a)<=>length($b)} keys(%DefaultBinPaths))
    {
        if(-f $Path."/".$Name)
        {
            $Cache{"get_CmdPath_Default"}{$Name} = $Path."/".$Name;
            return $Path."/".$Name;
        }
    }
    $Cache{"get_CmdPath_Default"}{$Name} = "";
    return "";
}

sub detectDisplay()
{
    my $DISPLAY_NUM=9; #default display number
    if(my $XPropCmd = get_CmdPath("xprop"))
    {
        #OK, found xprop, now use it to get a free display number
        foreach my $DNUM (9, 8, 7, 6, 5, 4, 3, 2, 10, 11, 12)
        {#try these display numbers only
            system("$XPropCmd -display :$DNUM".".0 -root \>\/dev\/null 2\>\&1");
            if($? ne 0)
            {
                #no properties found for this display, guess it is free
                $DISPLAY_NUM=$DNUM;
                last;
            }
        }
    }
    else
    {
        print STDERR "WARNING: can't find xprop\n";
    }
    return ":$DISPLAY_NUM.0";
}

sub runXvfb()
{
    my $Xvfb = get_CmdPath("Xvfb");
    if(not $Xvfb)
    {
        print STDERR "ERROR: can't find Xvfb\n";
        exit(1);
    }
    # Find a free display to use for Xvfb
    my $XT_DISPLAY = detectDisplay();
    my $TEST_DISPLAY=$XT_DISPLAY;
    my $running=0;
    if(my $PidofCmd = get_CmdPath("pidof"))
    {
        $running=`$PidofCmd Xvfb`;
        chomp($running);
    }
    else
    {
        print STDERR "WARNING: can't find pidof\n";
    }
    my $PsCmd = get_CmdPath("ps");
    if(not $running or $Config{"osname"}!~/\A(linux|freebsd|openbsd|netbsd)\Z/ or not $PsCmd)
    {
        print("starting X Virtual Frame Buffer on the display $TEST_DISPLAY\n");
        system("$Xvfb -screen 0 1024x768x24 $TEST_DISPLAY -ac +bs +kb -fp /usr/share/fonts/misc/ >\/dev\/null 2>&1 & sleep 1");
        if($?)
        {
            print STDERR "ERROR: can't start Xvfb: $?\n";
            exit(1);
        }
        $ENV{"DISPLAY"}=$TEST_DISPLAY;
        $ENV{"G_SLICE"}="always-malloc";
        return 1;
    }
    else
    {
        #Xvfb is running, determine the display number
        my $CMD_XVFB=`$PsCmd -p "$running" -f | tail -n 1`;
        chomp($CMD_XVFB);
        $CMD_XVFB=~/(\:\d+\.0)/;
        $XT_DISPLAY = $1;
        $ENV{"DISPLAY"}=$XT_DISPLAY;
        $ENV{"G_SLICE"}="always-malloc";
        print("Xvfb is already running (display: $XT_DISPLAY), so it will be used.\n");
        return 0;
    }
}

sub stopXvfb($)
{
    return if($_[0]!=1);
    if(my $PidofCmd = get_CmdPath("pidof"))
    {
        my $pid = `$PidofCmd Xvfb`;
        chomp($pid);
        if($pid)
        {
            kill(9, $pid);
        }
    }
    else
    {
        print STDERR "WARNING: can't find pidof\n";
    }
}

sub parseTag($$)
{
    my ($CodeRef, $Tag) = @_;
    return "" if(not $CodeRef or not ${$CodeRef} or not $Tag);
    if(${$CodeRef}=~s/\<\Q$Tag\E\>((.|\n)+?)\<\/\Q$Tag\E\>//)
    {
        my $Content = $1;
        $Content=~s/\A[\n]+//g;
        while($Content=~s/\A([ \t]+[\n]+)//g){}
        $Content=~s/\A[\n]+//g;
        $Content=~s/\s+\Z//g;
        if($Content=~/\n/)
        {
            $Content = alignSpaces($Content);
        }
        else
        {
            $Content=~s/\A[ \t]+//g;
        }
        return $Content;
    }
    else
    {
        return "";
    }
}

sub add_os_spectypes()
{
    if($Config{"osname"}=~/\A(haiku|beos)\Z/)
    {#http://www.haiku-os.org/legacy-docs/bebook/TheKernelKit_Miscellaneous.html
        readSpecTypes("
        <spec_type>
            <name>
                disable debugger in Haiku
            </name>
            <kind>
                common_env
            </kind>
            <global_code>
                #include <kernel/OS.h>
            </global_code>
            <init_code>
                disable_debugger(1);
            </init_code>
            <libs>
                libroot.so
            </libs>
            <associating>
                <except>
                    disable_debugger
                </except>
            </associating>
        </spec_type>");
    }
}

sub read_annotations_Splint()
{
    #reading interface specifications
    my (%HeaderInts, %InterfaceAnnotations,
    %TypeAnnotations, %ParamTypes, %HeaderTypes) = ();
    foreach my $Interface (keys(%Interface_Library))
    {
        if(my $Header = $CompleteSignature{$Interface}{"Header"})
        {
            $HeaderInts{$Header}{$Interface} = 1;
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
            {
                $ParamTypes{$CompleteSignature{$Interface}{"Param"}{$ParamPos}{"type"}} = 1;
            }
        }
    }
    foreach my $Header (keys(%HeaderInts))
    {
        if(my ($Inc, $Path) = identify_header($Header))
        {
            my @Lines = split(/\n/, readFile($Path));
            foreach my $Interface (keys(%{$HeaderInts{$Header}}))
            {
                my $IntLine = $CompleteSignature{$Interface}{"Line"}-1;
                my $Section = $Lines[$IntLine];
                my $LineNum = $IntLine;
                while($LineNum<=$#Lines-1 and $Lines[$LineNum]!~/;/)
                {#forward search
                    $LineNum+=1;
                    $Section .= "\n".$Lines[$LineNum];
                }
                $LineNum = $IntLine-1;
                while($LineNum>=0 and $Lines[$LineNum]!~/;/)
                {#backward search
                    $Section = $Lines[$LineNum]."\n".$Section;
                    $LineNum-=1;
                }
                my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
                if($Section=~/\W$ShortName\s*\(/)
                {
                    foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
                    {
                        my $ParamName = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"name"};
                        if($Section=~/(,|\()\s*([^,()]+?)\W$ParamName\s*(,|\)|\*)/)
                        {
                            my $ParamInfo = $2;
                            while($ParamInfo=~s/\/\*\s*\@\s*(returned|out|notnull|null)\s*\@\s*\*\///)
                            {
                                $InterfaceAnnotations{$Interface}{"Param"}{$ParamPos}{$1} = 1;
                            }
                        }
                    }
                    if($Section=~/\/\*\s*\@\s*modifies\s*(.+?)\s*\@\s*\*\//)
                    {
                        foreach my $Arg (split(/\s*,\s*/, $1))
                        {
                            $InterfaceAnnotations{$Interface}{"modifies"}{$Arg} = 1;
                        }
                    }
                    if($Section=~/\/\*\s*\@\s*requires\s*(.+?)\s*\@\s*\*\//)
                    {
                        $InterfaceAnnotations{$Interface}{"requires"} = $1;
                    }
                    if($Section=~/\/\*\s*\@\s*ensures\s*(.+?)\s*\@\s*\*\//)
                    {
                        $InterfaceAnnotations{$Interface}{"ensures"} = $1;
                    }
                }
            }
        }
    }
    foreach my $TypeId (keys(%ParamTypes))
    {
        if(my $Header = get_TypeHeader($TypeId))
        {
            if($DependencyHeaders_All{$Header})
            {
                $HeaderTypes{$Header}{$TypeId} = 1;
            }
        }
    }
    #reading data type specifications
    foreach my $Header (keys(%HeaderTypes))
    {
        if(my ($Inc, $Path) = identify_header($Header))
        {
            my @Lines = split(/\n/, readFile($Path));
            foreach my $TypeId (keys(%{$HeaderTypes{$Header}}))
            {
                my $TypeLine = $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Line"}-1;
                my $Section = $Lines[$TypeLine];
                $Section .= $Lines[$TypeLine-1]."\n".$Section if($TypeLine>1);
                $Section .= "\n".$Lines[$TypeLine+1] if($TypeLine<=$#Lines-1);
                if($Section=~/\/\*\s*\@\s*(abstract|concrete)\s*\@\s*\*\//)
                {
                    $TypeAnnotations{$TypeId}{$1} = 1;
                }
            }
        }
    }
    foreach my $Interface (keys(%InterfaceAnnotations))
    {
        foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$InterfaceAnnotations{$Interface}{"Param"}}))
        {
            my $ParamTypeId = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"type"};
            my $ParamName = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"name"};
            my %ParamAttr = %{$InterfaceAnnotations{$Interface}{"Param"}{$ParamPos}};
            if($ParamAttr{"out"} or $ParamAttr{"returned"})
            {
                register_out_param($Interface, $ParamPos, $ParamName, $ParamTypeId);
            }
            if($ParamAttr{"notnull"})
            {
                $Interface_PreCondition{$Interface}{"\$".($ParamPos+1)."!=0"} = 1;
            }
        }
        if(my $PreCondition = $InterfaceAnnotations{$Interface}{"requires"})
        {
            $Interface_PreCondition{$Interface}{condition_to_template($Interface, $PreCondition)} = 1;
        }
        if(my $PostCondition = $InterfaceAnnotations{$Interface}{"ensures"})
        {
            $Interface_PostCondition{$Interface}{condition_to_template($Interface, $PostCondition)} = 1;
        }
    }
    foreach my $TypeId (keys(%TypeAnnotations))
    {
        if($TypeAnnotations{$TypeId}{"abstract"})
        {
            $OpaqueTypes{get_TypeName($TypeId)} = 1;
        }
    }
}

sub condition_to_template($$$)
{
    my ($Interface, $Condition) = @_;
    return "" if(not $Interface or not $Condition);
    return "" if($Condition=~/(\W|\A)(maxSet|maxRead)(\W|\Z)/);
    foreach my $ParamPos (sort {int($b) <=> int($a)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $ParamName = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"name"};
        if($Condition=~/(\A|\W)$ParamName(\Z|\W)/)
        {
            my $ParamNum = $ParamPos+1;
            $Condition=~s/(\A|\W)$ParamName(\Z|\W)/$1\$$ParamNum$2/g;
        }
    }
    $Condition=~s/\/\\/&&/g;
    return $Condition;
}

sub register_out_param($$$$)
{
    my ($Interface, $ParamPos, $ParamName, $ParamTypeId) = @_;
    $OutParamInterface_Pos{$Interface}{$ParamPos}=1;
    $Interface_OutParam{$Interface}{$ParamName}=1;
    $BaseType_PLevel_OutParam{get_FoundationTypeId($ParamTypeId)}{get_PointerLevel($Tid_TDid{$ParamTypeId}, $ParamTypeId)-1}{$Interface}=1;
    foreach my $TypeId (get_OutParamFamily($ParamTypeId, 0))
    {
        $OutParam_Interface{$TypeId}{$Interface}=$ParamPos;
    }
}

sub cmpVersions($$)
{
    my ($First, $Second) = @_;
    if($First eq "" and $Second eq "")
    {
        return 0;
    }
    elsif($First eq ""
    or $Second eq "current")
    {
        return -1;
    }
    elsif($Second eq ""
    or $First eq "current")
    {
        return 1;
    }
    $First=~s/(\d)([a-z])/$1.$2/ig;
    $Second=~s/(\d)([a-z])/$1.$2/ig;
    $First=~s/(_|-)/./g;
    $Second=~s/(_|-)/./g;
    $First=~s/\A(_|-|\.)+//g;
    $Second=~s/\A(_|-|\.)+//g;
    $First=~s/\A[0]+([1-9]\d*)\Z/$1/g;
    $Second=~s/\A[0]+([1-9]\d*)\Z/$1/g;
    $First=~s/\A[0]+\Z/0/g;
    $Second=~s/\A[0]+\Z/0/g;
    if($First!~/\./ and $Second!~/\./)
    {
        return mixedCmp($_[0], $_[1]);
    }
    elsif($First!~/\./)
    {
        return cmpVersions($First.".0", $Second);
    }
    elsif($Second!~/\./)
    {
        return cmpVersions($First, $Second.".0");
    }
    else
    {
        my ($Part1, $Part2) = ();
        if($First =~ s/\A([^\.]+)\.//o)
        {
            $Part1 = $1;
        }
        if($Second =~ s/\A([^\.]+)\.//o)
        {
            $Part2 = $1;
        }
        if(my $CmpPartRes = mixedCmp($Part1, $Part2))
        {# compare first parts
            return $CmpPartRes;
        }
        else
        {# compare other parts
            return cmpVersions($First, $Second);
        }
    }
}

sub tokensCmp($$)
{
    my ($First, $Second) = @_;
    if($First eq $Second)
    {
        return 0;
    }
    elsif($First=~/\A[^a-z0-9]+\Z/i
    and $Second=~/\A[^a-z0-9]+\Z/i)
    {
        return 0;
    }
    elsif($First=~/\A[a-z]+\Z/i
    and $Second=~/\A[a-z]+\Z/i)
    {
        return trivialSymbCmp($_[0], $Second);
    }
    elsif($First=~/\A\d+\Z/
    and $Second=~/\A\d+\Z/)
    {
        return trivialNumerCmp($_[0], $Second);
    }
    elsif($First=~/\A[a-z]+\Z/i
    and $Second=~/\A\d+\Z/)
    {
        return -1;
    }
    elsif($First=~/\A\d+\Z/
    and $Second=~/\A[a-z]+\Z/i)
    {
        return 1;
    }
    elsif($First and $Second eq "")
    {
        return 1;
    }
    elsif($Second and $First eq "")
    {
        return -1;
    }
    else
    {
        return "undef";
    }
}

sub mixedCmp($$)
{
    my ($First, $Second) = @_;
    if($First eq $Second)
    {
        return 0;
    }
    while($First ne ""
    and $Second ne "")
    {
        my $First_Token = get_Token($First);
        my $Second_Token = get_Token($Second);
        my $CmpRes = tokensCmp($First_Token, $Second_Token);
        if($CmpRes eq "undef")
        {# safety Lock
            return 0;
        }
        elsif($CmpRes != 0)
        {
            return $CmpRes;
        }
        else
        {
            $First =~ s/\A\Q$First_Token\E//g;
            $Second =~ s/\A\Q$Second_Token\E//g;
        }
    }
    if($First ne ""
    or $First eq "0")
    {
        return 1;
    }
    elsif($Second ne ""
    or $Second eq "0")
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

sub get_Token($)
{
    if($_[0]=~/\A(\d+)[a-z]/i)
    {
        return $1;
    }
    elsif($_[0]=~/\A([a-z]+)\d/i)
    {
        return $1;
    }
    elsif($_[0]=~/\A(\d+)[^a-z0-9]/i)
    {
        return $1;
    }
    elsif($_[0]=~/\A([a-z]+)[^a-z0-9]/i)
    {
        return $1;
    }
    elsif($_[0]=~/\A([^a-z0-9]+)/i)
    {
        return $1;
    }
    else
    {
        return $_[0];
    }
}

sub trivialSymbCmp($$)
{
    if($_[0] gt $_[1])
    {
        return 1;
    }
    elsif($_[0] eq $_[1])
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

sub trivialNumerCmp($$)
{
    if(int($_[0]) > int($_[1]))
    {
        return 1;
    }
    elsif($_[0] eq $_[1])
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

sub verify_version($$)
{
    my ($Version, $Constraint) = @_;
    if($Constraint=~/>=\s*(.+)/)
    {
        return (cmpVersions($Version,$1)!=-1);
    }
    elsif($Constraint=~/<=\s*(.+)/)
    {
        return (cmpVersions($Version,$1)!=1);
    }
    else
    {
        return (cmpVersions($Version, $Constraint)==0);
    }
}

sub readSpecTypes($)
{
    my $Package = $_[0];
    return if(not $Package);
    $Package=~s/\/\*(.|\n)+?\*\///g;#removing C++ comments
    $Package=~s/<\!--(.|\n)+?-->//g;#removing XML comments
    if($Package!~/<collection>/ or $Package!~/<\/collection>/)
    {# add <collection> tag (support for old spectype packages)
        $Package = "<collection>\n".$Package."\n</collection>";
    }
    while(my $Collection = parseTag(\$Package, "collection"))
    {
        # verifying library version
        my $Collection_Copy = $Collection;
        while(parseTag(\$Collection_Copy, "spec_type")){};
        if(my $Collection_VersionConstraints = parseTag(\$Collection_Copy, "lib_version"))
        {
            my $Verified = 1;
            foreach my $Constraint (split(/\n/, $Collection_VersionConstraints))
            {
                $Constraint=~s/\A\s+|\s+\Z//g;
                if(not verify_version($Descriptor{"Version"}, $Constraint))
                {
                    $Verified=0;
                    last;
                }
            }
            next if(not $Verified);
        }
        # importing specialized types
        while(my $SpecType = parseTag(\$Collection, "spec_type"))
        {
            if(my $SpecType_VersionConstraints = parseTag(\$SpecType, "lib_version"))
            {
                my $Verified = 1;
                foreach my $Constraint (split(/\n/, $SpecType_VersionConstraints))
                {
                    $Constraint=~s/\A\s+|\s+\Z//g;
                    if(not verify_version($Descriptor{"Version"}, $Constraint))
                    {
                        $Verified=0;
                        last;
                    }
                }
                next if(not $Verified);
            }
            $ST_ID+=1;
            my (%Attr, %DataTypes) = ();
            $Attr{"Kind"} = parseTag(\$SpecType, "kind");
            $Attr{"Kind"} = "normal" if(not $Attr{"Kind"});
            foreach my $DataType (split(/\n/, parseTag(\$SpecType, "data_type")),
            split(/\n/, parseTag(\$SpecType, "data_types")))
            {# data_type==data_types, support of <= 1.5 versions
                $DataTypes{$DataType} = 1;
            }
            if(not keys(%DataTypes) and $Attr{"Kind"}=~/\A(normal|common_param|common_retval)\Z/)
            {
                print STDERR "ERROR: missed \'data_type\' attribute in one of the \'".$Attr{"Kind"}."\' spectypes\n";
                next;
            }
            $Attr{"Name"} = parseTag(\$SpecType, "name");
            $Attr{"Value"} = parseTag(\$SpecType, "value");
            $Attr{"PreCondition"} = parseTag(\$SpecType, "pre_condition");
            $Attr{"PostCondition"} = parseTag(\$SpecType, "post_condition");
            if(not $Attr{"PostCondition"})
            {# constraint==post_condition, support of <= 1.6 versions
                $Attr{"PostCondition"} = parseTag(\$SpecType, "constraint");
            }
            $Attr{"InitCode"} = parseTag(\$SpecType, "init_code");
            $Attr{"FinalCode"} = parseTag(\$SpecType, "final_code");
            $Attr{"GlobalCode"} = parseTag(\$SpecType, "global_code");
            foreach my $Lib (split(/\n/, parseTag(\$SpecType, "libs")))
            {
                $Attr{"Libs"}{$Lib} = 1;
            }
            if($Attr{"Kind"} eq "common_env")
            {
                $Common_SpecEnv{$ST_ID} = 1;
            }
            while(my $Associating = parseTag(\$SpecType, "associating"))
            {
                my (%Interfaces, %Except) = ();
                foreach my $Interface (split(/\n/, parseTag(\$Associating, "interfaces")))
                {
                    $Interface=~s/\A\s+|\s+\Z//g;
                    $Interfaces{$Interface} = 1;
                    $Common_SpecType_Exceptions{$Interface}{$ST_ID} = 0;
                }
                foreach my $Interface (split(/\n/, parseTag(\$Associating, "except")))
                {
                    $Interface=~s/\A\s+|\s+\Z//g;
                    $Except{$Interface} = 1;
                    $Common_SpecType_Exceptions{$Interface}{$ST_ID} = 1;
                }
                if($Attr{"Kind"} eq "env")
                {
                    foreach my $Interface (keys(%Interfaces))
                    {
                        next if($Except{$Interface});
                        $InterfaceSpecType{$Interface}{"SpecEnv"} = $ST_ID;
                    }
                }
                else
                {
                    foreach my $Link (split(/\n/, parseTag(\$Associating, "links")))
                    {
                        $Link=~s/\A\s+|\s+\Z//g;
                        if(lc($Link)=~/\Aparam(\d+)\Z/)
                        {
                            my $Param_Num = $1;
                            foreach my $Interface (keys(%Interfaces))
                            {
                                next if($Except{$Interface});
                                $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Num - 1} = $ST_ID;
                            }
                        }
                        elsif(lc($Link)=~/\Aobject\Z/)
                        {
                            foreach my $Interface (keys(%Interfaces))
                            {
                                next if($Except{$Interface});
                                $InterfaceSpecType{$Interface}{"SpecObject"} = $ST_ID;
                            }
                        }
                        elsif(lc($Link)=~/\Aretval\Z/)
                        {
                            foreach my $Interface (keys(%Interfaces))
                            {
                                next if($Except{$Interface});
                                $InterfaceSpecType{$Interface}{"SpecReturn"} = $ST_ID;
                            }
                        }
                        else
                        {
                            print STDERR "WARNING: unrecognized link \'$Link\' in the collection of specialized types\n";
                        }
                    }
                }
            }
            if($Attr{"Kind"}=~/\A(common_param|common_retval)\Z/)
            {
                foreach my $DataType (keys(%DataTypes))
                {
                    $Attr{"DataType"} = $DataType;
                    %{$SpecType{$ST_ID}} = %Attr;
                    $ST_ID+=1;
                }
            }
            elsif($Attr{"Kind"} eq "normal")
            {
                $Attr{"DataType"} = (keys(%DataTypes))[0];
                %{$SpecType{$ST_ID}} = %Attr;
            }
            else
            {
                %{$SpecType{$ST_ID}} = %Attr;
            }
        }
    }
}

sub readDescriptor($)
{
    my $Path = $_[0];
    return if(not $Path);
    $Descriptor{"Path"} = $Path;
    if(not -f $Path)
    {
        print STDERR "ERROR: can't access file \'$Path\'\n";
        exit(1);
    }
    my $Descriptor_File = readFile($Path);
    $Descriptor_File=~s/\/\*(.|\n)+?\*\///g;
    $Descriptor_File=~s/<\!--(.|\n)+?-->//g;
    if(not $Descriptor_File)
    {
        print STDERR "ERROR: library descriptor is empty\n";
        exit(1);
    }
    $Descriptor{"Version"} = parseTag(\$Descriptor_File, "version");
    if(not $Descriptor{"Version"})
    {
        print STDERR "ERROR: version in the library descriptor was not specified (section <version>)\n";
        exit(1);
    }
    $Descriptor{"Headers"} = parseTag(\$Descriptor_File, "headers");
    if(not $Descriptor{"Headers"})
    {
        print STDERR "ERROR: header files in the library descriptor were not specified (section <headers>)\n";
        exit(1);
    }
    $Descriptor{"Libs"} = parseTag(\$Descriptor_File, "libs");
    if(not $Descriptor{"Libs"} and not $NoLibs)
    {
        print STDERR "ERROR: shared objects in the library descriptor were not specified (section <libs>)\n";
        exit(1);
    }
    foreach my $Dest (split(/\n/, parseTag(\$Descriptor_File, "include_paths")))
    {
        $Dest=~s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        $Descriptor{"IncludePaths"}{$Dest} = 1;
    }
    foreach my $Dest (split(/\n/, parseTag(\$Descriptor_File, "add_include_paths")))
    {
        $Dest=~s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        $Descriptor{"AddIncludePaths"}{$Dest} = 1;
    }
    $Descriptor{"GccOptions"} = parseTag(\$Descriptor_File, "gcc_options");
    foreach my $Option (split(/\n/, $Descriptor{"GccOptions"}))
    {
        $Option=~s/\A\s+|\s+\Z//g;
        next if(not $Option);
        if($Option=~/\.so[0-9.]*\Z/)
        {
            $CompilerOptions_Libs{$Option} = 1;
        }
        else
        {
            $CompilerOptions_Headers .= " ".$Option;
        }
    }
    $Descriptor{"OutParams"} = parseTag(\$Descriptor_File, "out_params");
    foreach my $IntParam (split(/\n/, $Descriptor{"OutParams"}))
    {
        $IntParam=~s/\A\s+|\s+\Z//g;
        next if(not $IntParam);
        if($IntParam=~/(.+)(:|;)(.+)/)
        {
            $UserDefinedOutParam{$1}{$3} = 1;
        }
    }
    $Descriptor{"LibsDepend"} = parseTag(\$Descriptor_File, "libs_depend");
    foreach my $Dep (split(/\n/, $Descriptor{"LibsDepend"}))
    {
        $Dep=~s/\A\s+|\s+\Z//g;
        next if(not $Dep);
        if(not -f $Dep)
        {
            print STDERR "ERROR: can't access \'$Dep\': no such file\n";
            next;
        }
        $Dep = abs_path($Dep) if($Dep!~/\A\//);
        $CompilerOptions_Libs{$Dep} = 1;
    }
    $Descriptor{"SkipHeaders"} = parseTag(\$Descriptor_File, "skip_headers");
    foreach my $Name (split(/\n/, $Descriptor{"SkipHeaders"}))
    {
        $Name=~s/\A\s+|\s+\Z//g;
        next if(not $Name);
        if($Name=~/\*|\//)
        {
            $Name=~s/\*/.*/g;
            $SkipHeaders_Pattern{$Name} = 1;
        }
        else
        {
            $SkipHeaders{$Name} = 1;
        }
    }
    $Descriptor{"Include_Order"} = parseTag(\$Descriptor_File, "include_order");
    foreach my $Order (split(/\n/, $Descriptor{"Include_Order"}))
    {
        $Order=~s/\A\s+|\s+\Z//g;
        next if(not $Order);
        if($Order=~/\A(.+):(.+)\Z/)
        {
            $Include_Order{$1} = $2;
            $Include_RevOrder{$2} = $1;
        }
    }
    $Descriptor{"SkipWarnings"} = parseTag(\$Descriptor_File, "skip_warnings");
    foreach my $Warning (split(/\n/, $Descriptor{"SkipWarnings"}))
    {
        $Warning=~s/\A\s+|\s+\Z//g;
        next if(not $Warning);
        if($Warning=~s/\*/.*/g)
        {
            $SkipWarnings_Pattern{$Warning} = 1;
        }
        else
        {
            $SkipWarnings{$Warning} = 1;
        }
    }
    $Descriptor{"OpaqueTypes"} = parseTag(\$Descriptor_File, "opaque_types");
    foreach my $Type_Name (split(/\n/, $Descriptor{"OpaqueTypes"}))
    {
        $Type_Name=~s/\A\s+|\s+\Z//g;
        next if(not $Type_Name);
        $OpaqueTypes{$Type_Name} = 1;
    }
    $Descriptor{"SkipInterfaces"} = parseTag(\$Descriptor_File, "skip_interfaces");
    foreach my $Interface_Name (split(/\n/, $Descriptor{"SkipInterfaces"}))
    {
        $Interface_Name=~s/\A\s+|\s+\Z//g;
        next if(not $Interface_Name);
        if($Interface_Name=~s/\*/.*/g)
        {
            $SkipInterfaces_Pattern{$Interface_Name} = 1;
        }
        else
        {
            $SkipInterfaces{$Interface_Name} = 1;
        }
    }
    $Descriptor{"IncludePreamble"} = parseTag(\$Descriptor_File, "include_preamble");
    while(my $LibGroupTag = parseTag(\$Descriptor_File, "libgroup"))
    {
        my $LibGroupName = parseTag(\$LibGroupTag, "name");
        foreach my $Interface (split(/\n/, parseTag(\$LibGroupTag, "interfaces")))
        {
            $Interface=~s/\A\s+|\s+\Z//g;
            $LibGroups{$LibGroupName}{$Interface} = 1;
            $Interface_LibGroup{$Interface}=$LibGroupName;
        }
    }
    if(keys(%Interface_LibGroup))
    {
        if(keys(%InterfacesList))
        {
            %InterfacesList=();
        }
        foreach my $LibGroup (keys(%LibGroups))
        {
            foreach my $Interface (keys(%{$LibGroups{$LibGroup}}))
            {
                $InterfacesList{$Interface}=1;
            }
        }
    }
}

sub getArch()
{
    my $Arch = $ENV{"CPU"};
    if(not $Arch and my $UnameCmd = get_CmdPath("uname"))
    {
        $Arch = `$UnameCmd -m`;
        chomp($Arch);
        if(not $Arch)
        {
            $Arch = `$UnameCmd -p`;
            chomp($Arch);
        }
    }
    $Arch = $Config{archname} if(not $Arch);
    $Arch = "x86" if($Arch=~/i[3-7]86/);
    return $Arch;
}

sub get_Summary()
{
    my $Summary = "<h2 class='title2'>Summary</h2><hr/>";
    $Summary .= "<table cellpadding='3' border='1' style='border-collapse:collapse;'>";
    my $Verdict = "";
    if($ResultCounter{"Run"}{"Fail"} > 0)
    {
        $Verdict = "<span style='color:Red;'><b>Test Failed</b></span>";
        $STAT_FIRST_LINE .= "verdict:failed;";
    }
    else
    {
        $Verdict = "<span style='color:Green;'><b>Test Passed</b></span>";
        $STAT_FIRST_LINE .= "verdict:passed;";
    }
    $Summary .= "<tr><td class='table_header summary_item'>Total tests</td><td align='right' class='summary_item_value'>".($ResultCounter{"Run"}{"Success"}+$ResultCounter{"Run"}{"Fail"})."</td></tr>";
    $STAT_FIRST_LINE .= "total:".($ResultCounter{"Run"}{"Success"}+$ResultCounter{"Run"}{"Fail"}).";";
    my $Success_Tests_Link = "0";
    $Success_Tests_Link = $ResultCounter{"Run"}{"Success"} if($ResultCounter{"Run"}{"Success"}>0);
    $STAT_FIRST_LINE .= "passed:".$ResultCounter{"Run"}{"Success"}.";";
    my $Failed_Tests_Link = "0";
    $Failed_Tests_Link = "<a href='#Failed_Tests' style='color:Blue;'>".$ResultCounter{"Run"}{"Fail"}."</a>" if($ResultCounter{"Run"}{"Fail"}>0);
    $STAT_FIRST_LINE .= "failed:".$ResultCounter{"Run"}{"Fail"}.";";
    $Summary .= "<tr><td class='table_header summary_item'>Passed / Failed tests</td><td align='right' class='summary_item_value'>$Success_Tests_Link / $Failed_Tests_Link</td></tr>";
    if($ResultCounter{"Run"}{"Warnings"}>0)
    {
        my $Warnings_Link = "<a href='#Warnings' style='color:Blue;'>".$ResultCounter{"Run"}{"Warnings"}."</a>";
        $Summary .= "<tr><td class='table_header summary_item'>Warnings</td><td align='right' class='summary_item_value'>$Warnings_Link</td></tr>";
    }
    $STAT_FIRST_LINE .= "warnings:".$ResultCounter{"Run"}{"Warnings"};
    $Summary .= "<tr><td class='table_header summary_item'>Verdict</td><td align='right'>$Verdict</td></tr>";
    $Summary .= "</table>\n";
    return "<!--Summary-->\n".$Summary."<!--Summary_End-->\n";
}

sub get_Problem_Summary()
{
    my $Problem_Summary = "";
    my %ProblemType_Interface = ();
    foreach my $Interface (keys(%RunResult))
    {
        next if($RunResult{$Interface}{"Warnings"});
        $ProblemType_Interface{$RunResult{$Interface}{"Type"}}{$Interface} = 1;
    }
    my $ColSpan = 1;
    if(keys(%{$ProblemType_Interface{"Received_Signal"}}))
    {
        my %SignalName_Interface = ();
        foreach my $Interface (keys(%{$ProblemType_Interface{"Received_Signal"}}))
        {
            $SignalName_Interface{$RunResult{$Interface}{"Value"}}{$Interface} = 1;
        }
        if(keys(%SignalName_Interface)==1)
        {
            my $SignalName = (keys(%SignalName_Interface))[0];
            my $Amount = keys(%{$SignalName_Interface{$SignalName}});
            my $Link = "<a href=\'#Received_Signal_$SignalName\' style='color:Blue;'>$Amount</a>";
            $STAT_FIRST_LINE .= lc("Received_Signal_$SignalName:$Amount;");
            $Problem_Summary .= "<tr><td class='table_header summary_item'>Received signal $SignalName</td><td align='right' class='summary_item_value'>$Link</td></tr>";
        }
        elsif(keys(%SignalName_Interface)>1)
        {
            $Problem_Summary .= "<tr><td class='table_header summary_item' rowspan='".keys(%SignalName_Interface)."'>Received signal</td>";
            my $num = 1;
            foreach my $SignalName (sort keys(%SignalName_Interface))
            {
                my $Amount = keys(%{$SignalName_Interface{$SignalName}});
                my $Link = "<a href=\'#Received_Signal_$SignalName\' style='color:Blue;'>$Amount</a>";
                $STAT_FIRST_LINE .= lc("Received_Signal_$SignalName:$Amount;");
                $Problem_Summary .= (($num!=1)?"<tr>":"")."<td class='table_header summary_item'>$SignalName</td><td align='right' class='summary_item_value'>$Link</td></tr>";
                $num+=1;
            }
            $ColSpan = 2;
        }
    }
    if(keys(%{$ProblemType_Interface{"Exited_With_Value"}}))
    {
        my %ExitValue_Interface = ();
        foreach my $Interface (keys(%{$ProblemType_Interface{"Exited_With_Value"}}))
        {
            $ExitValue_Interface{$RunResult{$Interface}{"Value"}}{$Interface} = 1;
        }
        if(keys(%ExitValue_Interface)==1)
        {
            my $ExitValue = (keys(%ExitValue_Interface))[0];
            my $Amount = keys(%{$ExitValue_Interface{$ExitValue}});
            my $Link = "<a href=\'#Exited_With_Value_$ExitValue\' style='color:Blue;'>$Amount</a>";
            $STAT_FIRST_LINE .= lc("Exited_With_Value_$ExitValue:$Amount;");
            $Problem_Summary .= "<tr><td class='table_header summary_item' colspan=\'$ColSpan\'>Exited with value \"$ExitValue\"</td><td align='right' class='summary_item_value'>$Link</td></tr>";
        }
        elsif(keys(%ExitValue_Interface)>1)
        {
            $Problem_Summary .= "<tr><td class='table_header summary_item' rowspan='".keys(%ExitValue_Interface)."'>Exited with value</td>";
            foreach my $ExitValue (sort keys(%ExitValue_Interface))
            {
                my $Amount = keys(%{$ExitValue_Interface{$ExitValue}});
                my $Link = "<a href=\'#Exited_With_Value_$ExitValue\' style='color:Blue;'>$Amount</a>";
                $STAT_FIRST_LINE .= lc("Exited_With_Value_$ExitValue:$Amount;");
                $Problem_Summary .= "<td class='table_header summary_item'>\"$ExitValue\"</td><td align='right' class='summary_item_value'>$Link</td></tr>";
            }
            $Problem_Summary .= "</tr>";
            $ColSpan = 2;
        }
    }
    if(keys(%{$ProblemType_Interface{"Hanged_Execution"}}))
    {
        my $Amount = keys(%{$ProblemType_Interface{"Hanged_Execution"}});
        my $Link = "<a href=\'#Hanged_Execution\' style='color:Blue;'>$Amount</a>";
        $STAT_FIRST_LINE .= "hanged_execution:$Amount;";
        $Problem_Summary .= "<tr><td class='table_header summary_item' colspan=\'$ColSpan\'>Hanged execution</td><td align='right' class='summary_item_value'>$Link</td></tr>";
    }
    if(keys(%{$ProblemType_Interface{"Requirement_Failed"}}))
    {
        my $Amount = keys(%{$ProblemType_Interface{"Requirement_Failed"}});
        my $Link = "<a href=\'#Requirement_Failed\' style='color:Blue;'>$Amount</a>";
        $STAT_FIRST_LINE .= "requirement_failed:$Amount;";
        $Problem_Summary .= "<tr><td class='table_header summary_item' colspan=\'$ColSpan\'>Requirement failed</td><td align='right' class='summary_item_value'>$Link</td></tr>";
    }
    if(keys(%{$ProblemType_Interface{"Other_Problems"}}))
    {
        my $Amount = keys(%{$ProblemType_Interface{"Other_Problems"}});
        my $Link = "<a href=\'#Other_Problems\' style='color:Blue;'>$Amount</a>";
        $STAT_FIRST_LINE .= "other_problems:$Amount;";
        $Problem_Summary .= "<tr><td class='table_header summary_item' colspan=\'$ColSpan\'>Other problems</td><td align='right' class='summary_item_value'>$Link</td></tr>";
    }
    if($Problem_Summary)
    {
        $Problem_Summary = "<h2 class='title2'>Problem Summary</h2><hr/>"."<table cellpadding='3' border='1' style='border-collapse:collapse;'>".$Problem_Summary."</table>\n";
        return "<!--Problem_Summary-->\n".$Problem_Summary."<!--Problem_Summary_End-->\n";
    }
    else
    {
        return "";
    }
}

sub get_Report_Header()
{
    my $Report_Header = "<h1 class='title1'>Test results for the library <span style='color:Blue;'>$TargetLibraryName</span>-<span style='color:Blue;'>".$Descriptor{"Version"}."</span> on <span style='color:Blue;'>".getArch()."</span></h1>\n";
    return "<!--Header-->\n".$Report_Header."<!--Header_End-->\n";
}

sub get_TestSuite_Header()
{
    my $Report_Header = "<h1 class='title1'>Test suite for the library <span style='color:Blue;'>$TargetLibraryName</span>-<span style='color:Blue;'>".$Descriptor{"Version"}."</span> on <span style='color:Blue;'>".getArch()."</span></h1>\n";
    return "<!--Header-->\n".$Report_Header."<!--Header_End-->\n";
}

sub get_problem_title($$)
{
    my ($ProblemType, $Value) = @_;
    if($ProblemType eq "Received_Signal")
    {
        return "Received signal $Value";
    }
    elsif($ProblemType eq "Exited_With_Value")
    {
        return "Exited with value \"$Value\"";
    }
    elsif($ProblemType eq "Requirement_Failed")
    {
        return "Requirement failed";
    }
    elsif($ProblemType eq "Hanged_Execution")
    {
        return "Hanged execution";
    }
    elsif($ProblemType eq "Unexpected_Output")
    {
        return "Unexpected Output";
    }
    elsif($ProblemType eq "Other_Problems")
    {
        return "Other problems";
    }
    else
    {
        return "";
    }
}

sub get_count_title($$)
{
    my ($Word, $Number) = @_;
    if($Number>=2 or $Number==0)
    {
        return "$Number $Word"."s";
    }
    elsif($Number==1)
    {
        return "1 $Word";
    }
}

sub get_TestView($$)
{
    my ($Test, $Interface) = @_;
    $Test = highlight_code($Test, $Interface);
    $Test = htmlSpecChars($Test);
    $Test=~s/\@LT\@/</g;
    $Test=~s/\@GT\@/>/g;
    $Test=~s/\@SP\@/ /g;
    return "<table class='test_view'><tr><td>".$Test."</td></tr></table>\n";
}

sub rm_prefix($)
{
    my $Str = $_[0];
    $Str=~s/\A[_~]+//g;
    return $Str;
}

sub get_IntNameSpace($)
{
    my $Interface = $_[0];
    return "" if(not $Interface);
    return $Cache{"get_IntNameSpace"}{$Interface} if(defined $Cache{"get_IntNameSpace"}{$Interface});
    if(get_Signature($Interface)=~/\:\:/)
    {
        my $FounNameSpace = 0;
        foreach my $NameSpace (sort {get_depth($b)<=>get_depth($a)} keys(%NestedNameSpaces))
        {
            if(get_Signature($Interface)=~/\A\Q$NameSpace\E\:\:/)
            {
                $Cache{"get_IntNameSpace"}{$Interface} = $NameSpace;
                return $NameSpace;
            }
        }
    }
    else
    {
        $Cache{"get_IntNameSpace"}{$Interface} = "";
        return "";
    }
}

sub get_TestSuite_List()
{
    my ($TEST_LIST, %LibGroup_Header_Interface);
    my $Tests_Num = 0;
    my %Interface_Signature = ();
    return "" if(not keys(%Interface_TestDir));
    foreach my $Interface (keys(%Interface_TestDir))
    {
        my $Header = $CompleteSignature{$Interface}{"Header"};
        my $SharedObject = get_FileName($Interface_Library{$Interface});
        $SharedObject = get_FileName($NeededInterface_Library{$Interface}) if(not $SharedObject);
        $LibGroup_Header_Interface{$Interface_LibGroup{$Interface}}{$SharedObject}{$Header}{$Interface} = 1;
        $Tests_Num += 1;
    }
    foreach my $LibGroup (sort {lc($a) cmp lc($b)} keys(%LibGroup_Header_Interface))
    {
        foreach my $SoName (sort {lc($a) cmp lc($b)} keys(%{$LibGroup_Header_Interface{$LibGroup}}))
        {
            foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%{$LibGroup_Header_Interface{$LibGroup}{$SoName}}))
            {
                $TEST_LIST .= "<div style='height:15px;'></div><span class='header'>$HeaderName</span>".(($SoName ne "WithoutLib")?", <span class='solib'>$SoName</span>":"").(($LibGroup)?"&nbsp;<span class='libgroup'>\"$LibGroup\"</span>":"")."<br/>\n";
                my %NameSpace_Interface = ();
                foreach my $Interface (keys(%{$LibGroup_Header_Interface{$LibGroup}{$SoName}{$HeaderName}}))
                {
                    $NameSpace_Interface{get_IntNameSpace($Interface)}{$Interface} = 1;
                }
                foreach my $NameSpace (sort keys(%NameSpace_Interface))
                {
                    $TEST_LIST .= ($NameSpace)?"<span class='namespace_title'>namespace</span> <span class='namespace'>$NameSpace</span>"."<br/>\n":"";
                    my @SortedInterfaces = sort {lc(rm_prefix($CompleteSignature{$a}{"ShortName"})) cmp lc(rm_prefix($CompleteSignature{$b}{"ShortName"}))} keys(%{$NameSpace_Interface{$NameSpace}});
                    @SortedInterfaces = sort {$CompleteSignature{$a}{"Destructor"} <=> $CompleteSignature{$b}{"Destructor"}} @SortedInterfaces;
                    @SortedInterfaces = sort {lc(get_TypeName($CompleteSignature{$a}{"Class"})) cmp lc(get_TypeName($CompleteSignature{$b}{"Class"}))} @SortedInterfaces;
                    foreach my $Interface (@SortedInterfaces)
                    {
                        my $RelPath = $Interface_TestDir{$Interface};
                        $RelPath=~s/\A\Q$TEST_SUITE_PATH\E[\/]*//g;
                        my $Signature = get_Signature($Interface);
                        if($NameSpace)
                        {
                            $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                        }
                        $TEST_LIST .= "<a class='link' href=\'$RelPath/view.html\'><span class='int'>".highLight_Signature_Italic_Color(htmlSpecChars($Signature))."</span></a><br/><div style='height:1px'></div>\n";
                    }
                }
            }
        }
    }
    $STAT_FIRST_LINE .= "total:$Tests_Num";
    return "<h2 class='title2'>Tests ($Tests_Num)</h2><hr/>\n<!--Test_List-->\n".$TEST_LIST."<!--Test_List_End-->\n"."<a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
}

sub get_FailedTests($)
{
    my $Failures_Or_Warning = $_[0];
    my ($FAILED_TESTS, %Type_Value_LibGroup_Header_Interface);
    foreach my $Interface (keys(%RunResult))
    {
        if($Failures_Or_Warning eq "Failures")
        {
            next if($RunResult{$Interface}{"Warnings"});
        }
        elsif($Failures_Or_Warning eq "Warnings")
        {
            next if(not $RunResult{$Interface}{"Warnings"});
        }
        my $Header = $RunResult{$Interface}{"Header"};
        my $SharedObject = $RunResult{$Interface}{"SharedObject"};
        my $ProblemType = $RunResult{$Interface}{"Type"};
        my $ProblemValue = $RunResult{$Interface}{"Value"};
        $Type_Value_LibGroup_Header_Interface{$ProblemType}{$ProblemValue}{$Interface_LibGroup{$Interface}}{$SharedObject}{$Header}{$Interface} = 1;
    }
    foreach my $ProblemType ("Received_Signal", "Exited_With_Value", "Hanged_Execution", "Requirement_Failed", "Unexpected_Output", "Other_Problems")
    {
        next if(not keys(%{$Type_Value_LibGroup_Header_Interface{$ProblemType}}));
        foreach my $ProblemValue (sort keys(%{$Type_Value_LibGroup_Header_Interface{$ProblemType}}))
        {
            my $PROBLEM_REPORT = "";
            my $Problems_Count = 0;
            foreach my $LibGroup (sort {lc($a) cmp lc($b)} keys(%{$Type_Value_LibGroup_Header_Interface{$ProblemType}{$ProblemValue}}))
            {
                foreach my $SoName (sort {lc($a) cmp lc($b)} keys(%{$Type_Value_LibGroup_Header_Interface{$ProblemType}{$ProblemValue}{$LibGroup}}))
                {
                    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%{$Type_Value_LibGroup_Header_Interface{$ProblemType}{$ProblemValue}{$LibGroup}{$SoName}}))
                    {
                        next if(not $HeaderName or not $SoName);
                        my $HEADER_LIB_REPORT = "<div style='height:15px;'></div><span class='header'>$HeaderName</span>".(($SoName ne "WithoutLib")?", <span class='solib'>$SoName</span>":"").(($LibGroup)?"&nbsp;<span class='libgroup'>\"$LibGroup\"</span>":"")."<br/>\n";
                        my %NameSpace_Interface = ();
                        foreach my $Interface (keys(%{$Type_Value_LibGroup_Header_Interface{$ProblemType}{$ProblemValue}{$LibGroup}{$SoName}{$HeaderName}}))
                        {
                            $NameSpace_Interface{$RunResult{$Interface}{"NameSpace"}}{$Interface} = 1;
                        }
                        foreach my $NameSpace (sort keys(%NameSpace_Interface))
                        {
                            $HEADER_LIB_REPORT .= ($NameSpace)?"<span class='namespace_title'>namespace</span> <span class='namespace'>$NameSpace</span>"."<br/>\n":"";
                            my @SortedInterfaces = sort {$RunResult{$a}{"Signature"} cmp $RunResult{$b}{"Signature"}} keys(%{$NameSpace_Interface{$NameSpace}});
                            foreach my $Interface (@SortedInterfaces)
                            {
                                my $Signature = $RunResult{$Interface}{"Signature"};
                                if($NameSpace)
                                {
                                    $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
                                }
                                my $Info = $RunResult{$Interface}{"Info"};
                                my $Test = $RunResult{$Interface}{"Test"};
                                if($Interface=~/\A_Z/)
                                {
                                    if($Signature)
                                    {
                                        $HEADER_LIB_REPORT .= $ContentSpanStart.highLight_Signature_Italic_Color(htmlSpecChars($Signature)).$ContentSpanEnd."<br/>\n$ContentDivStart<span class='mangled'>[ symbol: <b>$Interface</b> ]</span><br/>";
                                    }
                                    else
                                    {
                                        $HEADER_LIB_REPORT .= $ContentSpanStart.$Interface.$ContentSpanEnd."<br/>\n$ContentDivStart";
                                    }
                                }
                                else
                                {
                                    if($Signature)
                                    {
                                        $HEADER_LIB_REPORT .= $ContentSpanStart.highLight_Signature_Italic_Color(htmlSpecChars($Signature)).$ContentSpanEnd."<br/>\n$ContentDivStart";
                                    }
                                    else
                                    {
                                        $HEADER_LIB_REPORT .= $ContentSpanStart.$Interface.$ContentSpanEnd."<br/>\n$ContentDivStart";
                                    }
                                }
                                my $RESULT_INFO = "<table class='test_result' cellpadding='2'><tr><td>".htmlSpecChars($Info)."</td></tr></table>";
                                $HEADER_LIB_REPORT .= $RESULT_INFO.$Test."<br/>".$ContentDivEnd;
                                $HEADER_LIB_REPORT = insertIDs($HEADER_LIB_REPORT);
                                $HEADER_LIB_REPORT = $HEADER_LIB_REPORT;
                                $Problems_Count += 1;
                            }
                        }
                        $PROBLEM_REPORT .= $HEADER_LIB_REPORT;
                    }
                }
            }
            if($PROBLEM_REPORT)
            {
                $PROBLEM_REPORT = "<a name=\'".$ProblemType.(($ProblemValue)?"_".$ProblemValue:"")."\'></a>".$ContentSpanStart_Title.get_problem_title($ProblemType, $ProblemValue)." <span class='ext_title'>(".get_count_title(($Failures_Or_Warning eq "Failures")?"problem":"warning", $Problems_Count).")</span>".$ContentSpanEnd."<br/>\n$ContentDivStart\n".$PROBLEM_REPORT."<a style='font-size:11px;' href='#Top'>to the top</a><br/><br/>\n".$ContentDivEnd;
                $PROBLEM_REPORT = insertIDs($PROBLEM_REPORT);
                $FAILED_TESTS .= $PROBLEM_REPORT;
            }
        }
    }
    if($FAILED_TESTS)
    {
        if($Failures_Or_Warning eq "Failures")
        {
            $FAILED_TESTS = "<a name='Failed_Tests'></a><h2 class='title2'>Failed Tests (".$ResultCounter{"Run"}{"Fail"}.")</h2><hr/>\n"."<!--Failed_Tests-->\n".$FAILED_TESTS."<!--Failed_Tests_End--><br/>\n";
        }
        elsif($Failures_Or_Warning eq "Warnings")
        {
            $FAILED_TESTS = "<a name='Warnings'></a><h2 class='title2'>Warnings (".$ResultCounter{"Run"}{"Warnings"}.")</h2><hr/>\n"."<!--Warnings-->\n".$FAILED_TESTS."<!--Warnings_End--><br/>\n";
        }
    }
    return $FAILED_TESTS;
}

sub composeHTML_Head($$$)
{
    my ($Title, $Keywords, $OtherInHead) = @_;
    return "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n<head>
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />
    <meta name=\"keywords\" content=\"$Keywords\" />
    <title>\n        $Title\n    </title>\n$OtherInHead\n</head>";
}

sub create_Index()
{
    my $CssStyles = "    <style type=\"text/css\">
    body{font-family:Arial}
    hr{color:Black;background-color:Black;height:1px;border:0;}
    h1.title1{margin-bottom:0px;padding-bottom:0px;font-size:26px;}
    h2.title2{margin-bottom:0px;padding-bottom:0px;font-size:20px;}
    span.int{font-weight:bold;cursor:pointer;margin-left:7px;font-size:16px;font-family:Arial;color:#003E69;}
    span.int_p{font-weight:normal;}
    span:hover.int{color:#336699;}
    span.header{color:#cc3300;font-size:14px;font-family:Arial;font-weight:bold;}
    span.namespace_title{margin-left:2px;color:#408080;font-size:13px;font-family:Arial;}
    span.namespace{color:#408080;font-size:13px;font-family:Arial;font-weight:bold;}
    span.solib{color:Green;font-size:14px;font-family:Arial;font-weight:bold;}
    span.libgroup{color:Black;font-size:14px;font-family:Arial;font-weight:bold;}
    span.mangled{padding-left:20px;font-size:13px;cursor:text;color:#444444;}
    span.color_p{font-style:italic;color:Brown;}
    span.focus_p{font-style:italic;color:Red;}
    a.link{text-decoration:none;}</style>";
    my $SuiteHeader = get_TestSuite_Header();
    if(my $SuiteList = get_TestSuite_List())
    {# initialized $STAT_FIRST_LINE variable
        my $Title = "Test suite for the library ".$TargetLibraryName."-".$Descriptor{"Version"}." on ".getArch();
        my $Keywords = "$TargetLibraryName, test, runtime, API";
        writeFile("$TEST_SUITE_PATH/view_tests.html", "<!-- $STAT_FIRST_LINE -->\n".composeHTML_Head($Title, $Keywords, $CssStyles)."\n<body>\n<div><a name='Top'></a>\n$SuiteHeader<br/>\n$SuiteList</div>\n"."<br/><br/>$TOOL_SIGNATURE\n<div style='height:99px;'></div>\n</body></html>");
    }
}

sub get_TestView_Style()
{
    return "table.line_numbers td{background-color:white;padding-right:3px;padding-left:3px;text-align:right;}
    table.code_lines td{padding-left:15px;text-align:left;white-space:nowrap;}
    span.comm{color:#888888;}
    span.str{color:#FF00FF;}
    span.var{color:Black;font-style:italic;}
    span.prepr{color:Green;}
    span.type{color:Brown;}
    span.keyw{font-weight:bold;}
    span.num{color:Blue;}
    span.targ{color:Red;}
    span.color_p{font-style:italic;color:Brown;}
    span.focus_p{font-style:italic;color:Red;}
    table.test_view{cursor:text;margin-top:7px;width:75%;margin-left:20px;font-family:Monaco, \"Courier New\", Courier;font-size:14px;padding:10px;border:1px solid #e0e8e5;color:#444444;background-color:#eff3f2;overflow:auto;}";
}

sub create_HtmlReport()
{
    my $CssStyles = "    <style type=\"text/css\">
    body{font-family:Arial}
    hr{color:Black;background-color:Black;height:1px;border:0;}
    h1.title1{margin-bottom:0px;padding-bottom:0px;font-size:26px;}
    h2.title2{margin-bottom:0px;padding-bottom:0px;font-size:20px;}
    span.section{white-space:normal;font-weight:bold;cursor:pointer;margin-left:7px;font-size:16px;font-family:Arial;color:#003E69;}
    span:hover.section{color:#336699;}
    span.section_title{font-weight:bold;cursor:pointer;font-size:18px;font-family:Arial;color:Black;}
    span:hover.section_title{color:#585858;}
    span.ext{font-weight:100;}
    span.ext_title{font-weight:100;font-size:16px;}
    span.header{color:#cc3300;font-size:14px;font-family:Arial;font-weight:bold;}
    span.namespace_title{margin-left:2px;color:#408080;font-size:13px;font-family:Arial;}
    span.namespace{color:#408080;font-size:13px;font-family:Arial;font-weight:bold;}
    span.solib{color:Green;font-size:14px;font-family:Arial;font-weight:bold;}
    span.libgroup{color:Black;font-size:14px;font-family:Arial;font-weight:bold;}
    span.int_p{font-weight:normal;}
    td.table_header{background-color:#eeeeee;}
    td.summary_item{font-size:15px;font-family:Arial;text-align:left;}
    td.summary_item_value{padding-right:5px;padding-left:5px;text-align:right;font-size:16px;}
    span.mangled{padding-left:20px;font-size:13px;cursor:text;color:#444444;}
    ".get_TestView_Style()."
    table.test_result{margin-top:3px;line-height:16px;margin-left:20px;font-family:Arial;border:0;background-color:#FFE4E1;}</style>";
    my $JScripts = "<script type=\"text/javascript\" language=\"JavaScript\">
    function showContent(header, id)   {
        e = document.getElementById(id);
        if(e.style.display == 'none')
        {
            e.style.display = '';
            e.style.visibility = 'visible';
            header.innerHTML = header.innerHTML.replace(/\\\[[^0-9 ]\\\]/gi,\"[&minus;]\");
        }
        else
        {
            e.style.display = 'none';
            e.style.visibility = 'hidden';
            header.innerHTML = header.innerHTML.replace(/\\\[[^0-9 ]\\\]/gi,\"[+]\");
        }
    }</script>";
    my $Summary = get_Summary();# initialized $STAT_FIRST_LINE variable
    my $Title = "Test results for the library ".$TargetLibraryName."-".$Descriptor{"Version"}." on ".getArch();
    my $Keywords = "$TargetLibraryName, test, runtime, API";
    writeFile("$REPORT_PATH/test_results.html", "<!-- $STAT_FIRST_LINE -->\n".composeHTML_Head($Title, $Keywords, $CssStyles."\n".$JScripts)."\n<body>\n<div><a name='Top'></a>\n".get_Report_Header()."<br/>\n$Summary<br/>\n".get_Problem_Summary()."<br/>\n".get_FailedTests("Failures")."<br/>\n".get_FailedTests("Warnings")."</div>\n"."<br/><br/>$TOOL_SIGNATURE\n<div style='height:999px;'></div>\n</body></html>");
}

sub detect_solib_default_paths()
{
    if($Config{"osname"}=~/\A(freebsd|openbsd|netbsd)\Z/)
    {
        if(my $LdConfig = get_CmdPath("ldconfig"))
        {
            foreach my $Line (split(/\n/, `$LdConfig -r`))
            {
                if($Line=~/\A[ \t]*\d+:\-l(.+) \=\> (.+)\Z/)
                {
                    $SoLib_DefaultPath{"lib".$1} = $2;
                    $DefaultLibPaths{get_Directory($2)} = 1;
                }
            }
        }
        else
        {
            print STDERR "WARNING: can't find ldconfig\n";
        }
    }
    else
    {
        if(my $LdConfig = get_CmdPath("ldconfig"))
        {
            foreach my $Line (split(/\n/, `$LdConfig -p`))
            {
                if($Line=~/\A[ \t]*([^ \t]+) .* \=\> (.+)\Z/)
                {
                    $SoLib_DefaultPath{$1} = $2;
                    $DefaultLibPaths{get_Directory($2)} = 1;
                }
            }
        }
        elsif($Config{"osname"}=~/\A(linux)\Z/)
        {
            print STDERR "WARNING: can't find ldconfig\n";
        }
    }
}

sub detect_include_default_paths()
{
    mkpath(".tmpdir");
    writeFile(".tmpdir/empty.h", "");
    foreach my $Line (split(/\n/, `$GPP_PATH -v -x c++ -E .tmpdir/empty.h 2>&1`))
    {# detecting gcc default include paths
        if($Line=~/\A[ \t]*(\/[^ ]+)[ \t]*\Z/)
        {
            my $Path = $1;
            while($Path=~s&/[^\/]+/\.\./&/&){};
            $Path=~s/[\/]+\Z//g;
            next if($Path eq "/usr/local/include");
            if($Path=~/c\+\+|g\+\+/)
            {
                $DefaultCppPaths{$Path}=1;
                $MAIN_CPP_DIR = $Path if(not defined $MAIN_CPP_DIR or get_depth($MAIN_CPP_DIR)>get_depth($Path));
            }
            elsif($Path=~/gcc/)
            {
                $DefaultGccPaths{$Path}=1;
            }
            else
            {
                $DefaultIncPaths{$Path}=1;
            }
        }
    }
    rmtree(".tmpdir");
}

sub detect_bin_default_paths()
{
    my $EnvPaths = $ENV{"PATH"};
    if($Config{"osname"}=~/\A(haiku|beos)\Z/)
    {
        $EnvPaths.=":".$ENV{"BETOOLS"};
    }
    foreach my $Path (sort {length($a)<=>length($b)} split(/:|;/, $EnvPaths))
    {
        if($Path ne "/")
        {
            $Path=~s/[\/]+\Z//g;
            next if(not $Path);
        }
        $DefaultBinPaths{$Path} = 1;
    }
}

sub getSymbols()
{
    print "\rshared object(s) analysis: [10.00%]";
    my @SoLibPaths = getSoPaths();
    print "\rshared object(s) analysis: [20.00%]";
    if($#SoLibPaths==-1)
    {
        print STDERR "\nERROR: shared objects were not found\n";
        exit(1);
    }
    foreach my $SoLibPath (sort {length($a)<=>length($b)} @SoLibPaths)
    {
        $SharedObjects{$SoLibPath} = 1;
        getSymbols_Lib($SoLibPath);
    }
    foreach my $SharedObject (keys(%SharedObject_UndefinedSymbols))
    {# checking dependencies
        foreach my $Symbol (keys(%{$SharedObject_UndefinedSymbols{$SharedObject}}))
        {
            if((not $NeededInterfaceVersion_Library{$Symbol}
            and not $InterfaceVersion_Library{$Symbol}) or $SharedObjects{$SharedObject})
            {
                foreach my $SoPath (find_symbol_libs($Symbol))
                {
                    $SystemObjects_Needed{$SharedObject}{$SoPath} = 1;
                }
            }
        }
    }
}

my %Symbol_Prefix_Lib=(
# symbols for autodetecting library dependencies
"pthread_" => ["libpthread.so"],
"g_" => ["libglib-2.0.so", "libgobject-2.0.so", "libgio-2.0.so"],
"cairo_" => ["libcairo.so"],
"gtk_" => ["libgtk-x11-2.0.so"],
"atk_" => ["libatk-1.0.so"],
"gdk_" => ["libgdk-x11-2.0.so"],
"gl[A-Z][a-z]" => ["libGL.so"],
"glu[A-Z][a-z]" => ["libGLU.so"],
"popt[A-Z][a-z]" => ["libpopt.so"],
"Py[A-Z][a-z]" => ["libpython"],
"jpeg_" => ["libjpeg.so"],
"BZ2_" => ["libbz2.so"],
"Fc[A-Z][a-z]" => ["libfontconfig.so"],
"Xft[A-Z][a-z]" => ["libXft.so"],
"SSL_" => ["libssl.so"],
"sem_" => ["libpthread.so"],
"art_" => ["libart_lgpl_2.so"]
);

my %Symbol_Lib=(
# symbols for autodetecting library dependencies
"pow" => ["libm.so"],
"sin" => ["libm.so"],
"floor" => ["libm.so"],
"cos" => ["libm.so"],
"dlopen" => ["libdl.so"],
"deflate" => ["libz.so"],
"inflate" => ["libz.so"]
);

sub find_symbol_libs($)
{# FIXME: needed more appropriate patterns for symbols
    my $Symbol = $_[0];
    return () if(not $Symbol);
    if($InterfaceVersion_Library{$Symbol}) {
        return ($InterfaceVersion_Library{$Symbol});
    }
    return () if($Symbol=~/\Ag_/ and $Symbol=~/[A-Z]/);
    my %LibPaths = ();
    foreach my $Prefix (keys(%Symbol_Prefix_Lib))
    {
        if($Symbol=~/\A$Prefix/)
        {
            foreach my $SoName (@{$Symbol_Prefix_Lib{$Prefix}})
            {
                if(my $SoPath = find_solib_path($SoName))
                {
                    $LibPaths{$SoPath} = 1;
                }
            }
        }
    }
    foreach my $LibSymbol (keys(%Symbol_Lib))
    {
        if($Symbol eq $LibSymbol)
        {
            foreach my $SoName (@{$Symbol_Lib{$LibSymbol}})
            {
                if(my $SoPath = find_solib_path($SoName))
                {
                    $LibPaths{$SoPath} = 1;
                }
            }
        }
    }
    if(my $Prefix = getPrefix($Symbol))
    {# try to find library by symbol prefix
        $Prefix=~s/[_]+\Z//g;
        if($Prefix eq "inotify" and
        get_symbol_version($Symbol)=~/GLIBC/)
        {
            if(my $SoPath = find_solib_path("libc.so"))
            {
                $LibPaths{$SoPath} = 1;
            }
        }
        else
        {
            if(my $SoPath = find_solib_path_by_prefix($Prefix))
            {
                $LibPaths{$SoPath} = 1;
            }
        }
    }
    my @Paths = keys(%LibPaths);
    return @Paths;
}

sub find_solib_path_by_prefix($)
{
    my $Prefix = $_[0];
    return "" if(not $Prefix);
    if(my $SoPath = find_solib_path("lib$Prefix-2.so"))
    {# libgnome-2.so
        return $SoPath;
    }
    elsif(my $SoPath = find_solib_path("lib$Prefix"."2.so"))
    {# libxml2.so
        return $SoPath;
    }
    elsif(my $SoPath = find_solib_path("lib$Prefix.so"))
    {
        return $SoPath;
    }
    elsif(my $SoPath = find_solib_path("lib$Prefix-1.so"))
    {# libgsf-1.so
        return $SoPath;
    }
    return "";
}

sub get_symbol_version($)
{
    if($_[0]=~/[\@]+(.+)\Z/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getSoPaths()
{
    my @SoPaths = ();
    foreach my $Dest (split(/\n/, $Descriptor{"Libs"}))
    {
        $Dest=~s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        if(my $RelDir = $RelativeDirectory){
            $Dest =~ s/{RELPATH}/$RelDir/g;
        }
        else{
            $Dest =~ s/{RELPATH}//g;
        }
        if(not -e $Dest)
        {
            print STDERR "\nERROR: can't access \'$Dest\'\n";
            next;
        }
        $Dest = abs_path($Dest) if($Dest!~/\A\//);
        my @SoPaths_Dest = getSOPaths_Dest($Dest);
        foreach (@SoPaths_Dest)
        {
            push(@SoPaths,$_);
        }
    }
    return @SoPaths;
}

sub getSOPaths_Dest($)
{
    my $Dest = $_[0];
    if(-f $Dest)
    {
        $SharedObject_Path{get_FileName($Dest)} = $Dest;
        return ($Dest);
    }
    elsif(-d $Dest)
    {
        $Dest=~s/[\/]+\Z//g;
        my @AllObjects = ();
        if($SystemPaths{"lib"}{$Dest})
        {
            my $CmdFind = "find $Dest -iname \"*".esc($TargetLibraryName)."*\.so*\"";
            foreach my $Path (split(/\n/, `$CmdFind`))
            {# all files and symlinks that match the name of library
                if(get_FileName($Path)=~/\A(|lib)\Q$TargetLibraryName\E[\d\-]*\.so[\d\.]*\Z/)
                {
                    push(@AllObjects, $Path);
                }
            }
        }
        else
        {# all files and symlinks
            foreach my $Path (cmd_find($Dest,"","*\.so*"))
            {
                next if(ignore_path($Dest, $Path));
                if(get_FileName($Path)=~/\A.+\.so[\d\.]*\Z/)
                {
                    push(@AllObjects, $Path);
                }
            }
        }
        my %SOPaths = ();
        foreach my $Path (@AllObjects)
        {
            $SharedObject_Path{get_FileName($Path)} = $Path;
            if(my $ResolvedPath = resolve_symlink($Path))
            {
                $SOPaths{$ResolvedPath}=1;
            }
        }
        my @Paths = keys(%SOPaths);
        return @Paths;
    }
    else
    {
        return ();
    }
}

sub read_symlink($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -f $Path);
    return $Cache{"read_symlink"}{$Path} if(defined $Cache{"read_symlink"}{$Path});
    if(my $ReadlinkCmd = get_CmdPath("readlink"))
    {
        my $Res = `$ReadlinkCmd -n $Path`;
        $Cache{"read_symlink"}{$Path} = $Res;
        return $Res;
    }
    elsif(my $FileCmd = get_CmdPath("file"))
    {
        my $Info = `$FileCmd $Path`;
        if($Info=~/symbolic\s+link\sto\s['`"]*([\w\d\.\-\/]+)['`"]*/i)
        {
            $Cache{"read_symlink"}{$Path} = $1;
            return $Cache{"read_symlink"}{$Path};
        }
    }
    $Cache{"read_symlink"}{$Path} = "";
    return "";
}

sub resolve_symlink($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -f $Path);
    return $Path if(isCyclical(\@RecurSymlink, $Path));
    push(@RecurSymlink, $Path);
    if(-l $Path and my $Redirect=read_symlink($Path))
    {
        if($Redirect=~/\A\//)
        {
            my $Res = resolve_symlink($Redirect);
            pop(@RecurSymlink);
            return $Res;
        }
        elsif($Redirect=~/\.\.\//)
        {
            $Redirect = get_Directory($Path)."/".$Redirect;
            while($Redirect=~s&/[^\/]+/\.\./&/&){};
            my $Res = resolve_symlink($Redirect);
            pop(@RecurSymlink);
            return $Res;
        }
        elsif(-f get_Directory($Path)."/".$Redirect)
        {
            my $Res = resolve_symlink(get_Directory($Path)."/".$Redirect);
            pop(@RecurSymlink);
            return $Res;
        }
        else
        {
            return $Path;
        }
    }
    else
    {
        pop(@RecurSymlink);
        return $Path;
    }
}

sub get_ShortName($)
{
    my $MnglName = $_[0];
    return $MnglName if($MnglName!~/\A(_Z[A-Z]*)(\d+)/);
    my $Prefix = $1;
    my $Length = $2;
    return substr($MnglName, length($Prefix)+length($Length), $Length);
}

sub readlile_ELF($)
{
    if($_[0]=~/\s*\d+:\s+(\w*)\s+\w+\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s((\w|@|\.)+)/)
    {#the line of 'readelf' output corresponding to the interface
        my ($value, $type, $bind, $vis, $Ndx, $fullname)=($1, $2, $3, $4, $5, $6);
        if( ($type eq "FUNC") and ($bind eq "LOCAL") ) {
            my ($realname, $version) = get_symbol_name_version($fullname);
            $InternalInterfaces{$realname} = 1 if(not defined $InternalInterfaces{$realname});
        }
        if(($bind ne "WEAK") and ($bind ne "GLOBAL")) {
            return ();
        }
        if(($type ne "FUNC") and ($type ne "OBJECT") and ($type ne "COMMON")
        and ($type ne "NOTYPE")) {
            return ();
        }
        if($vis ne "DEFAULT") {
            return ();
        }
        if(($Ndx eq "ABS") and ($value!~/\D|1|2|3|4|5|6|7|8|9/)) {
            return ();
        }
        return ($fullname, $value, $Ndx, $type);
    }
    else
    {
        return ();
    }
}

sub get_symbol_name_version($)
{
    my $fullname = $_[0];
    my ($realname, $version) = ($fullname, "");
    if($fullname=~/\A([^@]+)[\@]+([^@]+)\Z/)
    {
        ($realname, $version) = ($1, $2);
    }
    return ($realname, $version);
}

sub getSymbols_Lib($$)
{
    my ($Lib_Path, $IsNeededLib) = @_;
    return if(not $Lib_Path or not -f $Lib_Path);
    my ($Lib_Dir, $Lib_SoName) = separatePath($Lib_Path);
    return if($CheckedSoLib{$Lib_SoName} and $IsNeededLib);
    return if(isCyclical(\@RecurLib, $Lib_SoName));# or $#RecurLib>=5
    $CheckedSoLib{$Lib_SoName} = 1;
    push(@RecurLib, $Lib_SoName);
    my %NeededLib = ();
    $STDCXX_TESTING = 1 if($Lib_SoName=~/\Alibstdc\+\+\.so/ and not $IsNeededLib);
    $GLIBC_TESTING = 1 if($Lib_SoName=~/\Alibc.so/ and not $IsNeededLib);
    my $ReadelfCmd = get_CmdPath("readelf");
    if(not $ReadelfCmd)
    {
        print STDERR "ERROR: can't find readelf\n";
        exit(1);
    }
    open(SOLIB, "$ReadelfCmd -WhlSsdA $Lib_Path 2>&1 |");
    my $symtab=0; # indicates that we are processing 'symtab' section of 'readelf' output
    while(<SOLIB>)
    {
        if( /'.dynsym'/ ) {
            $symtab=0;
        }
        elsif( /'.symtab'/ ) {
            $symtab=1;
        }
        elsif(/NEEDED.+\[([^\[\]]+)\]/)
        {
            $NeededLib{$1} = 1;
        }
        elsif(my ($fullname, $idx, $Ndx, $type) = readlile_ELF($_)) {
            my ($realname, $version) = get_symbol_name_version($fullname);
            $fullname=~s/\@\@/\@/g;
            if( $Ndx eq "UND" ) {
                # ignore interfaces that are exported form somewhere else
                $SharedObject_UndefinedSymbols{$Lib_Path}{$fullname} = 1 if(not $IsNeededLib);
                next;
            }
            if($type eq "NOTYPE")
            {
                next;
            }
            if( $symtab == 1 ) {
                # do nothing with symtab (but there are some plans for the future)
                next;
            }
            $InternalInterfaces{$realname} = 0;
            if(not $SoLib_IntPrefix{$Lib_SoName})
            {# collecting prefixes
                $SoLib_IntPrefix{$Lib_SoName} = get_int_prefix($realname);
            }
            if($IsNeededLib)
            {
                if($SharedObject_Path{get_FileName($Lib_Path)} eq $Lib_Path)
                {# other shared objects in the same package
                    if(not $NeededInterface_Library{$realname})
                    {
                        $NeededInterface_Library{$realname} = $Lib_Path;
                        $NeededInterfaceVersion_Library{$fullname} = $Lib_Path;
                    }
                }
            }
            else
            {
                if(not $Interface_Library{$realname})
                {
                    $Interface_Library{$realname} = $Lib_Path;
                    $InterfaceVersion_Library{$fullname} = $Lib_Path;
                }
            }
            if(not $Language{$Lib_SoName})
            {
                if($realname=~/\A_Z[A-Z]*\d+/)
                {
                    $Language{$Lib_SoName} = "C++";
                    if(not $IsNeededLib)
                    {
                        $COMMON_LANGUAGE = "C++";
                    }
                }
            }
        }
    }
    close(SOLIB);
    if(not $Language{$Lib_SoName})
    {
        $Language{$Lib_SoName} = "C";
    }
    foreach my $SoLib (sort {length($a)<=>length($b)} keys(%NeededLib))
    {
        my $DepPath = find_solib_path($SoLib);
        if($DepPath and -f $DepPath)
        {
            getSymbols_Lib($DepPath, 1);
        }
    }
    pop(@RecurLib);
}

sub find_solib_path($)
{
    my $SoName = $_[0];
    return "" if(not $SoName);
    return $Cache{"find_solib_path"}{$SoName} if(defined $Cache{"find_solib_path"}{$SoName});
    if(my $Path = $SharedObject_Path{$SoName})
    {
        $Cache{"find_solib_path"}{$SoName} = $Path;
        return $Path;
    }
    elsif(my $DefaultPath = $SoLib_DefaultPath{$SoName})
    {
        $Cache{"find_solib_path"}{$SoName} = $DefaultPath;
        return $DefaultPath;
    }
    else
    {
        foreach my $Dir (keys(%DefaultLibPaths), keys(%{$SystemPaths{"lib"}}))
        {#search in default linker paths and then in the all system paths
            if(-f $Dir."/".$SoName)
            {
                $Cache{"find_solib_path"}{$SoName} = $Dir."/".$SoName;
                return $Dir."/".$SoName;
            }
        }
        detectSystemObjects() if(not keys(%SystemObjects));
        if(my @AllObjects = keys(%{$SystemObjects{$SoName}}))
        {
            $Cache{"find_solib_path"}{$SoName} = $AllObjects[0];
            return $AllObjects[0];
        }
        $Cache{"find_solib_path"}{$SoName} = "";
        return "";
    }
}

sub cmd_file($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    if(my $CmdPath = get_CmdPath("file"))
    {
        my $Cmd = $CmdPath." ".esc($Path);
        my $Cmd_Out = `$Cmd`;
        return $Cmd_Out;
    }
    else
    {
        return "";
    }
}

sub cmd_preprocessor($$$$)
{
    my ($Path, $AddOpt, $Lang, $Grep) = @_;
    return "" if(not $Path or not -f $Path);
    my $Header_Depend="";
    foreach my $Dep (get_HeaderDeps($Path))
    {
        $Header_Depend .= " -I".esc($Dep);
    }
    my $GccCall = ($Lang eq "C++")?"$GPP_PATH -x c++-header":"$GCC_PATH -x c-header";
    my $Cmd = "$GccCall -dD -E ".esc($Path)." 2>/dev/null $CompilerOptions_Headers $Header_Depend $AddOpt";
    if($Grep)
    {
        $Cmd .= " | grep \"$Grep\"";
    }
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub cmd_cat($$)
{
    my ($Path, $Grep) = @_;
    return "" if(not $Path or not -e $Path);
    my $Cmd = "cat ".esc($Path);
    if($Grep)
    {
        $Cmd .= " | grep -e \'$Grep\'";
    }
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub cmd_find($$$)
{
    my ($Path, $Type, $Name) = @_;
    return () if(not $Path or not -e $Path);
    my $Cmd = "find ".esc(abs_path($Path));
    if($Type)
    {
        $Cmd .= " -type $Type";
    }
    if($Name)
    {
        $Cmd .= " -name \"$Name\"";
    }
    return split(/\n/, `$Cmd`);
}

sub cmd_tar($)
{
    my $Path = $_[0];
    return if(not $Path or not -e $Path);
    my $Cmd = "tar -xvzf ".esc($Path);
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub is_header_ext($)
{
    my $Header = $_[0];
    return ($Header=~/\.(h|hh|H|hp|hxx|hpp|HPP|h\+\+|tcc)\Z/);
}

sub is_header($$)
{
    my ($Header, $UserDefined) = @_;
    return "" if(-d $Header);
    return "" if($Header=~/\.\w+\Z/i and not is_header_ext($Header) and not $UserDefined);#cpp|c|gch|tu|fs|pas
    if($Header=~/\A\//)
    {
        if(-f $Header and (is_header_ext($Header)
        or $UserDefined or cmd_file($Header)=~/:[ ]*ASCII C[\+]* program text/))
        {
            return $Header;
        }
        else
        {
            return "";
        }
    }
    elsif(-f $Header)
    {
        if(is_header_ext($Header) or $UserDefined
        or cmd_file($Header)=~/:[ ]*ASCII C[\+]* program text/)
        {
            return abs_path($Header);
        }
        else
        {
            return "";
        }
    }
    else
    {
        my ($Header_Inc, $Header_Path) = identify_header($Header);
        if($Header_Path and (is_header_ext($Header)
        or $UserDefined or cmd_file($Header_Path)=~/:[ ]*ASCII C[\+]* program text/))
        {
            return $Header_Path;
        }
        else
        {
            return "";
        }
    }
}

sub get_FileName($)
{
    my $Path = $_[0];
    if($Path=~/\A(.*\/)([^\/]*)\Z/)
    {
        return $2;
    }
    else
    {
        return $Path;
    }
}

sub get_Directory($)
{
    my $Path = $_[0];
    return "" if($Path=~m*\A\./*);
    if($Path=~/\A(.*)[\/]+([^\/]*)\Z/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub separatePath($)
{
    my $Path = $_[0];
    return (get_Directory($Path), get_FileName($Path));
}

sub find_in_dependencies($)
{
    my $Header = $_[0];
    return "" if(not $Header);
    return $Cache{"find_in_dependencies"}{$Header} if(defined $Cache{"find_in_dependencies"}{$Header});
    foreach my $Dependency (sort {get_depth($a)<=>get_depth($b)} keys(%Header_Dependency))
    {
        next if(not $Dependency);
        if(-f $Dependency."/".$Header)
        {
            $Dependency=~s/\/\Z//g;
            $Cache{"find_in_dependencies"}{$Header} = $Dependency;
            return $Dependency;
        }
    }
    return "";
}

sub find_in_defaults($)
{
    my $Header = $_[0];
    return "" if(not $Header);
    foreach my $DefaultPath (sort {get_depth($a)<=>get_depth($b)} keys(%DefaultIncPaths))
    {
        next if(not $DefaultPath);
        if(-f $DefaultPath."/".$Header)
        {
            return $DefaultPath;
        }
    }
    return "";
}

sub search_in_dirs($$)
{
    my ($Header, $Deps) = @_;
    return "" if(not $Header or not $Deps);
    foreach my $Dependency (sort {length($a)<=>length($b)} @{$Deps})
    {
        next if(not $Dependency);
        if(-f $Dependency."/".$Header)
        {
            $Dependency=~s/\/\Z//g;
            return $Dependency;
        }
    }
    return "";
}

sub register_header($$)
{#input: header absolute path, relative path or name
    my ($Header, $Position) = @_;
    return if(not $Header);
    if($Header=~/\A\// and not -f $Header)
    {
        print STDERR "\nERROR: can't access \'$Header\'\n";
        return;
    }
    my $Header_Name = get_FileName($Header);
    if($SkipHeaders{$Header_Name})
    {
        return;
    }
    foreach my $Pattern (keys(%SkipHeaders_Pattern))
    {
        return if($Header_Name=~/$Pattern/);
        return if($Pattern=~/\// and $Header=~/$Pattern/);
    }
    my ($Header_Inc, $Header_Path) = identify_header($Header);
    return if(not $Header_Path);
    if(my $RHeader_Path = $Header_ErrorRedirect{$Header_Path}{"Path"})
    {
        $Header_Path = $RHeader_Path;
        return if($RegisteredHeaders{$Header_Path});
    }
    elsif($Header_ShouldNotBeUsed{$Header_Path})
    {
        return;
    }
    $Headers{$Header_Path}{"Name"} = $Header_Name;
    $Headers{$Header_Path}{"Position"} = $Position;
    $RegisteredHeaders{$Header_Path} = 1;
    $RegisteredHeaders_Short{get_FileName($Header_Path)} = 1;
}

sub parse_redirect($$)
{
    my ($Content, $Path) = @_;
    return $Cache{"parse_redirect"}{$Path} if(defined $Cache{"parse_redirect"}{$Path});
    return "" if(not $Content);
    my @ErrorMacros = ();
    while($Content=~s/#\s*error\s+([^\n]+?)\s*(\n|\Z)//)
    {
        push(@ErrorMacros, $1);
    }
    my $Redirect = "";
    foreach my $ErrorMacro (@ErrorMacros)
    {
        if($ErrorMacro=~/(only|must\s+include|update\s+to\s+include|replaced\s+with|replaced\s+by|renamed\s+to|is\s+in)\s+(<[^<>]+>|[a-z0-9-_\\\/]+\.(h|hh|hp|hxx|hpp|h\+\+|tcc))/i)
        {
            $Redirect = $2;
            last;
        }
        elsif($ErrorMacro=~/(include|use|is\s+in)\s+(<[^<>]+>|[a-z0-9-_\\\/]+\.(h|hh|hp|hxx|hpp|h\+\+|tcc))\s+instead/i)
        {
            $Redirect = $2;
            last;
        }
        elsif($ErrorMacro=~/this\s+header\s+should\s+not\s+be\s+used/i
        or $ErrorMacro=~/programs\s+should\s+not\s+directly\s+include/i
        or $ErrorMacro=~/you\s+should\s+not\s+include/i
        or $ErrorMacro=~/you\s+should\s+not\s+be\s+including\s+this\s+file/i
        or $ErrorMacro=~/you\s+should\s+not\s+be\s+using\s+this\s+header/i
        or $ErrorMacro=~/is\s+not\s+supported\s+API\s+for\s+general\s+use/i)
        {
            $Header_ShouldNotBeUsed{$Path} = 1;
        }
    }
    $Redirect=~s/\A<//g;
    $Redirect=~s/>\Z//g;
    $Cache{"parse_redirect"}{$Path} = $Redirect;
    return $Redirect;
}

sub parse_preamble_include($)
{
    my $Content = $_[0];
    my @ErrorMacros = ();
    while($Content=~s/#\s*error\s+([^\n]+?)\s*(\n|\Z)//)
    {
        push(@ErrorMacros, $1);
    }
    my $Redirect = "";
    foreach my $ErrorMacro (@ErrorMacros)
    {
        if($ErrorMacro=~/(<[^<>]+>|[a-z0-9-_\\\/]+\.(h|hh|hp|hxx|hpp|h\+\+|tcc))\s+(must\s+be\s+included\s+before|has\s+to\s+be\s+included\s+before|hasn't\s+been\sincluded\syet)/i)
        {
            $Redirect = $1;
            last;
        }
        elsif($ErrorMacro=~/include\s+(<[^<>]+>|[a-z0-9-_\\\/]+\.(h|hh|hp|hxx|hpp|h\+\+|tcc))\s+before/i)
        {
            $Redirect = $1;
            last;
        }
        else
        {
            return "";
        }
    }
    if($Redirect)
    {
        $Redirect=~s/\A<//g;
        $Redirect=~s/>\Z//g;
        return $Redirect;
    }
    else
    {
        return "";
    }
}

sub parse_includes($)
{
    my $Content = $_[0];
    my %Includes = ();
    while($Content=~s/#([ \t]*)include([ \t]*)(<|")([^<>"]+)(>|")//)
    {
        $Includes{$4} = 1;
    }
    my @Includes_Arr = sort keys(%Includes);
    return @Includes_Arr;
}

sub detect_header_includes(@)
{
    my @HeaderPaths = @_;
    foreach my $Path (@HeaderPaths)
    {
        next if($Cache{"detect_header_includes"}{$Path});
        next if(not -f $Path);
        my $Content = readFile($Path);
        if($Content=~/#[ \t]*error[ \t]+/ and (my $Redirect = parse_redirect($Content, $Path)))
        {#detecting error directive in the headers
            if(my ($RInc, $RPath) = identify_header($Redirect))
            {
                $Header_ErrorRedirect{$Path}{"Inc"} = $RInc;
                $Header_ErrorRedirect{$Path}{"Path"} = $RPath;
            }
        }
        foreach my $Include (parse_includes($Content))
        {#detecting includes
            $Header_Includes{$Path}{$Include} = 1;
            if(my $Prefix = get_Directory($Include))
            {
                $Header_Prefix{get_FileName($Include)} = $Prefix;
            }
        }
        $Cache{"detect_header_includes"}{$Path} = 1;
    }
}

sub register_directory($)
{
    my $Dir = $_[0];
    return if(not $Dir or not -d $Dir);
    $Dir = abs_path($Dir) if($Dir!~/\A\//);
    if(not $RegisteredDirs{$Dir})
    {
        foreach my $Path (cmd_find($Dir,"f",""))
        {
            next if(ignore_path($Dir, $Path));
            $DependencyHeaders_All_FullPath{get_FileName($Path)} = $Path;
            $DependencyHeaders_All{get_FileName($Path)} = $Path;
        }
        $RegisteredDirs{$Dir} = 1;
        if(get_FileName($Dir) eq "include")
        {# search for lib/include directory
            my $LibDir = $Dir;
            if($LibDir=~s/\/include\Z/\/lib/g and -d $LibDir)
            {
                foreach my $Path (cmd_find($LibDir, "f", "*\.h"))
                {
                    next if(ignore_path($LibDir, $Path));
                    $Header_Dependency{get_Directory($Path)} = 1;
                    $DependencyHeaders_All_FullPath{get_FileName($Path)} = $Path;
                    $DependencyHeaders_All{get_FileName($Path)} = $Path;
                }
            }
        }
    }
}

sub ignore_path($$)
{
    my ($Prefix, $Path) = @_;
    return 1 if(not $Path or not -e $Path
      or not $Prefix or not -e $Prefix);
    return 1 if($Path=~/\~\Z/);# skipping system backup files
    # skipping hidden .svn, .git, .bzr, .hg and CVS directories
    return 1 if(cut_path_prefix($Path, $Prefix)=~/(\A|[\/]+)(\.(svn|git|bzr|hg)|CVS)([\/]+|\Z)/);
    return 0;
}

sub natural_header_sorting($$)
{
    my ($H1, $H2) = @_;
    $H1=~s/\.[a-z]+\Z//ig;
    $H2=~s/\.[a-z]+\Z//ig;
    if($H1 eq $H2)
    {
        return 0;
    }
    elsif($H1=~/\A$H2/)
    {
        return 1;
    }
    elsif($H2=~/\A$H1/)
    {
        return -1;
    }
    elsif($H1=~/config/i
    and $H2!~/config/i)
    {
        return -1;
    }
    elsif($H2=~/config/i
    and $H1!~/config/i)
    {
        return 1;
    }
    else
    {
        return (lc($H1) cmp lc($H2));
    }
}

sub searchForHeaders()
{
    # detecting system header paths
    foreach my $Path (sort {get_depth($b) <=> get_depth($a)} keys(%DefaultGccPaths))
    {
        foreach my $HeaderPath (sort {get_depth($a) <=> get_depth($b)} cmd_find($Path,"f",""))
        {
            my $FileName = get_FileName($HeaderPath);
            next if($DefaultGccHeader{$FileName}{"Path"});
            $DefaultGccHeader{$FileName}{"Path"} = $HeaderPath;
            $DefaultGccHeader{$FileName}{"Inc"} = cut_path_prefix($HeaderPath, $Path);
        }
    }
    if(($COMMON_LANGUAGE eq "C++" or $NoLibs) and not $STDCXX_TESTING)
    {
        foreach my $CppDir (sort {get_depth($b) <=> get_depth($a)} keys(%DefaultCppPaths))
        {
            my @AllCppHeaders = cmd_find($CppDir,"f","");
            foreach my $Path (sort {get_depth($a) <=> get_depth($b)} @AllCppHeaders)
            {
                my $FileName = get_FileName($Path);
                next if($DefaultCppHeader{$FileName}{"Path"});
                $DefaultCppHeader{$FileName}{"Path"} = $Path;
                $DefaultCppHeader{$FileName}{"Inc"} = cut_path_prefix($DefaultCppHeader{$FileName}{"Inc"}, $CppDir);
            }
            detect_header_includes(@AllCppHeaders);
        }
    }
    # detecting library header paths
    foreach my $Dest (keys(%{$Descriptor{"IncludePaths"}}),
    keys(%{$Descriptor{"AddIncludePaths"}}))
    {
        my $IDest = $Dest;
        if(my $RelDir = $RelativeDirectory){
            $Dest =~ s/{RELPATH}/$RelDir/g;
        }
        else{
            $Dest =~ s/{RELPATH}//g;
        }
        if(not -e $Dest)
        {
            print STDERR "\nERROR: can't access \'$Dest\'\n";
        }
        elsif(-f $Dest)
        {
            print STDERR "\nERROR: \'$Dest\' - not a directory\n";
        }
        elsif(-d $Dest)
        {
            $Dest = abs_path($Dest) if($Dest!~/\A\//);
            $Header_Dependency{$Dest} = 1;
            foreach my $Path (sort {length($b)<=>length($a)} cmd_find($Dest,"f",""))
            {
                next if(ignore_path($Dest, $Path));
                my $Header = get_FileName($Path);
                $DependencyHeaders_All{$Header} = $Path;
                $DependencyHeaders_All_FullPath{$Header} = $Path;
            }
            if($Descriptor{"AddIncludePaths"}{$IDest})
            {
                $Add_Include_Paths{$Dest} = 1;
            }
            else
            {
                $Include_Paths{$Dest} = 1;
            }
        }
    }
    # registering directories
    foreach my $Dest (split(/\n/, $Descriptor{"Headers"}))
    {
        $Dest=~s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        if(my $RelDir = $RelativeDirectory){
            $Dest =~ s/{RELPATH}/$RelDir/g;
        }
        else{
            $Dest =~ s/{RELPATH}//g;
        }
        if(-d $Dest)
        {
            foreach my $Dir (cmd_find($Dest,"d",""))
            {
                $Header_Dependency{$Dir} = 1;
            }
            register_directory($Dest);
        }
        elsif(-f $Dest)
        {
            $Dest = abs_path($Dest) if($Dest!~/\A\//);
            my $Dir = get_Directory($Dest);
            if(not $SystemPaths{"include"}{$Dir}
            and $Dir ne "/usr/local/include"
            and $Dir ne "/usr/local")
            {
                $Header_Dependency{$Dir} = 1;
                register_directory($Dir);
                if(my $OutDir = get_Directory($Dir))
                {
                    if(not $SystemPaths{"include"}{$Dir}
                    and $OutDir ne "/usr/local/include"
                    and $OutDir ne "/usr/local")
                    {
                        $Header_Dependency{$OutDir} = 1;
                        register_directory($OutDir);
                    }
                }
            }
        }
    }
    foreach my $Prefix (sort {get_depth($b) <=> get_depth($a)} keys(%Header_Dependency))
    {#removing default prefixes in DependencyHeaders_All
        if($Prefix=~/\A\//)
        {
            foreach my $Header (keys(%DependencyHeaders_All))
            {
                $DependencyHeaders_All{$Header} = cut_path_prefix($DependencyHeaders_All{$Header}, $Prefix);
            }
        }
    }
    # detecting library header includes
    detect_header_includes(values(%DependencyHeaders_All_FullPath));
    foreach my $Dir (keys(%DefaultIncPaths))
    {
        if(-d $Dir."/bits")
        {
            detect_header_includes(cmd_find($Dir."/bits","f",""));
            last;
        }
    }
    # recursive redirects
    foreach my $Path (keys(%Header_ErrorRedirect))
    {
        my $RedirectInc = $Header_ErrorRedirect{$Path}{"Inc"};
        my $RedirectPath = $Header_ErrorRedirect{$Path}{"Path"};
        if($Path eq $RedirectPath)
        {
            delete($Header_ErrorRedirect{$Path});
        }
        if(my $RecurRedirectPath = $Header_ErrorRedirect{$RedirectPath}{"Path"})
        {
            if($Path ne $RecurRedirectPath)
            {
                $Header_ErrorRedirect{$Path}{"Inc"} = $Header_ErrorRedirect{$RedirectPath}{"Inc"};
                $Header_ErrorRedirect{$Path}{"Path"} = $RecurRedirectPath;
            }
        }
    }
    # registering headers
    my $Position = 0;
    foreach my $Dest (split(/\n/, $Descriptor{"Headers"}))
    {
        $Dest=~s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        if(my $RelDir = $RelativeDirectory){
            $Dest =~ s/{RELPATH}/$RelDir/g;
        }
        else{
            $Dest =~ s/{RELPATH}//g;
        }
        if($Dest=~/\A\// and not -e $Dest)
        {
            print STDERR "\nERROR: can't access \'$Dest\'\n";
            next;
        }
        if(is_header($Dest, 1))
        {
            register_header($Dest, $Position);
            $Position += 1;
        }
        elsif(-d $Dest)
        {
            foreach my $Path (sort {natural_header_sorting($a, $b)} cmd_find($Dest,"f",""))
            {
                next if(ignore_path($Dest, $Path));
                next if(not is_header($Path, 0));
                $IsHeaderListSpecified = 0;
                register_header($Path, $Position);
                $Position += 1;
            }
        }
        else
        {
            print STDERR "\nERROR: can't identify \'$Dest\' as a header file\n";
        }
    }
    #preparing preamble headers
    my $Preamble_Position=0;
    foreach my $Header (split(/\n/, $Descriptor{"IncludePreamble"}))
    {
        $Header=~s/\A\s+|\s+\Z//g;
        next if(not $Header);
        if($Header=~/\A\// and not -f $Header)
        {
            print STDERR "\nERROR: can't access file \'$Header\'\n";
            next;
        }
        if(my $Header_Path = is_header($Header, 1))
        {
            $Include_Preamble{$Header_Path}{"Position"} = $Preamble_Position;
            $Preamble_Position+=1;
        }
        else
        {
            print STDERR "\nERROR: can't identify \'$Header\' as a header file\n";
        }
    }
    foreach my $Path (keys(%RegisteredHeaders))
    {
        set_header_namespaces($Path);
    }
    detect_header_includes(keys(%RegisteredHeaders), keys(%Include_Preamble));
    foreach my $Path (keys(%Header_Includes))
    {
        next if(not $RegisteredHeaders{$Path});
        if(keys(%{$Header_Includes{$Path}})>$Header_MaxIncludes
        or not defined $Header_MaxIncludes)
        {
            $Header_MaxIncludes = keys(%{$Header_Includes{$Path}});
        }
    }
    foreach my $AbsPath (keys(%Header_Includes))
    {
        detect_recursive_includes($AbsPath);
    }
    foreach my $AbsPath (keys(%RecursiveIncludes_Inverse))
    {
        detect_top_header($AbsPath);
    }
    foreach my $HeaderName (keys(%Include_Order))
    {# ordering headers according to descriptor
        my $PairName=$Include_Order{$HeaderName};
        my ($Pos, $PairPos)=(-1, -1);
        my ($Path, $PairPath)=();
        foreach my $Header_Path (sort {int($Headers{$a}{"Position"})<=>int($Headers{$b}{"Position"})} keys(%Headers))  {
            if(get_FileName($Header_Path) eq $PairName)
            {
                $PairPos = $Headers{$Header_Path}{"Position"};
                $PairPath = $Header_Path;
            }
            if(get_FileName($Header_Path) eq $HeaderName)
            {
                $Pos = $Headers{$Header_Path}{"Position"};
                $Path = $Header_Path;
            }
        }
        if($PairPos!=-1 and $Pos!=-1
        and int($PairPos)<int($Pos))
        {
            my %Tmp = %{$Headers{$Path}};
            %{$Headers{$Path}} = %{$Headers{$PairPath}};
            %{$Headers{$PairPath}} = %Tmp;
        }
    }
    if(not keys(%Headers))
    {
        print STDERR "ERROR: header files were not found\n";
        exit(1);
    }
}

sub detect_recursive_includes($)
{
    my $AbsPath = $_[0];
    return () if(not $AbsPath or isCyclical(\@RecurInclude, $AbsPath));
    return () if($SystemPaths{"include"}{get_Directory($AbsPath)} and $GlibcHeader{get_FileName($AbsPath)});
    return () if($SystemPaths{"include"}{get_Directory(get_Directory($AbsPath))}
    and (get_Directory($AbsPath)=~/\/(asm-.+)\Z/ or $GlibcDir{get_FileName(get_Directory($AbsPath))}));
    return keys(%{$RecursiveIncludes{$AbsPath}}) if(keys(%{$RecursiveIncludes{$AbsPath}}));
    return () if(get_FileName($AbsPath)=~/windows|win32|win64|atomic/i);
    push(@RecurInclude, $AbsPath);
    if(not keys(%{$Header_Includes{$AbsPath}}))
    {
        my $Content = readFile($AbsPath);
        if($Content=~/#[ \t]*error[ \t]+/ and (my $Redirect = parse_redirect($Content, $AbsPath)))
        {#detecting error directive in the headers
            if(my ($RInc, $RPath) = identify_header($Redirect))
            {
                $Header_ErrorRedirect{$AbsPath}{"Inc"} = $RInc;
                $Header_ErrorRedirect{$AbsPath}{"Path"} = $RPath;
            }
        }
        foreach my $Include (parse_includes($Content))
        {
            $Header_Includes{$AbsPath}{$Include} = 1;
            if(my $Prefix = get_Directory($Include))
            {
                $Header_Prefix{get_FileName($Include)} = $Prefix;
            }
        }
    }
    foreach my $Include (keys(%{$Header_Includes{$AbsPath}}))
    {
        my ($HInc, $HPath)=identify_header($Include);
        next if(not $HPath);
        $RecursiveIncludes{$AbsPath}{$HPath} = 1;
        $RecursiveIncludes_Inverse{$HPath}{$AbsPath} = 1 if($AbsPath ne $HPath);
        $Header_Include_Prefix{$AbsPath}{$HPath}{get_Directory($Include)} = 1;
        foreach my $IncPath (detect_recursive_includes($HPath))
        {
            $RecursiveIncludes{$AbsPath}{$IncPath} = 1;
            $RecursiveIncludes_Inverse{$IncPath}{$AbsPath} = 1 if($AbsPath ne $IncPath);
            foreach my $Prefix (keys(%{$Header_Include_Prefix{$HPath}{$IncPath}}))
            {
                $Header_Include_Prefix{$AbsPath}{$IncPath}{$Prefix} = 1;
            }
        }
    }
    pop(@RecurInclude);
    return keys(%{$RecursiveIncludes{$AbsPath}});
}

sub detect_top_header($)
{
    my $AbsPath = $_[0];
    return "" if(not $AbsPath);
    foreach my $Path (sort {keys(%{$Header_Includes{$b}})<=>keys(%{$Header_Includes{$a}})} keys(%{$RecursiveIncludes_Inverse{$AbsPath}}))
    {
        if($RegisteredHeaders{$Path} and not $Header_ErrorRedirect{$Path})
        {
            $Header_TopHeader{$AbsPath} = $Path;
            last;
        }
    }
}

sub get_depth($)
{
    my $Code=$_[0];
    my $Count=0;
    while($Code=~s/(\/|::)//)
    {
        $Count+=1;
    }
    return $Count;
}

sub unmangleArray(@)
{
    if($#_>$MAX_COMMAND_LINE_ARGUMENTS)
    {
        my @Half = splice(@_, 0, ($#_+1)/2);
        return (unmangleArray(@Half), unmangleArray(@_))
    }
    else
    {
        my $UnmangleCommand = $CPP_FILT." ".join(" ", @_);
        return split(/\n/, `$UnmangleCommand`);
    }
}

sub translateSymbols(@)
{
    my (@MnglNames, @UnMnglNames) = ();
    foreach my $Interface (sort @_)
    {
        if($Interface=~/\A_Z/ and not $tr_name{$Interface})
        {
            push(@MnglNames, $Interface);
        }
    }
    if($#MnglNames > -1)
    {
        @UnMnglNames = reverse(unmangleArray(@MnglNames));
        foreach my $Interface (@MnglNames)
        {
            if($Interface=~/\A_Z/)
            {
                $tr_name{$Interface} = pop(@UnMnglNames);
                $mangled_name{correctName($tr_name{$Interface})} = $Interface;
            }
        }
    }
}

my %check_node=(
"array_type"=>1,
"binfo"=>1,
"boolean_type"=>1,
"complex_type"=>1,
"const_decl"=>1,
"enumeral_type"=>1,
"field_decl"=>1,
"function_decl"=>1,
"function_type"=>1,
"identifier_node"=>1,
"integer_cst"=>1,
"integer_type"=>1,
"method_type"=>1,
"namespace_decl"=>1,
"parm_decl"=>1,
"pointer_type"=>1,
"real_cst"=>1,
"real_type"=>1,
"record_type"=>1,
"reference_type"=>1,
"string_cst"=>1,
"template_decl"=>1,
"template_type_parm"=>1,
"tree_list"=>1,
"tree_vec"=>1,
"type_decl"=>1,
"union_type"=>1,
"var_decl"=>1,
"void_type"=>1);

sub getInfo($)
{
    my $InfoPath = $_[0];
    return if(not $InfoPath or not -f $InfoPath);
    if($Config{"osname"} eq "linux")
    {
        my $InfoPath_New = $InfoPath.".1";
        system("sed ':a;N;\$!ba;s/\\n[^\@]/ /g' ".esc($InfoPath)."|sed 's/ [ ]\\+/  /g' > ".esc($InfoPath_New));
        unlink($InfoPath);
        print "\rheader(s) analysis: [40.00%]";
        #getting info
        open(INFO, $InfoPath_New) || die ("\ncan't open file \'$InfoPath_New\': $!\n");
        while(<INFO>)
        {
            chomp;
            if(/\A@(\d+)[ \t]+([a-z_]+)[ \t]+(.*)\Z/i)
            {
                next if(not $check_node{$2});
                $LibInfo{$1}{"info_type"}=$2;
                $LibInfo{$1}{"info"}=$3;
            }
        }
        close(INFO);
        unlink($InfoPath_New);
    }
    else
    {
        my $Content = readFile($InfoPath);
        $Content=~s/\n[^\@]/ /g;
        $Content=~s/[ ]{2,}/ /g;
        foreach my $Line (split(/\n/, $Content))
        {
            if($Line=~/\A@(\d+)[ \t]+([a-z_]+)[ \t]+(.*)\Z/i)
            {
                next if(not $check_node{$2});
                $LibInfo{$1}{"info_type"}=$2;
                $LibInfo{$1}{"info"}=$3;
            }
        }
    }
    print "\rheader(s) analysis: [45.00%]";
    #processing info
    setTemplateParams_All();
    print "\rheader(s) analysis: [50.00%]";
    setAnonTypedef_All();
    print "\rheader(s) analysis: [55.00%]";
    getTypeDescr_All();
    print "\rheader(s) analysis: [65.00%]";
    getFuncDescr_All();
    print "\rheader(s) analysis: [70.00%]";
    foreach my $NameSpace (keys(%Interface_Overloads))
    {
        foreach my $ClassName (keys(%{$Interface_Overloads{$NameSpace}}))
        {
            foreach my $ShortName (keys(%{$Interface_Overloads{$NameSpace}{$ClassName}}))
            {
                if(keys(%{$Interface_Overloads{$NameSpace}{$ClassName}{$ShortName}})>1)
                {
                    foreach my $MnglName (keys(%{$Interface_Overloads{$NameSpace}{$ClassName}{$ShortName}}))
                    {
                        $OverloadedInterface{$MnglName} = keys(%{$Interface_Overloads{$NameSpace}{$ClassName}{$ShortName}});
                    }
                }
                delete($Interface_Overloads{$NameSpace}{$ClassName}{$ShortName});
            }
        }
    }
    getVarDescr_All();
    print "\rheader(s) analysis: [75.00%]";
    addInlineTemplateFunctions();
    print "\rheader(s) analysis: [80.00%]";
    foreach my $TDid (keys(%TypeDescr))
    {
        foreach my $Tid (keys(%{$TypeDescr{$TDid}}))
        {
            my %Type = get_FoundationType($TDid, $Tid);
            my $PointerLevel = get_PointerLevel($TDid, $Tid);
            if($Type{"Type"}=~/\A(Struct|Union)\Z/ and $Type{"Name"}!~/\:\:/ and $PointerLevel>=1)
            {
                $StructUnionPName_Tid{get_TypeName($Tid)} = $Tid;
            }
        }
    }
    %LibInfo = ();
    %InlineTemplateFunction = ();
    %TypedefToAnon = ();
    %Interface_Overloads = ();
}

sub getTypeDeclId($)
{
    my $TypeInfo = $LibInfo{$_[0]}{"info"};
    if($TypeInfo=~/name[ ]*:[ ]*@(\d+)/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub isFuncPtr($)
{
    my $Ptd = pointTo($_[0]);
    if($Ptd)
    {
        if(($LibInfo{$_[0]}{"info"}=~/unql[ ]*:/) and not ($LibInfo{$_[0]}{"info"}=~/qual[ ]*:/))
        {
            return 0;
        }
        elsif(($LibInfo{$_[0]}{"info_type"} eq "pointer_type") and ($LibInfo{$Ptd}{"info_type"} eq "function_type" or $LibInfo{$Ptd}{"info_type"} eq "method_type"))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

sub pointTo($)
{
    my $TypeInfo = $LibInfo{$_[0]}{"info"};
    if($TypeInfo=~/ptd[ ]*:[ ]*@(\d+)/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getTypeDescr_All()
{
    foreach (sort {int($a)<=>int($b)} keys(%LibInfo))
    {# detecting explicit typedefs
        if($LibInfo{$_}{"info_type"} eq "type_decl")
        {
            my $TypeId = getTreeAttr($_, "type");
            my $TypeDeclId = getTypeDeclId($TypeId);
            my $TypedefName = getNameByInfo($_);
            if($TypeDeclId and ($TypeDeclId ne $_) and (getNameByInfo($TypeDeclId) ne $TypedefName) and isNotAnon($TypedefName))
            {
                $ExplicitTypedef{$TypeId}{$_} = 1;
                $Type_Typedef{$TypeId}{-$TypeId} = 1;
            }
        }
    }
    foreach my $Tid (sort {int($a)<=>int($b)} keys(%ExplicitTypedef))
    {# reflecting explicit typedefs to the parallel flatness
        foreach my $TDid (sort {int($a)<=>int($b)} keys(%{$ExplicitTypedef{$Tid}}))
        {
            getTypeDescr($TDid, $Tid);
            if($TypeDescr{$TDid}{$Tid}{"Name"})
            {
                $TypeDescr{$TDid}{$Tid}{"Tid"} = -$Tid;
                $TypeDescr{$TDid}{$Tid}{"TDid"} = $TDid;
                %{$TypeDescr{$TDid}{-$Tid}} = %{$TypeDescr{$TDid}{$Tid}};
                $Tid_TDid{-$Tid} = $TDid;
            }
            else
            {
                delete($ExplicitTypedef{$Tid}{$TDid});
            }
            delete($TypeDescr{$TDid}{$Tid});
        }
    }
    foreach (sort {int($a)<=>int($b)} keys(%LibInfo))
    {# enumerations
        if($LibInfo{$_}{"info_type"} eq "const_decl")
        {
            getTypeDescr($_, getTreeAttr($_, "type"));
        }
    }
    foreach (sort {int($a)<=>int($b)} keys(%LibInfo))
    {# other types
        if($LibInfo{$_}{"info_type"}=~/_type\Z/ and $LibInfo{$_}{"info_type"}!~/function_type|method_type/)
        {
            getTypeDescr(getTypeDeclId($_), $_);
        }
    }
    $TypeDescr{""}{-1}{"Name"} = "...";
    $TypeDescr{""}{-1}{"Type"} = "Intrinsic";
    $TypeDescr{""}{-1}{"Tid"} = -1;
    detectStructBases();
    detectStructMembers();
    $MaxTypeId_Start = $MaxTypeId;
    foreach my $TypeId (keys(%Type_Typedef))
    {
        foreach my $TypedefId (keys(%{$Type_Typedef{$TypeId}}))
        {
            if(not get_TypeName($TypedefId)
            or get_TypeName($TypedefId) eq get_TypeName($TypeId))
            {
                delete($Type_Typedef{$TypeId}{$TypedefId});
            }
        }
    }
}

sub detectStructMembers()
{
    foreach my $TypeId (sort {int($a)<=>int($b)} keys(%StructIDs))
    {
        my %Type = %{$TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}};
        foreach my $MembPos (sort {int($a)<=>int($b)} keys(%{$Type{"Memb"}}))
        {
            my $MembTypeId = $Type{"Memb"}{$MembPos}{"type"};
            my $MembFTypeId = get_FoundationTypeId($MembTypeId);
            if(get_TypeType($MembFTypeId)=~/\A(Struct|Union)\Z/)
            {
                $Member_Struct{$MembTypeId}{$TypeId}=1;
            }
        }
    }
}

my %Struct_Mapping;
sub init_struct_mapping($$$)
{
    my ($TypeId, $Ref, $KeysRef) = @_;
    my @Keys = @{$KeysRef};
    if($#Keys>=1)
    {
        my $FirstKey = $Keys[0];
        splice(@Keys, 0, 1);
        if(not defined $Ref->{$FirstKey})
        {
            %{$Ref->{$FirstKey}} = ();
        }
        init_struct_mapping($TypeId, $Ref->{$FirstKey}, \@Keys);
    }
    elsif($#Keys==0)
    {
        $Ref->{$Keys[0]}{"Types"}{$TypeId} = 1;
    }
}

sub read_struct_mapping($)
{
    my $Ref = $_[0];
    my %LevelTypes = ();
    @LevelTypes{keys(%{$Ref->{"Types"}})} = values(%{$Ref->{"Types"}});
    foreach my $Key (keys(%{$Ref}))
    {
        next if($Key eq "Types");
        foreach my $SubClassId (read_struct_mapping($Ref->{$Key}))
        {
            $LevelTypes{$SubClassId} = 1;
            foreach my $ParentId (keys(%{$Ref->{"Types"}}))
            {
                $Struct_SubClasses{$ParentId}{$SubClassId} = 1;
            }
        }
    }
    return keys(%LevelTypes);
}

sub detectStructBases()
{
    foreach my $TypeId (sort {int($a)<=>int($b)} keys(%StructIDs))
    {
        my %Type = %{$TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}};
        next if(not keys(%{$Type{"Memb"}}));
        my $FirstId = $Type{"Memb"}{0}{"type"};
        if($Type{"Memb"}{0}{"name"}=~/parent/i
        and get_TypeType(get_FoundationTypeId($FirstId)) eq "Struct"
        and get_TypeName($FirstId)!~/gobject/i)
        {
            $Struct_Parent{$TypeId} = $FirstId;
        }
        my @Keys = ();
        foreach my $MembPos (sort {int($a)<=>int($b)} keys(%{$Type{"Memb"}}))
        {
            push(@Keys, $Type{"Memb"}{$MembPos}{"name"}.":".$Type{"Memb"}{$MembPos}{"type"});
        }
        init_struct_mapping($TypeId, \%Struct_Mapping, \@Keys);
    }
    read_struct_mapping(\%Struct_Mapping);
}

sub getTypeDescr($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    $MaxTypeId = $TypeId if($TypeId>$MaxTypeId or not defined $MaxTypeId);
    if(not $ExplicitTypedef{$TypeId}{$TypeDeclId})
    {
        $Tid_TDid{$TypeId} = $TypeDeclId;
    }
    %{$TypeDescr{$TypeDeclId}{$TypeId}} = getTypeAttr($TypeDeclId, $TypeId);
    if(not $TypeDescr{$TypeDeclId}{$TypeId}{"Name"})
    {
        delete($TypeDescr{$TypeDeclId}{$TypeId});
        return;
    }
    if(not keys(%{$TypeDescr{$TypeDeclId}}))
    {
        delete($TypeDescr{$TypeDeclId});
        return;
    }
    if(not $TName_Tid{$TypeDescr{$TypeDeclId}{$TypeId}{"Name"}})
    {
        $TName_Tid{$TypeDescr{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        $TName_Tid{delete_keywords($TypeDescr{$TypeDeclId}{$TypeId}{"Name"})} = $TypeId;
    }
    my $BaseTypeId = get_FoundationTypeId($TypeId);
    my $PLevel = get_PointerLevel($TypeDeclId, $TypeId);
    $BaseType_PLevel_Type{$BaseTypeId}{$PLevel}{$TypeId}=1;
    if(my $Prefix = getPrefix($TypeDescr{$TypeDeclId}{$TypeId}{"Name"}))
    {
        $Library_Prefixes{$Prefix} += 1;
    }
}

sub getPrefix($)
{
    my $Str = $_[0];
    if($Str=~/\A[_]*(([a-z]|[A-Z])[a-z]+)[A-Z]/)
    {
        return $1;
    }
    elsif($Str=~/\A[_]*([A-Z]+)[A-Z][a-z]+([A-Z][a-z]+|\Z)/)
    {
        return $1;
    }
    elsif($Str=~/\A([a-z0-9]+_)[a-z]+/i)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getNodeType($)
{
    return $LibInfo{$_[0]}{"info_type"};
}

sub getNodeIntCst($)
{
    my $CstId = $_[0];
    my $CstTypeId = getTreeAttr($CstId, "type");
    if($EnumMembName_Id{$CstId})
    {
        return $EnumMembName_Id{$CstId};
    }
    elsif($LibInfo{$CstId}{"info"}=~/low[ ]*:[ ]*([^ ]+) /)
    {
        if($1 eq "0")
        {
            if(getNodeType($CstTypeId) eq "boolean_type")
            {
                return "false";
            }
            else
            {
                return "0";
            }
        }
        elsif($1 eq "1")
        {
            if(getNodeType($CstTypeId) eq "boolean_type")
            {
                return "true";
            }
            else
            {
                return "1";
            }
        }
        else
        {
            return $1;
        }
    }
    else
    {
        return "";
    }
}

sub getNodeStrCst($)
{
    if($LibInfo{$_[0]}{"info"}=~/low[ ]*:[ ]*(.+)[ ]+lngt/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getArraySize($$)
{
    my ($TypeId, $BaseName) = @_;
    my $SizeBytes = getSize($TypeId)/8;
    while($BaseName=~s/\s*\[(\d+)\]//)
    {
        $SizeBytes/=$1;
    }
    my $BasicId = $TName_Tid{$BaseName};
    my $BasicSize = $TypeDescr{getTypeDeclId($BasicId)}{$BasicId}{"Size"};
    $SizeBytes/=$BasicSize if($BasicSize);
    return $SizeBytes;
}

sub getTypeAttr($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    my ($BaseTypeSpec, %TypeAttr) = ();
    if($TypeDescr{$TypeDeclId}{$TypeId}{"Name"})
    {
        return %{$TypeDescr{$TypeDeclId}{$TypeId}};
    }
    $TypeAttr{"Tid"} = $TypeId;
    $TypeAttr{"TDid"} = $TypeDeclId;
    $TypeAttr{"Type"} = getTypeType($TypeDeclId, $TypeId);
    if($TypeAttr{"Type"} eq "Unknown")
    {
        return ();
    }
    elsif($TypeAttr{"Type"} eq "FuncPtr")
    {
        %{$TypeDescr{$TypeDeclId}{$TypeId}} = getFuncPtrAttr(pointTo($TypeId), $TypeDeclId, $TypeId);
        $TName_Tid{$TypeDescr{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %{$TypeDescr{$TypeDeclId}{$TypeId}};
    }
    elsif($TypeAttr{"Type"} eq "Array")
    {
        ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}, $BaseTypeSpec) = selectBaseType($TypeDeclId, $TypeId);
        my %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        if($TypeAttr{"Size"} = getArraySize($TypeId, $BaseTypeAttr{"Name"}))
        {
            if($BaseTypeAttr{"Name"}=~/\A([^\[\]]+)(\[(\d+|)\].*)\Z/)
            {
                $TypeAttr{"Name"} = $1."[".$TypeAttr{"Size"}."]".$2;
            }
            else
            {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}."[".$TypeAttr{"Size"}."]";
            }
        }
        else
        {
            if($BaseTypeAttr{"Name"}=~/\A([^\[\]]+)(\[(\d+|)\].*)\Z/)
            {
                $TypeAttr{"Name"} = $1."[]".$2;
            }
            else
            {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}."[]";
            }
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
        $TypeAttr{"Header"} = $BaseTypeAttr{"Header"};
        %{$TypeDescr{$TypeDeclId}{$TypeId}} = %TypeAttr;
        $TName_Tid{$TypeDescr{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %TypeAttr;
    }
    elsif($TypeAttr{"Type"}=~/\A(Intrinsic|Union|Struct|Enum|Class)\Z/)
    {
        if($TemplateInstance{$TypeDeclId}{$TypeId})
        {
            my @Template_Params = ();
            foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$TemplateInstance{$TypeDeclId}{$TypeId}}))
            {
                my $Param_TypeId = $TemplateInstance{$TypeDeclId}{$TypeId}{$Param_Pos};
                my $Param = get_TemplateParam($Param_TypeId);
                if($Param eq "")
                {
                    return ();
                }
                elsif($Param ne "\@skip\@")
                {
                    @Template_Params = (@Template_Params, $Param);
                }
            }
            %{$TypeDescr{$TypeDeclId}{$TypeId}} = getTrivialTypeAttr($TypeDeclId, $TypeId);
            my $PrmsInLine = join(", ", @Template_Params);
            $PrmsInLine = " ".$PrmsInLine." " if($PrmsInLine=~/>\Z/);
            $TypeDescr{$TypeDeclId}{$TypeId}{"Name"} = $TypeDescr{$TypeDeclId}{$TypeId}{"Name"}."<".$PrmsInLine.">";
            $TypeDescr{$TypeDeclId}{$TypeId}{"Name"} = correctName($TypeDescr{$TypeDeclId}{$TypeId}{"Name"});
            $TName_Tid{$TypeDescr{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
            return %{$TypeDescr{$TypeDeclId}{$TypeId}};
        }
        else
        {
            return () if($TemplateNotInst{$TypeDeclId}{$TypeId});
            return () if($IgnoreTmplInst{$TypeDeclId});
            %{$TypeDescr{$TypeDeclId}{$TypeId}} = getTrivialTypeAttr($TypeDeclId, $TypeId);
            return %{$TypeDescr{$TypeDeclId}{$TypeId}};
        }
    }
    else
    {
        ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}, $BaseTypeSpec) = selectBaseType($TypeDeclId, $TypeId);
        if(not $ExplicitTypedef{$TypeId}{$TypeDeclId} and keys(%{$ExplicitTypedef{$TypeAttr{"BaseType"}{"Tid"}}})==1)
        {# replace the type to according explicit typedef
            my $NewBase_TDid = (keys(%{$ExplicitTypedef{$TypeAttr{"BaseType"}{"Tid"}}}))[0];
            my $NewBase_Tid = -$TypeAttr{"BaseType"}{"Tid"};
            if($TypeDescr{$NewBase_TDid}{$NewBase_Tid}{"Name"})
            {
                $TypeAttr{"BaseType"}{"TDid"} = $NewBase_TDid;
                $TypeAttr{"BaseType"}{"Tid"} = $NewBase_Tid;
            }
        }
        my %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        if($BaseTypeAttr{"Name"}=~/\Astd::/ and $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"Tid"}
        and ($TypeAttr{"Type"} ne "Typedef") and ($TypeId ne $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"Tid"}))
        {
            $TypeAttr{"BaseType"}{"TDid"} = $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"TDid"};
            $TypeAttr{"BaseType"}{"Tid"} = $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"Tid"};
            %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        }
        if($BaseTypeSpec and $BaseTypeAttr{"Name"})
        {
            if(($TypeAttr{"Type"} eq "Pointer") and $BaseTypeAttr{"Name"}=~/\([\*]+\)/)
            {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"};
                $TypeAttr{"Name"}=~s/\(([\*]+)\)/($1*)/g;
            }
            else
            {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}." ".$BaseTypeSpec;
            }
        }
        elsif($BaseTypeAttr{"Name"})
        {
            $TypeAttr{"Name"} = $BaseTypeAttr{"Name"};
        }
        if($TypeAttr{"Type"} eq "Typedef")
        {
            $TypeAttr{"Name"} = getNameByInfo($TypeDeclId);
            $TypeAttr{"NameSpace"} = getNameSpace($TypeDeclId);
            return () if($TypeAttr{"NameSpace"} eq "\@skip\@");
            if($TypeAttr{"NameSpace"})
            {
                $TypeAttr{"Name"} = $TypeAttr{"NameSpace"}."::".$TypeAttr{"Name"};
            }
            if($ExplicitTypedef{$TypeId}{$TypeDeclId} and length($TypeAttr{"Name"})>length($BaseTypeAttr{"Name"}))
            {
                return ();
            }
            $TypeAttr{"Header"} = getHeader($TypeDeclId);
            $Type_Typedef{$BaseTypeAttr{"Tid"}}{$TypeAttr{"Tid"}} = 1;
            if($TypeAttr{"NameSpace"}=~/\Astd(::|\Z)/ and $BaseTypeAttr{"NameSpace"}=~/\Astd(::|\Z)/)
            {
                $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"Name"} = $TypeAttr{"Name"};
                $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"Tid"} = $TypeId;
                $StdCxxTypedef{$BaseTypeAttr{"Name"}}{"TDid"} = $TypeDeclId;
            }
            $Typedef_BaseName{$TypeAttr{"Name"}} = $BaseTypeAttr{"Name"};
            delete($TypeAttr{"NameSpace"}) if(not $TypeAttr{"NameSpace"});
        }
        if(not $TypeAttr{"Size"})
        {
            if($TypeAttr{"Type"} eq "Pointer")
            {
                $TypeAttr{"Size"} = $POINTER_SIZE;
            }
            else
            {
                $TypeAttr{"Size"} = $BaseTypeAttr{"Size"};
            }
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
        $TypeAttr{"Header"} = $BaseTypeAttr{"Header"} if(not $TypeAttr{"Header"});
        if(defined $SplintAnnotations)
        {
            $TypeAttr{"Line"} = getLine($TypeDeclId);
            $TypeAttr{"Line"} = $BaseTypeAttr{"Line"} if(not $TypeAttr{"Line"});
        }
        %{$TypeDescr{$TypeDeclId}{$TypeId}} = %TypeAttr;
        $TName_Tid{$TypeDescr{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %TypeAttr;
    }
}

sub get_TemplateParam($)
{
    my $Type_Id = $_[0];
    return "" if(not $Type_Id);
    if(getNodeType($Type_Id) eq "integer_cst")
    {
        return getNodeIntCst($Type_Id);
    }
    elsif(getNodeType($Type_Id) eq "string_cst")
    {
        return getNodeStrCst($Type_Id);
    }
    elsif(getNodeType($Type_Id) eq "tree_vec")
    {
        return "\@skip\@";
    }
    else
    {
        my $Type_DId = getTypeDeclId($Type_Id);
        my %ParamAttr = getTypeAttr($Type_DId, $Type_Id);
        if(not $ParamAttr{"Name"})
        {
            return "";
        }
        if($ParamAttr{"Name"}=~/\>/)
        {
            if($StdCxxTypedef{$ParamAttr{"Name"}}{"Name"})
            {
                return $StdCxxTypedef{$ParamAttr{"Name"}}{"Name"};
            }
            elsif(my $Covered = cover_stdcxx_typedef($ParamAttr{"Name"}))
            {
                return $Covered;
            }
            else
            {
                return $ParamAttr{"Name"};
            }
        }
        else
        {
            return $ParamAttr{"Name"};
        }
    }
}

sub cover_stdcxx_typedef($)
{
    my $TypeName = $_[0];
    my $TypeName_Covered = $TypeName;
    while($TypeName=~s/>[ ]*(const|volatile|restrict| |\*|\&)\Z/>/g){};
    if(my $Cover = $StdCxxTypedef{$TypeName}{"Name"})
    {
        $TypeName_Covered=~s/\Q$TypeName\E/$Cover /g;
    }
    return correctName($TypeName_Covered);
}

sub getFuncPtrAttr($$$)
{
    my ($FuncTypeId, $TypeDeclId, $TypeId) = @_;
    my $FuncInfo = $LibInfo{$FuncTypeId}{"info"};
    my $FuncInfo_Type = $LibInfo{$FuncTypeId}{"info_type"};
    my $FuncPtrCorrectName = "";
    my %TypeAttr = ("Size"=>$POINTER_SIZE, "Type"=>"FuncPtr", "TDid"=>$TypeDeclId, "Tid"=>$TypeId);
    my @ParamTypeName;
    if($FuncInfo=~/retn[ ]*:[ ]*\@(\d+) /)
    {
        my $ReturnTypeId = $1;
        my %ReturnAttr = getTypeAttr(getTypeDeclId($ReturnTypeId), $ReturnTypeId);
        $FuncPtrCorrectName .= $ReturnAttr{"Name"};
        $TypeAttr{"Return"} = $ReturnTypeId;
    }
    if($FuncInfo=~/prms[ ]*:[ ]*@(\d+) /)
    {
        my $ParamTypeInfoId = $1;
        my $Position = 0;
        while($ParamTypeInfoId)
        {
            my $ParamTypeInfo = $LibInfo{$ParamTypeInfoId}{"info"};
            last if($ParamTypeInfo!~/valu[ ]*:[ ]*@(\d+) /);
            my $ParamTypeId = $1;
            my %ParamAttr = getTypeAttr(getTypeDeclId($ParamTypeId), $ParamTypeId);
            last if($ParamAttr{"Name"} eq "void");
            $TypeAttr{"Memb"}{$Position}{"type"} = $ParamTypeId;
            push(@ParamTypeName, $ParamAttr{"Name"});
            last if($ParamTypeInfo!~/chan[ ]*:[ ]*@(\d+) /);
            $ParamTypeInfoId = $1;
            $Position+=1;
        }
    }
    if($FuncInfo_Type eq "function_type")
    {
        $FuncPtrCorrectName .= " (*) (".join(", ", @ParamTypeName).")";
    }
    elsif($FuncInfo_Type eq "method_type")
    {
        if($FuncInfo=~/clas[ ]*:[ ]*@(\d+) /)
        {
            my $ClassId = $1;
            my $ClassName = $TypeDescr{getTypeDeclId($ClassId)}{$ClassId}{"Name"};
            if($ClassName)
            {
                $FuncPtrCorrectName .= " ($ClassName\:\:*) (".join(", ", @ParamTypeName).")";
            }
            else
            {
                $FuncPtrCorrectName .= " (*) (".join(", ", @ParamTypeName).")";
            }
        }
        else
        {
            $FuncPtrCorrectName .= " (*) (".join(", ", @ParamTypeName).")";
        }
    }
    $TypeAttr{"Name"} = correctName($FuncPtrCorrectName);
    $TypeAttr{"FuncTypeId"} = $FuncTypeId;
    return %TypeAttr;
}

sub getTypeName($)
{
    my $Info = $LibInfo{$_[0]}{"info"};
    if($Info=~/name[ ]*:[ ]*@(\d+) /)
    {
        return getNameByInfo($1);
    }
    else
    {
        if($LibInfo{$_[0]}{"info_type"} eq "integer_type")
        {
            if($LibInfo{$_[0]}{"info"}=~/unsigned/)
            {
                return "unsigned int";
            }
            else
            {
                return "int";
            }
        }
        else
        {
            return "";
        }
    }
}

sub getTypeType($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    return "Typedef" if($ExplicitTypedef{$TypeId}{$TypeDeclId});
    return "Typedef" if($LibInfo{$TypeId}{"info"}=~/unql[ ]*:/ and $LibInfo{$TypeId}{"info"}!~/qual[ ]*:/);# or ($LibInfo{$TypeId}{"info"}=~/qual[ ]*:/ and $LibInfo{$TypeId}{"info"}=~/name[ ]*:/)));
    return "Const" if($LibInfo{$TypeId}{"info"}=~/qual[ ]*:[ ]*c / and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@/);
    return "Volatile" if($LibInfo{$TypeId}{"info"}=~/qual[ ]*:[ ]*v / and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@/);
    return "Restrict" if($LibInfo{$TypeId}{"info"}=~/qual[ ]*:[ ]*r / and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@/);
    my $TypeType = getTypeTypeByTypeId($TypeId);
    if($TypeType eq "Struct")
    {
        if($TypeDeclId and $LibInfo{$TypeDeclId}{"info_type"} eq "template_decl")
        {
            return "Template";
        }
        else
        {
            return "Struct";
        }
    }
    else
    {
        return $TypeType;
    }
}

sub getQual($)
{
    my $TypeId = $_[0];
    if($LibInfo{$TypeId}{"info"}=~ /qual[ ]*:[ ]*(r|c|v)/)
    {
        my $Qual = $1;
        if($Qual eq "r")
        {
            return "restrict";
        }
        elsif($Qual eq "v")
        {
            return "volatile";
        }
        elsif($Qual eq "c")
        {
            return "const";
        }
    }
    return "";
}

sub selectBaseType($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    if($ExplicitTypedef{$TypeId}{$TypeDeclId})
    {
        return ($TypeId, getTypeDeclId($TypeId), "");
    }
    my $TypeInfo = $LibInfo{$TypeId}{"info"};
    my $BaseTypeDeclId;
    my $Type_Type = getTypeType($TypeDeclId, $TypeId);
    # qualifiers
    if($LibInfo{$TypeId}{"info"}=~/unql[ ]*:/
    and $LibInfo{$TypeId}{"info"}=~/qual[ ]*:/
    and $LibInfo{$TypeId}{"info"}=~/name[ ]*:[ ]*\@(\d+) /
    and (getTypeId($1) ne $TypeId))
    {# typedefs
        return (getTypeId($1), $1, getQual($TypeId));
    }
    elsif($LibInfo{$TypeId}{"info"}!~/qual[ ]*:/
    and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@(\d+) /)
    {# typedefs
        return ($1, getTypeDeclId($1), "");
    }
    elsif($LibInfo{$TypeId}{"info"}=~/qual[ ]*:[ ]*c /
    and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@(\d+) /)
    {
        return ($1, getTypeDeclId($1), "const");
    }
    elsif($LibInfo{$TypeId}{"info"}=~/qual[ ]*:[ ]*r /
    and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@(\d+) /)
    {
        return ($1, getTypeDeclId($1), "restrict");
    }
    elsif($LibInfo{$TypeId}{"info"}=~/qual[ ]*:[ ]*v /
    and $LibInfo{$TypeId}{"info"}=~/unql[ ]*:[ ]*\@(\d+) /)
    {
        return ($1, getTypeDeclId($1), "volatile");
    }
    elsif($LibInfo{$TypeId}{"info_type"} eq "reference_type")
    {
        if($TypeInfo=~/refd[ ]*:[ ]*@(\d+) /)
        {
            return ($1, getTypeDeclId($1), "&");
        }
        else
        {
            return (0, 0, "");
        }
    }
    elsif($LibInfo{$TypeId}{"info_type"} eq "array_type")
    {
        if($TypeInfo=~/elts[ ]*:[ ]*@(\d+) /)
        {
            return ($1, getTypeDeclId($1), "");
        }
        else
        {
            return (0, 0, "");
        }
    }
    elsif($LibInfo{$TypeId}{"info_type"} eq "pointer_type")
    {
        if($TypeInfo=~/ptd[ ]*:[ ]*@(\d+) /)
        {
            return ($1, getTypeDeclId($1), "*");
        }
        else
        {
            return (0, 0, "");
        }
    }
    else
    {
        return (0, 0, "");
    }
}

sub getFuncDescr_All()
{
    foreach (sort {int($a)<=>int($b)} keys(%LibInfo))
    {
        if($LibInfo{$_}{"info_type"} eq "function_decl")
        {
            getFuncDescr($_);
        }
    }
}

sub getVarDescr_All()
{
    foreach (sort {int($b)<=>int($a)} keys(%LibInfo))
    {
        if($LibInfo{$_}{"info_type"} eq "var_decl")
        {
            getVarDescr($_);
        }
    }
}

sub getVarDescr($)
{
    my $FuncInfoId = $_[0];
    if($LibInfo{getNameSpaceId($FuncInfoId)}{"info_type"} eq "function_decl")
    {
        return;
    }
    $FuncDescr{$FuncInfoId}{"Header"} = getHeader($FuncInfoId);
    if(not $FuncDescr{$FuncInfoId}{"Header"}
    or $FuncDescr{$FuncInfoId}{"Header"}=~/\<built\-in\>|\<internal\>|\A\./)
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    setFuncAccess($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"Private"})
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    $FuncDescr{$FuncInfoId}{"ShortName"} = getNameByInfo($FuncInfoId);
    if(my $Prefix = getPrefix($FuncDescr{$FuncInfoId}{"ShortName"}))
    {
        $Library_Prefixes{$Prefix} += 1;
    }
    $FuncDescr{$FuncInfoId}{"MnglName"} = getFuncMnglName($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"MnglName"} and $FuncDescr{$FuncInfoId}{"MnglName"}!~/\A_Z/)
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    if(not $FuncDescr{$FuncInfoId}{"MnglName"})
    {
        $FuncDescr{$FuncInfoId}{"Name"} = $FuncDescr{$FuncInfoId}{"ShortName"};
        $FuncDescr{$FuncInfoId}{"MnglName"} = $FuncDescr{$FuncInfoId}{"ShortName"};
    }
    if($FuncDescr{$FuncInfoId}{"MnglName"}=~/\A(_ZTI|_ZTI|_ZTS|_ZTT|_ZTV|_ZThn|_ZTv0_n|_ZGV)/)
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    $FuncDescr{$FuncInfoId}{"Return"} = getTypeId($FuncInfoId);
    delete($FuncDescr{$FuncInfoId}{"Return"}) if(not $FuncDescr{$FuncInfoId}{"Return"});
    $FuncDescr{$FuncInfoId}{"Data"} = 1;
    set_Class_And_Namespace($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"ShortName"}=~/\A_Z/)
    {
        delete($FuncDescr{$FuncInfoId}{"ShortName"});
    }
    if(get_TypeName($FuncDescr{$FuncInfoId}{"Return"}) ne "void")
    {
        $ReturnTypeId_Interface{$FuncDescr{$FuncInfoId}{"Return"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
    }
}

sub setTemplateParams_All()
{
    foreach (sort {int($a)<=>int($b)} keys(%LibInfo))
    {
        if($LibInfo{$_}{"info_type"} eq "template_decl")
        {
            $IgnoreTmplInst{getTreeAttr($_,"rslt")} = 1;
            setTemplateParams($_);
        }
    }
}

sub setTemplateParams($)
{
    my $TypeInfoId = $_[0];
    my $Info = $LibInfo{$TypeInfoId}{"info"};
    my $TypeId = getTypeId($TypeInfoId);
    my $TDid = getTypeDeclId($TypeId);
    $TemplateHeader{getNameByInfo($TDid)} = getHeader($TDid);
    if($Info=~/(inst|spcs)[ ]*:[ ]*@(\d+) /)
    {
        my $TmplInst_InfoId = $2;
        setTemplateInstParams($TmplInst_InfoId);
        my $TmplInst_Info = $LibInfo{$TmplInst_InfoId}{"info"};
        while($TmplInst_Info=~/chan[ ]*:[ ]*@(\d+) /)
        {
            $TmplInst_InfoId = $1;
            $TmplInst_Info = $LibInfo{$TmplInst_InfoId}{"info"};
            setTemplateInstParams($TmplInst_InfoId);
        }
    }
}

sub setTemplateInstParams($)
{
    my $TmplInst_Id = $_[0];
    my $Info = $LibInfo{$TmplInst_Id}{"info"};
    my ($Params_InfoId, $ElemId) = ();
    if($Info=~/purp[ ]*:[ ]*@(\d+) /)
    {
        $Params_InfoId = $1;
    }
    if($Info=~/valu[ ]*:[ ]*@(\d+) /)
    {
        $ElemId = $1;
    }
    if($Params_InfoId and $ElemId)
    {
        my $Params_Info = $LibInfo{$Params_InfoId}{"info"};
        while($Params_Info=~s/ (\d+)[ ]*:[ ]*@(\d+) //)
        {
            my ($Param_Pos, $Param_TypeId) = ($1, $2);
            if($LibInfo{$Param_TypeId}{"info_type"} eq "template_type_parm")
            {
                $TemplateNotInst{getTypeDeclId($ElemId)}{$ElemId} = 1;
                return;
            }
            if($LibInfo{$ElemId}{"info_type"} eq "function_decl")
            {
                $TemplateInstance_Func{$ElemId}{$Param_Pos} = $Param_TypeId;
            }
            else
            {
                $TemplateInstance{getTypeDeclId($ElemId)}{$ElemId}{$Param_Pos} = $Param_TypeId;
            }
        }
    }
}

sub has_methods($)
{
    my $TypeId = $_[0];
    return getTreeAttr($TypeId, "fncs");
}

sub getIntLang($)
{
    my $Interface = $_[0];
    return "" if(not $Interface);
    if(my $Lib = get_FileName($Interface_Library{$Interface}))
    {
        return $Language{$Lib};
    }
    elsif(my $Lib = get_FileName($NeededInterface_Library{$Interface}))
    {
        return $Language{$Lib};
    }
    else
    {
        return (($Interface=~/\A_Z|\A_tg_inln_tmpl_\d+\Z/)?"C++":"C");
    }
}

sub setAnonTypedef_All()
{
    foreach (sort {int($a)<=>int($b)} keys(%LibInfo))
    {
        if($LibInfo{$_}{"info_type"} eq "type_decl")
        {
            setAnonTypedef($_);
        }
    }
}

sub setAnonTypedef($)
{
    my $TypeInfoId = $_[0];
    if(getNameByInfo($TypeInfoId)=~/\._\d+/)
    {
        $TypedefToAnon{getTypeId($TypeInfoId)} = 1;
    }
}

sub getTrivialTypeAttr($$)
{
    my ($TypeInfoId, $TypeId) = @_;
    $MaxTypeId = $TypeId if($TypeId>$MaxTypeId or not defined $MaxTypeId);
    my %TypeAttr = ();
    return () if(getTypeTypeByTypeId($TypeId)!~/\A(Intrinsic|Union|Struct|Enum|Class)\Z/);
    setTypeAccess($TypeId, \%TypeAttr);
    $TypeAttr{"Header"} = getHeader($TypeInfoId);
    if(($TypeAttr{"Header"} eq "<built-in>") or ($TypeAttr{"Header"} eq "<internal>"))
    {
        delete($TypeAttr{"Header"});
    }
    $TypeAttr{"Name"} = getNameByInfo($TypeInfoId);
    $TypeAttr{"ShortName"} = $TypeAttr{"Name"};
    $TypeAttr{"Name"} = getTypeName($TypeId) if(not $TypeAttr{"Name"});
    if(my $NameSpaceId = getNameSpaceId($TypeInfoId))
    {
        if($NameSpaceId ne $TypeId)
        {
            $TypeAttr{"NameSpace"} = getNameSpace($TypeInfoId);
            return () if($TypeAttr{"NameSpace"} eq "\@skip\@");
            if($LibInfo{$NameSpaceId}{"info_type"} eq "record_type")
            {
                $TypeAttr{"NameSpaceClassId"} = $NameSpaceId;
            }
        }
    }
    if($TypeAttr{"NameSpace"} and isNotAnon($TypeAttr{"Name"}))
    {
        $TypeAttr{"Name"} = $TypeAttr{"NameSpace"}."::".$TypeAttr{"Name"};
    }
    $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
    if(isAnon($TypeAttr{"Name"}))
    {
        my $HeaderLine = getLine($TypeInfoId);
        $TypeAttr{"Name"} = "anon-";
        $TypeAttr{"Name"} .= $TypeAttr{"Header"}."-".$HeaderLine;
    }
    if($TypeAttr{"Name"}=~/\Acomplex (int|float|double|long double)\Z/
    and $COMMON_LANGUAGE eq "C")
    {
        $TypeAttr{"Name"}=~s/complex/_Complex/g;
    }
    $TypeAttr{"Line"} = getLine($TypeInfoId) if(defined $SplintAnnotations);
    if($TypeAttr{"Name"} eq "__exception"
    and $TypeAttr{"Header"} eq "math.h")
    {# libm
        $TypeAttr{"Name"} = "exception";
    }
    $TypeAttr{"Size"} = getSize($TypeId)/8;
    $TypeAttr{"Type"} = getTypeType($TypeInfoId, $TypeId);
    if($TypeAttr{"Type"} eq "Struct"
    and has_methods($TypeId) and $COMMON_LANGUAGE eq "C++")
    {
        $TypeAttr{"Type"} = "Class";
    }
    if($TypeAttr{"Type"} eq "Class")
    {
        setBaseClasses($TypeInfoId, $TypeId, \%TypeAttr);
    }
    if(my $Prefix = getPrefix($TypeAttr{"Name"}))
    {
        $Library_Prefixes{$Prefix} += 1;
    }
    if($TypeAttr{"Type"}=~/\A(Struct|Union|Enum)\Z/)
    {
        if(not isAnonTypedef($TypeId) and not $TypedefToAnon{$TypeId} and not keys(%{$TemplateInstance{$TypeInfoId}{$TypeId}}))
        {
            $TypeAttr{"Name"} = lc($TypeAttr{"Type"})." ".$TypeAttr{"Name"};
        }
    }
    setTypeMemb($TypeId, \%TypeAttr);
    $TypeAttr{"Tid"} = $TypeId;
    $TypeAttr{"TDid"} = $TypeInfoId;
    $Tid_TDid{$TypeId} = $TypeInfoId;
    if(not $TName_Tid{$TypeAttr{"Name"}} and isNotAnon($TypeAttr{"Name"}))
    {
        $TName_Tid{$TypeAttr{"Name"}} = $TypeId;
        $TName_Tid{delete_keywords($TypeAttr{"Name"})} = $TypeId;
    }
    delete($TypeAttr{"NameSpace"}) if(not $TypeAttr{"NameSpace"});
    if($TypeAttr{"Type"} eq "Struct")
    {
        $StructIDs{$TypeId}=1;
    }
    return %TypeAttr;
}

sub setBaseClasses($$$)
{
    my ($TypeInfoId, $TypeId, $TypeAttr) = @_;
    my $Info = $LibInfo{$TypeId}{"info"};
    if($Info=~/binf[ ]*:[ ]*@(\d+) /)
    {
        $Info = $LibInfo{$1}{"info"};
        while($Info=~/accs[ ]*:[ ]*([a-z]+) /)
        {
            last if($Info !~ s/accs[ ]*:[ ]*([a-z]+) //);
            my $Access = $1;
            last if($Info !~ s/binf[ ]*:[ ]*@(\d+) //);
            my $BInfoId = $1;
            my $ClassId = getBinfClassId($BInfoId);
            if($Access eq "pub")
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "public";
            }
            elsif($Access eq "prot")
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "protected";
            }
            elsif($Access eq "priv")
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "private";
            }
            else
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "private";
            }
            $Class_SubClasses{$ClassId}{$TypeId} = 1;
            $Class_BaseClasses{$TypeId}{$ClassId} = 1;
        }
    }
}

sub getBinfClassId($)
{
    my $Info = $LibInfo{$_[0]}{"info"};
    $Info=~/type[ ]*:[ ]*@(\d+) /;
    return $1;
}

sub get_Type($$)
{
    my ($TypeDId, $TypeId) = @_;
    return "" if(not $TypeId);
    return "" if(not $TypeDescr{$TypeDId}{$TypeId});
    return %{$TypeDescr{$TypeDId}{$TypeId}};
}

sub get_func_signature($)
{
    my $FuncInfoId = $_[0];
    my $PureSignature = $FuncDescr{$FuncInfoId}{"ShortName"};
    my @ParamTypes = ();
    foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$FuncDescr{$FuncInfoId}{"Param"}}))
    {#Check Parameters
        my $ParamType_Id = $FuncDescr{$FuncInfoId}{"Param"}{$ParamPos}{"type"};
        my $ParamType_Name = uncover_typedefs(get_TypeName($ParamType_Id));
        @ParamTypes = (@ParamTypes, $ParamType_Name);
    }
    $PureSignature .= "(".join(", ", @ParamTypes).")";
    $PureSignature = delete_keywords($PureSignature);
    return correctName($PureSignature);
}

sub delete_keywords($)
{
    my $TypeName = $_[0];
    $TypeName=~s/(\W|\A)(enum |struct |union |class )/$1/g;
    return $TypeName;
}

sub uncover_typedefs($)
{
    my $TypeName = $_[0];
    return "" if(not $TypeName);
    return $Cache{"uncover_typedefs"}{$TypeName} if(defined $Cache{"uncover_typedefs"}{$TypeName});
    my ($TypeName_New, $TypeName_Pre) = (correctName($TypeName), "");
    while($TypeName_New ne $TypeName_Pre)
    {
        $TypeName_Pre = $TypeName_New;
        my $TypeName_Copy = $TypeName_New;
        my %Words = ();
        while($TypeName_Copy=~s/(\W|\A)([a-z_][\w:]*)(\W|\Z)//io)
        {
            my $Word = $2;
            next if(not $Word or $Word=~/\A(true|false|const|int|long|void|short|float|unsigned|char|double|class|struct|union|enum)\Z/);
            $Words{$Word} = 1;
        }
        foreach my $Word (sort keys(%Words))
        {
            my $BaseType_Name = $Typedef_BaseName{$Word};
            next if($TypeName_New=~/(\W|\A)(struct\s\Q$Word\E|union\s\Q$Word\E|enum\s\Q$Word\E)(\W|\Z)/);
            next if(not $BaseType_Name);
            if($BaseType_Name=~/\([\*]+\)/)
            {
                if($TypeName_New=~/\Q$Word\E(.*)\Z/)
                {
                    my $Type_Suffix = $1;
                    $TypeName_New = $BaseType_Name;
                    if($TypeName_New=~s/\(([\*]+)\)/($1 $Type_Suffix)/)
                    {
                        $TypeName_New = correctName($TypeName_New);
                    }
                }
            }
            else
            {
                if($TypeName_New=~s/(\W|\A)\Q$Word\E(\W|\Z)/$1$BaseType_Name$2/g)
                {
                    $TypeName_New = correctName($TypeName_New);
                }
            }
        }
    }
    $Cache{"uncover_typedefs"}{$TypeName} = $TypeName_New;
    return $TypeName_New;
}

sub get_type_short_name($)
{
    my $TypeName = $_[0];
    $TypeName=~s/[ ]*<.*>[ ]*//g;
    $TypeName=~s/\Astruct //g;
    $TypeName=~s/\Aunion //g;
    $TypeName=~s/\Aclass //g;
    return $TypeName;
}

sub addInlineTemplateFunctions()
{
    my $NewId = (sort {int($b)<=>int($a)} keys(%FuncDescr))[0] + 10;
    foreach my $TDid (sort keys(%TemplateInstance))
    {
        foreach my $Tid (sort keys(%{$TemplateInstance{$TDid}}))
        {
            my $ClassName = get_type_short_name($TypeDescr{$TDid}{$Tid}{"Name"});
            foreach my $FuncInfoId (sort {int($a)<=>int($b)} keys(%{$InlineTemplateFunction{$ClassName}}))
            {
                my $FuncClassId = $FuncDescr{$FuncInfoId}{"Class"};
                if($FuncDescr{$FuncInfoId}{"Constructor"}
                    and (not keys(%{$FuncDescr{$FuncInfoId}{"Param"}}) or (keys(%{$FuncDescr{$FuncInfoId}{"Param"}})==1 and keys(%{$TemplateInstance{$TDid}{$Tid}})==1 and $FuncDescr{$FuncInfoId}{"Param"}{0}{"type"} eq $TemplateInstance{$Tid_TDid{$FuncClassId}}{$FuncClassId}{0})))
                {
                    $FuncDescr{$NewId}{"Header"} = $FuncDescr{$FuncInfoId}{"Header"};
                    $FuncDescr{$NewId}{"Protected"}=1 if($FuncDescr{$FuncInfoId}{"Protected"});
                    $FuncDescr{$NewId}{"Private"}=1 if($FuncDescr{$FuncInfoId}{"Private"});
                    $FuncDescr{$NewId}{"Class"} = $Tid;
                    $FuncDescr{$NewId}{"Constructor"} = 1;
                    $FuncDescr{$NewId}{"MnglName"} = "_tg_inln_tmpl_$NewId";
                    $Class_Constructors{$Tid}{$FuncDescr{$NewId}{"MnglName"}} = 1;
                    if(keys(%{$FuncDescr{$FuncInfoId}{"Param"}})==1)
                    {
                        $FuncDescr{$NewId}{"Param"}{0}{"type"} = $TemplateInstance{$TDid}{$Tid}{0};
                    }
                    $TypeDescr{$TDid}{$Tid}{"Type"} = "Class";
                    $NewId+=1;
                    last;
                }
            }
        }
    }
}

sub isInternal($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    return 0 if($FuncInfo!~/mngl[ ]*:[ ]*@(\d+) /);
    my $FuncMnglNameInfoId = $1;
    return ($LibInfo{$FuncMnglNameInfoId}{"info"}=~/\*[ ]*INTERNAL[ ]*\*/);
}

sub getFuncDescr($)
{
    my $FuncInfoId = $_[0];
    return if(isInternal($FuncInfoId));
    $FuncDescr{$FuncInfoId}{"Header"} = getHeader($FuncInfoId);
    $FuncDescr{$FuncInfoId}{"Line"} = getLine($FuncInfoId) if(defined $SplintAnnotations);
    if(not $FuncDescr{$FuncInfoId}{"Header"}
    or $FuncDescr{$FuncInfoId}{"Header"}=~/\<built\-in\>|\<internal\>|\A\./)
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    if(getFuncSpec($FuncInfoId) eq "PureVirt")
    {#pure virtual methods
        $FuncDescr{$FuncInfoId}{"PureVirt"} = 1;
    }
    setFuncAccess($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"Private"} and not $FuncDescr{$FuncInfoId}{"PureVirt"})
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    setFuncKind($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"PseudoTemplate"})
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    $FuncDescr{$FuncInfoId}{"Type"} = getFuncType($FuncInfoId);
    $FuncDescr{$FuncInfoId}{"Return"} = getFuncReturn($FuncInfoId);
    delete($FuncDescr{$FuncInfoId}{"Return"}) if(not $FuncDescr{$FuncInfoId}{"Return"});
    if(keys(%{$ExplicitTypedef{$FuncDescr{$FuncInfoId}{"Return"}}})==1)
    {# replace the type to according explicit typedef
        $FuncDescr{$FuncInfoId}{"Return"} = -$FuncDescr{$FuncInfoId}{"Return"};
    }
    $FuncDescr{$FuncInfoId}{"ShortName"} = getFuncShortName(getFuncOrig($FuncInfoId));
    if(my $Prefix = getPrefix($FuncDescr{$FuncInfoId}{"ShortName"}))
    {
        $Library_Prefixes{$Prefix} += 1;
    }
    if($FuncDescr{$FuncInfoId}{"ShortName"}=~/\._/)
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    if(defined $TemplateInstance_Func{$FuncInfoId})
    {
        my @TmplParams = ();
        foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$TemplateInstance_Func{$FuncInfoId}}))
        {
            my $Param = get_TemplateParam($TemplateInstance_Func{$FuncInfoId}{$ParamPos});
            if($Param eq "")
            {
                delete($FuncDescr{$FuncInfoId});
                return;
            }
            elsif($Param ne "\@skip\@")
            {
                push(@TmplParams, $Param);
            }
        }
        my $PrmsInLine = join(", ", @TmplParams);
        $PrmsInLine = " ".$PrmsInLine." " if($PrmsInLine=~/>\Z/);
        $FuncDescr{$FuncInfoId}{"ShortName"} .= "<".$PrmsInLine.">";
    }
    setFuncParams($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"Skip"})
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    $FuncDescr{$FuncInfoId}{"MnglName"} = getFuncMnglName($FuncInfoId);
    if($FuncDescr{$FuncInfoId}{"MnglName"} and $FuncDescr{$FuncInfoId}{"MnglName"}!~/\A_Z/)
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    if($Interface_Library{$FuncDescr{$FuncInfoId}{"ShortName"}} and not $FuncDescr{$FuncInfoId}{"MnglName"} and ($FuncDescr{$FuncInfoId}{"Type"} eq "Function"))
    {
        $FuncDescr{$FuncInfoId}{"MnglName"} = $FuncDescr{$FuncInfoId}{"ShortName"};
    }
    set_Class_And_Namespace($FuncInfoId);
    if(not $FuncDescr{$FuncInfoId}{"MnglName"} and not $FuncDescr{$FuncInfoId}{"Class"})
    {#this section only for c++ functions without class that have not been mangled in the syntax tree
        $FuncDescr{$FuncInfoId}{"MnglName"} = $mangled_name{get_func_signature($FuncInfoId)};
    }
    if($FuncDescr{$FuncInfoId}{"Constructor"} or $FuncDescr{$FuncInfoId}{"Destructor"})
    {
        delete($FuncDescr{$FuncInfoId}{"Return"});
    }
    if(($FuncDescr{$FuncInfoId}{"Type"} eq "Method") or $FuncDescr{$FuncInfoId}{"Constructor"} or $FuncDescr{$FuncInfoId}{"Destructor"})
    {
        if($FuncDescr{$FuncInfoId}{"MnglName"}!~/\A_Z/)
        {
            delete($FuncDescr{$FuncInfoId});
            return;
        }
    }
    if(not $FuncDescr{$FuncInfoId}{"MnglName"} and not $FuncDescr{$FuncInfoId}{"Class"} and ($FuncDescr{$FuncInfoId}{"Type"} eq "Function"))
    {
        $FuncDescr{$FuncInfoId}{"MnglName"} = $FuncDescr{$FuncInfoId}{"ShortName"};
    }
    if($InternalInterfaces{$FuncDescr{$FuncInfoId}{"MnglName"}})
    {
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    if($MangledNames{$FuncDescr{$FuncInfoId}{"MnglName"}})
    {# one instance for one mangled name only
        delete($FuncDescr{$FuncInfoId});
        return;
    }
    else
    {
        $MangledNames{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
    }
    if($FuncDescr{$FuncInfoId}{"MnglName"}=~/\A_Z/
    and not $FuncDescr{$FuncInfoId}{"Class"})
    {
        $Func_ShortName_MangledName{$FuncDescr{$FuncInfoId}{"ShortName"}}{$FuncDescr{$FuncInfoId}{"MnglName"}}=1;
    }
    my $ReturnType_Id = $FuncDescr{$FuncInfoId}{"Return"};
    my $ReturnType_Name_Short = get_TypeName($ReturnType_Id);
    while($ReturnType_Name_Short=~s/(\*|\&)([^<>()]+|)\Z/$2/g){};
    my ($ParamName_Prev, $ParamTypeId_Prev) = ();
    foreach my $ParamPos (sort {int($a)<=>int($b)} keys(%{$FuncDescr{$FuncInfoId}{"Param"}}))
    {# detecting out-parameters by name
        if($FuncDescr{$FuncInfoId}{"Param"}{$ParamPos}{"name"}=~/\Ap\d+\Z/
        and (my $NewParamName = $AddIntParams{$FuncDescr{$FuncInfoId}{"MnglName"}}{$ParamPos}))
        {# names from the external file
            $FuncDescr{$FuncInfoId}{"Param"}{$ParamPos}{"name"} = $NewParamName;
        }
        my $ParamName = $FuncDescr{$FuncInfoId}{"Param"}{$ParamPos}{"name"};
        my $ParamTypeId = $FuncDescr{$FuncInfoId}{"Param"}{$ParamPos}{"type"};
        my $ParamPLevel = get_PointerLevel($Tid_TDid{$ParamTypeId}, $ParamTypeId);
        my $ParamFTypeId = get_FoundationTypeId($ParamTypeId);
        my $ParamFTypeName = get_TypeName($ParamFTypeId);
        my $ParamTypeName = get_TypeName($ParamTypeId);
        
        if($UserDefinedOutParam{$FuncDescr{$FuncInfoId}{"MnglName"}}{$ParamPos+1}
        or $UserDefinedOutParam{$FuncDescr{$FuncInfoId}{"MnglName"}}{$ParamName})
        {# user defined by <out_params>
            register_out_param($FuncDescr{$FuncInfoId}{"MnglName"}, $ParamPos, $ParamName, $ParamTypeId);
            next;
        }
        
        # particular accept
        if($ParamPLevel>=2 and $ParamFTypeName=~/\A(char|unsigned char|wchar_t)\Z/
        and not is_const_type($ParamTypeName) and $ParamName!~/argv/i and $ParamName!~/\A(s|str|string)\Z/i)
        {# soup_form_decode_multipart ( SoupMessage* msg, char const* file_control_name, char** filename, char** content_type, SoupBuffer** file )
         # direct_trim ( char** s )
            register_out_param($FuncDescr{$FuncInfoId}{"MnglName"}, $ParamPos, $ParamName, $ParamTypeId);
            next;
        }
        if($ParamPLevel==1 and isIntegerType($ParamFTypeName)
        and not is_const_type($ParamTypeName) and ($ParamName=~/((\A|_)(x|y)(\Z|_))|width|height|error|length|count/i or $ParamTypeName=~/bool/i
        or $ParamName=~/(\A|_)n(_|)(elem|item)/i or is_out_word($ParamName)))
        {# gail_misc_get_origins ( GtkWidget* widget, gint* x_window, gint* y_window, gint* x_toplevel, gint* y_toplevel )
         # glXGetFBConfigs ( Display* dpy, int screen, int* nelements )
            register_out_param($FuncDescr{$FuncInfoId}{"MnglName"}, $ParamPos, $ParamName, $ParamTypeId);
            next;
        }
        if(($ParamName=~/err/i and $ParamPLevel>=2 and $ParamTypeName=~/err/i)
        or ($ParamName=~/\A(error|err)(_|)(p|ptr)\Z/i and $ParamPLevel>=1))
        {# g_app_info_add_supports_type ( GAppInfo* appinfo, char const* content_type, GError** error )
         # rsvg_handle_new_from_data ( guint8 const* data, gsize data_len, GError** error )
            register_out_param($FuncDescr{$FuncInfoId}{"MnglName"}, $ParamPos, $ParamName, $ParamTypeId);
            next;
        }
        
        # strong reject
        next if(get_TypeType(get_FoundationTypeId($ReturnType_Id))!~/\A(Intrinsic|Enum)\Z/
        or $FuncDescr{$FuncInfoId}{"ShortName"}=~/\Q$ReturnType_Name_Short\E/
        or $FuncDescr{$FuncInfoId}{"ShortName"}=~/$ParamName(_|)get(_|)\w+/i
        or $ReturnType_Name_Short=~/pointer|ptr/i);
        next if($ParamPLevel<=0);
        next if($ParamPLevel==1 and get_TypeType($ParamFTypeId)=~/\A(Intrinsic|Enum|Array)\Z/ and $ParamName!~/error/i);
        next if($ParamPLevel==1 and (isOpaque($ParamFTypeId)
        or get_TypeName($ParamFTypeId)=~/\A(((struct |)(_IO_FILE|__FILE|FILE))|void)\Z/));
        next if(is_const_type($ParamTypeName) and $ParamPLevel<=2);
        next if($FuncDescr{$FuncInfoId}{"ShortName"}=~/memcpy/i);

        # allowed
        if((is_out_word($ParamName) and $FuncDescr{$FuncInfoId}{"ShortName"}!~/free/i
        #! xmlC14NDocSaveTo (xmlDocPtr doc, xmlNodeSetPtr nodes, int exclusive, xmlChar** inclusive_ns_prefixes, int with_comments, xmlOutputBufferPtr buf)
        # XGetMotionEvents (Display* display, Window w, Time start, Time stop, int* nevents_return)
        and ($ParamTypeName=~/\*/ or $ParamTypeName!~/ptr|pointer/i)
        
        # gsl_sf_bessel_il_scaled_array (int const lmax, double const x, double* result_array)
        and not grep(/\A(array)\Z/i, @{get_tokens($ParamName)}))
        
        # snd_card_get_name (int card, char** name)
        or $FuncDescr{$FuncInfoId}{"ShortName"}=~/(get|create)[_]*[0-9a-z]*$ParamName\Z/i
        
        # snd_config_get_ascii (snd_config_t const* config, char** value)
        or ($ParamPos==1 and $ParamName=~/value/i and $FuncDescr{$FuncInfoId}{"ShortName"}=~/$ParamName_Prev[_]*get/i)
        
        # poptDupArgv (int argc, char const** argv, int* argcPtr, char const*** argvPtr)
        or ($ParamName=~/ptr|pointer|(p\Z)/i and $ParamPLevel>=3))
        {
            my $IsTransit = 0;
            foreach my $Pos (keys(%{$FuncDescr{$FuncInfoId}{"Param"}}))
            {
                my $OtherParamTypeId = $FuncDescr{$FuncInfoId}{"Param"}{$Pos}{"type"};
                my $OtherParamName = $FuncDescr{$FuncInfoId}{"Param"}{$Pos}{"name"};
                next if($OtherParamName eq $ParamName);
                my $OtherParamFTypeId = get_FoundationTypeId($OtherParamTypeId);
                if($ParamFTypeId eq $OtherParamFTypeId)
                {
                    $IsTransit = 1;
                    last;
                }
            }
            if($IsTransit or get_TypeType($ParamFTypeId)=~/\A(Intrinsic|Enum|Array)\Z/)
            {
                $OutParamInterface_Pos_NoUsing{$FuncDescr{$FuncInfoId}{"MnglName"}}{$ParamPos}=1;
                $Interface_OutParam_NoUsing{$FuncDescr{$FuncInfoId}{"MnglName"}}{$ParamName}=1;
            }
            else
            {
                register_out_param($FuncDescr{$FuncInfoId}{"MnglName"}, $ParamPos, $ParamName, $ParamTypeId);
            }
        }
        ($ParamName_Prev, $ParamTypeId_Prev) = ($ParamName, $ParamTypeId);
    }
    if(getFuncBody($FuncInfoId) eq "defined")
    {
        $FuncDescr{$FuncInfoId}{"InLine"} = 1;
    }
    if(not $FuncDescr{$FuncInfoId}{"InLine"} and not $Interface_Library{$FuncDescr{$FuncInfoId}{"MnglName"}})
    {
        #delete $FuncDescr{$FuncInfoId};
        #return;
    }
    if($Interface_Library{$FuncDescr{$FuncInfoId}{"MnglName"}} and $FuncDescr{$FuncInfoId}{"Class"})
    {
        $ClassInTargetLibrary{$FuncDescr{$FuncInfoId}{"Class"}} = 1;
    }
    if($FuncDescr{$FuncInfoId}{"MnglName"}=~/\A_Z/ and $FuncDescr{$FuncInfoId}{"Class"})
    {
        if($FuncDescr{$FuncInfoId}{"Type"} eq "Function")
        {#static methods
            $FuncDescr{$FuncInfoId}{"Static"} = 1;
        }
    }
    if(hasThrow($FuncInfoId))
    {
        $FuncDescr{$FuncInfoId}{"Throw"} = 1;
    }
    if(getFuncLink($FuncInfoId) eq "Static")
    {
        $FuncDescr{$FuncInfoId}{"Static"} = 1;
    }
    if($FuncDescr{$FuncInfoId}{"Type"} eq "Function" and not $FuncDescr{$FuncInfoId}{"Class"})
    {
        $FuncDescr{$FuncInfoId}{"TypeId"} = getFuncTypeId($FuncInfoId);
        $Func_TypeId{$FuncDescr{$FuncInfoId}{"TypeId"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
    }
    if(not $FuncDescr{$FuncInfoId}{"PureVirt"})
    {
        if($FuncDescr{$FuncInfoId}{"Constructor"})
        {
            $Class_Constructors{$FuncDescr{$FuncInfoId}{"Class"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
        }
        elsif($FuncDescr{$FuncInfoId}{"Destructor"})
        {
            $Class_Destructors{$FuncDescr{$FuncInfoId}{"Class"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
        }
        else
        {
            if(get_TypeName($FuncDescr{$FuncInfoId}{"Return"}) ne "void")
            {
                my $DoNotUseReturn = 0;
                if(is_transit_function($FuncDescr{$FuncInfoId}{"ShortName"}))
                {
                    my $Return_FId = get_FoundationTypeId($FuncDescr{$FuncInfoId}{"Return"});
                    foreach my $Pos (keys(%{$FuncDescr{$FuncInfoId}{"Param"}}))
                    {
                        my $Param_FId = get_FoundationTypeId($FuncDescr{$FuncInfoId}{"Param"}{$Pos}{"type"});
                        if(($FuncDescr{$FuncInfoId}{"Param"}{$Pos}{"type"} eq $FuncDescr{$FuncInfoId}{"Return"})
                        or (get_TypeType($Return_FId)!~/\A(Intrinsic|Enum|Array)\Z/ and $Return_FId eq $Param_FId))
                        {
                            $DoNotUseReturn = 1;
                            last;
                        }
                    }
                }
                if(not $DoNotUseReturn)
                {
                    $ReturnTypeId_Interface{$FuncDescr{$FuncInfoId}{"Return"}}{$FuncDescr{$FuncInfoId}{"MnglName"}}=1;
                    my $Return_FId = get_FoundationTypeId($FuncDescr{$FuncInfoId}{"Return"});
                    my $PLevel = get_PointerLevel($Tid_TDid{$FuncDescr{$FuncInfoId}{"Return"}}, $FuncDescr{$FuncInfoId}{"Return"});
                    if(get_TypeType($Return_FId) ne "Intrinsic")
                    {
                        $BaseType_PLevel_Return{$Return_FId}{$PLevel}{$FuncDescr{$FuncInfoId}{"MnglName"}}=1;
                    }
                }
            }
        }
    }
    if($FuncDescr{$FuncInfoId}{"Class"}
    and not $FuncDescr{$FuncInfoId}{"Destructor"}
    and ($FuncDescr{$FuncInfoId}{"MnglName"}!~/C2E/ or not $FuncDescr{$FuncInfoId}{"Constructor"}))
    {
        $Interface_Overloads{$FuncDescr{$FuncInfoId}{"NameSpace"}}{getTypeName($FuncDescr{$FuncInfoId}{"Class"})}{$FuncDescr{$FuncInfoId}{"ShortName"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
    }
    if(not $Interface_Library{$FuncDescr{$FuncInfoId}{"MnglName"}} and keys(%{$TemplateInstance{$Tid_TDid{$FuncDescr{$FuncInfoId}{"Class"}}}{$FuncDescr{$FuncInfoId}{"Class"}}})>0)
    {
        my $ClassName = get_type_short_name(get_TypeName($FuncDescr{$FuncInfoId}{"Class"}));
        $InlineTemplateFunction{$ClassName}{$FuncInfoId} = 1;
    }
    if($FuncDescr{$FuncInfoId}{"Type"} eq "Method" and not $FuncDescr{$FuncInfoId}{"PureVirt"})
    {
        $Class_Method{$FuncDescr{$FuncInfoId}{"Class"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
    }
    $Header_Interface{$FuncDescr{$FuncInfoId}{"Header"}}{$FuncDescr{$FuncInfoId}{"MnglName"}} = 1;
    if(not $FuncDescr{$FuncInfoId}{"Class"} and not $LibraryMallocFunc
    and $Interface_Library{$FuncDescr{$FuncInfoId}{"MnglName"}}
    and $FuncDescr{$FuncInfoId}{"MnglName"} ne "malloc"
    and $FuncDescr{$FuncInfoId}{"ShortName"}!~/try/
    and $FuncDescr{$FuncInfoId}{"ShortName"}=~/(\A|_|\d)(malloc|alloc)(\Z|_|\d)/i
    and keys(%{$FuncDescr{$FuncInfoId}{"Param"}})==1
    and isIntegerType(get_TypeName($FuncDescr{$FuncInfoId}{"Param"}{0}{"type"})))
    {
        $LibraryMallocFunc = $FuncDescr{$FuncInfoId}{"MnglName"};
    }
    delete($FuncDescr{$FuncInfoId}{"Type"});
}

sub is_transit_function($)
{
    my $ShortName = $_[0];
    return 1 if($ShortName=~/(_|\A)dup(_|\Z)|(dup\Z)|_dup/i);
    return 1 if($ShortName=~/replace|merge|search|copy|append|duplicat|find|query|open|handle|first|next|entry/i);
    return grep(/\A(get|prev|last|from|of|dup)\Z/i, @{get_tokens($ShortName)});
}

sub get_TypeLib($)
{
    my $TypeId = $_[0];
    return $Cache{"get_TypeLib"}{$TypeId} if(defined $Cache{"get_TypeLib"}{$TypeId} and not defined $AuxType{$TypeId});
    my $Header = $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Header"};
    foreach my $Interface (sort keys(%{$Header_Interface{$Header}}))
    {
        if(my $SoLib = get_FileName($Interface_Library{$Interface}))
        {
            $Cache{"get_TypeLib"}{$TypeId} = $SoLib;
            return $SoLib;
        }
        elsif(my $SoLib = get_FileName($NeededInterface_Library{$Interface}))
        {
            $Cache{"get_TypeLib"}{$TypeId} = $SoLib;
            return $SoLib;
        }
    }
    $Cache{"get_TypeLib"}{$TypeId} = "unknown";
    return $Cache{"get_TypeLib"}{$TypeId};
}

sub getTypeShortName($)
{
    my $TypeName = $_[0];
    $TypeName=~s/\<.*\>//g;
    $TypeName=~s/.*\:\://g;
    return $TypeName;
}

sub getBackRef($)
{
    my $TypeInfo = $LibInfo{$_[0]}{"info"};
    if($TypeInfo=~/name[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getTypeId($)
{
    my $TypeInfo = $LibInfo{$_[0]}{"info"};
    if($TypeInfo=~/type[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncId($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    if($FuncInfo=~/type[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub setTypeMemb($$)
{
    my ($TypeId, $TypeAttr) = @_;
    my $TypeType = $TypeAttr->{"Type"};
    my $Position = 0;
    if($TypeType eq "Enum")
    {
        my $TypeMembInfoId = getEnumMembInfoId($TypeId);
        while($TypeMembInfoId)
        {
            $TypeAttr->{"Memb"}{$Position}{"value"} = getEnumMembVal($TypeMembInfoId);
            my $MembName = getEnumMembName($TypeMembInfoId);
            $TypeAttr->{"Memb"}{$Position}{"name"} = $MembName;
            $EnumMembers{$TypeAttr->{"Memb"}{$Position}{"name"}} = 1;
            $EnumMembName_Id{getTreeAttr($TypeMembInfoId, "valu")} = ($TypeAttr->{"NameSpace"})?$TypeAttr->{"NameSpace"}."::".$MembName:$MembName;
            $TypeMembInfoId = getNextMembInfoId($TypeMembInfoId);
            $Position += 1;
        }
    }
    elsif($TypeType=~/\A(Struct|Class|Union)\Z/)
    {
        my $TypeMembInfoId = getStructMembInfoId($TypeId);
        while($TypeMembInfoId)
        {
            if($LibInfo{$TypeMembInfoId}{"info_type"} ne "field_decl")
            {
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            my $StructMembName = getStructMembName($TypeMembInfoId);
            if($StructMembName=~/_vptr\./)
            {#virtual tables
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            if(not $StructMembName)
            {#base classes
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            $TypeAttr->{"Memb"}{$Position}{"type"} = getStructMembType($TypeMembInfoId);
            $TypeAttr->{"Memb"}{$Position}{"name"} = $StructMembName;
            $TypeAttr->{"Memb"}{$Position}{"access"} = getStructMembAccess($TypeMembInfoId);
            $TypeAttr->{"Memb"}{$Position}{"bitfield"} = getStructMembBitFieldSize($TypeMembInfoId);
            $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
            $Position += 1;
        }
    }
}

sub isAnonTypedef($)
{
    my $TypeId = $_[0];
    if(isAnon(getTypeName($TypeId)))
    {
        return 0;
    }
    else
    {
        return isAnon(getNameByInfo(anonTypedef($TypeId)));
    }
}

sub anonTypedef($)
{
    my $TypeId = $_[0];
    my $TypeMembInfoId;
    $TypeMembInfoId = getStructMembInfoId($TypeId);
    while($TypeMembInfoId)
    {
        my $NextTypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
        if(not $NextTypeMembInfoId)
        {
            last;
        }
        $TypeMembInfoId = $NextTypeMembInfoId;
        if($LibInfo{$TypeMembInfoId}{"info_type"} eq "type_decl" and getNameByInfo($TypeMembInfoId) eq getTypeName($TypeId))
        {
            return 0;
        }
    }
    return $TypeMembInfoId;
}

sub setFuncParams($)
{
    my $FuncInfoId = $_[0];
    my $ParamInfoId = getFuncParamInfoId($FuncInfoId);
    if(getFuncType($FuncInfoId) eq "Method")
    {
        $ParamInfoId = getNextElem($ParamInfoId);
    }
    my $Position = 0;
    while($ParamInfoId)
    {
        my $ParamTypeId = getFuncParamType($ParamInfoId);
        my $ParamName = getFuncParamName($ParamInfoId);
        last if(get_TypeName($ParamTypeId) eq "void");
        if(get_TypeType($ParamTypeId) eq "Restrict")
        {# delete restrict spec
            $ParamTypeId = getRestrictBase($ParamTypeId);
        }
        if(keys(%{$ExplicitTypedef{$ParamTypeId}})==1)
        {# replace the type to according explicit typedef
            $ParamTypeId = -$ParamTypeId;
        }
        if(not get_TypeName($ParamTypeId))
        {
            $FuncDescr{$FuncInfoId}{"Skip"} = 1;
            return;
        }
        if($ParamName eq "__vtt_parm"
        and get_TypeName($ParamTypeId) eq "void const**")
        {
            return;
        }
        $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"type"} = $ParamTypeId;
        $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"name"} = $ParamName;
        if(not $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"name"})
        {
            $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"name"} = "p".($Position+1);
        }
        $ParamInfoId = getNextElem($ParamInfoId);
        $Position += 1;
    }
    if(detect_nolimit_args($FuncInfoId))
    {
        $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"type"} = -1;
    }
}

sub detect_nolimit_args($)
{
    my $FuncInfoId = $_[0];
    my $FuncTypeId = getFuncTypeId($FuncInfoId);
    my $ParamListElemId = getFuncParamTreeListId($FuncTypeId);
    my $FunctionType = getFuncType($FuncInfoId);
    if(getFuncType($FuncInfoId) eq "Method")
    {
        $ParamListElemId = getNextElem($ParamListElemId);
    }
    my $HaveVoid = 0;
    my $Position = 0;
    while($ParamListElemId)
    {
        my $ParamTypeId = getTreeAttr($ParamListElemId, "valu");
        if(getTreeAttr($ParamListElemId, "purp"))
        {
            $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"default"} = 1;
        }
        if(get_TypeName($ParamTypeId) eq "void")
        {
            $HaveVoid = 1;
            last;
        }
        elsif(not defined $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"type"})
        {
            $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"type"} = $ParamTypeId;
            if(not $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"name"})
            {
                $FuncDescr{$FuncInfoId}{"Param"}{$Position}{"name"} = "p".($Position+1);
            }
        }
        $ParamListElemId = getNextElem($ParamListElemId);
        $Position += 1;
    }
    return ($Position>=1 and not $HaveVoid);
}

sub getFuncParamTreeListId($)
{
    my $FuncTypeId = $_[0];
    if($LibInfo{$FuncTypeId}{"info"}=~/prms[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getTreeAttr($$)
{
    my ($Id, $Attr) = @_;
    if($LibInfo{$Id}{"info"}=~/$Attr[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getRestrictBase($)
{
    my $TypeId = $_[0];
    my $TypeDeclId = getTypeDeclId($TypeId);
    my $BaseTypeId = $TypeDescr{$TypeDeclId}{$TypeId}{"BaseType"}{"Tid"};
    my $BaseTypeDeclId = $TypeDescr{$TypeDeclId}{$TypeId}{"BaseType"}{"TDid"};
    return $BaseTypeId;
}

sub setFuncAccess($)
{
    my $FuncInfoId = $_[0];
    if($LibInfo{$FuncInfoId}{"info"}=~/accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
        my $Access = $1;
        if($Access eq "prot")
        {
            $FuncDescr{$FuncInfoId}{"Protected"} = 1;
        }
        elsif($Access eq "priv")
        {
            $FuncDescr{$FuncInfoId}{"Private"} = 1;
        }
    }
}

sub setTypeAccess($$)
{
    my ($TypeId, $TypeAttr) = @_;
    my $TypeInfo = $LibInfo{$TypeId}{"info"};
    if($TypeInfo=~/accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
        my $Access = $1;
        if($Access eq "prot")
        {
            $TypeAttr->{"Protected"} = 1;
        }
        elsif($Access eq "priv")
        {
            $TypeAttr->{"Private"} = 1;
        }
    }
}

sub setFuncKind($)
{
    my $FuncInfoId = $_[0];
    if($LibInfo{$FuncInfoId}{"info"}=~/pseudo tmpl/)
    {
        $FuncDescr{$FuncInfoId}{"PseudoTemplate"} = 1;
    }
    elsif($LibInfo{$FuncInfoId}{"info"}=~/note[ ]*:[ ]*constructor /)
    {
        $FuncDescr{$FuncInfoId}{"Constructor"} = 1;
    }
    elsif($LibInfo{$FuncInfoId}{"info"}=~/note[ ]*:[ ]*destructor /)
    {
        $FuncDescr{$FuncInfoId}{"Destructor"} = 1;
    }
}

sub getFuncSpec($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$FuncInfoId}{"info"};
    if($FuncInfo=~/spec[ ]*:[ ]*pure /)
    {
        return "PureVirt";
    }
    elsif($FuncInfo=~/spec[ ]*:[ ]*virt /)
    {
        return "Virt";
    }
    else
    {
        if($FuncInfo=~/spec[ ]*:[ ]*([a-zA-Z]+) /)
        {
            return $1;
        }
        else
        {
            return "";
        }
    }
}

sub set_Class_And_Namespace($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$FuncInfoId}{"info"};
    if($FuncInfo=~/scpe[ ]*:[ ]*@(\d+) /)
    {
        my $NameSpaceInfoId = $1;
        if($LibInfo{$NameSpaceInfoId}{"info_type"} eq "namespace_decl")
        {
            if((my $NS = getNameSpace($FuncInfoId)) ne "\@skip\@")
            {
                $FuncDescr{$FuncInfoId}{"NameSpace"} = $NS;
            }
        }
        elsif($LibInfo{$NameSpaceInfoId}{"info_type"} eq "record_type")
        {
            $FuncDescr{$FuncInfoId}{"Class"} = $NameSpaceInfoId;
        }
    }
}

sub getFuncLink($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$FuncInfoId}{"info"};
    if($FuncInfo=~/link[ ]*:[ ]*static /)
    {
        return "Static";
    }
    else
    {
        if($FuncInfo=~/link[ ]*:[ ]*([a-zA-Z]+) /)
        {
            return $1;
        }
        else
        {
            return "";
        }
    }
}

sub getNextElem($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$FuncInfoId}{"info"};
    if($FuncInfo=~/chan[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncParamInfoId($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$FuncInfoId}{"info"};
    if($FuncInfo=~/args[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncParamType($)
{
    my $ParamInfoId = $_[0];
    my $ParamInfo = $LibInfo{$ParamInfoId}{"info"};
    if($ParamInfo=~/type[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncParamName($)
{
    my $ParamInfoId = $_[0];
    my $ParamInfo = $LibInfo{$ParamInfoId}{"info"};
    return "" if($ParamInfo!~/name[ ]*:[ ]*@(\d+) /);
    my $NameInfoId = $1;
    return "" if($LibInfo{$NameInfoId}{"info"}!~/strg[ ]*:[ ]*(.*)[ ]+lngt/);
    my $FuncParamName = $1;
    $FuncParamName=~s/[ ]+\Z//g;
    return $FuncParamName;
}

sub getEnumMembInfoId($)
{
    if($LibInfo{$_[0]}{"info"}=~/csts[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getStructMembInfoId($)
{
    if($LibInfo{$_[0]}{"info"}=~/flds[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getNameSpace($)
{
    my $TypeInfoId = $_[0];
    my $TypeInfo = $LibInfo{$TypeInfoId}{"info"};
    return "" if($TypeInfo!~/scpe[ ]*:[ ]*@(\d+) /);
    my $NameSpaceInfoId = $1;
    if($LibInfo{$NameSpaceInfoId}{"info_type"} eq "namespace_decl")
    {
        my $NameSpaceInfo = $LibInfo{$NameSpaceInfoId}{"info"};
        if($NameSpaceInfo=~/name[ ]*:[ ]*@(\d+) /)
        {
            my $NameSpaceId = $1;
            my $NameSpaceIdentifier = $LibInfo{$NameSpaceId}{"info"};
            return "" if($NameSpaceIdentifier!~/strg[ ]*:[ ]*(.*)[ ]+lngt/);
            my $NameSpace = $1;
            $NameSpace=~s/[ ]+\Z//g;
            if($NameSpace ne "::")
            {
                $NameSpaces{$NameSpace} = 1;
                if(my $BaseNameSpace = getNameSpace($NameSpaceInfoId))
                {
                    $NameSpace = $BaseNameSpace."::".$NameSpace;
                    $NameSpaces{$BaseNameSpace} = 1;
                }
                $NestedNameSpaces{$NameSpace} = 1;
                return $NameSpace;
            }
            else
            {
                return "";
            }
        }
        else
        {
            return "";
        }
    }
    elsif($LibInfo{$NameSpaceInfoId}{"info_type"} eq "record_type")
    {
        my %NameSpaceAttr = getTypeAttr(getTypeDeclId($NameSpaceInfoId), $NameSpaceInfoId);
        if(my $StructName = $NameSpaceAttr{"Name"})
        {
            return $StructName;
        }
        else
        {
            return "\@skip\@";
        }
    }
    else
    {
        return "";
    }
}

sub getNameSpaceId($)
{
    if($LibInfo{$_[0]}{"info"}=~/scpe[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getEnumMembName($)
{
    if($LibInfo{$_[0]}{"info"}=~/purp[ ]*:[ ]*@(\d+) /)
    {
        if($LibInfo{$1}{"info"}=~/strg[ ]*:[ ]*([^ ]+)/)
        {
            return $1;
        }
    }
}

sub getStructMembName($)
{
    if($LibInfo{$_[0]}{"info"}=~/name[ ]*:[ ]*@(\d+) /)
    {
        if($LibInfo{$1}{"info"}=~/strg[ ]*:[ ]*([^ ]+)/)
        {
            return $1;
        }
    }
}

sub getEnumMembVal($)
{
    if($LibInfo{$_[0]}{"info"}=~/valu[ ]*:[ ]*@(\d+) /)
    {
        if($LibInfo{$1}{"info"}=~/low[ ]*:[ ]*(-?\d+) /)
        {
            return $1;
        }
    }
    return "";
}

sub getSize($)
{
    if($LibInfo{$_[0]}{"info"}=~/size[ ]*:[ ]*@(\d+) /)
    {
        if($LibInfo{$1}{"info"}=~/low[ ]*:[ ]*(-?\d+) /)
        {
            return $1;
        }
        else
        {
            return "";
        }
    }
    else
    {
        return 0;
    }
}

sub getStructMembType($)
{
    if($LibInfo{$_[0]}{"info"}=~/type[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getStructMembBitFieldSize($)
{
    if($LibInfo{$_[0]}{"info"}=~/ bitfield /)
    {
        return getSize($_[0]);
    }
    else
    {
        return 0;
    }
}

sub getStructMembAccess($)
{
    if($LibInfo{$_[0]}{"info"}=~/accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
        my $Access = $1;
        if($Access eq "prot")
        {
            return "protected";
        }
        elsif($Access eq "priv")
        {
            return "private";
        }
        else
        {
            return "public";
        }
    }
    else
    {
        return "public";
    }
}

sub getNextMembInfoId($)
{
    if($LibInfo{$_[0]}{"info"}=~/chan[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getNextStructMembInfoId($)
{
    if($LibInfo{$_[0]}{"info"}=~/chan[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub fieldHasName($)
{
    my $TypeMembInfoId = $_[0];
    if($LibInfo{$TypeMembInfoId}{"info_type"} eq "field_decl")
    {
        if($LibInfo{$TypeMembInfoId}{"info"}=~/name[ ]*:[ ]*@(\d+) /)
        {
            return $1;
        }
        else
        {
            return "";
        }
    }
    else
    {
        return 0;
    }
}

sub getHeader($)
{
    my $TypeInfo = $LibInfo{$_[0]}{"info"};
    if($TypeInfo=~/srcp[ ]*:[ ]*([\w\-\<\>\.\+]+):(\d+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getLine($)
{
    if($LibInfo{$_[0]}{"info"}=~/srcp[ ]*:[ ]*([\w\-\<\>\.\+]+):(\d+) /)
    {
        return $2;
    }
    else
    {
        return "";
    }
}

sub getTypeTypeByTypeId($)
{
    my $TypeId = $_[0];
    my $TypeType = $LibInfo{$TypeId}{"info_type"};
    if($TypeType=~/integer_type|real_type|boolean_type|void_type|complex_type/)
    {
        return "Intrinsic";
    }
    elsif(isFuncPtr($TypeId))
    {
        return "FuncPtr";
    }
    elsif($TypeType eq "pointer_type")
    {
        return "Pointer";
    }
    elsif($TypeType eq "reference_type")
    {
        return "Ref";
    }
    elsif($TypeType eq "union_type")
    {
        return "Union";
    }
    elsif($TypeType eq "enumeral_type")
    {
        return "Enum";
    }
    elsif($TypeType eq "record_type")
    {
        return "Struct";
    }
    elsif($TypeType eq "array_type")
    {
        return "Array";
    }
    elsif($TypeType eq "function_type")
    {
        return "FunctionType";
    }
    else
    {
        return "Unknown";
    }
}

sub getNameByInfo($)
{
    my $TypeInfo = $LibInfo{$_[0]}{"info"};
    return "" if($TypeInfo!~/name[ ]*:[ ]*@(\d+) /);
    my $TypeNameInfoId = $1;
    return "" if($LibInfo{$TypeNameInfoId}{"info"}!~/strg[ ]*:[ ]*(.*)[ ]+lngt/);
    my $TypeName = $1;
    $TypeName=~s/[ ]+\Z//g;
    return $TypeName;
}

sub getFuncShortName($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    if($FuncInfo=~/ operator /)
    {
        if($FuncInfo=~/note[ ]*:[ ]*conversion /)
        {
            return "operator ".get_TypeName($FuncDescr{$_[0]}{"Return"});
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
        my $FuncNameInfoId = $1;
        return "" if($LibInfo{$FuncNameInfoId}{"info"}!~/strg[ ]*:[ ]*([^ ]*)[ ]+lngt/);
        return $1;
    }
}

sub getFuncMnglName($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    return "" if($FuncInfo!~/mngl[ ]*:[ ]*@(\d+) /);
    my $FuncMnglNameInfoId = $1;
    return "" if($LibInfo{$FuncMnglNameInfoId}{"info"}!~/strg[ ]*:[ ]*([^ ]*)[ ]+/);
    my $FuncMnglName = $1;
    $FuncMnglName=~s/[ ]+\Z//g;
    return $FuncMnglName;
}

sub getFuncReturn($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    return "" if($FuncInfo!~/type[ ]*:[ ]*@(\d+) /);
    my $FuncTypeInfoId = $1;
    return "" if($LibInfo{$FuncTypeInfoId}{"info"}!~/retn[ ]*:[ ]*@(\d+) /);
    my $FuncReturnTypeId = $1;
    if(get_TypeType($FuncReturnTypeId) eq "Restrict")
    {#delete restrict spec
        $FuncReturnTypeId = getRestrictBase($FuncReturnTypeId);
    }
    return $FuncReturnTypeId;
}

sub getFuncBody($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    if($FuncInfo=~/body[ ]*:[ ]*undefined(\ |\Z)/i)
    {
        return "undefined";
    }
#     elsif($FuncInfo=~/body[ ]*:[ ]*@(\d+)(\ |\Z)/i)
#     {
#         return "defined";
#     }
    else
    {
        return "defined";
    }
}

sub getFuncOrig($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    my $FuncOrigInfoId;
    if($FuncInfo=~/orig[ ]*:[ ]*@(\d+) /)
    {
        return $1;
    }
    else
    {
        return $_[0];
    }
}

sub getFuncName($)
{
    my $FuncInfoId = $_[0];
    return getFuncNameByUnmngl($FuncInfoId);
}

sub getVarName($)
{
    my $FuncInfoId = $_[0];
    return getFuncNameByUnmngl($FuncInfoId);
}

sub getFuncNameByUnmngl($)
{
    my $FuncInfoId = $_[0];
    return unmangle($FuncDescr{$FuncInfoId}{"MnglName"});
}

my %UnmangledName;
sub unmangle($)
{
    my $Interface_Name = $_[0];
    return "" if(not $Interface_Name);
    if($UnmangledName{$Interface_Name})
    {
        return $UnmangledName{$Interface_Name};
    }
    if($Interface_Name!~/\A_Z/)
    {
        return $Interface_Name;
    }
    my $Unmangled = `$CPP_FILT $Interface_Name`;
    chomp($Unmangled);
    $UnmangledName{$Interface_Name} = $Unmangled;
    return $Unmangled;
}

sub getFuncType($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    return "" if($FuncInfo!~/type[ ]*:[ ]*@(\d+) /);
    my $FunctionType = $LibInfo{$1}{"info_type"};
    if($FunctionType eq "method_type")
    {
        return "Method";
    }
    elsif($FunctionType eq "function_type")
    {
        return "Function";
    }
    else
    {
        return $FunctionType;
    }
}

sub hasThrow($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    return 0 if($FuncInfo!~/type[ ]*:[ ]*@(\d+) /);
    return getTreeAttr($1, "unql");
}

sub getFuncTypeId($)
{
    my $FuncInfo = $LibInfo{$_[0]}{"info"};
    if($FuncInfo=~/type[ ]*:[ ]*@(\d+)( |\Z)/)
    {
        return $1;
    }
    else
    {
        return 0;
    }
}

sub cover_typedefs_in_template($)
{
    my $ClassId = $_[0];
    return "" if(not $ClassId);
    my $ClassName = get_TypeName($ClassId);
    my $ClassDId = $Tid_TDid{$ClassId};
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$TemplateInstance{$ClassDId}{$ClassId}}))
    {
        my $ParamTypeId = $TemplateInstance{$ClassDId}{$ClassId}{$Param_Pos};
        my $ParamTypeName = get_TypeName($ParamTypeId);
        my $ParamFTypeId = get_FoundationTypeId($ParamTypeId);
        if($ParamTypeName=~/<|>|::/ and get_TypeType($ParamFTypeId)=~/\A(Class|Struct)\Z/)
        {
            if(my $Typedef_Id = detect_typedef($ParamTypeId))
            {
                $ClassName = cover_by_typedef($ClassName, $ParamFTypeId, $Typedef_Id);
            }
        }
    }
    return $ClassName;
}

sub detect_typedef($)
{
    my $Type_Id = $_[0];
    return "" if(not $Type_Id);
    my $Typedef_Id = get_base_typedef($Type_Id);
    $Typedef_Id = get_type_typedef(get_FoundationTypeId($Type_Id)) if(not $Typedef_Id);
    return $Typedef_Id;
}

sub get_Signature($)
{
    my $Interface = $_[0];
    return $Cache{"get_Signature"}{$Interface} if($Cache{"get_Signature"}{$Interface});
    if(not $CompleteSignature{$Interface})
    {
        if($Interface=~/\A_Z/)
        {
            $Cache{"get_Signature"}{$Interface} = $tr_name{$Interface};
            return $Cache{"get_Signature"}{$Interface};
        }
        else
        {
            $Cache{"get_Signature"}{$Interface} = $Interface;
            return $Interface;
        }
    }
    my ($Func_Signature, @Param_Types_FromUnmangledName) = ();
    my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
    if($Interface=~/\A_Z/)
    {
        if(my $ClassId = $CompleteSignature{$Interface}{"Class"})
        {
            if(get_TypeName($ClassId)=~/<|>|::/
            and my $Typedef_Id = detect_typedef($ClassId))
            {
                $ClassId = $Typedef_Id;
            }
            $Func_Signature = get_TypeName($ClassId)."::".((($CompleteSignature{$Interface}{"Destructor"}))?"~":"").$ShortName;
        }
        else
        {
            $Func_Signature = $ShortName;
        }
        @Param_Types_FromUnmangledName = get_Signature_Parts($tr_name{$Interface}, 0);
    }
    else
    {
        $Func_Signature = $Interface;
    }
    my @ParamArray = ();
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        next if($Pos eq "");
        my $ParamTypeId = $CompleteSignature{$Interface}{"Param"}{$Pos}{"type"};
        next if(not $ParamTypeId);
        my $ParamTypeName = get_TypeName($ParamTypeId);
        $ParamTypeName = $Param_Types_FromUnmangledName[$Pos] if(not $ParamTypeName);
        my $ParamFTypeId = get_FoundationTypeId($ParamTypeId);
        if($ParamTypeName=~/<|>|::/ and get_TypeType($ParamFTypeId)=~/\A(Class|Struct)\Z/)
        {
            if(my $Typedef_Id = detect_typedef($ParamTypeId))
            {
                $ParamTypeName = cover_by_typedef($ParamTypeName, $ParamFTypeId, $Typedef_Id);
            }
        }
        if(my $ParamName = $CompleteSignature{$Interface}{"Param"}{$Pos}{"name"})
        {
            push(@ParamArray, create_member_decl($ParamTypeName, $ParamName));
        }
        else
        {
            push(@ParamArray, $ParamTypeName);
        }
    }
    if(not $CompleteSignature{$Interface}{"Data"})
    {
        if($Interface=~/\A_Z/)
        {
            if($CompleteSignature{$Interface}{"Constructor"})
            {
                if($Interface=~/C1E/)
                {
                    $Func_Signature .= " [in-charge]";
                }
                elsif($Interface=~/C2E/)
                {
                    $Func_Signature .= " [not-in-charge]";
                }
            }
            elsif($CompleteSignature{$Interface}{"Destructor"})
            {
                if($Interface=~/D1E/)
                {
                    $Func_Signature .= " [in-charge]";
                }
                elsif($Interface=~/D2E/)
                {
                    $Func_Signature .= " [not-in-charge]";
                }
                elsif($Interface=~/D0E/)
                {
                    $Func_Signature .= " [in-charge-deleting]";
                }
            }
        }
        $Func_Signature .= " (".join(", ", @ParamArray).")";
    }
    if($Interface=~/\A_ZNK/)
    {
        $Func_Signature .= " const";
    }
    if(my $ReturnTId = $CompleteSignature{$Interface}{"Return"} and defined $ShowReturnTypes)
    {
        my $ReturnTypeName = get_TypeName($ReturnTId);
        my $ReturnFTypeId = get_FoundationTypeId($ReturnTId);
        if($ReturnTypeName=~/<|>|::/ and get_TypeType($ReturnFTypeId)=~/\A(Class|Struct)\Z/)
        {
            if(my $Typedef_Id = detect_typedef($ReturnTId))
            {
                $ReturnTypeName = cover_by_typedef($ReturnTypeName, $ReturnFTypeId, $Typedef_Id);
            }
        }
        $Func_Signature .= ":".$ReturnTypeName;
    }
    $Cache{"get_Signature"}{$Interface} = $Func_Signature;
    return $Func_Signature;
}

sub htmlSpecChars($)
{
    my $Str = $_[0];
    $Str=~s/\&([^#])/&amp;$1/g;
    $Str=~s/</&lt;/g;
    $Str=~s/>/&gt;/g;
    $Str=~s/([^ ])( )([^ ])/$1\@ALONE_SP\@$3/g;
    $Str=~s/ /&nbsp;/g;
    $Str=~s/\@ALONE_SP\@/ /g;
    $Str=~s/\n/<br\/>/g;
    return $Str;
}

sub highLight_Signature($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 0, 0);
}

sub highLight_Signature_Italic($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 1, 0);
}

sub highLight_Signature_Italic_Color($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 1, 1);
}

sub highLight_Signature_PPos_Italic($$$$)
{
    my ($Signature, $Parameter_Position, $ItalicParams, $ColorParams) = @_;
    my ($Begin, $End, $Return) = ();
    if($Signature=~s/([^:]):([^:].+?)\Z/$1/g)
    {
        $Return = $2;
    }
    if($Signature=~/(.+?)\(.*\)(| const)\Z/)
    {
        ($Begin, $End) = ($1, $2);
    }
    my @Parts = ();
    my $Part_Num = 0;
    foreach my $Part (get_Signature_Parts($Signature, 1))
    {
        $Part=~s/\A\s+|\s+\Z//g;
        my ($Part_Styled, $ParamName) = ($Part, "");
        if($Part=~/\([\*]+(\w+)\)/i)
        {#func-ptr
            $ParamName = $1;
        }
        elsif($Part=~/(\w+)[\,\)]*\Z/i)
        {
            $ParamName = $1;
        }
        if(not $ParamName)
        {
            push(@Parts, $Part);
            next;
        }
        if($ItalicParams and not $TName_Tid{$Part})
        {
            if(($Parameter_Position ne "") and ($Part_Num == $Parameter_Position))
            {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span class='focus_p'>$ParamName</span>$2!ig;
            }
            elsif($ColorParams)
            {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span class='color_p'>$ParamName</span>$2!ig;
            }
            else
            {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span style='font-style:italic;'>$ParamName</span>$2!ig;
            }
        }
        $Part_Styled = "<span style='white-space:nowrap;'>".$Part_Styled."</span>";
        push(@Parts, $Part_Styled);
        $Part_Num += 1;
    }
    $Signature = $Begin."<span class='int_p'>"."(&nbsp;".join(" ", @Parts).(@Parts?"&nbsp;":"").")"."</span>".$End;
    if($Return)
    {
        $Signature .= "<span class='int_p' style='margin-left:8px;font-weight:bold;'>:</span>"."<span class='int_p' style='margin-left:8px;font-weight:normal;'>".$Return."</span>";
    }
    $Signature =~ s!\[\]![<span style='padding-left:2px;'>]</span>!g;
    $Signature =~ s!operator=!operator<span style='padding-left:2px'>=</span>!g;
    $Signature =~ s!(\[in-charge\]|\[not-in-charge\]|\[in-charge-deleting\])!<span style='color:Black;font-weight:normal;'>$1</span>!g;
    return $Signature;
}

sub get_Signature_Parts($$)
{
    my ($Signature, $Comma) = @_;
    my @Parts = ();
    my ($Bracket_Num, $Bracket2_Num) = (0, 0);
    my $Parameters = $Signature;
    if($Signature=~/&gt;|&lt;/)
    {
        $Parameters=~s/&gt;/>/g;
        $Parameters=~s/&lt;/</g;
    }
    my $Part_Num = 0;
    if($Parameters=~s/.+?\((.*)\)(| const)\Z/$1/)
    {
        foreach my $Pos (0 .. length($Parameters) - 1)
        {
            my $Symbol = substr($Parameters, $Pos, 1);
            $Bracket_Num += 1 if($Symbol eq "(");
            $Bracket_Num -= 1 if($Symbol eq ")");
            $Bracket2_Num += 1 if($Symbol eq "<");
            $Bracket2_Num -= 1 if($Symbol eq ">");
            if($Symbol eq "," and $Bracket_Num==0 and $Bracket2_Num==0)
            {
                $Parts[$Part_Num] .= $Symbol if($Comma);
                $Part_Num += 1;
            }
            else
            {
                $Parts[$Part_Num] .= $Symbol;
            }
        }
        if($Signature=~/&gt;|&lt;/)
        {
            foreach my $Part (@Parts)
            {
                $Part=~s/\>/&gt;/g;
                $Part=~s/\</&lt;/g;
            }
        }
        return @Parts;
    }
    else
    {
        return ();
    }
}

sub getVarNameByAttr($)
{
    my $FuncInfoId = $_[0];
    my $VarName = "";
    return "" if(not $FuncDescr{$FuncInfoId}{"ShortName"});
    if($FuncDescr{$FuncInfoId}{"Class"})
    {
        $VarName .= get_TypeName($FuncDescr{$FuncInfoId}{"Class"})."::";
    }
    $VarName .= $FuncDescr{$FuncInfoId}{"ShortName"};
    return $VarName;
}

sub isNotAnon($)
{
    return (not isAnon($_[0]));
}

sub isAnon($)
{
    return (($_[0]=~/\.\_\d+/) or ($_[0]=~/anon-/));
}

sub unmangled_Compact($$)
#Throws all non-essential (for C++ language) whitespaces from a string.  If 
#the whitespace is essential it will be replaced with exactly one ' ' 
#character. Works correctly only for unmangled names.
#If level > 1 is supplied, can relax its intent to compact the string.
{
  my $result=$_[0];
  my $level = $_[1] || 1;
  my $o1 = ($level>1)?' ':'';
  #First, we reduce all spaces that we can
  my $coms='[-()<>:*&~!|+=%@~"?.,/[^'."']";
  my $coms_nobr='[-()<:*&~!|+=%@~"?.,'."']";
  my $clos='[),;:\]]';
  $result=~s/^\s+//gm;
  $result=~s/\s+$//gm;
  $result=~s/((?!\n)\s)+/ /g;
  $result=~s/(\w+)\s+($coms+)/$1$o1$2/gm;
  #$result=~s/(\w)(\()/$1$o1$2/gm if $o1;
  $result=~s/($coms+)\s+(\w+)/$1$o1$2/gm;
  $result=~s/(\()\s+(\w)/$1$2/gm if $o1;
  $result=~s/(\w)\s+($clos)/$1$2/gm;
  $result=~s/($coms+)\s+($coms+)/$1 $2/gm;
  $result=~s/($coms_nobr+)\s+($coms+)/$1$o1$2/gm;
  $result=~s/($coms+)\s+($coms_nobr+)/$1$o1$2/gm;
  #don't forget about >> and <:.  In unmangled names global-scope modifier 
  #is not used, so <: will always be a digraph and requires no special treatment.
  #We also try to remove other parts that are better to be removed here than in other places
  #double-cv
  $result=~s/\bconst\s+const\b/const/gm;
  $result=~s/\bvolatile\s+volatile\b/volatile/gm;
  $result=~s/\bconst\s+volatile\b\s+const\b/const volatile/gm;
  $result=~s/\bvolatile\s+const\b\s+volatile\b/const volatile/gm;
  #Place cv in proper order
  $result=~s/\bvolatile\s+const\b/const volatile/gm;
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
    $CorrectName = unmangled_Compact($CorrectName, 1);
    $CorrectName = unmangled_PostProcess($CorrectName);
    return $CorrectName;
}

sub set_header_namespaces($)
{
    my $AbsPath = $_[0];
    return "" if($COMMON_LANGUAGE eq "C");
    return "" if(not $AbsPath or not -e $AbsPath);
    return keys(%{$Header_NameSpaces{$AbsPath}}) if(keys(%{$Header_NameSpaces{$AbsPath}}));
    return "" if(isCyclical(\@RecurInclude, $AbsPath));
    push(@RecurInclude, $AbsPath);
    %{$Header_NameSpaces{$AbsPath}} = ();
    foreach my $Line (split(/\n/, `grep -n namespace $AbsPath`))
    {
        if($Line=~/namespace\s+([:\w]+)\s*({|\Z)/)
        {
            $Header_NameSpaces{$AbsPath}{$1} = 1;
        }
    }
    foreach my $Include (keys(%{$Header_Includes{$AbsPath}}))
    {
        if(my $HDir = find_in_dependencies($Include))
        {
            foreach my $NS (set_header_namespaces($HDir."/".$Include))
            {
                $Header_NameSpaces{$AbsPath}{$NS} = 1;
            }
        }
    }
    pop(@RecurInclude);
    return keys(%{$Header_NameSpaces{$AbsPath}});
}

sub get_namespace_additions(@)
{
    my ($Additions, $AddNameSpaceId) = ("", 1);
    foreach my $NS (@_)
    {
        next if(not $NS);
        my ($TypeDecl_Prefix, $TypeDecl_Suffix) = ();
        foreach my $NS_Part (split(/::/, $NS))
        {
            $TypeDecl_Prefix .= "namespace $NS_Part\{";
            $TypeDecl_Suffix .= "}";
        }
        my $TypeDecl = $TypeDecl_Prefix."typedef int tg_add_type_$AddNameSpaceId;".$TypeDecl_Suffix;
        my $FuncDecl = "$NS\:\:tg_add_type_$AddNameSpaceId tg_add_func_$AddNameSpaceId(){return 0;};";
        $Additions.="  $TypeDecl\n  $FuncDecl\n";
        $AddNameSpaceId+=1;
    }
    return $Additions;
}

sub getDump_AllInOne()
{
    return if(not keys(%Headers));
    mkpath(".tmpdir");
    `rm -fr *\.tu`;
    my %IncDir = ();
    my $TmpHeader = ".tmpdir/.$TargetLibraryName.h";
    unlink($TmpHeader);
    open(LIB_HEADER, ">$TmpHeader");
    foreach my $Header_Path (sort {int($Include_Preamble{$a}{"Position"})<=>int($Include_Preamble{$b}{"Position"})} keys(%Include_Preamble))
    {
        print LIB_HEADER "  #include <$Header_Path>\n";
        if(not keys(%Include_Paths))
        {# autodetecting dependencies
            foreach my $Dir (get_HeaderDeps($Header_Path))
            {
                $IncDir{$Dir}=1;
            }
        }
    }
    my %Dump_NameSpaces = ();
    foreach my $Header_Path (sort {int($Headers{$a}{"Position"})<=>int($Headers{$b}{"Position"})} keys(%Headers))
    {
        next if($Include_Preamble{$Header_Path});
        print LIB_HEADER "  #include <$Header_Path>\n";
        if(not keys(%Include_Paths))
        {# autodetecting dependencies
            foreach my $Dir (get_HeaderDeps($Header_Path))
            {
                $IncDir{$Dir}=1;
            }
        }
        foreach my $NS (keys(%{$Header_NameSpaces{$Header_Path}}))
        {
            $Dump_NameSpaces{$NS} = 1;
        }
    }
    print LIB_HEADER "\n".get_namespace_additions(keys(%Dump_NameSpaces))."\n";
    close(LIB_HEADER);
    appendFile($LOG_PATH, "Header file \'$TmpHeader\' with the following content will be compiled for creating gcc syntax tree:\n".readFile($TmpHeader)."\n");
    my $Headers_Depend = "";
    if(not keys(%Include_Paths))
    {# autodetecting dependencies
        foreach my $Dir (sort_include_paths(sort {get_depth($a)<=>get_depth($b)} sort {$b cmp $a} keys(%IncDir)))
        {
            $Headers_Depend .= " -I".esc($Dir);
        }
    }
    else
    {
        foreach my $Dir (sort {get_depth($a)<=>get_depth($b)} sort {$b cmp $a} keys(%Header_Dependency))
        {
            $Headers_Depend .= " -I".esc($Dir);
        }
    }
    foreach my $Dir (keys(%Add_Include_Paths))
    {
        next if($Header_Dependency{$Dir} or $IncDir{$Dir});
        $Headers_Depend .= " -I".esc($Dir);
    }
    my $SyntaxTreeCmd = "$GPP_PATH -fdump-translation-unit ".esc($TmpHeader)." $CompilerOptions_Headers $Headers_Depend";
    appendFile($LOG_PATH, "Command for compilation:\n  $SyntaxTreeCmd\n\n");
    system($SyntaxTreeCmd." >>".esc($LOG_PATH)." 2>&1");
    if($?)
    {
        print STDERR "\n\nERROR: some errors have occurred, see log file \'$LOG_PATH\' for details\n\n";
    }
    $ConstantsSrc = cmd_preprocessor($TmpHeader, $Headers_Depend, "C++", "define\\ \\|undef\\ \\|#[ ]\\+[0-9]\\+ \".*\"");
    my $Cmd_Find_TU = "find . -maxdepth 1 -name \".".esc($TargetLibraryName)."\.h*\.tu\"";
    rmtree(".tmpdir");
    return (split(/\n/, `$Cmd_Find_TU`))[0];
}

sub parse_constants()
{
    my $CurHeader = "";
    foreach my $String (split(/\n/, $ConstantsSrc))
    {#detecting public and private constants
        if($String=~/#[ \t]+\d+[ \t]+\"(.+)\"/)
        {
            $CurHeader=$1;
        }
        if($String=~/\#[ \t]*define[ \t]+([_A-Z0-9a-z]+)[ \t]+(.*)[ \t]*\Z/)
        {
            my ($Name, $Value) = ($1, $2);
            if($Name=~/[a-z]/)
            {
                $AllDefines{$Name} = 1;
            }
            elsif(not $Constants{$Name}{"Access"})
            {
                $Constants{$Name}{"Access"} = "public";
                $Constants{$Name}{"Value"} = $Value;
                $Constants{$Name}{"Header"} = $CurHeader;
                $Constants{$Name}{"HeaderName"} = get_FileName($CurHeader);
            }
        }
        elsif($String=~/\#[ \t]*undef[ \t]+([_A-Z]+)[ \t]*/)
        {
            my $Name = $1;
            $Constants{$Name}{"Access"} = "private";
        }
    }
    foreach my $Constant (keys(%Constants))
    {
        if($Constants{$Constant}{"Access"} eq "private"
        or not $Constants{$Constant}{"Value"} or $Constant=~/_h\Z/i)
        {
            delete($Constants{$Constant});
        }
        else
        {
            delete($Constants{$Constant}{"Access"});
        }
    }
}

sub parseHeaders_AllInOne()
{
    print "\rheader(s) analysis: [25.00%]";
    my $DumpPath = getDump_AllInOne();
    if(not $DumpPath)
    {
        print STDERR "\nERROR: can't create gcc syntax tree for header(s)\n\n";
        exit(1);
    }
    print "\rheader(s) analysis: [30.00%]";
    parse_constants();
    print "\rheader(s) analysis: [35.00%]";
    getInfo($DumpPath);
}

sub prepareInterfaces()
{
    my (@MnglNames, @UnMnglNames) = ();
    foreach my $FuncInfoId (sort keys(%FuncDescr))
    {
        if($FuncDescr{$FuncInfoId}{"MnglName"}=~/\A_Z/)
        {
            push(@MnglNames, $FuncDescr{$FuncInfoId}{"MnglName"});
        }
    }
    if($#MnglNames > -1)
    {
        @UnMnglNames = reverse(unmangleArray(@MnglNames));
        foreach my $FuncInfoId (sort keys(%FuncDescr))
        {
            if($FuncDescr{$FuncInfoId}{"MnglName"}=~/\A_Z/)
            {
                my $UnmangledName = pop(@UnMnglNames);
                $tr_name{$FuncDescr{$FuncInfoId}{"MnglName"}} = $UnmangledName;
            }
        }
    }
    foreach my $FuncInfoId (keys(%FuncDescr))
    {
        my $MnglName = $FuncDescr{$FuncInfoId}{"MnglName"};
        next if(not $MnglName);
        next if($MnglName=~/\A_Z/ and $tr_name{$MnglName}=~/\.\_\d+/);
        if($FuncDescr{$FuncInfoId}{"Data"})
        {
            if($MnglName=~/\A_Z/)
            {
                $GlobVarNames{$tr_name{$MnglName}} = 1;
            }
            else
            {
                 $GlobVarNames{$FuncDescr{$FuncInfoId}{"ShortName"}} = 1;
            }
        }
        else
        {
            if($MnglName=~/\A_Z/)
            {
                $MethodNames{$FuncDescr{$FuncInfoId}{"ShortName"}} = 1;
            }
            else
            {
                $FuncNames{$FuncDescr{$FuncInfoId}{"ShortName"}} = 1;
            }
        }
        %{$CompleteSignature{$MnglName}} = %{$FuncDescr{$FuncInfoId}};
        delete($FuncDescr{$FuncInfoId});
    }
    %FuncDescr=();
    foreach my $Interface (keys(%CompleteSignature))
    {#detecting out-parameters by function name and parameter type
        my $ReturnType_Id = $CompleteSignature{$Interface}{"Return"};
        my $ReturnType_Name_Short = get_TypeName($ReturnType_Id);
        while($ReturnType_Name_Short=~s/(\*|\&)([^<>()]+|)\Z/$2/g){};
        next if(get_TypeType(get_FoundationTypeId($ReturnType_Id))!~/\A(Intrinsic|Enum)\Z/
        or $CompleteSignature{$Interface}{"ShortName"}=~/\Q$ReturnType_Name_Short\E/);
        my $Func_ShortName = $CompleteSignature{$Interface}{"ShortName"};
        next if($Func_ShortName!~/(new|create|open|top|update|start)/i and not is_alloc_func($Func_ShortName)
        and ($Func_ShortName!~/init/i or get_TypeName($ReturnType_Id) ne "void") and not $UserDefinedOutParam{$Interface});
        next if(not keys(%{$CompleteSignature{$Interface}{"Param"}}));
        if(not detect_out_parameters($Interface, 1))
        {
            detect_out_parameters($Interface, 0);
        }
    }
}

sub detect_out_parameters($$)
{
    my ($Interface, $Strong) = @_;
    foreach my $ParamPos (sort{int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $ParamTypeId = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"type"};
        my $ParamName = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"name"};
        if(isOutParam($ParamTypeId, $ParamPos, $Interface, $Strong))
        {
            register_out_param($Interface, $ParamPos, $ParamName, $ParamTypeId);
            return 1;
        }
    }
    return 0;
}

sub get_outparam_candidate($$)
{
    my ($Interface, $Right) = @_;
    my $Func_ShortName = $CompleteSignature{$Interface}{"ShortName"};
    if($Right)
    {
        if($Func_ShortName=~/([a-z0-9]+)(_|)(new|open|init)\Z/i)
        {
            return $1;
        }
    }
    else
    {
        if($Func_ShortName=~/(new|open|init)(_|)([a-z0-9]+)/i)
        {
            return $3;
        }
    }
}

sub isOutParam($$$$)
{
    my ($Param_TypeId, $ParamPos, $Interface, $Strong) = @_;
    my $Param_Name = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"name"};
    my $PLevel = get_PointerLevel($Tid_TDid{$Param_TypeId}, $Param_TypeId);
    my $TypeName = get_TypeName($Param_TypeId);
    my $Param_FTypeId = get_FoundationTypeId($Param_TypeId);
    my $Param_FTypeName = get_TypeName($Param_FTypeId);
    $Param_FTypeName=~s/\A(struct|union) //g;
    my $Param_FTypeType = get_TypeType($Param_FTypeId);
    return 0 if($PLevel<=0);
    return 0 if($PLevel==1 and isOpaque($Param_FTypeId));
    return 0 if($Param_FTypeType!~/\A(Struct|Union|Class)\Z/);
    return 0 if(keys(%{$BaseType_PLevel_Return{$Param_FTypeId}{$PLevel}}));
    return 0 if(keys(%{$ReturnTypeId_Interface{$Param_TypeId}}));
    return 0 if(is_const_type($TypeName));
    my $Func_ShortName = $CompleteSignature{$Interface}{"ShortName"};
    return 1 if($Func_ShortName=~/\A$Param_FTypeName(_|)init/);
    if($Strong)
    {
        if(my $Candidate = get_outparam_candidate($Interface, 1))
        {
            return ($Param_Name=~/\Q$Candidate\E/i);
        }
    }
    if(my $Candidate = get_outparam_candidate($Interface, 0))
    {
        return 0 if($Param_Name!~/\Q$Candidate\E/i);
    }
    return 1 if(($Func_ShortName=~/(new|create|open|start)/i and $Func_ShortName!~/restart|test/)
    or is_alloc_func($Func_ShortName));
    return 1 if($Func_ShortName=~/top/i and $PLevel==2);
    # snd_config_top
    return 1 if($UserDefinedOutParam{$Interface}{$Param_Name}
    or $UserDefinedOutParam{$Interface}{$ParamPos+1});
    return 1 if($Func_ShortName=~/update/i and $Func_ShortName!~/add|append/i
    and $Func_ShortName=~/$Param_Name/i and $PLevel>=1);
    return 1 if($Func_ShortName=~/init/i
    and (keys(%{$CompleteSignature{$Interface}{"Param"}})==1
          or number_of_simple_params($Interface)==keys(%{$CompleteSignature{$Interface}{"Param"}})-1));
    
    return 0;
}

sub number_of_simple_params($)
{
    my $Interface = $_[0];
    return 0 if(not $Interface);
    my $Count = 0;
    foreach my $Pos (keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $TypeId = $CompleteSignature{$Interface}{"Param"}{$Pos}{"type"};
        my $PName = $CompleteSignature{$Interface}{"Param"}{$Pos}{"name"};
        if(get_TypeType($TypeId)=~/\A(Intrinsic|Enum)\Z/
        or isString($TypeId, $PName, $Interface))
        {
            $Count+=1;
        }
    }
    return $Count;
}

sub get_OutParamFamily($$)
{
    my ($TypeId, $IncludeOuter) = @_;
    my $FTypeId = get_FoundationTypeId($TypeId);
    if(get_TypeType($FTypeId)=~/Struct|Union|Class/)
    {
        my @Types = ($IncludeOuter and ($TypeId ne $FTypeId))?($TypeId, $FTypeId):($FTypeId);
        while(my $ReducedTypeId = reduce_pointer_level($TypeId))
        {
            push(@Types, $ReducedTypeId);
            $TypeId = $ReducedTypeId;
        }
        return @Types;
    }
    else
    {
        my @Types = ($IncludeOuter)?($TypeId):();
        my $ReducedTypeId = reduce_pointer_level($TypeId);
        if(get_TypeType($ReducedTypeId) eq "Typedef")
        {
            push(@Types, $ReducedTypeId);
        }
        return @Types;
    }
    return ();
}

sub is_alloc_func($)
{
    my $FuncName = $_[0];
    return ($FuncName=~/alloc/i and $FuncName!~/dealloc|realloc/i);
}

sub initializeClass_PureVirtFunc()
{
    foreach my $Interface (keys(%CompleteSignature))
    {
        if($CompleteSignature{$Interface}{"PureVirt"})
        {
            initializeSubClasses_PureVirtFunc($CompleteSignature{$Interface}{"Class"}, $Interface);
        }
    }
}

sub initializeSubClasses_PureVirtFunc($$)
{
    my ($ClassId, $Interface) = @_;
    return if(not $ClassId or not $Interface);
    my $TargetSuffix = getFuncSuffix($Interface);
    foreach my $InterfaceCandidate (keys(%{$Class_Method{$ClassId}}))
    {
        if($TargetSuffix eq getFuncSuffix($InterfaceCandidate))
        {
            if($CompleteSignature{$Interface}{"Constructor"})
            {
                return if($CompleteSignature{$InterfaceCandidate}{"Constructor"});
            }
            elsif($CompleteSignature{$Interface}{"Destructor"})
            {
                return if($CompleteSignature{$InterfaceCandidate}{"Destructor"});
            }
            else
            {
                return if($CompleteSignature{$Interface}{"ShortName"} eq $CompleteSignature{$InterfaceCandidate}{"ShortName"});
            }
        }
    }
    $Class_PureVirtFunc{get_TypeName($ClassId)}{$Interface} = 1;
    foreach my $SubClass_Id (keys(%{$Class_SubClasses{$ClassId}}))
    {
        initializeSubClasses_PureVirtFunc($SubClass_Id, $Interface);
    }
}

sub getFuncSuffix($)
{
    my $Interface = $_[0];
    my $ClassId = $CompleteSignature{$Interface}{"Class"};
    my $ClassName = get_TypeName($ClassId);
    my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
    my $Suffix = $tr_name{$Interface};
    $Suffix=~s/\A\Q$ClassName\E\:\:[~]*\Q$ShortName\E[ ]*//g;
    return $Suffix;
}

sub cleanName($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    foreach my $Token (sort keys(%Operator_Indication))
    {
        my $Token_Translate = $Operator_Indication{$Token};
        $Name=~s/\Q$Token_Translate\E/\_$Token\_/g;
    }
    $Name=~s/\,/_/g;
    $Name=~s/\./_p_/g;
    $Name=~s/\:/_/g;
    $Name=~s/\]/_rb_/g;
    $Name=~s/\[/_lb_/g;
    $Name=~s/\(/_/g;
    $Name=~s/\)/_/g;
    $Name=~s/ /_/g;
    while($Name=~/__/)
    {
        $Name=~s/__/_/g;
    }
    $Name=~s/\_\Z//;
    return $Name;
}

sub getTestPath($)
{
    my $Interface = $_[0];
    if($Interface_LibGroup{$Interface})
    {
        return $TEST_SUITE_PATH."/groups/".clean_libgroup_name($Interface_LibGroup{$Interface})."/".$Interface;
    }
    else
    {
        my $ClassName = get_TypeName($CompleteSignature{$Interface}{"Class"});
        my $Header = $CompleteSignature{$Interface}{"Header"};
        $Header=~s/(\.\w+)\Z//g;
        my $TestPath = $TEST_SUITE_PATH."/groups/".$Header."/".(($ClassName)?"classes/".get_type_short_name($ClassName):"functions")."/".$Interface;
        return $TestPath;
    }
}

sub getLibGroupPath($$$)
{
    my ($C1, $C2, $TwoComponets) = @_;
    return () if(not $C1);
    $C1=clean_libgroup_name($C1);
    if($TwoComponets)
    {
        if($C2)
        {
            return ($TEST_SUITE_PATH."/$TargetLibraryName-t2c/", $C1, $C2);
        }
        else
        {
            return ($TEST_SUITE_PATH."/$TargetLibraryName-t2c/", $C1, "functions");
        }
    }
    else
    {
        return ($TEST_SUITE_PATH."/$TargetLibraryName-t2c/", "", $C1);
    }
}

sub getLibGroupName($$)
{
    my ($C1, $C2) = @_;
    return "" if(not $C1);
    if($C2)
    {
        return $C2;
    }
    else
    {
        return $C1;
    }
}

sub clean_libgroup_name($)
{
    my $LibGroup = $_[0];
    $LibGroup=~s/(\.\w+)\Z//g;
    $LibGroup=~s/( |-)/_/g;
    $LibGroup=~s/\([^()]+\)//g;
    $LibGroup=~s/[_]{2,}/_/g;
    return $LibGroup;
}

sub interfaceFilter($)
{
    my $Interface = $_[0];
    return 0 if($SkipInterfaces{$Interface});
    foreach my $SkipPattern (keys(%SkipInterfaces_Pattern))
    {
        return 0 if($Interface=~/$SkipPattern/);
    }
    return 0 if($Interface=~/\A_tg_inln_tmpl_\d+/);
    return 0 if(not $CompleteSignature{$Interface}{"Header"});
    return 0 if($CompleteSignature{$Interface}{"Data"});
    if(not defined $CheckStdCxxInterfaces and not $TargetInterfaceName and not $STDCXX_TESTING)
    {
        return 0 if(get_TypeName($CompleteSignature{$Interface}{"Class"})=~/\Astd(::|\Z)/);
        my ($Header_Inc, $Header_Path) = identify_header($CompleteSignature{$Interface}{"Header"});
        return 0 if($Header_Path=~/\A\Q$MAIN_CPP_DIR\E(\/|\Z)/);
    }
    if($CompleteSignature{$Interface}{"Constructor"})
    {
        my $ClassId = $CompleteSignature{$Interface}{"Class"};
        return ( not ($Interface=~/C1E/ and ($CompleteSignature{$Interface}{"Protected"} or isAbstractClass($ClassId))) );
    }
    elsif($CompleteSignature{$Interface}{"Destructor"})
    {
        my $ClassId = $CompleteSignature{$Interface}{"Class"};
        return ( not ($Interface=~/D0E|D1E/ and ($CompleteSignature{$Interface}{"Protected"} or isAbstractClass($ClassId))) );
    }
    else
    {
        return 1;
    }
}

sub addHeaders($$)
{
    my ($NewHeaders, $OldHeaders) = @_;
    $OldHeaders = [] if(not $OldHeaders);
    $NewHeaders = [] if(not $NewHeaders);
    my (%Old, @Result_Before, @Result_After) = ();
    foreach (@{$OldHeaders})
    {
        $Old{$_} = 1;
        @Result_After = (@Result_After, $_) if($_);
    }
    foreach my $NewHeader (@{$NewHeaders})
    {
        if(not $Old{$NewHeader} and $NewHeader)
        {
            @Result_Before = (@Result_Before, $NewHeader);
        }
    }
    my @Result = (@Result_Before, @Result_After);
    return \@Result;
}

sub getTypeHeaders($)
{
    my $TypeId = $_[0];
    return [] if(not $TypeId);
    my %Type = delete_quals($Tid_TDid{$TypeId}, $TypeId);
    my $Headers = [$TypeDescr{$Type{"TDid"}}{$Type{"Tid"}}{"Header"}];
    $Headers = addHeaders(tmplInstHeaders($Type{"Tid"}), $Headers);
    $Headers = addHeaders([$TemplateHeader{$Type{"ShortName"}}], $Headers);
    if($Type{"NameSpaceClassId"})
    {
        $Headers = addHeaders(getTypeHeaders($Type{"NameSpaceClassId"}), $Headers);
    }
    return $Headers;
}

sub get_TypeName($)
{
    my $TypeId = $_[0];
    return $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Name"};
}

sub get_TypeType($)
{
    my $TypeId = $_[0];
    return $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Type"};
}

sub get_TypeSize($)
{
    my $TypeId = $_[0];
    return $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Size"};
}

sub get_TypeHeader($)
{
    my $TypeId = $_[0];
    return $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Header"};
}

sub isNotInCharge($)
{
    my $Interface = $_[0];
    return ($CompleteSignature{$Interface}{"Constructor"}
    and $Interface=~/C2E/);
}

sub isInCharge($)
{
    my $Interface = $_[0];
    return ($CompleteSignature{$Interface}{"Constructor"}
    and $Interface=~/C1E/);
}

sub replace_c2c1($)
{
    my $Interface = $_[0];
    if($CompleteSignature{$Interface}{"Constructor"})
    {
        $Interface=~s/C2E/C1E/;
    }
    return $Interface;
}

sub getSubClassName($)
{
    my $ClassName = $_[0];
    return getSubClassBaseName($ClassName)."_SubClass";
}

sub getSubClassBaseName($)
{
    my $ClassName = $_[0];
    $ClassName=~s/\:\:|<|>|\(|\)|\[|\]|\ |,|\*/_/g;
    $ClassName=~s/[_][_]+/_/g;
    return $ClassName;
}

sub getNumOfParams($)
{
    my $Interface = $_[0];
    my @Params = keys(%{$CompleteSignature{$Interface}{"Param"}});
    return ($#Params + 1);
}

sub sort_byCriteria($$)
{
    my ($Interfaces, $Criteria) = @_;
    my (@NewInterfaces1, @NewInterfaces2) = ();
    foreach my $Interface (@{$Interfaces})
    {
        if(compare_byCriteria($Interface, $Criteria))
        {
            push(@NewInterfaces1, $Interface);
        }
        else
        {
            push(@NewInterfaces2, $Interface);
        }
    }
    @{$Interfaces} = (@NewInterfaces1, @NewInterfaces2);
}

sub get_int_prefix($)
{
    my $Interface = $_[0];
    if($Interface=~/\A([a-z0-9]+)_[a-z0-9]/i)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub sort_byLibrary($$)
{
    my ($Interfaces, $Library) = @_;
    return if(not $Library);
    my $LibPrefix = $SoLib_IntPrefix{$Library};
    my (@NewInterfaces1, @NewInterfaces2, @NewInterfaces3) = ();
    foreach my $Interface (@{$Interfaces})
    {
        my $IntPrefix = get_int_prefix($Interface);
        if(get_FileName($Interface_Library{$Interface}) eq $Library
        or get_FileName($NeededInterface_Library{$Interface}) eq $Library)
        {
            push(@NewInterfaces1, $Interface);
        }
        elsif(not $Interface_Library{$Interface}
        and not $NeededInterface_Library{$Interface})
        {
            push(@NewInterfaces1, $Interface);
        }
        elsif($LibPrefix and ($LibPrefix eq $IntPrefix))
        {
            push(@NewInterfaces2, $Interface);
        }
        else
        {
            push(@NewInterfaces3, $Interface);
        }
    }
    @{$Interfaces} = (@NewInterfaces1, @NewInterfaces2, @NewInterfaces3);
}

sub get_tokens($)
{
    my $Word = $_[0];
    return $Cache{"get_tokens"}{$Word} if(defined $Cache{"get_tokens"}{$Word});
    my $Copy = $Word;
    my %Tokens = ();
    while($Word=~s/([A-Z]*[a-z]+|\d+)([_ ]+|\Z)/$2/)
    {
        $Tokens{lc($1)}=1;
    }
    $Word=$Copy;
    while($Word=~s/([A-Z]{2,})//)
    {
        $Tokens{lc($1)}=1;
    }
    $Word=$Copy;
    while($Word=~s/([A-Z]{2,}|\A|_| )([a-z]{2,})//)
    {
        $Tokens{lc($2)}=1;
    }
    $Word=$Copy;
    while($Word=~s/([A-Z][a-z]+)([A-Z]|\Z)/$2/)
    {
        $Tokens{lc($1)}=1;
    }
    my @Tkns = keys(%Tokens);
    $Cache{"get_tokens"}{$Word} = \@Tkns;
    return \@Tkns;
}

sub sort_byName($$$)
{
    my ($Words, $KeyWords, $Type) = @_;
    my %Word_Coincidence = ();
    foreach my $Word (@{$Words})
    {
        $Word_Coincidence{$Word} = get_word_coinsidence($Word, $KeyWords);
    }
    @{$Words} = sort {$Word_Coincidence{$b} <=> $Word_Coincidence{$a}} @{$Words};
    if($Type eq "Constants")
    {
        my @Words_With_Tokens = ();
        foreach my $Word (@{$Words})
        {
            if($Word_Coincidence{$Word}>0)
            {
                push(@Words_With_Tokens, $Word);
            }
        }
        @{$Words} = @Words_With_Tokens;
    }
}

sub sort_FileOpen($)
{
    my @Interfaces = @{$_[0]};
    my (@FileOpen, @Other) = ();
    foreach my $Interface (sort {length($a) <=> length($b)} @Interfaces)
    {
        if($CompleteSignature{$Interface}{"ShortName"}=~/fopen/i)
        {
            push(@FileOpen, $Interface);
        }
        else
        {
            push(@Other, $Interface);
        }
    }
    @{$_[0]} = (@FileOpen, @Other);
}

sub get_word_coinsidence($$)
{
    my ($Word1, $Word2) = @_;
    my (%WordTokens1, %WordTokens2) = ();
    foreach (@{get_tokens($Word1)})
    {
        $WordTokens1{$_} = 1;
    }
    foreach (@{get_tokens($Word2)})
    {
        $WordTokens2{$_} = 1;
    }
    my $Weight=keys(%WordTokens1);
    my $WordCoincidence = 0;
    foreach (keys(%WordTokens1))
    {
        if(defined $WordTokens2{$_})
        {
            $WordCoincidence+=$Weight*3;
        }
        $Weight-=1;
    }
    return $WordCoincidence;
}

sub compare_byCriteria($$)
{
    my ($Interface, $Criteria) = @_;
    if($Criteria eq "DeleteSmth")
    {
        return $CompleteSignature{$Interface}{"ShortName"}!~/delete|remove|destroy|cancel/i;
    }
    elsif($Criteria eq "InLine")
    {
        return $CompleteSignature{$Interface}{"InLine"};
    }
    elsif($Criteria eq "WithParams")
    {
        return getNumOfParams($Interface);
    }
    elsif($Criteria eq "WithoutParams")
    {
        return getNumOfParams($Interface)==0;
    }
    elsif($Criteria eq "Public")
    {
        return (not $CompleteSignature{$Interface}{"Protected"});
    }
    elsif($Criteria eq "Default")
    {
        return ($Interface=~/default/i);
    }
    elsif($Criteria eq "VaList")
    {
        return ($Interface!~/valist/i);
    }
    elsif($Criteria eq "NotInCharge")
    {
        return (not isNotInCharge($Interface));
    }
    elsif($Criteria eq "Class")
    {
        return (get_TypeName($CompleteSignature{$Interface}{"Class"}) ne "QApplication");
    }
    elsif($Criteria eq "Data")
    {
        return (not $CompleteSignature{$Interface}{"Data"});
    }
    elsif($Criteria eq "FirstParam_Intrinsic")
    {
        if(defined $CompleteSignature{$Interface}{"Param"}
        and defined $CompleteSignature{$Interface}{"Param"}{"0"})
        {
            my $FirstParamType_Id = $CompleteSignature{$Interface}{"Param"}{"0"}{"type"};
            return (get_TypeType(get_FoundationTypeId($FirstParamType_Id)) eq "Intrinsic");
        }
        else
        {
            return 0;
        }
    }
    elsif($Criteria eq "FirstParam_Enum")
    {
        if(defined $CompleteSignature{$Interface}{"Param"}
        and defined $CompleteSignature{$Interface}{"Param"}{"0"})
        {
            my $FirstParamType_Id = $CompleteSignature{$Interface}{"Param"}{"0"}{"type"};
            return (get_TypeType(get_FoundationTypeId($FirstParamType_Id)) eq "Enum");
        }
        else
        {
            return 0;
        }
    }
    elsif($Criteria eq "FirstParam_PKc")
    {
        if(defined $CompleteSignature{$Interface}{"Param"}
        and defined $CompleteSignature{$Interface}{"Param"}{"0"})
        {
            my $FirstParamType_Id = $CompleteSignature{$Interface}{"Param"}{"0"}{"type"};
            return (get_TypeName($FirstParamType_Id) eq "char const*");
        }
        else
        {
            return 0;
        }
    }
    elsif($Criteria eq "FirstParam_char")
    {
        if(defined $CompleteSignature{$Interface}{"Param"}
        and defined $CompleteSignature{$Interface}{"Param"}{"0"})
        {
            my $FirstParamType_Id = $CompleteSignature{$Interface}{"Param"}{"0"}{"type"};
            return (get_TypeName($FirstParamType_Id) eq "char");
        }
        else
        {
            return 0;
        }
    }
    elsif($Criteria eq "Operator")
    {
        return ($tr_name{$Interface}!~/operator[^a-z]/i);
    }
    elsif($Criteria eq "Library")
    {
        return ($Interface_Library{$Interface} or $ClassInTargetLibrary{$CompleteSignature{$Interface}{"Class"}});
    }
    elsif($Criteria eq "Internal")
    {
        return ($CompleteSignature{$Interface}{"ShortName"}!~/\A_/);
    }
    elsif($Criteria eq "Internal")
    {
        return ($CompleteSignature{$Interface}{"ShortName"}!~/debug/i);
    }
    elsif($Criteria eq "FileManipulating")
    {
        return 0 if($CompleteSignature{$Interface}{"ShortName"}=~/fopen|file/);
        foreach my $ParamPos (keys(%{$CompleteSignature{$Interface}{"Param"}}))
        {
            my $ParamTypeId = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"type"};
            my $ParamName = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"name"};
            if(isString($ParamTypeId, $ParamName, $Interface))
            {
                return 0 if(isStr_FileName($ParamPos, $ParamName, $CompleteSignature{$Interface}{"ShortName"})
                or isStr_Dir($ParamName, $CompleteSignature{$Interface}{"ShortName"}));
            }
            else
            {
                return 0 if(isFD($ParamTypeId, $ParamName));
            }
        }
        return 1;
    }
    else
    {
        return 1;
    }
}

sub sort_byRecurParams($)
{
    my @Interfaces = @{$_[0]};
    my (@Other, @WithRecurParams) = ();
    foreach my $Interface (@Interfaces)
    {
        my $WithRecur = 0;
        foreach my $ParamPos (keys(%{$CompleteSignature{$Interface}{"Param"}}))
        {
            my $ParamType_Id = $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"type"};
            if(isCyclical(\@RecurTypeId, get_TypeStackId($ParamType_Id)))
            {
                $WithRecur=1;
                last;
            }
        }
        if($WithRecur)
        {
            push(@WithRecurParams, $Interface);
        }
        else
        {
            if($CompleteSignature{$Interface}{"ShortName"}!~/copy|duplicate/i)
            {
                push(@Other, $Interface);
            }
        }
    }
    @{$_[0]} = (@Other, @WithRecurParams);
    return $#WithRecurParams;
}

sub sort_LibMainFunc($)
{
    my @Interfaces = @{$_[0]};
    my (@First, @Other) = ();
    foreach my $Interface (@Interfaces)
    {
        my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
        foreach my $Prefix (keys(%Library_Prefixes))
        {
            if($Library_Prefixes{$Prefix}>=10)
            {
                $ShortName=~s/\A$Prefix//g;
            }
        }
        if($ShortName=~/\A(create|default|get|new|init)\Z/i)
        {
            push(@First, $Interface);
        }
        else
        {
             push(@Other, $Interface);
        }
    }
    @{$_[0]} = (@First, @Other);
}

sub sort_CreateParam($$)
{
    my @Interfaces = @{$_[0]};
    my $KeyWords = $_[1];
    foreach my $Prefix (keys(%Library_Prefixes))
    {
        if($Library_Prefixes{$Prefix}>=10)
        {
            $KeyWords=~s/(\A| )$Prefix/$1/g;
        }
    }
    $KeyWords=~s/(\A|_)(new|get|create|default|alloc|init)(_|\Z)//g;
    my (@First, @Other) = ();
    foreach my $Interface (@Interfaces)
    {
        my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
        if($ShortName=~/create|default|get|new|init/i
        and get_word_coinsidence($KeyWords, $ShortName)>0)
        {
            push(@First, $Interface);
        }
        else
        {
             push(@Other, $Interface);
        }
    }
    @{$_[0]} = (@First, @Other);
}

sub sort_GetCreate($)
{
    my @Interfaces = @{$_[0]};
    my (@CreateWithoutParams, @Root, @Create, @Default, @New, @Alloc, @Init, @Get, @Other, @Copy, @Wait) = ();
    foreach my $Interface (@Interfaces)
    {
        my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
        if($ShortName=~/create|default|new/i
        and not keys(%{$CompleteSignature{$Interface}{"Param"}}))
        {
            push(@CreateWithoutParams, $Interface);
        }
        elsif($ShortName=~/root/i
        and $ShortName=~/default/i)
        {
            push(@Root, $Interface);
        }
        elsif($ShortName=~/create/i)
        {
            push(@Create, $Interface);
        }
        elsif($ShortName=~/default/i
        and $ShortName!~/get/i)
        {
            push(@Default, $Interface);
        }
        elsif($ShortName=~/new/i)
        {
            push(@New, $Interface);
        }
        elsif(is_alloc_func($ShortName))
        {
            push(@Alloc, $Interface);
        }
        elsif($ShortName=~/init/i)
        {
            push(@Init, $Interface);
        }
        elsif($ShortName=~/get/i)
        {
            push(@Get, $Interface);
        }
        elsif($ShortName=~/copy/i)
        {
            push(@Copy, $Interface);
        }
        elsif($ShortName=~/wait/i)
        {
            push(@Wait, $Interface);
        }
        else
        {
            push(@Other, $Interface);
        }
    }
    @{$_[0]} = (@CreateWithoutParams, @Root, @Create, @Default, @New, @Alloc, @Init, @Get, @Other, @Copy, @Wait);
}

sub get_CompatibleInterfaces($$$)
{
    my ($TypeId, $Method, $KeyWords) = @_;
    return () if(not $TypeId or not $Method);
    my @Ints = compatible_interfaces($TypeId, $Method, $KeyWords);
    sort_byRecurParams(\@Ints);
    return @Ints;
}

sub compatible_interfaces($$$)
{
    my ($TypeId, $Method, $KeyWords) = @_;
    return () if(not $TypeId or not $Method);
    return @{$Cache{"compatible_interfaces"}{$TypeId}{$Method}{$KeyWords}} if(defined $Cache{"compatible_interfaces"}{$TypeId}{$Method}{$KeyWords} and not defined $RandomCode and not defined $AuxType{$TypeId});
    my @CompatibleInterfaces = ();
    if($Method eq "Construct")
    {
        foreach my $Constructor (keys(%{$Class_Constructors{$TypeId}}))
        {
            @CompatibleInterfaces = (@CompatibleInterfaces, $Constructor);
        }
    }
    elsif($Method eq "Return")
    {
        foreach my $Interface (keys(%{$ReturnTypeId_Interface{$TypeId}}))
        {
            next if($CompleteSignature{$Interface}{"PureVirt"});
            @CompatibleInterfaces = (@CompatibleInterfaces, $Interface);
        }
    }
    elsif($Method eq "OutParam")
    {
        foreach my $Interface (keys(%{$OutParam_Interface{$TypeId}}))
        {
            next if($CompleteSignature{$Interface}{"Protected"});
            next if($CompleteSignature{$Interface}{"PureVirt"});
            @CompatibleInterfaces = (@CompatibleInterfaces, $Interface);
        }
    }
    elsif($Method eq "OnlyReturn")
    {
        foreach my $Interface (keys(%{$ReturnTypeId_Interface{$TypeId}}))
        {
            next if($CompleteSignature{$Interface}{"PureVirt"});
            next if($CompleteSignature{$Interface}{"Data"});
            @CompatibleInterfaces = (@CompatibleInterfaces, $Interface);
        }
    }
    elsif($Method eq "OnlyData")
    {
        foreach my $Interface (keys(%{$ReturnTypeId_Interface{$TypeId}}))
        {
            next if(not $CompleteSignature{$Interface}{"Data"});
            @CompatibleInterfaces = (@CompatibleInterfaces, $Interface);
        }
    }
    else
    {
        @{$Cache{"compatible_interfaces"}{$TypeId}{$Method}{$KeyWords}} = ();
        return ();
    }
    if($#CompatibleInterfaces==-1)
    {
        @{$Cache{"compatible_interfaces"}{$TypeId}{$Method}{$KeyWords}} = ();
        return ();
    }
    elsif($#CompatibleInterfaces==0)
    {
        @{$Cache{"compatible_interfaces"}{$TypeId}{$Method}{$KeyWords}} = @CompatibleInterfaces;
        return @CompatibleInterfaces;
    }
    #Sort by name
    @CompatibleInterfaces = sort @CompatibleInterfaces;
    @CompatibleInterfaces = sort {$CompleteSignature{$a}{"ShortName"} cmp $CompleteSignature{$b}{"ShortName"}} (@CompatibleInterfaces);
    @CompatibleInterfaces = sort {length($CompleteSignature{$a}{"ShortName"}) <=> length($CompleteSignature{$b}{"ShortName"})} (@CompatibleInterfaces);
    #Sort by amount of parameters
    if(defined $MinimumCode)
    {
        @CompatibleInterfaces = sort {int(keys(%{$CompleteSignature{$a}{"Param"}}))<=>int(keys(%{$CompleteSignature{$b}{"Param"}}))} (@CompatibleInterfaces);
    }
    elsif(defined $MaximumCode)
    {
        @CompatibleInterfaces = sort {int(keys(%{$CompleteSignature{$b}{"Param"}}))<=>int(keys(%{$CompleteSignature{$a}{"Param"}}))} (@CompatibleInterfaces);
    }
    else
    {
        sort_byCriteria(\@CompatibleInterfaces, "FirstParam_Intrinsic");
        sort_byCriteria(\@CompatibleInterfaces, "FirstParam_char");
        sort_byCriteria(\@CompatibleInterfaces, "FirstParam_PKc");
        sort_byCriteria(\@CompatibleInterfaces, "FirstParam_Enum") if(get_TypeName($TypeId)!~/char|string/i or $Method ne "Construct");
        @CompatibleInterfaces = sort {int(keys(%{$CompleteSignature{$a}{"Param"}}))<=>int(keys(%{$CompleteSignature{$b}{"Param"}}))} (@CompatibleInterfaces);
        sort_byCriteria(\@CompatibleInterfaces, "WithoutParams");
        sort_byCriteria(\@CompatibleInterfaces, "WithParams") if($Method eq "Construct");
    }
    sort_byCriteria(\@CompatibleInterfaces, "Operator");
    sort_byCriteria(\@CompatibleInterfaces, "FileManipulating");
    if($Method ne "Construct")
    {
        sort_byCriteria(\@CompatibleInterfaces, "Class");
        sort_byName(\@CompatibleInterfaces, [$KeyWords], "Interfaces");
        sort_FileOpen(\@CompatibleInterfaces) if(get_TypeName(get_FoundationTypeId($TypeId))=~/\A(struct |)(_IO_FILE|__FILE|FILE)\Z/);
        sort_GetCreate(\@CompatibleInterfaces);
        sort_CreateParam(\@CompatibleInterfaces, $KeyWords);
        sort_LibMainFunc(\@CompatibleInterfaces);
        sort_byCriteria(\@CompatibleInterfaces, "Data");
        sort_byCriteria(\@CompatibleInterfaces, "Library");
        sort_byCriteria(\@CompatibleInterfaces, "Internal");
        sort_byCriteria(\@CompatibleInterfaces, "Debug");
        sort_byLibrary(\@CompatibleInterfaces, get_TypeLib($TypeId)) if(get_TypeName($TypeId) ne "GType");
    }
    if(defined $RandomCode)
    {
        @CompatibleInterfaces = mix_array(@CompatibleInterfaces);
    }
    sort_byCriteria(\@CompatibleInterfaces, "Public");
    sort_byCriteria(\@CompatibleInterfaces, "NotInCharge") if($Method eq "Construct");
    @{$Cache{"compatible_interfaces"}{$TypeId}{$Method}{$KeyWords}} = @CompatibleInterfaces if(not defined $RandomCode);
    return @CompatibleInterfaces;
}

sub mix_array(@)
{
    my @Array = @_;
    return sort {2 * rand($#Array) - $#Array} @_;
}

sub getSomeConstructor($)
{
    my $ClassId = $_[0];
    my @Constructors = get_CompatibleInterfaces($ClassId, "Construct", "");
    return $Constructors[0];
}

sub getFirstEnumMember($)
{
    my $EnumId = $_[0];
    my %Enum = get_Type($Tid_TDid{$EnumId}, $EnumId);
    my $FirstMemberName = $Enum{"Memb"}{0}{"name"};
    my $NameSpace = $Enum{"NameSpace"};
    if($NameSpace and $FirstMemberName and getIntLang($TestedInterface) eq "C++")
    {
        $FirstMemberName = $NameSpace."::".$FirstMemberName;
    }
    return $FirstMemberName;
}

sub getTypeParString($)
{
    my $Interface = $_[0];
    my $NumOfParams = getNumOfParams($Interface);
    if($NumOfParams == 0)
    {
        return ("()", "()", "()");
    }
    else
    {
        my (@TypeParList, @ParList, @TypeList) = ();
        foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
        {
            next if(apply_default_value($Interface, $Param_Pos) and not $CompleteSignature{$Interface}{"PureVirt"});
            my $ParamName = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"name"};
            $ParamName = "p".($Param_Pos + 1) if(not $ParamName);
            my $TypeId = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"type"};
            my %Type = get_Type($Tid_TDid{$TypeId}, $TypeId);
            next if($Type{"Name"} eq "...");
            push(@ParList, $ParamName);
            push(@TypeList, $Type{"Name"});
            push(@TypeParList, create_member_decl($Type{"Name"}, $ParamName));
        }
        my $TypeParString .= "(".create_list(\@TypeParList, "    ").")";
        my $ParString .= "(".create_list(\@ParList, "    ").")";
        my $TypeString .= "(".create_list(\@TypeList, "    ").")";
        return ($TypeParString, $ParString, $TypeString);
    }
}

sub getValueClass($)
{
    my $Value = $_[0];
    $Value=~/([^()"]+)\(.*\)[^()]*/;
    my $ValueClass = $1;
    $ValueClass=~s/[ ]+\Z//g;
    if(get_TypeIdByName($ValueClass))
    {
        return $ValueClass;
    }
    else
    {
        return "";
    }
}

sub get_FoundationType($$)
{
    my ($TypeDId, $TypeId) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_FoundationType"}{$TypeDId}{$TypeId} and not defined $AuxType{$TypeId})
    {
        return %{$Cache{"get_FoundationType"}{$TypeDId}{$TypeId}};
    }
    return "" if(not $TypeDescr{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    return %Type if($Type{"Type"} eq "Array");
    %Type = get_FoundationType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"});
     %{$Cache{"get_FoundationType"}{$TypeDId}{$TypeId}} = %Type;
    return %Type;
}

sub get_BaseType($$)
{
    my ($TypeDId, $TypeId) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_BaseType"}{$TypeDId}{$TypeId} and not defined $AuxType{$TypeId})
    {
        return %{$Cache{"get_BaseType"}{$TypeDId}{$TypeId}};
    }
    return "" if(not $TypeDescr{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    %Type = get_BaseType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"});
     %{$Cache{"get_BaseType"}{$TypeDId}{$TypeId}} = %Type;
    return %Type;
}

sub get_FoundationTypeId($)
{
    my $TypeId = $_[0];
    return $Cache{"get_FoundationTypeId"}{$TypeId} if(defined $Cache{"get_FoundationTypeId"}{$TypeId} and not defined $AuxType{$TypeId});
    my %BaseType = get_FoundationType($Tid_TDid{$TypeId}, $TypeId);
    $Cache{"get_FoundationTypeId"}{$TypeId} = $BaseType{"Tid"};
    return $BaseType{"Tid"};
}

sub create_SubClass($)
{
    my $ClassId = $_[0];
    return () if(not $ClassId);
    my ($Declaration, $Headers, $Code);
    foreach my $Constructor (keys(%{$UsedConstructors{$ClassId}}))
    {
        if(isNotInCharge($Constructor)
        and my $InChargeConstructor = replace_c2c1($Constructor))
        {
            if($CompleteSignature{$InChargeConstructor})
            {
                $UsedConstructors{$ClassId}{$Constructor} = 0;
                $UsedConstructors{$ClassId}{$InChargeConstructor} = 1;
            }
        }
    }
    $Headers = addHeaders(getTypeHeaders($ClassId), $Headers);
    my $ClassName = ($Class_SubClassTypedef{$ClassId})?get_TypeName($Class_SubClassTypedef{$ClassId}):get_TypeName($ClassId);
    my $ClassNameChild = getSubClassName($ClassName);
    $Declaration .= "class $ClassNameChild".": public $ClassName\n{\n";
    $Declaration .= "public:\n";
    if(not keys(%{$UsedConstructors{$ClassId}}))
    {
        if(my $SomeConstructor = getSomeConstructor($ClassId))
        {
            $UsedConstructors{$ClassId}{$SomeConstructor} = 1;
        }
    }
    if(defined $UsedConstructors{$ClassId}
    and keys(%{$UsedConstructors{$ClassId}}))
    {
        foreach my $Constructor (sort keys(%{$UsedConstructors{$ClassId}}))
        {
            next if(not $Constructor);
            my $PreviousBlock = $CurrentBlock;
            $CurrentBlock = $Constructor;
            if($UsedConstructors{$ClassId}{$Constructor})
            {
                my ($TypeParString, $ParString, $TypeString) = getTypeParString($Constructor);
                $TypeParString = alignCode($TypeParString, "    ", 1);
                $ParString = alignCode($ParString, "        ", 1);
                $Declaration .= "    $ClassNameChild"."$TypeParString\:$ClassName"."$ParString\{\}\n\n";
                foreach my $Param_Pos (sort {int($b)<=>int($a)} keys(%{$CompleteSignature{$Constructor}{"Param"}}))
                {
                    my $Param_TypeId = $CompleteSignature{$Constructor}{"Param"}{$Param_Pos}{"type"};
                    my $Param_Name = $CompleteSignature{$Constructor}{"Param"}{$Param_Pos}{"name"};
                    $Param_Name = "p".($Param_Pos + 1) if(not $Param_Name);
                    $ValueCollection{$CurrentBlock}{$Param_Name} = $Param_TypeId;
                    $Block_Param{$CurrentBlock}{$Param_Name} = $Param_TypeId;
                    $Block_Variable{$CurrentBlock}{$Param_Name} = 1;
                }
            }
            $CurrentBlock = $PreviousBlock;
        }
    }
    else
    {
        $Declaration .= "    ".$ClassNameChild."();\n";
    }
    if(defined $Class_PureVirtFunc{$ClassName})
    {
        my %RedefinedTwice = ();
        foreach my $PureVirtualMethod (sort {lc($CompleteSignature{$a}{"ShortName"}) cmp lc($CompleteSignature{$b}{"ShortName"})} keys(%{$Class_PureVirtFunc{$ClassName}}))
        {
            my $PreviousBlock = $CurrentBlock;
            $CurrentBlock = $PureVirtualMethod;
            delete($ValueCollection{$CurrentBlock});
            delete($Block_Variable{$CurrentBlock});
            my $ReturnTypeId = $CompleteSignature{$PureVirtualMethod}{"Return"};
            my $ReturnTypeName = get_TypeName($ReturnTypeId);
            my ($TypeParString, $ParString, $TypeString) = getTypeParString($PureVirtualMethod);
            $TypeParString = alignCode($TypeParString, "    ", 1);
            my ($PureVirtualMethodName, $ShortName) = ("", "");
            if($CompleteSignature{$PureVirtualMethod}{"Constructor"})
            {
                $ShortName = $ClassNameChild;
                $PureVirtualMethodName = "    ".$ShortName.$TypeParString;
            }
            if($CompleteSignature{$PureVirtualMethod}{"Destructor"})
            {
                $ShortName = "~".$ClassNameChild;
                $PureVirtualMethodName = "   ".$ShortName.$TypeParString;
            }
            else
            {
                $ShortName = $CompleteSignature{$PureVirtualMethod}{"ShortName"};
                $PureVirtualMethodName = "    ".$ReturnTypeName." ".$ShortName.$TypeParString;
            }
            if($CompleteSignature{$PureVirtualMethod}{"Throw"})
            {
                $PureVirtualMethodName .= " throw()";
            }
            my $Const = ($PureVirtualMethod=~/\A_ZNK/)?" const":"";
            next if($RedefinedTwice{$ShortName.$TypeString.$Const});
            $RedefinedTwice{$ShortName.$TypeString.$Const} = 1;
            $Declaration .= "\n" if($PureVirtualMethodName=~/\n/);
            foreach my $Param_Pos (sort {int($b)<=>int($a)} keys(%{$CompleteSignature{$PureVirtualMethod}{"Param"}}))
            {
                my $Param_TypeId = $CompleteSignature{$PureVirtualMethod}{"Param"}{$Param_Pos}{"type"};
                my $Param_Name = $CompleteSignature{$PureVirtualMethod}{"Param"}{$Param_Pos}{"name"};
                $Param_Name = "p".($Param_Pos + 1) if(not $Param_Name);
                $ValueCollection{$CurrentBlock}{$Param_Name} = $Param_TypeId;
                $Block_Param{$CurrentBlock}{$Param_Name} = $Param_TypeId;
                $Block_Variable{$CurrentBlock}{$Param_Name} = 1;
            }
            if(get_TypeName($ReturnTypeId) eq "void"
            or $CompleteSignature{$PureVirtualMethod}{"Constructor"}
            or $CompleteSignature{$PureVirtualMethod}{"Destructor"})
            {
                $Declaration .= $PureVirtualMethodName.$Const."\{\}\n\n";
            }
            else
            {
                $Declaration .= $PureVirtualMethodName.$Const." {\n";
                my $ReturnTypeHeaders = getTypeHeaders($ReturnTypeId);
                push(@RecurInterface, $PureVirtualMethod);
                my %Param_Init = initializeParameter((
                "ParamName" => "retval",
                "AccessToParam" => {"obj"=>"no object"},
                "TypeId" => $ReturnTypeId,
                "Key" => "_ret",
                "InLine" => 1,
                "Value" => "no value",
                "CreateChild" => 0,
                "SpecType" => 0,
                "Usage" => "Common",
                "RetVal" => 1));
                pop(@RecurInterface);
                $Code .= $Param_Init{"Code"};
                $Headers = addHeaders($Param_Init{"Headers"}, $Headers);
                $Headers = addHeaders($ReturnTypeHeaders, $Headers);
                $Param_Init{"Init"} = alignCode($Param_Init{"Init"}, "       ", 0);
                $Param_Init{"Call"} = alignCode($Param_Init{"Call"}, "       ", 1);
                $Declaration .= $Param_Init{"Init"}."       return ".$Param_Init{"Call"}.";\n    }\n\n";
            }
            $CurrentBlock = $PreviousBlock;
        }
    }
    if(defined $UsedProtectedMethods{$ClassId})
    {
        foreach my $ProtectedMethod (sort {lc($CompleteSignature{$a}{"ShortName"}) cmp lc($CompleteSignature{$b}{"ShortName"})} keys(%{$UsedProtectedMethods{$ClassId}}))
        {
            my $ReturnType_Id = $CompleteSignature{$ProtectedMethod}{"Return"};
            my $ReturnType_Name = get_TypeName($ReturnType_Id);
            my $ReturnType_PointerLevel = get_PointerLevel($Tid_TDid{$ReturnType_Id}, $ReturnType_Id);
            my $ReturnFType_Id = get_FoundationTypeId($ReturnType_Id);
            my $ReturnFType_Name = get_TypeName($ReturnFType_Id);
            my $Break = ((length($ReturnType_Name)>20)?"\n":" ");
            my $ShortName = $CompleteSignature{$ProtectedMethod}{"ShortName"};
            my $ShortNameAdv = $ShortName."_Wrapper";
            $ShortNameAdv = cleanName($ShortNameAdv);
            $Declaration .= "    ".$ReturnType_Name." ".$ShortNameAdv."() {\n";
            if($Wrappers{$ProtectedMethod}{"Init"})
            {
                $Declaration .= alignCode($Wrappers{$ProtectedMethod}{"Init"}, "       ", 0);
            }
            $Declaration .= alignCode($Wrappers{$ProtectedMethod}{"PreCondition"}, "      ", 0);
            my $FuncCall = "this->".alignCode($ShortName.$Wrappers{$ProtectedMethod}{"Parameters_Call"}, "      ", 1);
            if($Wrappers{$ProtectedMethod}{"PostCondition"} or $Wrappers{$ProtectedMethod}{"FinalCode"})
            {
                my $PostCode = alignCode($Wrappers{$ProtectedMethod}{"PostCondition"}, "      ", 0).alignCode($Wrappers{$ProtectedMethod}{"FinalCode"}, "      ", 0);
                # FIXME: destructors
                if($ReturnFType_Name eq "void" and $ReturnType_PointerLevel==0)
                {
                    $Declaration .= "       $FuncCall;\n".$PostCode;
                }
                else
                {
                    my $RetVal = select_var_name("retval", "");
                    my ($InitializedEType_Id, $Ret_Declarations, $Ret_Headers) = get_ExtTypeId($RetVal, $ReturnType_Id);
                    $Code .= $Ret_Declarations;
                    $Headers = addHeaders($Ret_Headers, $Headers);
                    my $InitializedType_Name = get_TypeName($InitializedEType_Id);
                    if($InitializedType_Name eq $ReturnType_Name)
                    {
                        $Declaration .= "      ".$InitializedType_Name.$Break.$RetVal." = $FuncCall;\n".$PostCode;
                    }
                    else
                    {
                        $Declaration .= "      ".$InitializedType_Name.$Break.$RetVal." = ($InitializedType_Name)$FuncCall;\n".$PostCode;
                    }
                    $Block_Variable{$ProtectedMethod}{$RetVal} = 1;
                    $Declaration .= "       return $RetVal;\n";
                }
            }
            else
            {
                if($ReturnFType_Name eq "void" and $ReturnType_PointerLevel==0)
                {
                    $Declaration .= "       $FuncCall;\n";
                }
                else
                {
                    $Declaration .= "       return $FuncCall;\n";
                }
            }
            $Code .= $Wrappers{$ProtectedMethod}{"Code"};
            $Declaration .= "    }\n\n";
            foreach my $ClassId (keys(%{$Wrappers_SubClasses{$ProtectedMethod}}))
            {
                $Create_SubClass{$ClassId} = 1;
            }
        }
    }
    $Declaration .= "};//$ClassNameChild\n\n";
    return ($Code.$Declaration, $Headers);
}

sub create_SubClasses(@)
{
    my ($Code, $Headers) = ("", []);
    foreach my $ClassId (sort @_)
    {
        my (%Before, %After, %New) = ();
        next if(not $ClassId or $SubClass_Created{$ClassId});
        %Create_SubClass = ();
        push(@RecurTypeId, $ClassId);
        my ($Code_SubClass, $Headers_SubClass) = create_SubClass($ClassId);
        $SubClass_Created{$ClassId} = 1;
        if(keys(%Create_SubClass))
        {
            my ($Code_Depend, $Headers_Depend) = create_SubClasses(keys(%Create_SubClass));
            $Code_SubClass = $Code_Depend.$Code_SubClass;
            $Headers_SubClass = addHeaders($Headers_Depend, $Headers_SubClass);
        }
        pop(@RecurTypeId);
        $Code .= $Code_SubClass;
        $Headers = addHeaders($Headers_SubClass, $Headers);
    }
    return ($Code, $Headers);
}

sub save_state()
{
    my %Saved_State = ();
    foreach (keys(%IntSubClass))
    {
        @{$Saved_State{"IntSubClass"}{$_}}{keys(%{$IntSubClass{$_}})} = values %{$IntSubClass{$_}};
    }
    foreach (keys(%Wrappers))
    {
        @{$Saved_State{"Wrappers"}{$_}}{keys(%{$Wrappers{$_}})} = values %{$Wrappers{$_}};
    }
    foreach (keys(%Wrappers_SubClasses))
    {
        @{$Saved_State{"Wrappers_SubClasses"}{$_}}{keys(%{$Wrappers_SubClasses{$_}})} = values %{$Wrappers_SubClasses{$_}};
    }
    foreach (keys(%ValueCollection))
    {
        @{$Saved_State{"ValueCollection"}{$_}}{keys(%{$ValueCollection{$_}})} = values %{$ValueCollection{$_}};
    }
    foreach (keys(%Block_Variable))
    {
        @{$Saved_State{"Block_Variable"}{$_}}{keys(%{$Block_Variable{$_}})} = values %{$Block_Variable{$_}};
    }
    foreach (keys(%UseVarEveryWhere))
    {
        @{$Saved_State{"UseVarEveryWhere"}{$_}}{keys(%{$UseVarEveryWhere{$_}})} = values %{$UseVarEveryWhere{$_}};
    }
    foreach (keys(%OpenStreams))
    {
        @{$Saved_State{"OpenStreams"}{$_}}{keys(%{$OpenStreams{$_}})} = values %{$OpenStreams{$_}};
    }
    foreach (keys(%Block_Param))
    {
        @{$Saved_State{"Block_Param"}{$_}}{keys(%{$Block_Param{$_}})} = values %{$Block_Param{$_}};
    }
    foreach (keys(%UsedConstructors))
    {
        @{$Saved_State{"UsedConstructors"}{$_}}{keys(%{$UsedConstructors{$_}})} = values %{$UsedConstructors{$_}};
    }
    foreach (keys(%UsedProtectedMethods))
    {
        @{$Saved_State{"UsedProtectedMethods"}{$_}}{keys(%{$UsedProtectedMethods{$_}})} = values %{$UsedProtectedMethods{$_}};
    }
    foreach (keys(%IntSpecType))
    {
        @{$Saved_State{"IntSpecType"}{$_}}{keys(%{$IntSpecType{$_}})} = values %{$IntSpecType{$_}};
    }
    foreach (keys(%RequirementsCatalog))
    {
        @{$Saved_State{"RequirementsCatalog"}{$_}}{keys(%{$RequirementsCatalog{$_}})} = values %{$RequirementsCatalog{$_}};
    }
    @{$Saved_State{"Template2Code_Defines"}}{keys(%Template2Code_Defines)} = values %Template2Code_Defines;
    @{$Saved_State{"TraceFunc"}}{keys(%TraceFunc)} = values %TraceFunc;
    $Saved_State{"MaxTypeId"} = $MaxTypeId;
    @{$Saved_State{"IntrinsicNum"}}{keys(%IntrinsicNum)} = values %IntrinsicNum;
    @{$Saved_State{"AuxHeaders"}}{keys(%AuxHeaders)} = values %AuxHeaders;
    @{$Saved_State{"Class_SubClassTypedef"}}{keys(%Class_SubClassTypedef)} = values %Class_SubClassTypedef;
    @{$Saved_State{"SubClass_Instance"}}{keys(%SubClass_Instance)} = values %SubClass_Instance;
    @{$Saved_State{"SubClass_ObjInstance"}}{keys(%SubClass_ObjInstance)} = values %SubClass_ObjInstance;
    @{$Saved_State{"SpecEnv"}}{keys(%SpecEnv)} = values %SpecEnv;
    @{$Saved_State{"Block_InsNum"}}{keys(%Block_InsNum)} = values %Block_InsNum;
    @{$Saved_State{"AuxType"}}{keys %AuxType} = values %AuxType;
    @{$Saved_State{"AuxFunc"}}{keys %AuxFunc} = values %AuxFunc;
    @{$Saved_State{"Create_SubClass"}}{keys %Create_SubClass} = values %Create_SubClass;
    @{$Saved_State{"SpecCode"}}{keys %SpecCode} = values %SpecCode;
    @{$Saved_State{"SpecLibs"}}{keys %SpecLibs} = values %SpecLibs;
    @{$Saved_State{"UsedInterfaces"}}{keys %UsedInterfaces} = values %UsedInterfaces;
    @{$Saved_State{"ConstraintNum"}}{keys %ConstraintNum} = values %ConstraintNum;
    return \%Saved_State;
}

sub restore_state($)
{
    restore_state_p($_[0], 0);
}

sub restore_local_state($)
{
    restore_state_p($_[0], 1);
}

sub restore_state_p($$)
{
    my ($Saved_State, $Local) = @_;
    if(not $Local)
    {
        foreach my $AuxType_Id (keys(%AuxType))
        {
            if(my $OldName = $TypeDescr{$Tid_TDid{$AuxType_Id}}{$AuxType_Id}{"Name_Old"})
            {
                $TypeDescr{$Tid_TDid{$AuxType_Id}}{$AuxType_Id}{"Name"} = $OldName;
            }
        }
        if(not $Saved_State)
        {#restoring aux types
            foreach my $AuxType_Id (sort {int($a)<=>int($b)} keys(%AuxType))
            {
                if(not $TypeDescr{""}{$AuxType_Id}{"Name_Old"})
                {
                    delete($TypeDescr{""}{$AuxType_Id});
                }
                delete($TName_Tid{$AuxType{$AuxType_Id}});
                delete($AuxType{$AuxType_Id});
            }
            $MaxTypeId = $MaxTypeId_Start;
        }
        elsif($Saved_State->{"MaxTypeId"})
        {
            foreach my $AuxType_Id (sort {int($a)<=>int($b)} keys(%AuxType))
            {
                if($AuxType_Id<=$MaxTypeId and $AuxType_Id>$Saved_State->{"MaxTypeId"})
                {
                    if(not $TypeDescr{""}{$AuxType_Id}{"Name_Old"})
                    {
                        delete($TypeDescr{""}{$AuxType_Id});
                    }
                    delete($TName_Tid{$AuxType{$AuxType_Id}});
                    delete($AuxType{$AuxType_Id});
                }
            }
        }
    }
    (%Block_Variable, %UseVarEveryWhere, %OpenStreams, %SpecEnv, %Block_InsNum,
    %ValueCollection, %IntrinsicNum, %ConstraintNum, %SubClass_Instance,
    %SubClass_ObjInstance, %Block_Param,%Class_SubClassTypedef, %AuxHeaders, %Template2Code_Defines) = ();
    if(not $Local)
    {
        (%Wrappers, %Wrappers_SubClasses, %IntSubClass, %AuxType, %AuxFunc,
        %UsedConstructors, %UsedProtectedMethods, %Create_SubClass, %SpecCode,
        %SpecLibs, %UsedInterfaces, %IntSpecType, %RequirementsCatalog, %TraceFunc) = ();
    }
    if(not $Saved_State)
    {#initializing
        %IntrinsicNum=(
            "Char"=>64,
            "Int"=>0,
            "Str"=>0,
            "Float"=>0);
        return;
    }
    foreach (keys(%{$Saved_State->{"Block_Variable"}}))
    {
        @{$Block_Variable{$_}}{keys(%{$Saved_State->{"Block_Variable"}{$_}})} = values %{$Saved_State->{"Block_Variable"}{$_}};
    }
    foreach (keys(%{$Saved_State->{"UseVarEveryWhere"}}))
    {
        @{$UseVarEveryWhere{$_}}{keys(%{$Saved_State->{"UseVarEveryWhere"}{$_}})} = values %{$Saved_State->{"UseVarEveryWhere"}{$_}};
    }
    foreach (keys(%{$Saved_State->{"OpenStreams"}}))
    {
        @{$OpenStreams{$_}}{keys(%{$Saved_State->{"OpenStreams"}{$_}})} = values %{$Saved_State->{"OpenStreams"}{$_}};
    }
    @SpecEnv{keys(%{$Saved_State->{"SpecEnv"}})} = values %{$Saved_State->{"SpecEnv"}};
    @Block_InsNum{keys(%{$Saved_State->{"Block_InsNum"}})} = values %{$Saved_State->{"Block_InsNum"}};
    foreach (keys(%{$Saved_State->{"ValueCollection"}}))
    {
        @{$ValueCollection{$_}}{keys(%{$Saved_State->{"ValueCollection"}{$_}})} = values %{$Saved_State->{"ValueCollection"}{$_}};
    }
    @Template2Code_Defines{keys(%{$Saved_State->{"Template2Code_Defines"}})} = values %{$Saved_State->{"Template2Code_Defines"}};
    @IntrinsicNum{keys(%{$Saved_State->{"IntrinsicNum"}})} = values %{$Saved_State->{"IntrinsicNum"}};
    @ConstraintNum{keys(%{$Saved_State->{"ConstraintNum"}})} = values %{$Saved_State->{"ConstraintNum"}};
    @SubClass_Instance{keys(%{$Saved_State->{"SubClass_Instance"}})} = values %{$Saved_State->{"SubClass_Instance"}};
    @SubClass_ObjInstance{keys(%{$Saved_State->{"SubClass_ObjInstance"}})} = values %{$Saved_State->{"SubClass_ObjInstance"}};
    foreach (keys(%{$Saved_State->{"Block_Param"}}))
    {
        @{$Block_Param{$_}}{keys(%{$Saved_State->{"Block_Param"}{$_}})} = values %{$Saved_State->{"Block_Param"}{$_}};
    }
    @Class_SubClassTypedef{keys(%{$Saved_State->{"Class_SubClassTypedef"}})} = values %{$Saved_State->{"Class_SubClassTypedef"}};
    @AuxHeaders{keys(%{$Saved_State->{"AuxHeaders"}})} = values %{$Saved_State->{"AuxHeaders"}};
    if(not $Local)
    {
        foreach my $AuxType_Id (sort {int($a)<=>int($b)} keys(%{$Saved_State->{"AuxType"}}))
        {
            $TypeDescr{$Tid_TDid{$AuxType_Id}}{$AuxType_Id}{"Name"} = $Saved_State->{"AuxType"}{$AuxType_Id};
            $TName_Tid{$Saved_State->{"AuxType"}{$AuxType_Id}} = $AuxType_Id;
        }
        foreach (keys(%{$Saved_State->{"IntSubClass"}}))
        {
            @{$IntSubClass{$_}}{keys(%{$Saved_State->{"IntSubClass"}{$_}})} = values %{$Saved_State->{"IntSubClass"}{$_}};
        }
        foreach (keys(%{$Saved_State->{"Wrappers"}}))
        {
            @{$Wrappers{$_}}{keys(%{$Saved_State->{"Wrappers"}{$_}})} = values %{$Saved_State->{"Wrappers"}{$_}};
        }
        foreach (keys(%{$Saved_State->{"Wrappers_SubClasses"}}))
        {
            @{$Wrappers_SubClasses{$_}}{keys(%{$Saved_State->{"Wrappers_SubClasses"}{$_}})} = values %{$Saved_State->{"Wrappers_SubClasses"}{$_}};
        }
        foreach (keys(%{$Saved_State->{"UsedConstructors"}}))
        {
            @{$UsedConstructors{$_}}{keys(%{$Saved_State->{"UsedConstructors"}{$_}})} = values %{$Saved_State->{"UsedConstructors"}{$_}};
        }
        foreach (keys(%{$Saved_State->{"UsedProtectedMethods"}}))
        {
            @{$UsedProtectedMethods{$_}}{keys(%{$Saved_State->{"UsedProtectedMethods"}{$_}})} = values %{$Saved_State->{"UsedProtectedMethods"}{$_}};
        }
        foreach (keys(%{$Saved_State->{"IntSpecType"}}))
        {
            @{$IntSpecType{$_}}{keys(%{$Saved_State->{"IntSpecType"}{$_}})} = values %{$Saved_State->{"IntSpecType"}{$_}};
        }
        foreach (keys(%{$Saved_State->{"RequirementsCatalog"}}))
        {
            @{$RequirementsCatalog{$_}}{keys(%{$Saved_State->{"RequirementsCatalog"}{$_}})} = values %{$Saved_State->{"RequirementsCatalog"}{$_}};
        }
        $MaxTypeId = $Saved_State->{"MaxTypeId"};
        @AuxType{keys(%{$Saved_State->{"AuxType"}})} = values %{$Saved_State->{"AuxType"}};
        @TraceFunc{keys(%{$Saved_State->{"TraceFunc"}})} = values %{$Saved_State->{"TraceFunc"}};
        @AuxFunc{keys(%{$Saved_State->{"AuxFunc"}})} = values %{$Saved_State->{"AuxFunc"}};
        @Create_SubClass{keys(%{$Saved_State->{"Create_SubClass"}})} = values %{$Saved_State->{"Create_SubClass"}};
        @SpecCode{keys(%{$Saved_State->{"SpecCode"}})} = values %{$Saved_State->{"SpecCode"}};
        @SpecLibs{keys(%{$Saved_State->{"SpecLibs"}})} = values %{$Saved_State->{"SpecLibs"}};
        @UsedInterfaces{keys(%{$Saved_State->{"UsedInterfaces"}})} = values %{$Saved_State->{"UsedInterfaces"}};
        @IntSpecType{keys(%{$Saved_State->{"IntSpecType"}})} = values %{$Saved_State->{"IntSpecType"}};
    }
}

sub isAbstractClass($)
{
    my $ClassId = $_[0];
    return (keys(%{$Class_PureVirtFunc{get_TypeName($ClassId)}}) > 0);
}

sub needToInherit($)
{
    my $Interface = $_[0];
    return ($CompleteSignature{$Interface}{"Class"} and (isAbstractClass($CompleteSignature{$Interface}{"Class"}) or isNotInCharge($Interface) or ($CompleteSignature{$Interface}{"Protected"})));
}

sub parseCode($$)
{
    my ($Code, $Mode) = @_;
    my $Global_State = save_state();
    my %ParsedCode = parseCode_m($Code, $Mode);
    if(not $ParsedCode{"IsCorrect"})
    {
        restore_state($Global_State);
        return ();
    }
    else
    {
        return %ParsedCode;
    }
}

sub get_TypeIdByName($)
{
    my $TypeName = $_[0];
    if(my $ExactId = $TName_Tid{correctName($TypeName)})
    {
        return $ExactId;
    }
    else
    {
        return $TName_Tid{remove_quals(correctName($TypeName))};
    }
}

sub callInterfaceParameters(@)
{
    my %Init_Desc = @_;
    my $Interface = $Init_Desc{"Interface"};
    return () if(not $Interface);
    return () if($SkipInterfaces{$Interface});
    foreach my $SkipPattern (keys(%SkipInterfaces_Pattern))
    {
        return () if($Interface=~/$SkipPattern/);
    }
    if(defined $MakeIsolated and $Interface_Library{$Interface}
    and keys(%InterfacesList) and not $InterfacesList{$Interface})
    {
        return ();
    }
    my $Global_State = save_state();
    return () if(isCyclical(\@RecurInterface, $Interface));
    push(@RecurInterface, $Interface);
    my $PreviousBlock = $CurrentBlock;
    if(($CompleteSignature{$Interface}{"Protected"}) and (not $CompleteSignature{$Interface}{"Constructor"}))
    {
        $CurrentBlock = $Interface;
    }
    $NodeInterface = $Interface;
    $UsedInterfaces{$NodeInterface} = 1;
    my %Params_Init = callInterfaceParameters_m(%Init_Desc);
    $CurrentBlock = $PreviousBlock;
    if(not $Params_Init{"IsCorrect"})
    {
        pop(@RecurInterface);
        restore_state($Global_State);
        return ();
    }
    pop(@RecurInterface);
    if($InterfaceSpecType{$Interface}{"SpecEnv"})
    {
        $SpecEnv{$InterfaceSpecType{$Interface}{"SpecEnv"}} = 1;
    }
    $Params_Init{"ReturnTypeId"} = $CompleteSignature{$Interface}{"Return"};
    return %Params_Init;
}

sub detectInLineParams($)
{
    my $Interface = $_[0];
    my ($SpecAttributes, %Param_SpecAttributes, %InLineParam) = ();
    foreach my $Param_Pos (keys(%{$InterfaceSpecType{$Interface}{"SpecParam"}}))
    {
        my $SpecType_Id = $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos};
        my %SpecType = %{$SpecType{$SpecType_Id}};
        $Param_SpecAttributes{$Param_Pos} = $SpecType{"Value"}.$SpecType{"PreCondition"}.$SpecType{"PostCondition"}.$SpecType{"InitCode"}.$SpecType{"FinalCode"};
        $SpecAttributes .= $Param_SpecAttributes{$Param_Pos};
    }
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $Param_Num = $Param_Pos + 1;
        if($SpecAttributes=~/\$$Param_Num(\W|\Z)/ or $Param_SpecAttributes{$Param_Pos}=~/\$0(\W|\Z)/)
        {
            $InLineParam{$Param_Num} = 0;
        }
        else
        {
            $InLineParam{$Param_Num} = 1;
        }
    }
    return %InLineParam;
}

sub detectParamsOrder($)
{
    my $Interface = $_[0];
    my ($SpecAttributes, %OrderParam) = ();
    foreach my $Param_Pos (keys(%{$InterfaceSpecType{$Interface}{"SpecParam"}}))
    {#detect all dependencies
        my $SpecType_Id = $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos};
        my %SpecType = %{$SpecType{$SpecType_Id}};
        $SpecAttributes .= $SpecType{"Value"}.$SpecType{"PreCondition"}.$SpecType{"PostCondition"}.$SpecType{"InitCode"}.$SpecType{"FinalCode"};
    }
    my $Orded = 1;
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $Param_Num = $Param_Pos + 1;
        if($SpecAttributes=~/\$$Param_Num(\W|\Z)/)
        {
            $OrderParam{$Param_Num} = $Orded;
            $Orded += 1;
        }
    }
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $Param_Num = $Param_Pos + 1;
        if(not defined $OrderParam{$Param_Pos+1})
        {
            $OrderParam{$Param_Num} = $Orded;
            $Orded += 1;
        }
    }
    return %OrderParam;
}

sub chooseSpecType($$$)
{
    my ($TypeId, $Kind, $Interface) = @_;
    if(my $SpecTypeId_Strong = chooseSpecType_Strong($TypeId, $Kind, $Interface, 1))
    {
        return $SpecTypeId_Strong;
    }
    elsif(get_TypeType(get_FoundationTypeId($TypeId))!~/\A(Intrinsic)\Z/)
    {
        return chooseSpecType_Strong($TypeId, $Kind, $Interface, 0);
    }
    else
    {
        return "";
    }
}

sub chooseSpecType_Strong($$$$)
{
    my ($TypeId, $Kind, $Interface, $Strong) = @_;
    return 0 if(not $TypeId or not $Kind);
    foreach my $SpecType_Id (sort {int($a)<=>int($b)} keys(%SpecType))
    {
        next if($Interface and $Common_SpecType_Exceptions{$Interface}{$SpecType_Id});
        if($SpecType{$SpecType_Id}{"Kind"} eq $Kind)
        {
            if($Strong)
            {
                if($TypeId==get_TypeIdByName($SpecType{$SpecType_Id}{"DataType"}))
                {
                    return $SpecType_Id;
                }
            }
            else
            {
                my $FoundationTypeId = get_FoundationTypeId($TypeId);
                my $SpecType_FTypeId = get_FoundationTypeId(get_TypeIdByName($SpecType{$SpecType_Id}{"DataType"}));
                if($FoundationTypeId==$SpecType_FTypeId)
                {
                    return $SpecType_Id;
                }
            }
        }
    }
    return 0;
}

sub getAutoConstraint($)
{
    my $ReturnType_Id = $_[0];
    if(get_PointerLevel($Tid_TDid{$ReturnType_Id}, $ReturnType_Id) > 0)
    {
        return ("\$0 != ".get_null(), $ReturnType_Id);
    }
    else
    {
        return ();
    }
}

sub isValidForOutput($)
{
    my $TypeId = $_[0];
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    my $FoundationType_Id = get_FoundationTypeId($TypeId);
    my $FoundationType_Name = get_TypeName($FoundationType_Id);
    my $FoundationType_Type = get_TypeType($FoundationType_Id);
    if($PointerLevel eq 0)
    {
        if($FoundationType_Name eq "QString")
        {
            return 1;
        }
        elsif(($FoundationType_Type eq "Enum") or ($FoundationType_Type eq "Intrinsic"))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    elsif($PointerLevel eq 1)
    {
        if(($FoundationType_Name eq "char") or ($FoundationType_Name eq "unsigned char"))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

sub requirementReturn($$$$)
{
    my ($Interface, $Ireturn, $Ispecreturn, $CallObj) = @_;
    return "" if(defined $Template2Code and $Interface ne $TestedInterface);
    return "" if(not $Ireturn or not $Interface);
    my ($PostCondition, $TargetTypeId, $Requirement_Code) = ();
    if($Ispecreturn)
    {
        ($PostCondition, $TargetTypeId) = ($SpecType{$Ispecreturn}{"PostCondition"}, get_TypeIdByName($SpecType{$Ispecreturn}{"DataType"}));
    }
    elsif(defined $CheckReturn)
    {
        ($PostCondition, $TargetTypeId) = getAutoConstraint($Ireturn);
    }
    return "" if(not $PostCondition or not $TargetTypeId);
    my $IreturnTypeName = get_TypeName($Ireturn);
    my $BaseIreturnTypeName = get_TypeName(get_FoundationTypeId($Ireturn));
    my $PointerLevelReturn = get_PointerLevel($Tid_TDid{$Ireturn}, $Ireturn);
    my ($TargetCallReturn, $TmpPreamble) =
    convert_familiar_types((
        "InputTypeName"=>get_TypeName($Ireturn),
        "InputPointerLevel"=>$PointerLevelReturn,
        "OutputTypeId"=>$TargetTypeId,
        "Value"=>"\$0",
        "Key"=>"\$0",
        "Destination"=>"Target",
        "MustConvert"=>0));
    if($TmpPreamble)
    {
        $Requirement_Code .= $TmpPreamble."\n";
    }
    if(($TargetCallReturn=~/\A\*/) or ($TargetCallReturn=~/\A\&/))
    {
        $TargetCallReturn = "(".$TargetCallReturn.")";
    }
    if(($CallObj=~/\A\*/) or ($CallObj=~/\A\&/))
    {
        $CallObj = "(".$CallObj.")";
    }
    $PostCondition=~s/\$0/$TargetCallReturn/g;
    if($CallObj ne "no object")
    {
        $PostCondition=~s/\$obj/$CallObj/g;
    }
    $PostCondition = clearSyntax($PostCondition);
    my $NormalResult = $PostCondition;
    while($PostCondition=~s/([^\\])"/$1\\\"/g){}
    $ConstraintNum{$Interface}+=1;
    $RequirementsCatalog{$Interface}{$ConstraintNum{$Interface}} = "constraint for the return value: \'$PostCondition\'";
    my $ReqId = short_interface_name($Interface).".".normalize_num($ConstraintNum{$Interface});
    if(my $Format = is_printable(get_TypeName($TargetTypeId)))
    {
        my $Comment = "constraint for the return value failed: \'$PostCondition\', returned value: $Format";
        $Requirement_Code .= "REQva(\"$ReqId\",\n    $NormalResult,\n    \"$Comment\", $TargetCallReturn);\n";
        $TraceFunc{"REQva"}=1;
    }
    else
    {
        my $Comment = "constraint for the return value failed: \'$PostCondition\'";
        $Requirement_Code .= "REQ(\"$ReqId\",\n    \"$Comment\",\n    $NormalResult);\n";
        $TraceFunc{"REQ"}=1;
    }
    return $Requirement_Code;
}

sub is_printable($)
{
    my $TypeName = remove_quals(uncover_typedefs($_[0]));
    if(isIntegerType($TypeName))
    {
        return "\%d";
    }
    elsif($TypeName=~/\A(char|unsigned char|wchar_t|void|short|unsigned short) const\*\Z/)
    {
        return "\%s";
    }
    elsif($TypeName=~/\A(char|unsigned char|wchar_t)\Z/)
    {
        return "\%c";
    }
    elsif($TypeName=~/\A(float|double|long double)\Z/)
    {
        return "\%f";
    }
    else
    {
        return "";
    }
}

sub short_interface_name($)
{
    my $Interface = $_[0];
    my $ClassName = get_TypeName($CompleteSignature{$Interface}{"Class"});
    return (($ClassName)?$ClassName."::":"").$CompleteSignature{$Interface}{"ShortName"};
}

sub normalize_num($)
{
    my $Num = $_[0];
    if(int($Num)<10)
    {
        return "0".$Num;
    }
    else
    {
        return $Num;
    }
}

sub get_PointerLevel($$)
{
    my ($TypeDId, $TypeId) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId} and not defined $AuxType{$TypeId})
    {
        return $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId};
    }
    return "" if(not $TypeDescr{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return 0 if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    return 0 if($Type{"Type"} eq "Array");
    my $PointerLevel = 0;
    if($Type{"Type"} eq "Pointer")
    {
        $PointerLevel += 1;
    }
    $PointerLevel += get_PointerLevel($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"});
    $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId} = $PointerLevel;
    return $PointerLevel;
}

sub select_ValueFromCollection(@)
{
    my %Init_Desc = @_;
    my ($TypeId, $Name, $Interface, $CreateChild, $IsObj) = ($Init_Desc{"TypeId"}, $Init_Desc{"ParamName"}, $Init_Desc{"Interface"}, $Init_Desc{"CreateChild"}, $Init_Desc{"ObjectInit"});
    return () if($Init_Desc{"DoNotReuse"});
    my $TypeName = get_TypeName($TypeId);
    my $FTypeId = get_FoundationTypeId($TypeId);
    my $FTypeName = get_TypeName($FTypeId);
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
    my $IsRef = (uncover_typedefs(get_TypeName($TypeId))=~/&/);
    return () if(isString($TypeId, $Name, $Interface));
    return () if(uncover_typedefs($TypeName)=~/\A(char|unsigned char|wchar_t|void\*)\Z/);
    return () if(isCyclical(\@RecurTypeId, get_TypeStackId($TypeId)));
    if($CurrentBlock and keys(%{$ValueCollection{$CurrentBlock}}))
    {
        my (@Name_Type_Coinsidence, @Name_FType_Coinsidence, @Type_Coinsidence, @FType_Coinsidence) = ();
        foreach my $Value (sort {$b=~/$Name/i<=>$a=~/$Name/i} sort keys(%{$ValueCollection{$CurrentBlock}}))
        {
            return () if($Name=~/dest|source/i and $Value=~/source|dest/i and $ShortName=~/copy|move|backup/i);
            my $Value_TypeId = $ValueCollection{$CurrentBlock}{$Value};
            my $PointerLevel_Value = get_PointerLevel($Tid_TDid{$Value_TypeId}, $Value_TypeId);
            if($Value!~/\A(argc|argv)\Z/)
            {
                next if(get_TypeName($Value_TypeId)=~/\A(string|date|time|file)\Z/i and $Name!~/\Ap\d+\Z/);
                next if($CreateChild and not $SubClass_Instance{$Value});
                #next if(not $IsObj and $SubClass_Instance{$Value});
                next if(($Interface eq $TestedInterface) and ($Name ne $Value)
                and not $UseVarEveryWhere{$CurrentBlock}{$Value});#and $Name!~/\Ap\d+\Z/
            }
            if($TypeName eq get_TypeName($Value_TypeId))
            {
                if($Value=~/\A(argc|argv)\Z/)
                {
                    next if($PointerLevel > $PointerLevel_Value);
                }
                else
                {
                    next if($PointerLevel==0 and isNumericType(get_TypeName($TypeId)) and $TypeName!~/[A-Z]/ and not (isIntegerType($TypeName) and $Name=~/\Afd\Z/i and $Value=~/\Afd\Z/i));
                }
                if($Name=~/\A[_]*$Value(|[_]*[a-zA-Z0-9]|[_]*ptr)\Z/i)
                {
                    push(@Name_Type_Coinsidence, $Value);
                }
                else
                {
                    next if($Value=~/\A(argc|argv)\Z/ and $CurrentBlock eq "main");
                    push(@Type_Coinsidence, $Value);
                }
            }
            else
            {
                if($Value=~/\A(argc|argv)\Z/)
                {
                    next if($PointerLevel > $PointerLevel_Value);
                }
                else
                {
                    next if($PointerLevel==0 and isNumericType(get_TypeName($TypeId)) and not (isIntegerType($TypeName) and $Name=~/\Afd\Z/i and $Value=~/\Afd\Z/i));
                    next if(($PointerLevel > $PointerLevel_Value) and ($PointerLevel-$PointerLevel_Value!=1));# and not $Block_Param{$CurrentBlock}{$Value}
                    next if(($PointerLevel ne $PointerLevel_Value) and ($PointerLevel-$PointerLevel_Value!=1) and (get_TypeType($FTypeId)=~/\A(Intrinsic|Array|Enum)\Z/ or isArray($Value_TypeId, $Value, $Interface)));
                    next if(($PointerLevel<$PointerLevel_Value) and $Init_Desc{"OuterType_Type"} eq "Array");
                }
                my $Value_FTypeId = get_FoundationTypeId($Value_TypeId);
                if($FTypeName eq get_TypeName($Value_FTypeId))
                {
                    if($Name=~/\A[_]*\Q$Value\E(|[_]*[a-z0-9]|[_]*ptr)\Z/i)
                    {
                        push(@Name_FType_Coinsidence, $Value);
                    }
                    else
                    {
                        next if($Value=~/\A(argc|argv)\Z/ and $CurrentBlock eq "main");
                        push(@FType_Coinsidence, $Value);
                    }
                }
            }
        }
        my @All_Coinsidence = (@Name_Type_Coinsidence, @Name_FType_Coinsidence, @Type_Coinsidence, @FType_Coinsidence);
        if($#All_Coinsidence>-1)
        {
            return ($All_Coinsidence[0], $ValueCollection{$CurrentBlock}{$All_Coinsidence[0]});
        }
    }
    return ();
}

sub get_interface_param_pos($$)
{
    my ($Interface, $Name) = @_;
    foreach my $Pos (keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        if($CompleteSignature{$Interface}{"Param"}{$Pos}{"name"} eq $Name)
        {
            return $Pos;
        }
    }
    return "";
}

sub isArray($$$)
{# detecting parameter semantic
    my ($TypeId, $ParamName, $Interface) = @_;
    return 0 if(not $TypeId or not $ParamName);
    my $I_ShortName = $CompleteSignature{$Interface}{"ShortName"};
    my $FTypeId = get_FoundationTypeId($TypeId);
    my $FTypeType = get_TypeType($FTypeId);
    my $FTypeName = get_TypeName($FTypeId);
    my $TypeName = get_TypeName($TypeId);
    my $PLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    my $ParamPos = get_interface_param_pos($Interface, $ParamName);
    # strong reject
    return 0 if($PLevel <= 0);
    return 0 if($PLevel==1 and (isOpaque($FTypeId) or $FTypeName eq "void"));
    return 0 if($ParamName=~/ptr|pointer/i and $FTypeType=~/\A(Struct|Union|Class)\Z/);
    return 0 if($Interface_OutParam{$Interface}{$ParamName});
    
    # particular reject
    # FILE *fopen(const char *path, const char *__modes)
    return 0 if(is_const_type($TypeName) and $FTypeName=~/\A(char|unsigned char|wchar_t)\Z/
    and $PLevel==1 and $ParamName=~/mode/i);
    
    # returned by function
    return 0 if(($FTypeType=~/\A(Struct|Union|Class)\Z/
    or ($TypeName ne uncover_typedefs($TypeName) and $TypeName!~/size_t|int/))
    and check_type_returned($TypeId));
    
    # array followed by the number
    return 1 if(not is_const_type($TypeName) and defined $CompleteSignature{$Interface}{"Param"}{$ParamPos+1}
    and isIntegerType(get_TypeName($CompleteSignature{$Interface}{"Param"}{$ParamPos+1}{"type"}))
    and is_array_count($ParamName, $CompleteSignature{$Interface}{"Param"}{$ParamPos+1}{"name"}));
    
    return 0 if($PLevel>=2 and $FTypeName=~/\A(char|unsigned char|wchar_t)\Z/
    and not is_const_type($TypeName));
    
    # allowed configurations
    # array of arguments
    return 1 if($ParamName=~/argv/i);
    # array, list, matrix
    return 1 if($ParamName!~/out|context|name/i and $ParamName=~/([a-z][a-rt-z]s\Z|matrix|list|set|range)/i
    and (getParamNameByTypeName(get_TypeName($TypeId)) ne $ParamName or get_TypeName($TypeId)!~/\*/)
    and get_TypeName($TypeId)!~/$ParamName/i);
    # array of function pointers
    return 1 if($PLevel==1 and $FTypeType=~/\A(FuncPtr|Array)\Z/);
    # QString::vsprintf ( char const* format, va_list ap )
    return 1 if($ParamName!~/out|context/i and $TypeName=~/matrix|list|set|range/i);
    # high pointer level
    # xmlSchemaSAXPlug (xmlSchemaValidCtxtPtr ctxt, xmlSAXHandlerPtr* sax, void** user_data)
    return 1 if($PLevel>=2);
    # symbol array for reading
    return 1 if($PLevel==1 and not is_const_type($TypeName) and $FTypeName=~/\A(char|unsigned char|wchar_t)\Z/
    and not grep(/\A(name|cur|current|out|ret|return|buf|buffer|res|result|rslt)\Z/i, @{get_tokens($ParamName)}));
    # array followed by the two numbers
    return 1 if(not is_const_type($TypeName) and defined $CompleteSignature{$Interface}{"Param"}{$ParamPos+1}
    and defined $CompleteSignature{$Interface}{"Param"}{$ParamPos+2}
    and isIntegerType(get_TypeName($CompleteSignature{$Interface}{"Param"}{$ParamPos+1}{"type"}))
    and isIntegerType(get_TypeName($CompleteSignature{$Interface}{"Param"}{$ParamPos+2}{"type"}))
    and is_array_count($ParamName, $CompleteSignature{$Interface}{"Param"}{$ParamPos+2}{"name"}));
    # numeric arrays for reading
    return 1 if(is_const_type($TypeName) and isNumericType($FTypeName));
    # symbol buffer for reading
    return 1 if(is_const_type($TypeName) and $ParamName=~/buf/i and $I_ShortName=~/memory/i
    and $FTypeName=~/\A(char|unsigned char|wchar_t)\Z/);
    
    #isn't array
    return 0;
}

sub check_type_returned($)
{
    my $TypeId = $_[0];
    return 0 if(not $TypeId);
    my $BaseTypeId = get_FoundationTypeId($TypeId);
    if(get_TypeType($BaseTypeId) eq "Intrinsic")
    {
        return 0;
    }
    else
    {
        # by return value
        return 1 if(keys(%{$ReturnTypeId_Interface{$TypeId}}) or keys(%{$ReturnTypeId_Interface{$BaseTypeId}}));
        # by out param
        my $PLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
        foreach (0 .. $PLevel)
        {
            return 1 if(keys(%{$BaseType_PLevel_OutParam{$BaseTypeId}{$_}})
            or keys(%{$BaseType_PLevel_Return{$BaseTypeId}{$_}}));
        }
        return 0;
    }
}

sub isBuffer($$$)
{
    my ($TypeId, $ParamName, $Interface) = @_;
    return 0 if(not $TypeId or not $ParamName);
    my $I_ShortName = $CompleteSignature{$Interface}{"ShortName"};
    my $FTypeId = get_FoundationTypeId($TypeId);
    my $FTypeType = get_TypeType($FTypeId);
    my $FTypeName = get_TypeName($FTypeId);
    my $TypeName = get_TypeName($TypeId);
    my $PLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    
    # exceptions
    # bmp_read24 (uintptr_t addr)
    # bmp_write24 (uintptr_t addr, int c)
    return 1 if($PLevel==0 and $ParamName=~/addr/i and isIntegerType($FTypeName));
    # cblas_zdotu_sub (int const N, void const* X, int const incX, void const* Y, int const incY, void* dotu)
    return 1 if($PLevel==1 and $FTypeName eq "void");
    if(get_TypeType($FTypeId) eq "Array" and $Interface)
    {
        my $ArrayElemType_Id = get_FoundationTypeId(get_OneStep_BaseTypeId($Tid_TDid{$FTypeId}, $FTypeId));
        if(get_TypeType($ArrayElemType_Id)=~/\A(Intrinsic|Enum)\Z/)
        {
            return 1 if(get_TypeSize($FTypeId)>1024);
        }
        else
        {
            return 1 if(get_TypeSize($FTypeId)>256);
        }
    }
    
    # strong reject
    return 0 if($PLevel <= 0);
    return 0 if(is_const_type($TypeName));
    return 0 if($PLevel==1 and isOpaque($FTypeId));
    return 0 if(($FTypeType=~/\A(Struct|Union|Class)\Z/
    or ($TypeName ne uncover_typedefs($TypeName) and $TypeName!~/size_t|int/))
    and check_type_returned($TypeId));
    
    # allowed configurations
    # symbol buffer for writing
    return 1 if(isSymbolBuffer($TypeId, $ParamName, $Interface));
    if($ParamName=~/\Ap\d+\Z/)
    {
        # buffer of void* type for writing
        return 1 if($PLevel==1 and $FTypeName eq "void");
        # buffer of arrays for writing
        return 1 if($FTypeType eq "Array");
    }
    return 1 if(is_out_word($ParamName));
    # gsl_fft_real_radix2_transform (double* data, size_t const stride, size_t const n)
    return 1 if($PLevel==1 and isNumericType($FTypeName) and $ParamName!~/(len|size)/i);
    
    # isn't array
    return 0;
}

sub is_out_word($)
{
    my $Word = $_[0];
    return grep(/\A(out|dest|buf|buff|buffer|ptr|pointer|result|res|ret|return|rtrn)\Z/i, @{get_tokens($Word)});
}

sub isSymbolBuffer($$$)
{
    my ($TypeId, $ParamName, $Interface) = @_;
    return 0 if(not $TypeId or not $ParamName);
    my $FTypeId = get_FoundationTypeId($TypeId);
    my $FTypeName = get_TypeName($FTypeId);
    my $TypeName = get_TypeName($TypeId);
    my $PLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    return (not is_const_type($TypeName) and $PLevel==1
    and $FTypeName=~/\A(char|unsigned char|wchar_t)\Z/
    and $ParamName!~/data|value|arg|var/i and $TypeName!~/list|va_/
    and (grep(/\A(name|cur|current)\Z/i, @{get_tokens($ParamName)}) or is_out_word($ParamName)));
}

sub isOutParam_NoUsing($$$)
{
    my ($TypeId, $ParamName, $Interface) = @_;
    return 0 if(not $TypeId or not $ParamName);
    my $Func_ShortName = $CompleteSignature{$Interface}{"ShortName"};
    my $FTypeId = get_FoundationTypeId($TypeId);
    my $FTypeName = get_TypeName($FTypeId);
    my $TypeName = get_TypeName($TypeId);
    my $PLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    return 0 if($PLevel==1 and isOpaque($FTypeId)); # size of the structure/union is unknown
    return 0 if(is_const_type($TypeName) or $PLevel<=0);
    return 1 if(grep(/\A(err|error)(_|)(p|ptr|)\Z/i, @{get_tokens($ParamName." ".$TypeName)}) and $Func_ShortName!~/error/i);
    return 1 if(grep(/\A(out|ret|return)\Z/i, @{get_tokens($ParamName)}));
    return 1 if($PLevel>=2 and $FTypeName=~/\A(char|unsigned char|wchar_t)\Z/ and not is_const_type($TypeName));
    return 0;
}

sub isString($$$)
{
    my ($TypeId, $ParamName, $Interface) = @_;
    return 0 if(not $TypeId or not $ParamName);
    my $TypeName_Trivial = uncover_typedefs(get_TypeName($TypeId));
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    my $TypeName = get_TypeName($TypeId);
    my $FoundationTypeName = get_TypeName(get_FoundationTypeId($TypeId));
    # not a pointer
    return 0 if($ParamName=~/ptr|pointer/i);
    # standard string (std::string)
    return 1 if($FoundationTypeName eq "std::basic_string<char,std::char_traits<char>,std::allocator<char> >"
    and $PointerLevel==0);
    if($FoundationTypeName=~/\A(char|unsigned char|wchar_t|short|unsigned short)\Z/)
    {
        # char const*, unsigned char const*, wchar_t const*
        # void const*, short const*, unsigned short const*
        return 1 if($PointerLevel==1 and (is_const_type($TypeName_Trivial) or $ParamName=~/\A(file|)(_|)path\Z/i));
        # direct_trim ( char** s )
        return 1 if($PointerLevel>=1 and $ParamName=~/\A(s|str|string)\Z/i);
    }
    
    # isn't a string
    return 0;
}

sub isOpaque($)
{
    my $TypeId = $_[0];
    return 0 if(not $TypeId);
    my %Type = get_Type($Tid_TDid{$TypeId}, $TypeId);
    return ($Type{"Type"}=~/\A(Struct|Union)\Z/ and not keys(%{$Type{"Memb"}}) and not $Type{"Memb"}{0}{"name"});
}

sub isStr_FileName($$$)
{#should be called after the "isString" function
    my ($ParamPos, $ParamName, $Interface_ShortName) = @_;
    return 0 if(not $ParamName);
    # not an extension
    return 0 if($ParamName=~/ext/i);
    # any files, dtds
    return 1 if($ParamName=~/file|dtd/i);
    return 1 if(lc($ParamName) eq "fname");
    # files as buffers
    return 1 if($ParamName=~/buf/i and $Interface_ShortName!~/memory|write/i and $Interface_ShortName=~/file/i);
    # name of the file at the first parameter of read/write/open functions
    return 1 if($ParamName=~/\A[_]*name\Z/i and $Interface_ShortName=~/read|write|open/i and $ParamPos=="0");
    # file path
    return 1 if($ParamName=~/path/i
    and $Interface_ShortName=~/open/i
    and $Interface_ShortName!~/(open|_)dir(_|\Z)/i);
    # path to the configs
    return 1 if($ParamName=~/path|cfgs/i and $Interface_ShortName=~/config/i);
    # parameter of the string constructor
    return 1 if($ParamName=~/src/i and $Interface_ShortName!~/string/i and $ParamPos=="0");
    # uri/url of the local files
    return 1 if($ParamName=~/uri|url/i and $Interface_ShortName!~/http|ftp/i);
    
    # isn't a file path
    return 0;
}

sub isStr_Dir($$)
{
    my ($ParamName, $Interface_ShortName) = @_;
    return 0 if(not $ParamName);
    return 1 if($ParamName=~/path/i
    and $Interface_ShortName=~/(open|_)dir(_|\Z)/i);
    return 1 if($ParamName=~/dir/i);
    
    # isn't a directory
    return 0;
}

sub equal_types($$)
{
    my ($Type1_Id, $Type2_Id) = @_;
    return (uncover_typedefs(get_TypeName($Type1_Id)) eq uncover_typedefs(get_TypeName($Type2_Id)));
}

sub reduce_pointer_level($)
{
    my $TypeId = $_[0];
    my %PureType = get_PureType($Tid_TDid{$TypeId}, $TypeId);
    my $BaseTypeId = get_OneStep_BaseTypeId($PureType{"TDid"}, $PureType{"Tid"});
    return ($BaseTypeId eq $TypeId)?"":$BaseTypeId;
}

sub reassemble_array($)
{
    my $TypeId = $_[0];
    return () if(not $TypeId);
    my $FoundationTypeId = get_FoundationTypeId($TypeId);
    if(get_TypeType($FoundationTypeId) eq "Array")
    {
        my ($BaseName, $Length) = (get_TypeName($FoundationTypeId), 1);
        while($BaseName=~s/\[(\d+)\]//)
        {
            $Length*=$1;
        }
        return ($BaseName, $Length);
    }
    else
    {
        return ();
    }
}

sub get_call_malloc($)
{
    my $TypeId = $_[0];
    return "" if(not $TypeId);
    my $FoundationTypeId = get_FoundationTypeId($TypeId);
    my $FoundationTypeName = get_TypeName($FoundationTypeId);
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    my $Conv = ($FoundationTypeName ne "void")?"(".get_TypeName($TypeId).") ":"";
    $Conv=~s/\&//g;
    my $BuffSize = 0;
    if(get_TypeType($FoundationTypeId) eq "Array")
    {
        my ($Array_BaseName, $Array_Length) = reassemble_array($TypeId);
        $Conv = "($Array_BaseName*)";
        $BuffSize = $Array_Length;
        $FoundationTypeName = $Array_BaseName;
        my %ArrayBase = get_BaseType($Tid_TDid{$TypeId}, $TypeId);
        $FoundationTypeId = $ArrayBase{"Tid"};
    }
    else
    {
        $BuffSize = $BUFF_SIZE;
    }
    my $MallocCall = $LibraryMallocFunc?$CompleteSignature{$LibraryMallocFunc}{"ShortName"}:"malloc";
    if($FoundationTypeName eq "void")
    {
        return $Conv.$MallocCall."($BuffSize)";
    }
    else
    {
        if(isOpaque($FoundationTypeId))
        {# opaque buffers
            if(get_TypeType($FoundationTypeId) eq "Array")
            {
                $BuffSize*=$BUFF_SIZE;
            }
            else
            {
                $BuffSize*=4;
            }
            return $Conv.$MallocCall."($BuffSize)";
        }
        else
        {
            if($PointerLevel==1)
            {
                my $ReducedTypeId = reduce_pointer_level($TypeId);
                return $Conv.$MallocCall."(sizeof(".get_TypeName($ReducedTypeId).")".($BuffSize>1?"*$BuffSize":"").")";
            }
            else
            {
                return $Conv.$MallocCall."(sizeof($FoundationTypeName)".($BuffSize>1?"*$BuffSize":"").")";
            }
        }
    }
}

sub add_VirtualSpecType(@)
{
    my %Init_Desc = @_;
    my %NewInit_Desc = %Init_Desc;
    if($Init_Desc{"Value"} eq "")
    {
        $Init_Desc{"Value"} = "no value";
    }
    my ($TypeId, $ParamName, $Interface) = ($Init_Desc{"TypeId"}, $Init_Desc{"ParamName"}, $Init_Desc{"Interface"});
    my $FoundationTypeId = get_FoundationTypeId($TypeId);
    my $FoundationTypeName = get_TypeName($FoundationTypeId);
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    my $FoundationTypeType = $TypeDescr{$Tid_TDid{$FoundationTypeId}}{$FoundationTypeId}{"Type"};
    my $TypeName = get_TypeName($TypeId);
    my $TypeType = get_TypeType($TypeId);
    my $I_ShortName = $CompleteSignature{$Init_Desc{"Interface"}}{"ShortName"};
    my $BlockInterface_ShortName = $CompleteSignature{$CurrentBlock}{"ShortName"};
    if($Init_Desc{"Value"} eq "no value"
    or (defined $ValueCollection{$CurrentBlock}{$ParamName} and $ValueCollection{$CurrentBlock}{$ParamName}==$TypeId))
    {# create value atribute
        if($CurrentBlock and keys(%{$ValueCollection{$CurrentBlock}}) and not $Init_Desc{"InLineArray"})
        {
            ($NewInit_Desc{"Value"}, $NewInit_Desc{"ValueTypeId"}) = select_ValueFromCollection(%Init_Desc);
            if($NewInit_Desc{"Value"} and $NewInit_Desc{"ValueTypeId"})
            {
                my ($Call, $TmpPreamble)=convert_familiar_types((
                    "InputTypeName"=>get_TypeName($NewInit_Desc{"ValueTypeId"}),
                    "InputPointerLevel"=>get_PointerLevel($Tid_TDid{$NewInit_Desc{"ValueTypeId"}}, $NewInit_Desc{"ValueTypeId"}),
                    "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$TypeId,
                    "Value"=>$NewInit_Desc{"Value"},
                    "Key"=>$LongVarNames?$Init_Desc{"Key"}:$ParamName,
                    "Destination"=>"Param",
                    "MustConvert"=>0));
                if($Call and not $TmpPreamble)
                {#try to create simple value
                    $NewInit_Desc{"ValueTypeId"}=$TypeId;
                    $NewInit_Desc{"Value"} = $Call;
                }
                if($NewInit_Desc{"ValueTypeId"}==$TypeId)
                {
                    $NewInit_Desc{"InLine"} = 1;
                }
                $NewInit_Desc{"Reuse"} = 1;
                return %NewInit_Desc;
            }
        }
        if($TypeName=~/\&/
        or not $Init_Desc{"InLine"})
        {
            $NewInit_Desc{"InLine"} = 0;
        }
        else
        {
            $NewInit_Desc{"InLine"} = 1;
        }
        #creating virtual specialized type
        if($TypeName eq "...")
        {
            $NewInit_Desc{"Value"} = get_null();
            $NewInit_Desc{"ValueTypeId"} = get_TypeIdByName("int");
        }
        elsif($FoundationTypeName eq "int" and $ParamName=~/\Aargc(_|)(p|ptr|)\Z/i
        and not $Interface_OutParam{$Interface}{$ParamName} and $PointerLevel>=1
        and my $Value_TId = register_new_type(get_TypeIdByName("int"), 1))
        {# gtk_init ( int* argc, char*** argv )
            $NewInit_Desc{"Value"} = "&argc";
            $NewInit_Desc{"ValueTypeId"} = $Value_TId;
        }
        elsif($FoundationTypeName eq "char" and $ParamName=~/\Aargv(_|)(p|ptr|)\Z/i
        and not $Interface_OutParam{$Interface}{$ParamName} and $PointerLevel>=3
        and my $Value_TId = register_new_type(get_TypeIdByName("char"), 3))
        {# gtk_init ( int* argc, char*** argv )
            $NewInit_Desc{"Value"} = "&argv";
            $NewInit_Desc{"ValueTypeId"} = $Value_TId;
        }
        elsif($FoundationTypeName eq "complex float")
        {
            $NewInit_Desc{"Value"} = getIntrinsicValue("float")." + I*".getIntrinsicValue("float");
            $NewInit_Desc{"ValueTypeId"} = $FoundationTypeId;
        }
        elsif($FoundationTypeName eq "complex double")
        {
            $NewInit_Desc{"Value"} = getIntrinsicValue("double")." + I*".getIntrinsicValue("double");
            $NewInit_Desc{"ValueTypeId"} = $FoundationTypeId;
        }
        elsif($FoundationTypeName eq "complex long double")
        {
            $NewInit_Desc{"Value"} = getIntrinsicValue("long double")." + I*".getIntrinsicValue("long double");
            $NewInit_Desc{"ValueTypeId"} = $FoundationTypeId;
        }
        elsif((($Interface_OutParam{$Interface}{$ParamName} and $PointerLevel>=1) or ($Interface_OutParam_NoUsing{$Interface}{$ParamName}
        and $PointerLevel>=1)) and not grep(/\A(in|input)\Z/, @{get_tokens($ParamName)}) and not isSymbolBuffer($TypeId, $ParamName, $Interface))
        {
            $NewInit_Desc{"InLine"} = 0;
            $NewInit_Desc{"ValueTypeId"} = reduce_pointer_level($TypeId);
            if($PointerLevel>=2)
            {
                $NewInit_Desc{"Value"} = get_null();
            }
            elsif($PointerLevel==1 and isIntegerType(get_TypeName($FoundationTypeId)))
            {
                $NewInit_Desc{"Value"} = "0";
                $NewInit_Desc{"OnlyByValue"} = 1;
            }
            else
            {
                $NewInit_Desc{"OnlyDecl"} = 1;
            }
            $NewInit_Desc{"UseableValue"} = 1;
        }
        elsif($FoundationTypeName eq "void" and $PointerLevel==1
        and my $SimilarType_Id = find_similar_type($NewInit_Desc{"TypeId"}, $ParamName)
        and $TypeName=~/(\W|\A)void(\W|\Z)/ and not $NewInit_Desc{"TypeId_Changed"})
        {
            $NewInit_Desc{"TypeId"} = $SimilarType_Id;
            $NewInit_Desc{"DenyMalloc"} = 1;
            %NewInit_Desc = add_VirtualSpecType(%NewInit_Desc);
            $NewInit_Desc{"TypeId_Changed"} = $TypeId;
        }
        elsif(isArray($TypeId, $ParamName, $Interface)
        and not $Init_Desc{"IsString"})
        {
            $NewInit_Desc{"FoundationType_Type"} = "Array";
            if($ParamName=~/matrix/)
            {
                $NewInit_Desc{"ArraySize"} = 16;
            }
            $NewInit_Desc{"TypeType_Changed"} = 1;
        }
        elsif($Init_Desc{"FuncPtrName"}=~/realloc/i and $PointerLevel==1
        and $Init_Desc{"RetVal"} and $Init_Desc{"FuncPtrTypeId"})
        {
            my %FuncPtrType = get_Type($Tid_TDid{$Init_Desc{"FuncPtrTypeId"}}, $Init_Desc{"FuncPtrTypeId"});
            my ($IntParam, $IntParam2, $PtrParam, $PtrTypeId) = ("", "", "", 0);
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$FuncPtrType{"Memb"}}))
            {
                my $ParamTypeId = $FuncPtrType{"Memb"}{$ParamPos}{"type"};
                my $ParamName = $FuncPtrType{"Memb"}{$ParamPos}{"name"};
                $ParamName = "p".($ParamPos+1) if(not $ParamName);
                my $ParamFTypeId = get_FoundationTypeId($ParamTypeId);
                if(isIntegerType(get_TypeName($ParamTypeId)))
                {
                    if(not $IntParam)
                    {
                        $IntParam = $ParamName;
                    }
                    elsif(not $IntParam2)
                    {
                        $IntParam2 = $ParamName;
                    }
                }
                elsif(get_PointerLevel($Tid_TDid{$ParamTypeId}, $ParamTypeId)==1
                and get_TypeType($ParamFTypeId) eq "Intrinsic")
                {
                    $PtrParam = $ParamName;
                    $PtrTypeId = $ParamTypeId;
                }
            }
            if($IntParam and $PtrParam)
            {# function has an integer parameter
                my $Conv = ($FoundationTypeName ne "void")?"(".get_TypeName($TypeId).") ":"";
                $Conv=~s/\&//g;
                my $VoidConv = (get_TypeName(get_FoundationTypeId($PtrTypeId)) ne "void")?"(void*)":"";
                if($IntParam2)
                {
                    $NewInit_Desc{"Value"} = $Conv."realloc($VoidConv$PtrParam, $IntParam2)";
                }
                else
                {
                    $NewInit_Desc{"Value"} = $Conv."realloc($VoidConv$PtrParam, $IntParam)";
                }
            }
            else
            {
                $NewInit_Desc{"Value"} = get_call_malloc($TypeId);
            }
            $NewInit_Desc{"ValueTypeId"} = $TypeId;
            $NewInit_Desc{"InLine"} = ($Init_Desc{"RetVal"} or ($Init_Desc{"OuterType_Type"} eq "Array"))?1:0;
            if(not $LibraryMallocFunc)
            {
                $NewInit_Desc{"Headers"} = addHeaders(["stdlib.h"], $NewInit_Desc{"Headers"});
                $AuxHeaders{"stdlib.h"} = 1;
            }
        }
        elsif($Init_Desc{"FuncPtrName"}=~/alloc/i and $PointerLevel==1
        and $Init_Desc{"RetVal"} and $Init_Desc{"FuncPtrTypeId"})
        {
            my %FuncPtrType = get_Type($Tid_TDid{$Init_Desc{"FuncPtrTypeId"}}, $Init_Desc{"FuncPtrTypeId"});
            my $IntParam = "";
            foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$FuncPtrType{"Memb"}}))
            {
                my $ParamTypeId = $FuncPtrType{"Memb"}{$ParamPos}{"type"};
                my $ParamName = $FuncPtrType{"Memb"}{$ParamPos}{"name"};
                $ParamName = "p".($ParamPos+1) if(not $ParamName);
                if(isIntegerType(get_TypeName($ParamTypeId)))
                {
                    $IntParam = $ParamName;
                    last;
                }
            }
            if($IntParam)
            {# function has an integer parameter
                my $Conv = ($FoundationTypeName ne "void")?"(".get_TypeName($TypeId).") ":"";
                $Conv=~s/\&//g;
                $NewInit_Desc{"Value"} = $Conv."malloc($IntParam)";
            }
            else
            {
                $NewInit_Desc{"Value"} = get_call_malloc($TypeId);
            }
            $NewInit_Desc{"ValueTypeId"} = $TypeId;
            $NewInit_Desc{"InLine"} = ($Init_Desc{"RetVal"} or ($Init_Desc{"OuterType_Type"} eq "Array"))?1:0;
            if(not $LibraryMallocFunc)
            {
                $NewInit_Desc{"Headers"} = addHeaders(["stdlib.h"], $NewInit_Desc{"Headers"});
                $AuxHeaders{"stdlib.h"} = 1;
            }
        }
        elsif((isBuffer($TypeId, $ParamName, $Interface)
        or ($PointerLevel==1 and $I_ShortName=~/free/i and $FoundationTypeName=~/\A(void|char|unsigned char|wchar_t)\Z/))
        and not $NewInit_Desc{"InLineArray"} and not $Init_Desc{"IsString"} and not $Init_Desc{"DenyMalloc"})
        {
            if(get_TypeName($TypeId) eq "char const*" and (my $NewTypeId = get_TypeIdByName("char*")))
            {
                $TypeId = $NewTypeId;
            }
            $NewInit_Desc{"Value"} = get_call_malloc($TypeId);
            $NewInit_Desc{"ValueTypeId"} = $TypeId;
            $NewInit_Desc{"InLine"} = ($Init_Desc{"RetVal"} or ($Init_Desc{"OuterType_Type"} eq "Array"))?1:0;
            if(not $LibraryMallocFunc)
            {
                $NewInit_Desc{"Headers"} = addHeaders(["stdlib.h"], $NewInit_Desc{"Headers"});
                $AuxHeaders{"stdlib.h"} = 1;
            }
        }
        elsif(isString($TypeId, $ParamName, $Interface)
        or $Init_Desc{"IsString"})
        {
            my @Values = ();
            if($ParamName and $ParamName!~/\Ap\d+\Z/)
            {
                if($I_ShortName=~/Display/ and $ParamName=~/name|display/i)
                {
                    @Values = ("getenv(\"DISPLAY\")");
                    $NewInit_Desc{"Headers"} = addHeaders(["stdlib.h"], $NewInit_Desc{"Headers"});
                    $AuxHeaders{"stdlib.h"} = 1;
                }
                elsif($ParamName=~/uri|url|href/i and $I_ShortName!~/file/i)
                {
                    @Values = ("\"http://ispras.linuxfoundation.org\"", "\"http://www.w3.org/\"");
                }
                elsif($ParamName=~/language/i)
                {
                    @Values = ("\"$COMMON_LANGUAGE\"");
                }
                elsif($ParamName=~/mount/i and $ParamName=~/path/i)
                {
                    @Values = ("\"/dev\"");
                }
                elsif(isStr_FileName($Init_Desc{"ParamPos"}, $ParamName, $I_ShortName))
                {
                    if($I_ShortName=~/sqlite/i)
                    {
                        @Values = ("TG_TEST_DATA_DB");
                    }
                    elsif($TestedInterface=~/\A(ov_|vorbis_)/i)
                    {
                        @Values = ("TG_TEST_DATA_AUDIO");
                    }
                    elsif($TestedInterface=~/\A(zip_)/i)
                    {
                        @Values = ("TG_TEST_DATA_ZIP_FILE");
                    }
                    elsif($ParamName=~/dtd/i or $I_ShortName=~/dtd/i)
                    {
                        @Values = ("TG_TEST_DATA_DTD_FILE");
                    }
                    elsif($ParamName=~/xml/i or $I_ShortName=~/xml/i
                    or ($Init_Desc{"OuterType_Type"}=~/\A(Struct|Union)\Z/ and get_TypeName($Init_Desc{"OuterType_Id"})=~/xml/i))
                    {
                        @Values = ("TG_TEST_DATA_XML_FILE");
                    }
                    elsif($ParamName=~/html/i or $I_ShortName=~/html/i
                    or ($Init_Desc{"OuterType_Type"}=~/\A(Struct|Union)\Z/ and get_TypeName($Init_Desc{"OuterType_Id"})=~/html/i))
                    {
                        @Values = ("TG_TEST_DATA_HTML_FILE");
                    }
                    elsif($ParamName=~/path/i and $I_ShortName=~/\Asnd_/)
                    {# ALSA
                        @Values = ("TG_TEST_DATA_ASOUNDRC_FILE");
                    }
                    else
                    {
                        my $Prefix = getPrefix($I_ShortName);
                        if($Prefix=~/(png|tiff|zip|bmp|bitmap)/i)
                        {
                            @Values = ("TG_TEST_DATA_FILE_".uc($1));
                        }
                        else
                        {
                            @Values = ("TG_TEST_DATA_PLAIN_FILE");
                        }
                    }
                }
                elsif(isStr_Dir($ParamName, $I_ShortName)
                or ($ParamName=~/path/ and get_TypeName($Init_Desc{"OuterType_Id"})=~/Dir|directory/))
                {
                    @Values = ("TG_TEST_DATA_DIRECTORY");
                }
                elsif($ParamName=~/path/i and $I_ShortName=~/\Adbus_/)
                {# D-Bus
                    @Values = ("TG_TEST_DATA_ABS_FILE");
                }
                elsif($ParamName=~/path/i)
                {
                    @Values = ("TG_TEST_DATA_PLAIN_FILE");
                }
                elsif($ParamName=~/\A(ext|extension(s|))\Z/i)
                {
                    @Values = ("\".txt\"", "\".so\"");
                }
                elsif($ParamName=~/mode/i and $I_ShortName=~/fopen/i)
                {# FILE *fopen(const char *path, const char *mode)
                    @Values = ("\"r+\"");
                }
                elsif($ParamName=~/mode/i and $I_ShortName=~/open/i)
                {
                    @Values = ("\"rw\"");
                }
                elsif($ParamName=~/date/i)
                {
                    @Values = ("\"Sun, 06 Nov 1994 08:49:37 GMT\"");
                }
                elsif($ParamName=~/day/i)
                {
                    @Values = ("\"monday\"", "\"tuesday\"");
                }
                elsif($ParamName=~/month/i)
                {
                    @Values = ("\"november\"", "\"october\"");
                }
                elsif($ParamName=~/name/i and $I_ShortName=~/font/i)
                {
                    if($I_ShortName=~/\A[_]*X/)
                    {
                        @Values = ("\"10x20\"", "\"fixed\"");
                    }
                    else
                    {
                        @Values = ("\"times\"", "\"arial\"", "\"courier\"");
                    }
                }
                elsif($ParamName=~/version/i)
                {
                    @Values = ("\"1.0\"", "\"2.0\"");
                }
                elsif($ParamName=~/encoding/i or $Init_Desc{"Key"}=~/encoding/i)
                {
                    @Values = ("\"utf-8\"", "\"koi-8\"");
                }
                elsif($ParamName=~/method/i and $I_ShortName=~/http|ftp|url|uri|request/i)
                {
                    @Values = ("\"GET\"", "\"PUT\"");
                }
                elsif($I_ShortName=~/cast/i and $CompleteSignature{$Interface}{"Class"})
                {
                    @Values = ("\"".get_TypeName($CompleteSignature{$Interface}{"Class"})."\"");
                }
                elsif($I_ShortName=~/\Asnd_/ and $I_ShortName!~/\Asnd_seq_/ and $ParamName=~/name/i) {
                    @Values = ("\"hw:0\"");# ALSA
                }
                elsif($ParamName=~/var/i and $I_ShortName=~/env/i) {
                    @Values = ("\"HOME\"", "\"PATH\"");
                }
                elsif($ParamName=~/error_name/i and $I_ShortName=~/\Adbus_/) {# D-Bus
                    if($Constants{"DBUS_ERROR_FAILED"}{"Value"}) {
                        @Values = ("DBUS_ERROR_FAILED");
                    }
                    else {
                        @Values = ("\"org.freedesktop.DBus.Error.Failed\"");
                    }
                }
                elsif($ParamName=~/name/i and $I_ShortName=~/\Adbus_/) {# D-Bus
                    @Values = ("\"sample.bus\"");
                }
                elsif($ParamName=~/interface/i and $I_ShortName=~/\Adbus_/) {
                    @Values = ("\"sample.interface\"");# D-Bus
                }
                elsif($ParamName=~/address/i and $I_ShortName=~/\Adbus_server/) {
                    @Values = ("\"unix:tmpdir=/tmp\"");# D-Bus
                }
                elsif($CompleteSignature{$Interface}{"Constructor"} and not $Init_Desc{"ParamRenamed"})
                {
                    my $KeyPart = $Init_Desc{"Key"};
                    my $IgnoreSiffix = lc($I_ShortName)."_".$ParamName;
                    $KeyPart=~s/_\Q$ParamName\E\Z// if($I_ShortName=~/string|char/i and $KeyPart!~/(\A|_)\Q$IgnoreSiffix\E\Z/);
                    $KeyPart=~s/_\d+\Z//g;
                    $KeyPart=~s/\A.*_([^_]+)\Z/$1/g;
                    if($KeyPart!~/(\A|_)p\d+\Z/)
                    {
                        $NewInit_Desc{"ParamName"} = $KeyPart;
                        $NewInit_Desc{"ParamRenamed"} = 1;
                        %NewInit_Desc = add_VirtualSpecType(%NewInit_Desc);
                    }
                    else
                    {
                        @Values = ("\"".$ParamName."\"");
                    }
                }
                else
                {
                    @Values = ("\"".$ParamName."\"");
                }
            }
            else
            {
                if($I_ShortName=~/Display/)
                {
                    @Values = ("getenv(\"DISPLAY\")");
                    $NewInit_Desc{"Headers"} = addHeaders(["stdlib.h"], $NewInit_Desc{"Headers"});
                    $AuxHeaders{"stdlib.h"} = 1;
                }
                elsif($I_ShortName=~/cast/ and $CompleteSignature{$Interface}{"Class"})
                {
                    @Values = ("\"".get_TypeName($CompleteSignature{$Interface}{"Class"})."\"");
                }
                else
                {
                    @Values = (getIntrinsicValue("char*"));
                }
            }
            if($FoundationTypeName eq "wchar_t")
            {
                foreach my $Str (@Values)
                {
                    $Str = "L".$Str if($Str=~/\A\"/);
                }
                $NewInit_Desc{"ValueTypeId"} = get_TypeIdByName("wchar_t const*");
            }
            elsif($FoundationTypeType eq "Intrinsic")
            {
                $NewInit_Desc{"ValueTypeId"} = get_TypeIdByName("char const*");
            }
            else
            {# std::string
                $NewInit_Desc{"ValueTypeId"} = $FoundationTypeId;
            }
            $NewInit_Desc{"Value"} = vary_values(\@Values, \%Init_Desc) if($#Values>=0);
            if(not is_const_type(uncover_typedefs(get_TypeName($TypeId))) and not $Init_Desc{"IsString"})
            {
                $NewInit_Desc{"InLine"} = 0;
            }
        }
        elsif(($FoundationTypeName eq "void") and ($PointerLevel==1))
        {
            $NewInit_Desc{"FoundationType_Type"} = "Array";
            $NewInit_Desc{"TypeType_Changed"} = 1;
            $NewInit_Desc{"TypeId"} = get_TypeIdByName("char*");
            $NewInit_Desc{"TypeId_Changed"} = $TypeId;
        }
        elsif($FoundationTypeType eq "Intrinsic")
        {
            if($PointerLevel==1 and $ParamName=~/matrix/i)
            {
                $NewInit_Desc{"FoundationType_Type"} = "Array";
                $NewInit_Desc{"TypeType_Changed"} = 1;
                $NewInit_Desc{"ArraySize"} = 16;
            }
            elsif(isIntegerType($FoundationTypeName))
            {
                if($PointerLevel==0)
                {
                    if($Init_Desc{"RetVal"} and $CurrentBlock=~/read/i)
                    {
                        $NewInit_Desc{"Value"} = "0";
                    }
                    elsif($Init_Desc{"RetVal"} and $TypeName=~/err/i)
                    {
                        $NewInit_Desc{"Value"} = "1";
                    }
                    elsif($ParamName=~/socket/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/freq/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["50"], \%Init_Desc);
                    }
                    elsif($ParamName=~/verbose/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0", "1"], \%Init_Desc);
                    }
                    elsif($ParamName=~/year/i or ($ParamName eq "y" and $I_ShortName=~/date/i))
                    {
                        $NewInit_Desc{"Value"} = vary_values(["2009", "2010"], \%Init_Desc);
                    }
                    elsif($ParamName eq "sa_family"
                    and get_TypeName($Init_Desc{"OuterType_Id"}) eq "struct sockaddr")
                    {
                        $NewInit_Desc{"Value"} = vary_values(["AF_INET", "AF_INET6"], \%Init_Desc);
                    }
                    elsif($ParamName=~/day/i or ($ParamName eq "d" and $I_ShortName=~/date/i))
                    {
                        $NewInit_Desc{"Value"} = vary_values(["30", "13"], \%Init_Desc);
                    }
                    elsif($ParamName=~/month/i or ($ParamName eq "m" and $I_ShortName=~/date/i))
                    {
                        $NewInit_Desc{"Value"} = vary_values(["11", "10"], \%Init_Desc);
                    }
                    elsif($ParamName=~/\Ac\Z/i and $I_ShortName=~/char/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values([get_CharNum()], \%Init_Desc);
                    }
                    elsif($ParamName=~/n_param_values/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["2"], \%Init_Desc);
                    }
                    elsif($ParamName=~/debug/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0", "1"], \%Init_Desc);
                    }
                    elsif($ParamName=~/hook/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["128"], \%Init_Desc);
                    }
                    elsif($ParamName=~/size|length|count/i and $I_ShortName=~/char|string/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["7"], \%Init_Desc);
                    }
                    elsif($ParamName=~/size|length|capacity|count|max|(\A(n|l|s|c)_)/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values([$DEFAULT_ARRAY_AMOUNT], \%Init_Desc);
                    }
                    elsif($ParamName=~/time/i or ($ParamName=~/len/i and $ParamName!~/error/i))
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1", "0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/depth/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1"], \%Init_Desc);
                    }
                    elsif($ParamName=~/delay/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0", "1"], \%Init_Desc);
                    }
                    elsif($TypeName=~/(count|size)_t/i and $ParamName=~/items/)
                    {
                        $NewInit_Desc{"Value"} = vary_values([$DEFAULT_ARRAY_AMOUNT], \%Init_Desc);
                    }
                    elsif($ParamName=~/exists|start/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0", "1"], \%Init_Desc);
                    }
                    elsif($ParamName=~/make/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1", "0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/\A(n|l|s|c)[0-9_]*\Z/i
                    # gsl_vector_complex_float_alloc (size_t const n)
                    # gsl_matrix_complex_float_alloc (size_t const n1, size_t const n2)
                    or (is_alloc_func($I_ShortName) and $ParamName=~/(num|len)[0-9_]*/i))
                    {
                        if($I_ShortName=~/column/)
                        {
                            $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                        }
                        else
                        {
                            $NewInit_Desc{"Value"} = vary_values([$DEFAULT_ARRAY_AMOUNT], \%Init_Desc);
                        }
                    }
                    elsif($Init_Desc{"OuterType_Type"} eq "Array"
                    and $Init_Desc{"Index"} ne "")
                    {
                        $NewInit_Desc{"Value"} = vary_values([$Init_Desc{"Index"}], \%Init_Desc);
                    }
                    elsif(($ParamName=~/index|from|pos|field|line|column|row/i and $ParamName!~/[a-z][a-rt-z]s\Z/i)
                    or $ParamName=~/\A(i|j|k|icol)\Z/i)
                    # gsl_vector_complex_float_get (gsl_vector_complex_float const* v, size_t const i)
                    {
                        if($Init_Desc{"OuterType_Type"} eq "Array")
                        {
                            $NewInit_Desc{"Value"} = vary_values([$Init_Desc{"Index"}], \%Init_Desc);
                        }
                        else
                        {
                            $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                        }
                    }
                    elsif($TypeName=~/bool/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1", "0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/with/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1", "0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/sign/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1", "0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/endian|order/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1", "0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/width|height/i or $ParamName=~/\A(x|y|z|w|h)\d*\Z/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values([8 * getIntrinsicValue($FoundationTypeName)], \%Init_Desc);
                    }
                    elsif($ParamName=~/offset/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["8", "16"], \%Init_Desc);
                    }
                    elsif($ParamName=~/stride|step|spacing|iter|interval|move/i
                    or $ParamName=~/\A(to)\Z/)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1"], \%Init_Desc);
                    }
                    elsif($ParamName=~/channels|frames/i and $I_ShortName=~/\Asnd_/i)
                    {# ALSA
                        $NewInit_Desc{"Value"} = vary_values([$DEFAULT_ARRAY_AMOUNT], \%Init_Desc);
                    }
                    elsif($ParamName=~/first/i and ($Init_Desc{"OuterType_Type"} eq "Struct" and get_TypeName($Init_Desc{"OuterType_Id"})=~/_snd_/i))
                    {# ALSA
                        $NewInit_Desc{"Value"} = vary_values([8 * getIntrinsicValue($FoundationTypeName)], \%Init_Desc);
                    }
                    elsif(isFD($TypeId, $ParamName))
                    {
                        $NewInit_Desc{"Value"} = vary_values(["open(TG_TEST_DATA_PLAIN_FILE, O_RDWR)"], \%Init_Desc);
                        $NewInit_Desc{"Headers"} = addHeaders(["sys/stat.h", "fcntl.h"], $NewInit_Desc{"Headers"});
                        $AuxHeaders{"sys/stat.h"}=1;
                        $NewInit_Desc{"InLine"}=0;
                        $AuxHeaders{"fcntl.h"}=1;
                        $FuncNames{"open"} = 1;
                    }
                    elsif(($TypeName=~/enum/i or $ParamName=~/message_type/i) and my $EnumConstant = selectConstant($TypeName, $ParamName, $Interface))
                    {# or ($TypeName eq "int" and $ParamName=~/\Amode|type\Z/i and $I_ShortName=~/\Asnd_/i) or $ParamName=~/mask/
                        $NewInit_Desc{"Value"} = vary_values([$EnumConstant], \%Init_Desc);
                        $NewInit_Desc{"Headers"} = addHeaders([$Constants{$EnumConstant}{"Header"}], $NewInit_Desc{"Headers"});
                    }
                    elsif($TypeName=~/enum/i or $ParamName=~/mode|type|flag|option/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/mask|alloc/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/screen|format/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["1"], \%Init_Desc);
                    }
                    elsif($ParamName=~/ed\Z/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                    }
                    elsif($ParamName=~/key/i and $I_ShortName=~/\A[_]*X/)
                    {#X11
                        $NewInit_Desc{"Value"} = vary_values(["9"], \%Init_Desc);
                    }
                    elsif($ParamName=~/\Ap\d+\Z/
                    and $Init_Desc{"ParamPos"}==$Init_Desc{"MaxParamPos"}
                    and $I_ShortName=~/create|intern|privat/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values(["0"], \%Init_Desc);
                    }
                    elsif($TypeName=~/size/i)
                    {
                        $NewInit_Desc{"Value"} = vary_values([$DEFAULT_ARRAY_AMOUNT], \%Init_Desc);
                    }
                    else
                    {
                        $NewInit_Desc{"Value"} = vary_values([getIntrinsicValue($FoundationTypeName)], \%Init_Desc);
                    }
                }
                else
                {
                    $NewInit_Desc{"Value"} = "0";
                }
            }
            else
            {
                $NewInit_Desc{"Value"} = vary_values([getIntrinsicValue($FoundationTypeName)], \%Init_Desc);
            }
            $NewInit_Desc{"ValueTypeId"} = ($PointerLevel==0)?$TypeId:$FoundationTypeId;
        }
        elsif($FoundationTypeType eq "Enum")
        {
            if(my $EnumMember = getSomeEnumMember($FoundationTypeId))
            {
                if(defined $Template2Code and $PointerLevel==0)
                {
                    my $Members = [];
                    foreach my $Member (@{getEnumMembers($FoundationTypeId)})
                    {
                        if(is_valid_constant($Member))
                        {
                            push(@{$Members}, $Member);
                        }
                    }
                    if($#{$Members}>=0)
                    {
                        $NewInit_Desc{"Value"} = vary_values($Members, \%Init_Desc);
                    }
                    else
                    {
                        $NewInit_Desc{"Value"} = vary_values(getEnumMembers($FoundationTypeId), \%Init_Desc);
                    }
                }
                else
                {
                    $NewInit_Desc{"Value"} = $EnumMember;
                }
            }
            else
            {
                $NewInit_Desc{"Value"} = "0";
            }
            $NewInit_Desc{"ValueTypeId"} = $FoundationTypeId;
        }
    }
    else
    {
        if(not $NewInit_Desc{"ValueTypeId"})
        {#for union spectypes
            $NewInit_Desc{"ValueTypeId"} = $TypeId;
        }
    }
    if($NewInit_Desc{"Value"} eq "")
    {
        $NewInit_Desc{"Value"} = "no value";
    }
    return %NewInit_Desc;
}

sub is_valid_constant($)
{
    my $Constant = $_[0];
    return $Constant!~/(unknown|invalid|null|err|none|(_|\A)(ms|win\d*)(_|\Z))/i;
}

sub get_CharNum()
{
    $IntrinsicNum{"Char"}=64 if($IntrinsicNum{"Char"} > 89 or $IntrinsicNum{"Char"} < 64);
    if($RandomCode)
    {
        $IntrinsicNum{"Char"} = 64+int(rand(25));
    }
    $IntrinsicNum{"Char"}+=1;
    return $IntrinsicNum{"Char"};
}

sub vary_values($$)
{
    my ($ValuesArrayRef, $Init_Desc) = @_;
    my @ValuesArray = @{$ValuesArrayRef};
    return "" if($#ValuesArray==-1);
    if(defined $Template2Code and ($Init_Desc->{"Interface"} eq $TestedInterface) and not $Init_Desc->{"OuterType_Type"} and length($Init_Desc->{"ParamName"})>=2 and $Init_Desc->{"ParamName"}!~/\Ap\d+\Z/i)
    {
        my $Define = uc($Init_Desc->{"ParamName"});
        if(defined $Constants{$Define})
        {
            $Define = "_".$Define;
        }
        $Define = select_var_name($Define, "");
        $Block_Variable{$CurrentBlock}{$Define} = 1;
        my $DefineWithNum = keys(%Template2Code_Defines).":".$Define;
        if($#ValuesArray>=1)
        {
            $Template2Code_Defines{$DefineWithNum} = "SET(".$ValuesArray[0]."; ".$ValuesArray[1].")";
        }
        else
        {
            $Template2Code_Defines{$DefineWithNum} = $ValuesArray[0];
        }
        return $Define;
    }
    else
    {#standalone
        return $ValuesArray[0];
    }
}

sub selectConstant($$$)
{
    my ($TypeName, $ParamName, $Interface) = @_;
    return $Cache{"selectConstant"}{$TypeName}{$ParamName}{$Interface} if(defined $Cache{"selectConstant"}{$TypeName}{$ParamName}{$Interface});
    my @Csts = ();
    foreach (keys(%Constants))
    {
        if($RegisteredHeaders_Short{$Constants{$_}{"HeaderName"}})
        {
            push(@Csts, $_);
        }
    }
    @Csts = sort @Csts;
    @Csts = sort {length($a)<=>length($b)} @Csts;
    @Csts = sort {$CompleteSignature{$Interface}{"Header"} cmp $Constants{$a}{"HeaderName"}} @Csts;
    my (@Valid, @Invalid) = ();
    foreach (@Csts)
    {
        if(is_valid_constant($_))
        {
            push(@Valid, $_);
        }
        else
        {
            push(@Invalid, $_);
        }
    }
    @Csts = (@Valid, @Invalid);
    sort_byName(\@Csts, [$ParamName, $CompleteSignature{$Interface}{"ShortName"}, $TypeName], "Constants");
    if($#Csts>=0)
    {
        $Cache{"selectConstant"}{$TypeName}{$ParamName}{$Interface} = $Csts[0];
        return $Csts[0];
    }
    else
    {
        $Cache{"selectConstant"}{$TypeName}{$ParamName}{$Interface} = "";
        return "";
    }
}

sub isFD($$)
{
    my ($TypeId, $ParamName) = @_;
    my $FoundationTypeId = get_FoundationTypeId($TypeId);
    my $FoundationTypeName = get_TypeName($FoundationTypeId);
    if($ParamName=~/(\A|[_]+)fd(s|)\Z/i and isIntegerType($FoundationTypeName))
    {
        return (selectSystemHeader("sys/stat.h") and selectSystemHeader("fcntl.h"));
    }
    else
    {
        return "";
    }
}

sub find_similar_type($$)
{
    my ($TypeId, $ParamName) = @_;
    return 0 if(not $TypeId or not $ParamName);
    return 0 if($ParamName=~/\A(p\d+|data)\Z/i or length($ParamName)<=2 or is_out_word($ParamName));
    return $Cache{"find_similar_type"}{$TypeId}{$ParamName} if(defined $Cache{"find_similar_type"}{$TypeId}{$ParamName} and not defined $AuxType{$TypeId});
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    $ParamName=~s/([a-z][a-df-rt-z])s\Z/$1/i;
    my @TypeNames = ();
    foreach my $TypeName (keys(%StructUnionPName_Tid))
    {
        if($TypeName=~/\Q$ParamName\E/i)
        {
            my $Tid = $StructUnionPName_Tid{$TypeName};
            next if(not $Tid);
            next if(not $DependencyHeaders_All{get_TypeHeader($Tid)});
            my $FTid = get_FoundationTypeId($Tid);
            next if(get_TypeType($FTid)!~/\A(Struct|Union)\Z/);
            next if(isOpaque($FTid) and not keys(%{$ReturnTypeId_Interface{$Tid}}));
            next if(get_PointerLevel($Tid_TDid{$Tid}, $Tid)!=$PointerLevel);
            push(@TypeNames, $TypeName);
        }
    }
    @TypeNames = sort {lc($a) cmp lc($b)} @TypeNames;
    @TypeNames = sort {length($a)<=>length($b)} @TypeNames;
    @TypeNames = sort {$a=~/\*/<=>$b=~/\*/} @TypeNames;
    #@TypeNames = sort {keys(%{$ReturnTypeId_Interface{$TName_Tid{$b}}})<=>keys(%{$ReturnTypeId_Interface{$TName_Tid{$a}}})} @TypeNames;
    if($#TypeNames>=0)
    {
        $Cache{"find_similar_type"}{$TypeId}{$ParamName} = $TName_Tid{$TypeNames[0]};
        return $StructUnionPName_Tid{$TypeNames[0]};
    }
    else
    {
        $Cache{"find_similar_type"}{$TypeId}{$ParamName} = 0;
        return 0;
    }
}

sub isCyclical($$)
{
    return (grep {$_ eq $_[1]} @{$_[0]});
}

sub tmplInstHeaders($)
{
    my $ClassId = $_[0];
    my $Headers = [];
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$TemplateInstance{$Tid_TDid{$ClassId}}{$ClassId}}))
    {
        my $Type_Id = $TemplateInstance{$Tid_TDid{$ClassId}}{$ClassId}{$Param_Pos};
        $Headers = addHeaders(getTypeHeaders($Type_Id), $Headers);
    }
    return $Headers;
}

sub convert_familiar_types(@)
{
    my %Conv = @_;
    return () if(not $Conv{"OutputTypeId"} or not $Conv{"InputTypeName"} or not $Conv{"Value"} or not $Conv{"Key"});
    my $OutputType_PointerLevel = get_PointerLevel($Tid_TDid{$Conv{"OutputTypeId"}}, $Conv{"OutputTypeId"});
    my $OutputType_Name = get_TypeName($Conv{"OutputTypeId"});
    my $OutputFType_Id = get_FoundationTypeId($Conv{"OutputTypeId"});
    my $OutputFType_Name = get_TypeName($OutputFType_Id);
    my $OutputType_BaseTypeType = get_TypeType($OutputFType_Id);
    my $PLevelDelta = $OutputType_PointerLevel - $Conv{"InputPointerLevel"};
    return ($Conv{"Value"}, "") if($OutputType_Name eq "...");
    my $Tmp_Var = $Conv{"Key"};
    $Tmp_Var .= ($Conv{"Destination"} eq "Target")?"_tp":"_p";
    my $NeedTypeConvertion = 0;
    my ($Preamble, $ToCall) = ();
    # pointer convertion
    if($PLevelDelta==0)
    {
        $ToCall = $Conv{"Value"};
    }
    elsif($PLevelDelta==1)
    {
        if($Conv{"Value"}=~/\A\&/)
        {
            $Preamble .= $Conv{"InputTypeName"}." $Tmp_Var = (".$Conv{"InputTypeName"}.")".$Conv{"Value"}.";\n";
            $Block_Variable{$CurrentBlock}{$Tmp_Var} = 1;
            $ToCall = "&".$Tmp_Var;
        }
        else
        {
            $ToCall = "&".$Conv{"Value"};
        }
    }
    elsif($PLevelDelta<0)
    {
        foreach (0 .. - 1 - $PLevelDelta)
        {
            $ToCall = $ToCall."*";
        }
        $ToCall = $ToCall.$Conv{"Value"};
    }
    else
    {#this section must be deprecated in future
        my $Stars = "**";
        if($Conv{"Value"}=~/\A\&/)
        {
            $Preamble .= $Conv{"InputTypeName"}." $Tmp_Var = (".$Conv{"InputTypeName"}.")".$Conv{"Value"}.";\n";
            $Block_Variable{$CurrentBlock}{$Tmp_Var} = 1;
            $Conv{"Value"} = $Tmp_Var;
            $Tmp_Var .= "p";
        }
        $Preamble .= $Conv{"InputTypeName"}." *$Tmp_Var = (".$Conv{"InputTypeName"}." *)&".$Conv{"Value"}.";\n";
        $Block_Variable{$CurrentBlock}{$Tmp_Var} = 1;
        my $Tmp_Var_Pre = $Tmp_Var;
        foreach my $Itr (1 .. $PLevelDelta - 1)
        {
            my $ItrM1 = $Itr - 1;
            $Tmp_Var .= "p";
            $Block_Variable{$CurrentBlock}{$Tmp_Var} = 1;
            $Preamble .= $Conv{"InputTypeName"}." $Stars$Tmp_Var = &$Tmp_Var_Pre;\n";
            $Stars .= "*";
            $NeedTypeConvertion = 1;
            $Tmp_Var_Pre = $Tmp_Var;
            $ToCall = $Tmp_Var;
        }
    }
    $Preamble .= "\n" if($Preamble);
    $NeedTypeConvertion = 1 if(get_base_type_name($OutputType_Name) ne get_base_type_name($Conv{"InputTypeName"}));
    $NeedTypeConvertion = 1 if(not is_equal_types($OutputType_Name,$Conv{"InputTypeName"}) and $PLevelDelta==0);
    $NeedTypeConvertion = 1 if(not is_const_type($OutputType_Name) and is_const_type($Conv{"InputTypeName"}));
    $NeedTypeConvertion = 0 if(($OutputType_PointerLevel eq 0) and (($OutputType_BaseTypeType eq "Class") or ($OutputType_BaseTypeType eq "Struct")));
    $NeedTypeConvertion = 1 if((($OutputType_Name=~/\&/) or $Conv{"MustConvert"}) and ($OutputType_PointerLevel > 0) and (($OutputType_BaseTypeType eq "Class") or ($OutputType_BaseTypeType eq "Struct")));
    $NeedTypeConvertion = 1 if($OutputType_PointerLevel eq 2);
    $NeedTypeConvertion = 0 if($OutputType_Name eq $Conv{"InputTypeName"});
    $NeedTypeConvertion = 0 if(uncover_typedefs($OutputType_Name)=~/\[(\d+|)\]/);#arrays
    
    #type convertion
    if($NeedTypeConvertion and ($Conv{"Destination"} eq "Param"))
    {
        if($ToCall=~/\-\>/)
        {
            $ToCall = "(".$OutputType_Name.")"."(".$ToCall.")";
        }
        else
        {
            $ToCall = "(".$OutputType_Name.")".$ToCall;
        }
    }
    return ($ToCall, $Preamble);
}

sub sortTypes_ByPLevel($$)
{
    my ($Types, $PLevel) = @_;
    my (@Eq, @Lt, @Gt) = ();
    foreach my $TypeId (@{$Types})
    {
        my $Type_PLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
        if($Type_PLevel==$PLevel)
        {
            push(@Eq, $TypeId);
        }
        elsif($Type_PLevel<$PLevel)
        {
            push(@Lt, $TypeId);
        }
        elsif($Type_PLevel>$PLevel)
        {
            push(@Gt, $TypeId);
        }
    }
    @{$Types} = (@Eq, @Lt, @Gt);
}

sub familyTypes($)
{
    my $TypeId = $_[0];
    return [] if(not $TypeId);
    my $FoundationTypeId = get_FoundationTypeId($TypeId);
    return $Cache{"familyTypes"}{$TypeId} if($Cache{"familyTypes"}{$TypeId} and not defined $AuxType{$TypeId});
    my (@FamilyTypes_Const, @FamilyTypes_NotConst) = ();
    foreach my $TDid (sort {int($a)<=>int($b)} keys(%TypeDescr))
    {
        foreach my $Tid (sort {int($a)<=>int($b)} keys(%{$TypeDescr{$TDid}}))
        {
            if((get_FoundationTypeId($Tid) eq $FoundationTypeId) and ($Tid ne $TypeId))
            {
                if(is_const_type(get_TypeName($Tid)))
                {
                    @FamilyTypes_Const = (@FamilyTypes_Const, $Tid);
                }
                else
                {
                    @FamilyTypes_NotConst = (@FamilyTypes_NotConst, $Tid);
                }
            }
        }
    }
    sortTypes_ByPLevel(\@FamilyTypes_Const, get_PointerLevel($Tid_TDid{$TypeId}, $TypeId));
    sortTypes_ByPLevel(\@FamilyTypes_NotConst, get_PointerLevel($Tid_TDid{$TypeId}, $TypeId));
    my @FamilyTypes = ((is_const_type(get_TypeName($TypeId)))?(@FamilyTypes_NotConst, $TypeId, @FamilyTypes_Const):($TypeId, @FamilyTypes_NotConst, @FamilyTypes_Const));
    $Cache{"familyTypes"}{$TypeId} = \@FamilyTypes;
    return \@FamilyTypes;
}

sub register_ExtType($$$)
{
    my ($Type_Name, $Type_Type, $BaseTypeId) = @_;
    return "" if(not $Type_Name or not $Type_Type or not $BaseTypeId);
    return $TName_Tid{$Type_Name} if($TName_Tid{$Type_Name});
    $MaxTypeId += 1;
    $TName_Tid{$Type_Name} = $MaxTypeId;
    $Tid_TDid{$MaxTypeId}="";
    %{$TypeDescr{""}{$MaxTypeId}}=(
        "Name" => $Type_Name,
        "Type" => $Type_Type,
        "BaseType"=>{"TDid" => $Tid_TDid{$BaseTypeId},
                     "Tid" => $BaseTypeId},
        "TDid" => "",
        "Tid" => $MaxTypeId,
        "Headers"=>getTypeHeaders($BaseTypeId)
    );
    $AuxType{$MaxTypeId}=$Type_Name;
    return $MaxTypeId;
}


sub get_ExtTypeId($$)
{
    my ($Key, $TypeId) = @_;
    return () if(not $TypeId);
    my ($Declarations, $Headers) = ("", []);
    if(get_TypeType($TypeId) eq "Typedef")
    {
        return ($TypeId, "", "");
    }
    my $FTypeId = get_FoundationTypeId($TypeId);
    my %BaseType = goToFirst($Tid_TDid{$TypeId}, $TypeId, "Typedef");
    my $BaseTypeId = $BaseType{"Tid"};
    if(not $BaseTypeId)
    {
        $BaseTypeId = $FTypeId;
        if(get_TypeName($BaseTypeId)=~/\Astd::/)
        {
            if(my $CxxTypedefId = get_type_typedef($BaseTypeId))
            {
                $BaseTypeId = $CxxTypedefId;
            }
        }
    }
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId) - get_PointerLevel($Tid_TDid{$BaseTypeId}, $BaseTypeId);
    if(get_TypeType($FTypeId) eq "Array")
    {
        my ($Array_BaseName, $Array_Length) = reassemble_array($FTypeId);
        $BaseTypeId = get_TypeIdByName($Array_BaseName);
        $PointerLevel+=1;
    }
    my $BaseTypeName = get_TypeName($BaseTypeId);
    my $BaseTypeType = get_TypeType($BaseTypeId);
    if($BaseTypeType eq "FuncPtr")
    {
        $Declarations .= declare_funcptr_typedef($Key, $BaseTypeId);
    }
    if(isAnon($BaseTypeName))
    {
        if($BaseTypeType eq "Struct")
        {
            my ($AnonStruct_Declarations, $AnonStruct_Headers) = declare_anon_struct($Key, $BaseTypeId);
            $Declarations .= $AnonStruct_Declarations;
            $Headers = addHeaders($AnonStruct_Headers, $Headers);
        }
        elsif($BaseTypeType eq "Union")
        {
            my ($AnonUnion_Declarations, $AnonUnion_Headers) = declare_anon_union($Key, $BaseTypeId);
            $Declarations .= $AnonUnion_Declarations;
            $Headers = addHeaders($AnonUnion_Headers, $Headers);
        }
    }
    if($PointerLevel>=1)
    {
#         if(get_TypeType(get_FoundationTypeId($TypeId)) eq "FuncPtr" and get_TypeName($TypeId)=~/\A[^*]+const\W/)
#         {
#             $BaseTypeId = register_ExtType(get_TypeName($BaseTypeId)." const", "Const", $BaseTypeId);
#         }
        
        my $ExtTypeId = register_new_type($BaseTypeId, $PointerLevel);
        return ($ExtTypeId, $Declarations, $Headers);
    }
    else
    {
        return ($BaseTypeId, $Declarations, $Headers);
    }
}

sub register_new_type($$)
{
    my ($BaseTypeId, $PLevel) = @_;
    my $ExtTypeName = get_TypeName($BaseTypeId);
    my $ExtTypeId = $BaseTypeId;
    foreach (1 .. $PLevel)
    {
        $ExtTypeName .= "*";
        $ExtTypeName = correctName($ExtTypeName);
        if(not $TName_Tid{$ExtTypeName})
        {
            register_ExtType($ExtTypeName, "Pointer", $ExtTypeId);
        }
        $ExtTypeId = $TName_Tid{$ExtTypeName};
    }
    return $ExtTypeId;
}

sub correct_init_stmt($$$)
{
    my ($String, $TypeName, $ParamName) = @_;
    my $Stmt = $TypeName." ".$ParamName." = ".$TypeName;
    if($String=~/\Q$Stmt\E\:\:/)
    {
        return $String;
    }
    else
    {
        $String=~s/(\W|\A)\Q$Stmt\E\(\)(\W|\Z)/$1$TypeName $ParamName$2/g;
        $String=~s/(\W|\A)\Q$Stmt\E(\W|\Z)/$1$TypeName $ParamName$2/g;
        return $String;
    }
}

sub isValidConv($)
{
    return ($_[0]!~/\A(__va_list_tag|...)\Z/);
}

sub emptyDeclaration(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    my $InitializedType_PLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"ValueTypeId"}}, $Init_Desc{"ValueTypeId"});
    my ($ETypeId, $Declarations, $Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ValueTypeId"});
    my $InitializedType_Name = get_TypeName($ETypeId);
    $Type_Init{"Code"} .= $Declarations;
    $Type_Init{"Headers"} = addHeaders($Headers, $Type_Init{"Headers"});
    $Type_Init{"Headers"} = addHeaders($Headers, getTypeHeaders($ETypeId));
    $Type_Init{"Headers"} = addHeaders($Headers, getTypeHeaders(get_FoundationTypeId($ETypeId))) if($InitializedType_PLevel==0);
    $Type_Init{"Init"} = $InitializedType_Name." ".$Var.";\n";
    $Block_Variable{$CurrentBlock}{$Var} = 1;
    #create call
    my ($Call, $Preamble) = convert_familiar_types((
        "InputTypeName"=>$InitializedType_Name,
        "InputPointerLevel"=>$InitializedType_PLevel,
        "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
        "Value"=>$Var,
        "Key"=>$Var,
        "Destination"=>"Param",
        "MustConvert"=>0));
    $Type_Init{"Init"} .= $Preamble;
    $Type_Init{"Call"} = $Call;
    #call to constraint
    if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
    {
        $Type_Init{"TargetCall"} = $Type_Init{"Call"};
    }
    else
    {
        my ($TargetCall, $TargetPreamble) =
        convert_familiar_types((
            "InputTypeName"=>$InitializedType_Name,
            "InputPointerLevel"=>$InitializedType_PLevel,
            "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Target",
            "MustConvert"=>0));
        $Type_Init{"TargetCall"} = $TargetCall;
        $Type_Init{"Init"} .= $TargetPreamble;
    }
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub initializeByValue(@)
{
    my %Init_Desc = @_;
    return () if($Init_Desc{"DoNotAssembly"} and $Init_Desc{"ByNull"});
    my %Type_Init = ();
    $Init_Desc{"InLine"} = 1 if($Init_Desc{"Value"}=~/\$\d+/);
    my $TName_Trivial = get_TypeName($Init_Desc{"TypeId"});
    $TName_Trivial=~s/&//g;
    my $TType = get_TypeType($Init_Desc{"TypeId"});
    my $FoundationType_Id = get_FoundationTypeId($Init_Desc{"TypeId"});
    #$Type_Init{"Headers"} = addHeaders(getTypeHeaders($FoundationType_Id), $Type_Init{"Headers"});
    $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Init_Desc{"TypeId"}), $Type_Init{"Headers"});
    if(uncover_typedefs(get_TypeName($Init_Desc{"TypeId"}))=~/\&/ and $Init_Desc{"OuterType_Type"}=~/\A(Struct|Union|Array)\Z/)
    {
        $Init_Desc{"InLine"} = 0;
    }
    my $FoundationType_Name = get_TypeName($FoundationType_Id);
    my $FoundationType_Type = get_TypeType($FoundationType_Id);
    my $PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $Target_PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TargetTypeId"}}, $Init_Desc{"TargetTypeId"});
    if($FoundationType_Name eq "...")
    {
        $PointerLevel=get_PointerLevel($Tid_TDid{$Init_Desc{"ValueTypeId"}}, $Init_Desc{"ValueTypeId"});
        $Target_PointerLevel=$PointerLevel;
    }
    my $Value_PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"ValueTypeId"}}, $Init_Desc{"ValueTypeId"});
    return () if(not $Init_Desc{"ValueTypeId"} or $Init_Desc{"Value"} eq "");
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    my ($Value_ETypeId, $Declarations, $Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ValueTypeId"});
    my $Value_ETypeName = get_TypeName($Value_ETypeId);
    $Type_Init{"Code"} .= $Declarations;
    $Type_Init{"Headers"} = addHeaders($Headers, $Type_Init{"Headers"});
    if($FoundationType_Type eq "Class")
    {#classes
        my ($ChildCreated, $CallDestructor) = (0, 1);
        my $ValueClass = getValueClass($Init_Desc{"Value"});
        if($ValueClass and ($Target_PointerLevel eq 0))
        {#class object construction by constructor in value
            if($FoundationType_Name eq $ValueClass)
            {
                if(isAbstractClass($FoundationType_Id) or $Init_Desc{"CreateChild"})
                {#when don't know constructor in value, so declaring all in the child
                    my $ChildClassName = getSubClassName($FoundationType_Name);
                    my $FoundationChildName = getSubClassName($FoundationType_Name);
                    $ChildCreated = 1;
                    if(($Init_Desc{"Value"}=~/$FoundationType_Name/) and ($Init_Desc{"Value"}!~/\Q$ChildClassName\E/))
                    {
                        substr($Init_Desc{"Value"}, index($Init_Desc{"Value"}, $FoundationType_Name), pos($FoundationType_Name) + length($FoundationType_Name)) = $FoundationChildName;
                    }
                    $IntSubClass{$TestedInterface}{$FoundationType_Id} = 1;
                    $Create_SubClass{$FoundationType_Id} = 1;
                    foreach my $ClassConstructor (getClassConstructors($FoundationType_Id))
                    {
                        $UsedConstructors{$FoundationType_Id}{$ClassConstructor} = 1;
                    }
                    $FoundationType_Name = $ChildClassName;
                }
            }
            else
            {#new class
                $FoundationType_Name = $ValueClass;
            }
            if($Init_Desc{"InLine"} and ($PointerLevel eq 0))
            {
                $Type_Init{"Call"} = $Init_Desc{"Value"};
                $CallDestructor = 0;
            }
            else
            {
                $Block_Variable{$CurrentBlock}{$Var} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var} = $FoundationType_Id;
                }
                $Type_Init{"Init"} .= $FoundationType_Name." $Var = ".$Init_Desc{"Value"}.";".($Init_Desc{"ByNull"}?" //can't initialize":"")."\n";
                $Type_Init{"Headers"} = addHeaders(getTypeHeaders($FoundationType_Id), $Type_Init{"Headers"});
                $Type_Init{"Init"} = correct_init_stmt($Type_Init{"Init"}, $FoundationType_Name, $Var);
                my ($Call, $TmpPreamble) =
                convert_familiar_types((
                    "InputTypeName"=>$FoundationType_Name,
                    "InputPointerLevel"=>$Value_PointerLevel,
                    "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                    "Value"=>$Var,
                    "Key"=>$Var,
                    "Destination"=>"Param",
                    "MustConvert"=>0));
                $Type_Init{"Init"} .= $TmpPreamble;
                $Type_Init{"Call"} = $Call;
            }
        }
        else
        {#class object returned by some interface in value
            if($Init_Desc{"CreateChild"})
            {
                $ChildCreated = 1;
                my $FoundationChildName = getSubClassName($FoundationType_Name);
                my $TNameChild = $TName_Trivial;
                substr($Value_ETypeName, index($Value_ETypeName, $FoundationType_Name), pos($FoundationType_Name) + length($FoundationType_Name)) = $FoundationChildName;
                substr($TNameChild, index($TNameChild, $FoundationType_Name), pos($FoundationType_Name) + length($FoundationType_Name)) = $FoundationChildName;
                $IntSubClass{$TestedInterface}{$FoundationType_Id} = 1;
                $Create_SubClass{$FoundationType_Id} = 1;
                if($Value_PointerLevel==0
                and my $SomeConstructor = getSomeConstructor($FoundationType_Id))
                {
                    $UsedConstructors{$FoundationType_Id}{$SomeConstructor} = 1;
                }
                if($Init_Desc{"InLine"} and ($PointerLevel eq $Value_PointerLevel))
                {
                    if(($Init_Desc{"Value"} eq "NULL") or ($Init_Desc{"Value"} eq "0"))
                    {
                        $Type_Init{"Call"} = "($TNameChild) ".$Init_Desc{"Value"};
                    }
                    else
                    {
                        my ($Call, $TmpPreamble) =
                        convert_familiar_types((
                            "InputTypeName"=>get_TypeName($Init_Desc{"ValueTypeId"}),
                            "InputPointerLevel"=>$Value_PointerLevel,
                            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                            "Value"=>$Init_Desc{"Value"},
                            "Key"=>$LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"},
                            "Destination"=>"Param",
                            "MustConvert"=>1));
                        $Type_Init{"Call"} = $Call;
                        $Type_Init{"Init"} .= $TmpPreamble;
                    }
                    $CallDestructor = 0;
                }
                else
                {
                    $Block_Variable{$CurrentBlock}{$Var} = 1;
                    if((not defined $DisableReuse and ($Init_Desc{"Value"} ne "NULL") and ($Init_Desc{"Value"} ne "0"))
                    or $Init_Desc{"ByNull"} or $Init_Desc{"UseableValue"})
                    {
                        $ValueCollection{$CurrentBlock}{$Var} = $Value_ETypeId;
                    }
                    $Type_Init{"Init"} .= $Value_ETypeName." $Var = ($Value_ETypeName)".$Init_Desc{"Value"}.";".($Init_Desc{"ByNull"}?" //can't initialize":"")."\n";
                    $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Value_ETypeId), $Type_Init{"Headers"});
                    my ($Call, $TmpPreamble) =
                    convert_familiar_types((
                        "InputTypeName"=>$Value_ETypeName,
                        "InputPointerLevel"=>$Value_PointerLevel,
                        "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                        "Value"=>$Var,
                        "Key"=>$Var,
                        "Destination"=>"Param",
                        "MustConvert"=>0));
                    $Type_Init{"Init"} .= $TmpPreamble;
                    $Type_Init{"Call"} = $Call;
                }
            }
            else
            {
                if($Init_Desc{"InLine"} and ($PointerLevel eq $Value_PointerLevel))
                {
                    if(($Init_Desc{"Value"} eq "NULL") or ($Init_Desc{"Value"} eq "0"))
                    {
                        $Type_Init{"Call"} = "($TName_Trivial) ".$Init_Desc{"Value"};
                        $CallDestructor = 0;
                    }
                    else
                    {
                        $CallDestructor = 0;
                        my ($Call, $TmpPreamble) =
                        convert_familiar_types((
                            "InputTypeName"=>get_TypeName($Init_Desc{"ValueTypeId"}),
                            "InputPointerLevel"=>$Value_PointerLevel,
                            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                            "Value"=>$Init_Desc{"Value"},
                            "Key"=>$LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"},
                            "Destination"=>"Param",
                            "MustConvert"=>1));
                        $Type_Init{"Call"} = $Call;
                        $Type_Init{"Init"} .= $TmpPreamble;
                    }
                }
                else
                {
                    $Block_Variable{$CurrentBlock}{$Var} = 1;
                    if((not defined $DisableReuse and ($Init_Desc{"Value"} ne "NULL") and ($Init_Desc{"Value"} ne "0"))
                    or $Init_Desc{"ByNull"} or $Init_Desc{"UseableValue"})
                    {
                        $ValueCollection{$CurrentBlock}{$Var} = $Value_ETypeId;
                    }
                    $Type_Init{"Init"} .= $Value_ETypeName." $Var = ".$Init_Desc{"Value"}.";".($Init_Desc{"ByNull"}?" //can't initialize":"")."\n";
                    $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Value_ETypeId), $Type_Init{"Headers"});
                    my ($Call, $TmpPreamble) =
                    convert_familiar_types((
                        "InputTypeName"=>$Value_ETypeName,
                        "InputPointerLevel"=>$Value_PointerLevel,
                        "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                        "Value"=>$Var,
                        "Key"=>$Var,
                        "Destination"=>"Param",
                        "MustConvert"=>0));
                    $Type_Init{"Init"} .= $TmpPreamble;
                    $Type_Init{"Call"} = $Call;
                }
            }
        }
        
        #create destructor call for class object
        if($CallDestructor and
        ((has_public_destructor($FoundationType_Id, "D2") and $ChildCreated) or
        (has_public_destructor($FoundationType_Id, "D0") and not $ChildCreated)) )
        {
            if($Value_PointerLevel > 0)
            {
                if($Value_PointerLevel eq 1)
                {
                    $Type_Init{"Destructors"} .= "delete($Var);\n";
                }
                else
                {
                    $Type_Init{"Destructors"} .= "delete(";
                    foreach (0 .. $Value_PointerLevel - 2)
                    {
                        $Type_Init{"Destructors"} .= "*";
                    }
                    $Type_Init{"Destructors"} .= $Var.");\n";
                }
            }
        }
    }
    else
    {#intrinsics, structs
        if($Init_Desc{"InLine"} and ($PointerLevel eq $Value_PointerLevel))
        {
            if(($Init_Desc{"Value"} eq "NULL") or ($Init_Desc{"Value"} eq "0"))
            {
                if((getIntLang($TestedInterface) eq "C++" or $Init_Desc{"StrongConvert"})
                and isValidConv($TName_Trivial) and ($Init_Desc{"OuterType_Type"} ne "Array"))
                {
                    $Type_Init{"Call"} = "($TName_Trivial) ".$Init_Desc{"Value"};
                }
                else
                {
                    $Type_Init{"Call"} = $Init_Desc{"Value"};
                }
            }
            else
            {
                if((not is_equal_types(get_TypeName($Init_Desc{"TypeId"}), get_TypeName($Init_Desc{"ValueTypeId"})) or $Init_Desc{"StrongConvert"}) and isValidConv($TName_Trivial))
                {
                    $Type_Init{"Call"} = "($TName_Trivial) ".$Init_Desc{"Value"};
                }
                else
                {
                    $Type_Init{"Call"} = $Init_Desc{"Value"};
                }
            }
        }
        else
        {
            $Block_Variable{$CurrentBlock}{$Var} = 1;
            if((not defined $DisableReuse and ($Init_Desc{"Value"} ne "NULL") and ($Init_Desc{"Value"} ne "0"))
            or $Init_Desc{"ByNull"} or $Init_Desc{"UseableValue"})
            {
                $ValueCollection{$CurrentBlock}{$Var} = $Value_ETypeId;
            }
            $Type_Init{"Init"} .= $Value_ETypeName." $Var = ".$Init_Desc{"Value"}.";".($Init_Desc{"ByNull"}?" //can't initialize":"")."\n";
            $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Value_ETypeId), $Type_Init{"Headers"});
            my ($Call, $TmpPreamble) =
            convert_familiar_types((
                "InputTypeName"=>$Value_ETypeName,
                "InputPointerLevel"=>$Value_PointerLevel,
                "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Param",
                "MustConvert"=>$Init_Desc{"StrongConvert"}));
            $Type_Init{"Init"} .= $TmpPreamble;
            $Type_Init{"Call"} = $Call;
        }
    }
    #call to constraint
    if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
    {
        $Type_Init{"TargetCall"} = $Type_Init{"Call"};
    }
    else
    {
        my ($TargetCall, $TargetPreamble) =
        convert_familiar_types((
            "InputTypeName"=>$Value_ETypeName,
            "InputPointerLevel"=>$Value_PointerLevel,
            "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Target",
            "MustConvert"=>0));
        $Type_Init{"TargetCall"} = $TargetCall;
        $Type_Init{"Init"} .= $TargetPreamble;
    }
    if(get_TypeType($Init_Desc{"TypeId"}) eq "Ref")
    {#ref handler
        my $BaseRefId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
        my $BaseRefName = get_TypeName($BaseRefId);
        if(get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId) > $Value_PointerLevel)
        {
            $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
            $Type_Init{"Call"} = $Var."_ref";
            $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
            if(not defined $DisableReuse and ($Init_Desc{"Value"} ne "NULL") and ($Init_Desc{"Value"} ne "0"))
            {
                $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
            }
        }
    }
    $Type_Init{"Code"} = $Type_Init{"Code"};
    $Type_Init{"IsCorrect"} = 1;
    $Type_Init{"ByNull"} = 1 if($Init_Desc{"ByNull"});
    return %Type_Init;
}

sub remove_quals($)
{
    my $Type_Name = $_[0];
    $Type_Name=~s/ (const|volatile|restrict)\Z//g;
    $Type_Name=~s/\A(const|volatile|restrict) //g;
    while($Type_Name=~s/(\W|\A|>)(const|volatile|restrict)(\W([^<>()]+|)|)\Z/$1$3/g){};
    return correctName($Type_Name);
}

sub is_equal_types($$)
{
    my ($Type1_Name, $Type2_Name) = @_;
    return (remove_quals(uncover_typedefs($Type1_Name)) eq
            remove_quals(uncover_typedefs($Type2_Name)));
}

sub get_base_type_name($)
{
    my $Type_Name = $_[0];
    while($Type_Name=~s/(\*|\&)([^<>()]+|)\Z/$2/g){};
    my $Type_Name = remove_quals(uncover_typedefs($Type_Name));
    while($Type_Name=~s/(\*|\&)([^<>()]+|)\Z/$2/g){};
    return $Type_Name;
}

sub isIntegerType($)
{
    my $TName = remove_quals(uncover_typedefs($_[0]));
    return 0 if($TName=~/[(<*]/);
    if($TName eq "bool")
    {
        return (getIntLang($TestedInterface) ne "C++");
    }
    return ($TName=~/(\W|\A| )(int)(\W|\Z| )/
    or $TName=~/\A(short|size_t|unsigned|long|long long|unsigned long|unsigned long long|unsigned short)\Z/);
}

sub isNumericType($)
{
    my $TName = uncover_typedefs($_[0]);
    return 0 if($TName=~/[(<*]/);
    if(isIntegerType($TName))
    {
        return 1;
    }
    else
    {
        return ($TName=~/\A(float|double|long double|float const|double const|long double const)\Z/);
    }
}

sub getIntrinsicValue($)
{
    my $TypeName = $_[0];
    $IntrinsicNum{"Char"}=64 if($IntrinsicNum{"Char"}>89 or $IntrinsicNum{"Char"}<64);
    $IntrinsicNum{"Int"}=0 if($IntrinsicNum{"Int"} >= 10);
    if($RandomCode)
    {
        $IntrinsicNum{"Char"} = 64+int(rand(25));
        $IntrinsicNum{"Int"} = int(rand(5));
    }
    if($TypeName eq "char*")
    {
        $IntrinsicNum{"Str"}+=1;
        if($IntrinsicNum{"Str"}==1)
        {
            return "\"str\"";
        }
        else
        {
            return "\"str".$IntrinsicNum{"Str"}."\"";
        }
    }
    elsif($TypeName=~/(\A| )char(\Z| )/)
    {
        $IntrinsicNum{"Char"} += 1;
        return "'".chr($IntrinsicNum{"Char"})."'";
    }
    elsif($TypeName eq "wchar_t")
    {
        $IntrinsicNum{"Char"}+=1;
        return "L'".chr($IntrinsicNum{"Char"})."'";
    }
    elsif($TypeName eq "wchar_t*")
    {
        $IntrinsicNum{"Str"}+=1;
        if($IntrinsicNum{"Str"}==1)
        {
            return "L\"str\"";
        }
        else
        {
            return "L\"str".$IntrinsicNum{"Str"}."\"";
        }
    }
    elsif($TypeName eq "wint_t")
    {
        $IntrinsicNum{"Int"}+=1;
        return "L".$IntrinsicNum{"Int"};
    }
    elsif($TypeName=~/\A(long|long int)\Z/)
    {
        $IntrinsicNum{"Int"} += 1;
        return $IntrinsicNum{"Int"}."L";
    }
    elsif($TypeName=~/\A(long long|long long int)\Z/)
    {
        $IntrinsicNum{"Int"} += 1;
        return $IntrinsicNum{"Int"}."LL";
    }
    elsif(isIntegerType($TypeName))
    {
        $IntrinsicNum{"Int"} += 1;
        return $IntrinsicNum{"Int"};
    }
    elsif($TypeName eq "float")
    {
        $IntrinsicNum{"Float"} += 1;
        return $IntrinsicNum{"Float"}.".5f";
    }
    elsif($TypeName eq "double")
    {
        $IntrinsicNum{"Float"} += 1;
        return $IntrinsicNum{"Float"}.".5";
    }
    elsif($TypeName eq "long double")
    {
        $IntrinsicNum{"Float"} += 1;
        return $IntrinsicNum{"Float"}.".5L";
    }
    elsif($TypeName eq "bool")
    {
        if(getIntLang($TestedInterface) eq "C++")
        {
            return "true";
        }
        else
        {
            return "1";
        }
    }
    else
    {#void, "..." and other
        return "";
    }
}

sub findInterface_OutParam($$$$$$)
{
    my ($TypeId, $Key, $StrongTypeCompliance, $Var, $ParamName, $Strong) = @_;
    return () if(not $TypeId);
    foreach my $FamilyTypeId (get_OutParamFamily($TypeId, 1))
    {
        foreach my $Interface (get_CompatibleInterfaces($FamilyTypeId, "OutParam", $ParamName))
        {#find interface to create some type in the family as output parameter
            if($Strong)
            {
                foreach my $PPos (keys(%{$CompleteSignature{$Interface}{"Param"}}))
                {# only one possible structural out parameter
                    my $PTypeId = $CompleteSignature{$Interface}{"Param"}{$PPos}{"type"};
                    my $P_FTypeId = get_FoundationTypeId($PTypeId);
                    return () if(get_TypeType($P_FTypeId)!~/\A(Intrinsic|Enum)\Z/
                    and $P_FTypeId ne get_FoundationTypeId($FamilyTypeId)
                    and not is_const_type(get_TypeName($PTypeId)));
                }
            }
            my $OutParam_Pos = $OutParam_Interface{$FamilyTypeId}{$Interface};
            my %Interface_Init = callInterface((
                "Interface"=>$Interface, 
                "Key"=>$Key, 
                "OutParam"=>$OutParam_Pos,
                "OutVar"=>$Var));
            if($Interface_Init{"IsCorrect"})
            {
                $Interface_Init{"Interface"} = $Interface;
                $Interface_Init{"OutParamPos"} = $OutParam_Pos;
                return %Interface_Init;
            }
        }
    }
    return ();
}

sub findInterface(@)
{
    my %Init_Desc = @_;
    my ($TypeId, $Key, $StrongTypeCompliance, $ParamName) = ($Init_Desc{"TypeId"}, $Init_Desc{"Key"}, $Init_Desc{"StrongTypeCompliance"}, $Init_Desc{"ParamName"});
    return () if(not $TypeId);
    my @FamilyTypes = ();
    if($StrongTypeCompliance)
    {
        @FamilyTypes = ($TypeId);
        # try to initialize basic typedef
        my $BaseTypeId = $TypeId;
        $BaseTypeId = get_OneStep_BaseTypeId($Tid_TDid{$TypeId}, $TypeId) if(get_TypeType($BaseTypeId) eq "Const");
        $BaseTypeId = get_OneStep_BaseTypeId($Tid_TDid{$TypeId}, $TypeId) if(get_TypeType($BaseTypeId) eq "Pointer");
        push(@FamilyTypes, $BaseTypeId) if(get_TypeType($BaseTypeId) eq "Typedef");
    }
    else
    {
        @FamilyTypes = @{familyTypes($TypeId)};
    }
    my @Ints = ();
    foreach my $FamilyTypeId (@FamilyTypes)
    {
        next if((get_PointerLevel($Tid_TDid{$TypeId}, $TypeId)<get_PointerLevel($Tid_TDid{$FamilyTypeId}, $FamilyTypeId)) and $Init_Desc{"OuterType_Type"} eq "Array");
        next if(get_TypeType($TypeId) eq "Class" and get_PointerLevel($Tid_TDid{$FamilyTypeId}, $FamilyTypeId)==0);
        if($Init_Desc{"OnlyData"})
        {
            @Ints = (@Ints, get_CompatibleInterfaces($FamilyTypeId, "OnlyData",
                              $ParamName." ".$Init_Desc{"KeyWords"}." ".$Init_Desc{"Interface"}));
        }
        elsif($Init_Desc{"OnlyReturn"})
        {
            @Ints = (@Ints, get_CompatibleInterfaces($FamilyTypeId, "OnlyReturn",
                              $ParamName." ".$Init_Desc{"KeyWords"}." ".$Init_Desc{"Interface"}));
        }
        else
        {
            @Ints = (@Ints, get_CompatibleInterfaces($FamilyTypeId, "Return",
                              $ParamName." ".$Init_Desc{"KeyWords"}." ".$Init_Desc{"Interface"}));
        }
    }
    sort_byCriteria(\@Ints, "DeleteSmth");
    foreach my $Interface (@Ints)
    {# find interface for returning some type in the family
        my %Interface_Init = callInterface((
            "Interface"=>$Interface, 
            "Key"=>$Key,
            "RetParam"=>$ParamName,
            "GetReturn"=>1));
        if($Interface_Init{"IsCorrect"})
        {
            $Interface_Init{"Interface"} = $Interface;
            return %Interface_Init;
        }
    }
    return ();
}

sub initializeByInterface_OutParam(@)
{
    my %Init_Desc = @_;
    return () if(not $Init_Desc{"TypeId"});
    my $Global_State = save_state();
    my %Type_Init = ();
    my $FTypeId = get_FoundationTypeId($Init_Desc{"TypeId"});
    my $PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    $Block_Variable{$CurrentBlock}{$Var} = 1;
    my %Interface_Init = findInterface_OutParam($Init_Desc{"TypeId"}, $Init_Desc{"Key"}, $Init_Desc{"StrongTypeCompliance"}, "\@OUT_PARAM\@", $Init_Desc{"ParamName"}, $Init_Desc{"Strong"});
    if(not $Interface_Init{"IsCorrect"})
    {
        restore_state($Global_State);
        return ();
    }
    $Type_Init{"Init"} = $Interface_Init{"Init"};
    $Type_Init{"Destructors"} = $Interface_Init{"Destructors"};
    $Type_Init{"Code"} .= $Interface_Init{"Code"};
    $Type_Init{"Headers"} = addHeaders($Interface_Init{"Headers"}, $Type_Init{"Headers"});
    
    #initialization
    my $OutParam_Pos = $Interface_Init{"OutParamPos"};
    my $OutParam_TypeId = $CompleteSignature{$Interface_Init{"Interface"}}{"Param"}{$OutParam_Pos}{"type"};
    my $PLevel_Out = get_PointerLevel($Tid_TDid{$OutParam_TypeId}, $OutParam_TypeId);
    my ($InitializedEType_Id, $Declarations, $Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $OutParam_TypeId);
    my $InitializedType_Name = get_TypeName($InitializedEType_Id);
    $Type_Init{"Code"} .= $Declarations;
    $Type_Init{"Headers"} = addHeaders($Headers, $Type_Init{"Headers"});
    my $InitializedFType_Id = get_FoundationTypeId($OutParam_TypeId);
    my $InitializedFType_Type = get_TypeType($InitializedFType_Id);
    my $InitializedType_PointerLevel = get_PointerLevel($Tid_TDid{$OutParam_TypeId}, $OutParam_TypeId);
    my $VarNameForReplace = $Var;
    if($PLevel_Out>=1)
    {
        $OutParam_TypeId = reduce_pointer_level($InitializedEType_Id);
        $InitializedType_Name=get_TypeName($OutParam_TypeId);
        $VarNameForReplace="&".$Var;
        $InitializedType_PointerLevel-=1;
    }
    foreach (keys(%Interface_Init))
    {
        $Interface_Init{$_}=~s/\@OUT_PARAM\@/$VarNameForReplace/g;
        $Interface_Init{$_} = clearSyntax($Interface_Init{$_});
    }
    if(uncover_typedefs($InitializedType_Name)=~/&|\[/ or $PLevel_Out==1)
    {
#         if($InitializedFType_Type eq "Struct")
#         {
#             my %Struct_Desc = %Init_Desc;
#             $Struct_Desc{"TypeId"} = $OutParam_TypeId;
#             $Struct_Desc{"InLine"} = 0;
#             my $Key = $Struct_Desc{"Key"};
#             delete($Block_Variable{$CurrentBlock}{$Var});
#             my %Assembly = assembleStruct(%Struct_Desc);
#             $Block_Variable{$CurrentBlock}{$Var} = 1;
#             $Type_Init{"Init"} .= $Assembly{"Init"};
#             $Type_Init{"Code"} .= $Assembly{"Code"};
#             $Type_Init{"Headers"} = addHeaders($Assembly{"Headers"}, $Type_Init{"Headers"});
#         }
#         else
#         {
        $Type_Init{"Init"} .= $InitializedType_Name." $Var;\n";
        if(get_TypeType($InitializedFType_Id) eq "Struct")
        {
            my %Type = get_Type($Tid_TDid{$InitializedFType_Id}, $InitializedFType_Id);
            foreach my $MemPos (keys(%{$Type{"Memb"}}))
            {
                if($Type{"Memb"}{$MemPos}{"name"}=~/initialized/i
                and isNumericType(get_TypeName($Type{"Memb"}{$MemPos}{"type"})))
                {
                    $Type_Init{"Init"} .= "$Var.initialized = 0;\n";
                    last;
                }
            }
        }
    }
    else
    {
        $Type_Init{"Init"} .= $InitializedType_Name." $Var = ".get_null().";\n";
    }
    if(not defined $DisableReuse)
    {
        $ValueCollection{$CurrentBlock}{$Var} = $OutParam_TypeId;
    }
    $Type_Init{"Init"} .= $Interface_Init{"PreCondition"} if($Interface_Init{"PreCondition"});
    $Type_Init{"Init"} .= $Interface_Init{"Call"}.";\n";
    $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Init_Desc{"TypeId"}), $Type_Init{"Headers"});
    $Type_Init{"Init"} .= $Interface_Init{"PostCondition"} if($Interface_Init{"PostCondition"});
    if($Interface_Init{"FinalCode"})
    {
        $Type_Init{"Init"} .= "//final code\n";
        $Type_Init{"Init"} .= $Interface_Init{"FinalCode"}."\n";
    }
    #create call
    my ($Call, $Preamble) = convert_familiar_types((
        "InputTypeName"=>$InitializedType_Name,
        "InputPointerLevel"=>$InitializedType_PointerLevel,
        "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
        "Value"=>$Var,
        "Key"=>$Var,
        "Destination"=>"Param",
        "MustConvert"=>0));
    $Type_Init{"Init"} .= $Preamble;
    $Type_Init{"Call"} = $Call;
    #create call to constraint
    if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
    {
        $Type_Init{"TargetCall"} = $Type_Init{"Call"};
    }
    else
    {
        my ($TargetCall, $TargetPreamble) = convert_familiar_types((
            "InputTypeName"=>$InitializedType_Name,
            "InputPointerLevel"=>$InitializedType_PointerLevel,
            "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Target",
            "MustConvert"=>0));
        $Type_Init{"TargetCall"} = $TargetCall;
        $Type_Init{"Init"} .= $TargetPreamble;
    }
    if(get_TypeType($Init_Desc{"TypeId"}) eq "Ref")
    {#ref handler
        my $BaseRefTypeId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
        if(get_PointerLevel($Tid_TDid{$BaseRefTypeId}, $BaseRefTypeId) > $InitializedType_PointerLevel)
        {
            my $BaseRefTypeName = get_TypeName($BaseRefTypeId);
            $Type_Init{"Init"} .= $BaseRefTypeName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
            $Type_Init{"Call"} = $Var."_ref";
            $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
            if(not defined $DisableReuse)
            {
                $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
            }
        }
    }
    $Type_Init{"Init"} .= "\n";
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub declare_funcptr_typedef($$)
{
    my ($Key, $TypeId) = @_;
    return "" if($AuxType{$TypeId} or not $TypeId or not $Key);
    my $TypedefTo = $Key."_type";
    my $Typedef = "typedef ".get_TypeName($TypeId).";\n";
    $Typedef=~s/[ ]*\(\*\)[ ]*/ \(\*$TypedefTo\) /;
    $AuxType{$TypeId} = $TypedefTo;
    $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Name_Old"} = get_TypeName($TypeId);
    $TypeDescr{$Tid_TDid{$TypeId}}{$TypeId}{"Name"} = $AuxType{$TypeId};
    $TName_Tid{$TypedefTo} = $TypeId;
    return $Typedef;
}

sub have_copying_constructor($)
{
    my $ClassId = $_[0];
    return 0 if(not $ClassId);
    foreach my $Constructor (keys(%{$Class_Constructors{$ClassId}}))
    {
        if(keys(%{$CompleteSignature{$Constructor}{"Param"}})==1
        and not $CompleteSignature{$Constructor}{"Protected"}
        and not $CompleteSignature{$Constructor}{"Private"})
        {
            my $FirstParamTypeId = $CompleteSignature{$Constructor}{"Param"}{0}{"type"};
            if(get_FoundationTypeId($FirstParamTypeId) eq $ClassId
            and get_PointerLevel($Tid_TDid{$FirstParamTypeId}, $FirstParamTypeId)==0)
            {
                return 1;
            }
        }
    }
    return 0;
}

sub initializeByInterface(@)
{
    my %Init_Desc = @_;
    return () if(not $Init_Desc{"TypeId"});
    my $Global_State = save_state();
    my %Type_Init = ();
    my $PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $FTypeId = get_FoundationTypeId($Init_Desc{"TypeId"});
    if(get_TypeType($FTypeId) eq "Class" and $PointerLevel==0
    and not have_copying_constructor($FTypeId))
    {
        return ();
    }
    my %Interface_Init = ();
    if($Init_Desc{"ByInterface"})
    {
        %Interface_Init = callInterface((
          "Interface"=>$Init_Desc{"ByInterface"}, 
          "Key"=>$Init_Desc{"Key"},
          "RetParam"=>$Init_Desc{"ParamName"},
          "GetReturn"=>1,
          "OnlyReturn"=>1));
    }
    else
    {
        %Interface_Init = findInterface(%Init_Desc);
    }
    if(not $Interface_Init{"IsCorrect"})
    {
        restore_state($Global_State);
        return ();
    }
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    $Type_Init{"Init"} = $Interface_Init{"Init"};
    $Type_Init{"Destructors"} = $Interface_Init{"Destructors"};
    $Type_Init{"Code"} = $Interface_Init{"Code"};
    $Type_Init{"Headers"} = addHeaders($Interface_Init{"Headers"}, $Type_Init{"Headers"});
    if(keys(%{$CompleteSignature{$Interface_Init{"Interface"}}{"Param"}})>$MAX_PARAMS_INLINE)
    {
        $Init_Desc{"InLine"} = 0;
    }
    #initialization
    my $ReturnType_PointerLevel = get_PointerLevel($Tid_TDid{$Interface_Init{"ReturnTypeId"}}, $Interface_Init{"ReturnTypeId"});
    if($ReturnType_PointerLevel==$PointerLevel and $Init_Desc{"InLine"}
    and not $Interface_Init{"PreCondition"} and not $Interface_Init{"PostCondition"}
    and not $Interface_Init{"ReturnFinalCode"})
    {
        my ($Call, $Preamble) = convert_familiar_types((
            "InputTypeName"=>get_TypeName($Interface_Init{"ReturnTypeId"}),
            "InputPointerLevel"=>$ReturnType_PointerLevel,
            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
            "Value"=>$Interface_Init{"Call"},
            "Key"=>$Init_Desc{"Var"},
            "Destination"=>"Param",
            "MustConvert"=>0));
        $Type_Init{"Init"} .= $Preamble;
        $Type_Init{"Call"} = $Call;
        $Type_Init{"TypeName"} = get_TypeName($Interface_Init{"ReturnTypeId"});
    }
    else
    {
        my $Var = $Init_Desc{"Var"};
        $Block_Variable{$CurrentBlock}{$Var} = 1;
        my ($InitializedEType_Id, $Declarations, $Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Interface_Init{"ReturnTypeId"});
        my $InitializedType_Name = get_TypeName($InitializedEType_Id);
        $Type_Init{"TypeName"} = $InitializedType_Name;
        $Type_Init{"Code"} .= $Declarations;
        $Type_Init{"Headers"} = addHeaders($Headers, $Type_Init{"Headers"});
        my %ReturnType = get_Type($Tid_TDid{$Interface_Init{"ReturnTypeId"}}, $Interface_Init{"ReturnTypeId"});
        if(not defined $DisableReuse)
        {
            $ValueCollection{$CurrentBlock}{$Var} = $Interface_Init{"ReturnTypeId"};
        }
        $Type_Init{"Init"} .= $Interface_Init{"PreCondition"} if($Interface_Init{"PreCondition"});
        if(($InitializedType_Name eq $ReturnType{"Name"}))
        {
            $Type_Init{"Init"} .= $InitializedType_Name." $Var = ".$Interface_Init{"Call"}.";\n";
        }
        else
        {
            $Type_Init{"Init"} .= $InitializedType_Name." $Var = "."(".$InitializedType_Name.")".$Interface_Init{"Call"}.";\n";
        }
        if($Interface_Init{"Interface"} eq "fopen")
        {
            $OpenStreams{$CurrentBlock}{$Var} = 1;
        }
        #create call
        my ($Call, $Preamble) = convert_familiar_types((
            "InputTypeName"=>$InitializedType_Name,
            "InputPointerLevel"=>$ReturnType_PointerLevel,
            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Param",
            "MustConvert"=>0));
        $Type_Init{"Init"} .= $Preamble;
        $Type_Init{"Call"} = $Call;
        #create call to constraint
        if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
        {
            $Type_Init{"TargetCall"} = $Type_Init{"Call"};
        }
        else
        {
            my ($TargetCall, $TargetPreamble) = convert_familiar_types((
                "InputTypeName"=>$InitializedType_Name,
                "InputPointerLevel"=>$ReturnType_PointerLevel,
                "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Target",
                "MustConvert"=>0));
            $Type_Init{"TargetCall"} = $TargetCall;
            $Type_Init{"Init"} .= $TargetPreamble;
        }
        if(get_TypeType($Init_Desc{"TypeId"}) eq "Ref")
        {#ref handler
            my $BaseRefTypeId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
            if(get_PointerLevel($Tid_TDid{$BaseRefTypeId}, $BaseRefTypeId) > $ReturnType_PointerLevel)
            {
                my $BaseRefTypeName = get_TypeName($BaseRefTypeId);
                $Type_Init{"Init"} .= $BaseRefTypeName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
                $Type_Init{"Call"} = $Var."_ref";
                $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
                }
            }
        }
        if($Interface_Init{"ReturnRequirement"})
        {
            $Interface_Init{"ReturnRequirement"}=~s/(\$0|\$retval)/$Var/gi;
            $Type_Init{"Init"} .= $Interface_Init{"ReturnRequirement"};
        }
        if($Interface_Init{"ReturnFinalCode"})
        {
            $Interface_Init{"ReturnFinalCode"}=~s/(\$0|\$retval)/$Var/gi;
            $Type_Init{"Init"} .= "//final code\n";
            $Type_Init{"Init"} .= $Interface_Init{"ReturnFinalCode"}."\n";
        }
    }
    $Type_Init{"Init"} .= $Interface_Init{"PostCondition"} if($Interface_Init{"PostCondition"});
    if($Interface_Init{"FinalCode"})
    {
        $Type_Init{"Init"} .= "//final code\n";
        $Type_Init{"Init"} .= $Interface_Init{"FinalCode"}."\n";
    }
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub initializeFuncPtr(@)
{
    my %Init_Desc = @_;
    my %Type_Init = initializeByInterface(%Init_Desc);
    if($Type_Init{"IsCorrect"})
    {
        return %Type_Init;
    }
    else
    {
        return assembleFuncPtr(%Init_Desc);
    }
}

sub get_OneStep_BaseTypeId($$)
{
    my ($TypeDId, $TypeId) = @_;
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return $Type{"Tid"} if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    return $Type{"BaseType"}{"Tid"};
}

sub get_OneStep_BaseType($$)
{
    my ($TypeDId, $TypeId) = @_;
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    return get_Type($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"});
}

sub initializeArray(@)
{
    my %Init_Desc = @_;
    if($Init_Desc{"TypeType_Changed"})
    {
        my %Type_Init = assembleArray(%Init_Desc);
        if($Type_Init{"IsCorrect"})
        {
            return %Type_Init;
        }
        else
        {
            $Init_Desc{"FoundationType_Type"} = get_TypeType(get_FoundationTypeId($Init_Desc{"TypeId"}));
            return selectInitializingWay(%Init_Desc);
        }
    }
    else
    {
        $Init_Desc{"StrongTypeCompliance"} = 1;
        my %Type_Init = initializeByInterface(%Init_Desc);
        if($Type_Init{"IsCorrect"})
        {
            return %Type_Init;
        }
        else
        {
            %Type_Init = initializeByInterface_OutParam(%Init_Desc);
            if($Type_Init{"IsCorrect"})
            {
                return %Type_Init;
            }
            else
            {
                $Init_Desc{"StrongTypeCompliance"} = 0;
                return assembleArray(%Init_Desc);
            }
        }
    }
}

sub get_PureType($$)
{
    my ($TypeDId, $TypeId) = @_;
    return () if(not $TypeId);
    if(defined $Cache{"get_PureType"}{$TypeDId}{$TypeId} and not defined $AuxType{$TypeId})
    {
        return %{$Cache{"get_PureType"}{$TypeDId}{$TypeId}};
    }
    return () if(not $TypeDescr{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    if($Type{"Type"}=~/\A(Ref|Const|Volatile|Restrict|Typedef)\Z/)
    {
        %Type = get_PureType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"});
    }
    $Cache{"get_PureType"}{$TypeDId}{$TypeId} = \%Type;
    return %Type;
}

sub get_PureTypeId($$)
{
    my ($TypeDId, $TypeId) = @_;
    my %Type = get_PureType($TypeDId, $TypeId);
    return $Type{"Tid"};
}

sub delete_quals($$)
{
    my ($TypeDId, $TypeId) = @_;
    return () if(not $TypeId);
    if(defined $Cache{"delete_quals"}{$TypeDId}{$TypeId} and not defined $AuxType{$TypeId})
    {
        return %{$Cache{"delete_quals"}{$TypeDId}{$TypeId}};
    }
    return () if(not $TypeDescr{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    if($Type{"Type"}=~/\A(Ref|Const|Volatile|Restrict)\Z/)
    {
        %Type = delete_quals($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"});
    }
    $Cache{"delete_quals"}{$TypeDId}{$TypeId} = \%Type;
    return %Type;
}

sub goToFirst($$$)
{
    my ($TypeDId, $TypeId, $Type_Type) = @_;
    if(defined $Cache{"goToFirst"}{$TypeDId}{$TypeId}{$Type_Type} and not defined $AuxType{$TypeId})
    {
        return %{$Cache{"goToFirst"}{$TypeDId}{$TypeId}{$Type_Type}};
    }
    return () if(not $TypeDescr{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$TypeDId}{$TypeId}};
    return () if(not $Type{"Type"});
    if($Type{"Type"} ne $Type_Type)
    {
        return () if(not $Type{"BaseType"}{"Tid"});
        %Type = goToFirst($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $Type_Type);
    }
    $Cache{"goToFirst"}{$TypeDId}{$TypeId}{$Type_Type} = \%Type;
    return %Type;
}

sub detectArrayTypeId($)
{
    my $TypeId = $_[0];
    my $ArrayType_Id = get_FoundationTypeId($TypeId);
    my $PointerLevel = get_PointerLevel($Tid_TDid{$TypeId}, $TypeId);
    if(get_TypeType($ArrayType_Id) eq "Array")# and $PointerLevel==0
    {
        return $ArrayType_Id;
    }
    else
    {#this branch for types like arrays (char* like char[])
        return get_PureTypeId($Tid_TDid{$TypeId}, $TypeId);
    }
}

sub assembleArray(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    my $Global_State = save_state();
    my $PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my %Type = get_Type($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    #determine base array
    my $ArrayType_Id = detectArrayTypeId($Init_Desc{"TypeId"});
    my %ArrayType = get_Type($Tid_TDid{$ArrayType_Id}, $ArrayType_Id);
    my $AmountArray = ($ArrayType{"Type"} eq "Array")?$ArrayType{"Size"}:(($Init_Desc{"ArraySize"})?$Init_Desc{"ArraySize"}:$DEFAULT_ARRAY_AMOUNT);
    if($AmountArray>1024)
    {# such too long arrays should be initialized by other methods
        restore_state($Global_State);
        return ();
    }
    #array base type attributes
    my $ArrayElemType_Id = get_OneStep_BaseTypeId($Tid_TDid{$ArrayType_Id}, $ArrayType_Id);
    my $ArrayElemType_PLevel = get_PointerLevel($Tid_TDid{$ArrayElemType_Id}, $ArrayElemType_Id);
    my $ArrayElemFType_Id = get_FoundationTypeId($ArrayElemType_Id);
    my $IsInlineDef = (($ArrayType{"Type"} eq "Array") and $PointerLevel==0 and ($Type{"Type"} ne "Ref") and $Init_Desc{"InLine"} or $Init_Desc{"InLineArray"});
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    if(not $IsInlineDef)
    {
        $Block_Variable{$CurrentBlock}{$Var} = 1;
    }
    if(get_TypeName($ArrayElemFType_Id)!~/\A(char|unsigned char|wchar_t)\Z/ and not $IsInlineDef)
    {
        my ($ExtTypeId, $Declarations, $Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $ArrayElemType_Id);
        $ArrayElemType_Id = $ExtTypeId;
        $Type_Init{"Code"} .= $Declarations;
        $Type_Init{"Headers"} = addHeaders($Headers, $Type_Init{"Headers"});
    }
    my @ElemStr = ();
    foreach my $Elem_Pos (1 .. $AmountArray)
    {#initialize array members
        my $ElemName = "";
        if(get_TypeName($ArrayElemFType_Id)=~/\A(char|unsigned char|wchar_t)\Z/
        and $ArrayElemType_PLevel==1)
        {
            $ElemName = $Init_Desc{"ParamName"}."_".$Elem_Pos;
        }
        else
        {
            $ElemName = $Init_Desc{"ParamName"}.((not defined $DisableReuse)?"_elem":"");
            $ElemName=~s/es_elem\Z/e/g;
        }
        my %Elem_Init = initializeParameter((
        "TypeId" => $ArrayElemType_Id,
        "Key" => $Init_Desc{"Key"}."_".$Elem_Pos,
        "InLine" => 1,
        "Value" => "no value",
        "ValueTypeId" => 0,
        "TargetTypeId" => 0,
        "CreateChild" => 0,
        "Usage" => "Common",
        "ParamName" => $ElemName,
        "OuterType_Type" => "Array",
        "Index" => $Elem_Pos-1,
        "InLineArray" => ($ArrayElemType_PLevel==1 and get_TypeName($ArrayElemFType_Id)=~/\A(char|unsigned char|wchar_t)\Z/ and $Init_Desc{"ParamName"}=~/text|txt|doc/i)?1:0,
        "IsString" => ($ArrayElemType_PLevel==1 and get_TypeName($ArrayElemFType_Id)=~/\A(char|unsigned char|wchar_t)\Z/ and $Init_Desc{"ParamName"}=~/prefixes/i)?1:0 ));
        if(not $Elem_Init{"IsCorrect"} or $Elem_Init{"ByNull"})
        {
            restore_state($Global_State);
            return ();
        }
        if($Elem_Pos eq 1)
        {
            $Type_Init{"Headers"} = addHeaders($Elem_Init{"Headers"}, $Type_Init{"Headers"});
        }
        @ElemStr = (@ElemStr, $Elem_Init{"Call"});
        $Type_Init{"Init"} .= $Elem_Init{"Init"};
        $Type_Init{"Destructors"} .= $Elem_Init{"Destructors"};
        $Type_Init{"Code"} .= $Elem_Init{"Code"};
    }
    if(($ArrayType{"Type"} ne "Array") and not isNumericType(get_TypeName($ArrayElemType_Id)))
    {# the last array element
        if($ArrayElemType_PLevel==0 and get_TypeName($ArrayElemFType_Id)=~/\A(char|unsigned char)\Z/)
        {
            @ElemStr = (@ElemStr, "\'\\0\'");
        }
        elsif($ArrayElemType_PLevel==0 and is_equal_types(get_TypeName($ArrayElemType_Id), "wchar_t"))
        {
            @ElemStr = (@ElemStr, "L\'\\0\'");
        }
        elsif($ArrayElemType_PLevel>=1)
        {
            @ElemStr = (@ElemStr, get_null());
        }
        elsif($ArrayElemType_PLevel==0
        and get_TypeType($ArrayElemFType_Id)=~/\A(Struct|Union)\Z/)
        {
            @ElemStr = (@ElemStr, "(".get_TypeName($ArrayElemType_Id).") "."{0}");
        }
    }
    #initialization
    if($IsInlineDef)
    {
        $Type_Init{"Call"} = "{".create_matrix(\@ElemStr, "    ")."}";
    }
    else
    {
        if(not defined $DisableReuse)
        {
            $ValueCollection{$CurrentBlock}{$Var} = $ArrayType_Id;
        }
        #$Type_Init{"Init"} .= "//parameter initialization\n";
        $Type_Init{"Init"} .= get_TypeName($ArrayElemType_Id)." $Var [".(($ArrayType{"Type"} eq "Array")?$AmountArray:"")."] = {".create_matrix(\@ElemStr, "    ")."};\n";
        #create call
        my ($Call, $TmpPreamble) =
        convert_familiar_types((
            "InputTypeName"=>correctName(get_TypeName($ArrayElemType_Id)."*"),
            "InputPointerLevel"=>get_PointerLevel($Tid_TDid{$ArrayType_Id}, $ArrayType_Id),
            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Param",
            "MustConvert"=>0));
        $Type_Init{"Init"} .= $TmpPreamble;
        $Type_Init{"Call"} = $Call;
        #create type
        
        #create call to constraint
        if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
        {
            $Type_Init{"TargetCall"} = $Type_Init{"Call"};
        }
        else
        {
            my ($TargetCall, $Target_TmpPreamble) =
            convert_familiar_types((
                "InputTypeName"=>correctName(get_TypeName($ArrayElemType_Id)."*"),
                "InputPointerLevel"=>get_PointerLevel($Tid_TDid{$ArrayType_Id}, $ArrayType_Id),
                "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Target",
                "MustConvert"=>0));
            $Type_Init{"TargetCall"} = $TargetCall;
            $Type_Init{"Init"} .= $Target_TmpPreamble;
        }
        #ref handler
        if($Type{"Type"} eq "Ref")
        {
            my $BaseRefId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
            if($ArrayType{"Type"} eq "Pointer" or (get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId) > 0))
            {
                my $BaseRefName = get_TypeName($BaseRefId);
                $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
                $Type_Init{"Call"} = $Var."_ref";
                $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
                }
            }
        }
    }
    $Type_Init{"TypeName"} = get_TypeName($ArrayElemType_Id)." [".(($ArrayType{"Type"} eq "Array")?$AmountArray:"")."]";
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub get_null()
{
    if(getIntLang($TestedInterface) eq "C++")
    {
        return "NULL";
    }
    else
    {
        return "0";
    }
}

sub create_list($$)
{
    my ($Array, $Spaces) = @_;
    my @Elems = @{$Array};
    my ($MaxLength, $SumLength);
    foreach my $Elem (@Elems)
    {
        $SumLength += length($Elem);
        if(not defined $MaxLength or $MaxLength<length($Elem))
        {
            $MaxLength = length($Elem);
        }
    }
    if(($#Elems+1>$MAX_PARAMS_INLINE)
    or ($SumLength>$MAX_PARAMS_LENGTH_INLINE and $#Elems>0)
    or join("", @Elems)=~/\n/)
    {
        return "\n$Spaces".join(",\n$Spaces", @Elems);
    }
    else
    {
        return join(", ", @Elems);
    }
}

sub create_matrix($$)
{
    my ($Array, $Spaces) = @_;
    my @Elems = @{$Array};
    my $MaxLength;
    foreach my $Elem (@Elems)
    {
        if(length($Elem) > $MATRIX_MAX_ELEM_LENGTH)
        {
            return create_list($Array, $Spaces);
        }
        if(not defined $MaxLength or $MaxLength<length($Elem))
        {
            $MaxLength = length($Elem);
        }
    }
    if($#Elems+1 >= $MIN_PARAMS_MATRIX)
    {
        my (@Rows, @Row) = ();
        foreach my $Num (0 .. $#Elems)
        {
            my $Elem = $Elems[$Num];
            if($Num%$MATRIX_WIDTH==0 and $Num!=0)
            {
                push(@Rows, join(", ", @Row));
                @Row = ();
            }
            push(@Row, aligh_str($Elem, $MaxLength));
        }
        push(@Rows, join(", ", @Row)) if($#Row>=0);
        return "\n$Spaces".join(",\n$Spaces", @Rows);
    }
    else
    {
        return create_list($Array, $Spaces);
    }
}

sub aligh_str($$)
{
    my ($Str, $Length) = @_;
    if(length($Str)<$Length)
    {
        foreach (1 .. $Length - length($Str))
        {
            $Str = " ".$Str;
        }
    }
    return $Str;
}

sub findFuncPtr_RealFunc($$)
{
    my ($FuncTypeId, $ParamName) = @_;
    my @AvailableRealFuncs = ();
    foreach my $Interface (sort {length($a)<=>length($b)} sort {$a cmp $b} keys(%{$Func_TypeId{$FuncTypeId}}))
    {
        next if(isCyclical(\@RecurInterface, $Interface));
        if($Interface_Library{$Interface}
        or $NeededInterface_Library{$Interface})
        {
            push(@AvailableRealFuncs, $Interface);
        }
    }
    sort_byCriteria(\@AvailableRealFuncs, "Internal");
    @AvailableRealFuncs = sort {($b=~/\Q$ParamName\E/i)<=>($a=~/\Q$ParamName\E/i)} @AvailableRealFuncs if($ParamName!~/\Ap\d+\Z/);
    if($#AvailableRealFuncs>=0)
    {
        return $AvailableRealFuncs[0];
    }
    else
    {
        return "";
    }
}

sub get_base_typedef($)
{
    my $TypeId = $_[0];
    my %TypeDef = goToFirst($Tid_TDid{$TypeId}, $TypeId, "Typedef");
    return 0 if(not $TypeDef{"Type"});
    if(get_PointerLevel($TypeDef{"TDid"}, $TypeDef{"Tid"})==0)
    {
        return $TypeDef{"Tid"};
    }
    my $BaseTypeId = get_OneStep_BaseTypeId($TypeDef{"TDid"}, $TypeDef{"Tid"});
    return get_base_typedef($BaseTypeId);
}

sub assembleFuncPtr(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    my $Global_State = save_state();
    my %Type = get_Type($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $FuncPtr_TypeId = get_FoundationTypeId($Init_Desc{"TypeId"});
    my %FuncPtrType = get_Type($Tid_TDid{$FuncPtr_TypeId}, $FuncPtr_TypeId);
    my ($TypeName, $AuxFuncName) = ($FuncPtrType{"Name"}, "");
    if(get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"})>0)
    {
        if(my $Typedef_Id = get_base_typedef($Init_Desc{"TypeId"}))
        {
            $TypeName = get_TypeName($Typedef_Id);
        }
        elsif(my $Typedef_Id = get_type_typedef($FuncPtr_TypeId))
        {
            $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Typedef_Id), $Type_Init{"Headers"});
            $TypeName = get_TypeName($Typedef_Id);
        }
        else
        {
            $Type_Init{"Code"} .= declare_funcptr_typedef($Init_Desc{"Key"}, $FuncPtr_TypeId);
            $TypeName = get_TypeName($FuncPtr_TypeId);
        }
    }
    if($FuncPtrType{"Name"} eq "void*(*)(size_t)")
    {
        $Type_Init{"Headers"} = addHeaders(["stdlib.h"], $Type_Init{"Headers"});
        $AuxHeaders{"stdlib.h"} = 1;
        $AuxFuncName = "malloc";
    }
    elsif(my $Interface_FuncPtr = findFuncPtr_RealFunc($FuncPtrType{"FuncTypeId"}, $Init_Desc{"ParamName"}))
    {
        $UsedInterfaces{$Interface_FuncPtr} = 1;
        $Type_Init{"Headers"} = addHeaders([$CompleteSignature{$Interface_FuncPtr}{"Header"}], $Type_Init{"Headers"});
        $AuxFuncName = $CompleteSignature{$Interface_FuncPtr}{"ShortName"};
        if($CompleteSignature{$Interface_FuncPtr}{"NameSpace"})
        {
            $AuxFuncName = $CompleteSignature{$Interface_FuncPtr}{"NameSpace"}."::".$AuxFuncName;
        }
    }
    else
    {
        if($AuxFunc{$FuncPtr_TypeId})
        {
            $AuxFuncName = $AuxFunc{$FuncPtr_TypeId};
        }
        else
        {
            my @FuncParams = ();
            $AuxFuncName = select_func_name($LongVarNames?$Init_Desc{"Key"}:(($Init_Desc{"ParamName"}=~/\Ap\d+\Z/)?"aux_func":$Init_Desc{"ParamName"}));
            #global
            $AuxFunc{$FuncPtr_TypeId} = $AuxFuncName;
            my $PreviousBlock = $CurrentBlock;
            $CurrentBlock = $AuxFuncName;
            #function declaration
            my $FuncReturnType_Id = $FuncPtrType{"Return"};
            my $FuncReturnFType_Id = get_FoundationTypeId($FuncReturnType_Id);
            my $FuncReturnFType_Type = get_TypeType($FuncReturnFType_Id);
            foreach my $ParamPos (sort {int($a)<=>int($b)} keys(%{$FuncPtrType{"Memb"}}))
            {
                my $ParamTypeId = $FuncPtrType{"Memb"}{$ParamPos}{"type"};
                $Type_Init{"Headers"} = addHeaders(getTypeHeaders($ParamTypeId), $Type_Init{"Headers"});
                my $ParamName = $FuncPtrType{"Memb"}{$ParamPos}{"name"};
                $ParamName = "p".($ParamPos+1) if(not $ParamName);
                #my ($ParamEType_Id, $Param_Declarations, $Param_Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $ParamTypeId);
                my $ParamTypeName = get_TypeName($ParamTypeId);#get_TypeName($ParamEType_Id);
                #$Type_Init{"Header"} = addHeaders($Param_Headers, $Type_Init{"Header"});
                #$Type_Init{"Code"} .= $Param_Declarations;
                if($ParamTypeName and ($ParamTypeName ne "..."))
                {
                    my $Field = create_member_decl($ParamTypeName, $ParamName);
                    @FuncParams = (@FuncParams, $Field);
                }
                $ValueCollection{$AuxFuncName}{$ParamName} = $ParamTypeId;
                $Block_Param{$AuxFuncName}{$ParamName} = $ParamTypeId;
                $Block_Variable{$CurrentBlock}{$ParamName} = 1;
            }
            #definition of function
            if(get_TypeName($FuncReturnType_Id) eq "void")
            {
                my $FuncDef = "//auxiliary function\n";
                $FuncDef .= "void\n".$AuxFuncName."(".create_list(\@FuncParams, "    ").")";
                if($AuxFuncName=~/free/i)
                {
                    my $PtrParam = "";
                    foreach my $ParamPos (sort {int($a)<=>int($b)} keys(%{$FuncPtrType{"Memb"}}))
                    {
                        my $ParamTypeId = $FuncPtrType{"Memb"}{$ParamPos}{"type"};
                        my $ParamName = $FuncPtrType{"Memb"}{$ParamPos}{"name"};
                        $ParamName = "p".($ParamPos+1) if(not $ParamName);
                        my $ParamFTypeId = get_FoundationTypeId($ParamTypeId);
                        if(get_PointerLevel($Tid_TDid{$ParamTypeId}, $ParamTypeId)==1
                        and get_TypeType($ParamFTypeId) eq "Intrinsic")
                        {
                            $PtrParam = $ParamName;
                            last;
                        }
                    }
                    if($PtrParam)
                    {
                        $FuncDef .= "{\n";
                        $FuncDef .= "    free($PtrParam);\n";
                        $FuncDef .= "}\n\n";
                    }
                    else
                    {
                        $FuncDef .= "{}\n\n";
                    }
                }
                else
                {
                    $FuncDef .= "{}\n\n";
                }
                $Type_Init{"Code"} .= "\n".$FuncDef;
            }
            else
            {
                my %ReturnType_Init = initializeParameter((
                "TypeId" => $FuncReturnType_Id,
                "Key" => "retval",
                "InLine" => 1,
                "Value" => "no value",
                "ValueTypeId" => 0,
                "TargetTypeId" => 0,
                "CreateChild" => 0,
                "Usage" => "Common",
                "RetVal" => 1,
                "ParamName" => "retval",
                "FuncPtrTypeId" => $FuncPtr_TypeId),
                "FuncPtrName" => $AuxFuncName);
                if(not $ReturnType_Init{"IsCorrect"})
                {
                    restore_state($Global_State);
                    $CurrentBlock = $PreviousBlock;
                    return ();
                }
                $ReturnType_Init{"Init"} = alignCode($ReturnType_Init{"Init"}, "    ", 0);
                $ReturnType_Init{"Call"} = alignCode($ReturnType_Init{"Call"}, "    ", 1);
                $Type_Init{"Code"} .= $ReturnType_Init{"Code"};
                $Type_Init{"Headers"} = addHeaders($ReturnType_Init{"Headers"}, $Type_Init{"Headers"});
                my ($FuncReturnEType_Id, $Declarations, $Headers) = get_ExtTypeId($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $FuncReturnType_Id);
                my $FuncReturnType_Name = get_TypeName($FuncReturnEType_Id);
                $Type_Init{"Code"} .= $Declarations;
                $Type_Init{"Headers"} = addHeaders($Headers, $Type_Init{"Headers"});
                my $FuncDef = "//auxiliary function\n";
                $FuncDef .= $FuncReturnType_Name."\n".$AuxFuncName."(".create_list(\@FuncParams, "    ").")";
                $FuncDef .= "{\n";
                $FuncDef .= $ReturnType_Init{"Init"};
                $FuncDef .= "    return ".$ReturnType_Init{"Call"}.";\n}\n\n";
                $Type_Init{"Code"} .= "\n".$FuncDef;
            }
            $CurrentBlock = $PreviousBlock;
        }
    }
    
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    
    #create call
    my ($Call, $TmpPreamble) =
    convert_familiar_types((
        "InputTypeName"=>$TypeName,
        "InputPointerLevel"=>0,
        "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
        "Value"=>"&".$AuxFuncName,
        "Key"=>$LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"},
        "Destination"=>"Param",
        "MustConvert"=>0));
    $Type_Init{"Init"} .= $TmpPreamble;
    $Type_Init{"Call"} = $Call;
    #create type
    $Type_Init{"TypeName"} = get_TypeName($Init_Desc{"TypeId"});
    #create call to constraint
    if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
    {
        $Type_Init{"TargetCall"} = $Type_Init{"Call"};
    }
    else
    {
        my ($TargetCall, $Target_TmpPreamble) =
        convert_familiar_types((
            "InputTypeName"=>$TypeName,
            "InputPointerLevel"=>0,
            "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
            "Value"=>"&".$AuxFuncName,
            "Key"=>$LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"},
            "Destination"=>"Target",
            "MustConvert"=>0));
        $Type_Init{"TargetCall"} = $TargetCall;
        $Type_Init{"Init"} .= $Target_TmpPreamble;
    }
    
    #ref handler
    if($Type{"Type"} eq "Ref")
    {
        my $BaseRefId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
        if(get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId) > 0)
        {
            my $BaseRefName = get_TypeName($BaseRefId);
            $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
            $Type_Init{"Call"} = $Var."_ref";
            $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
        }
    }
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub declare_anon_union($$)
{
    my ($Key, $UnionId) = @_;
    return "" if($AuxType{$UnionId} or not $UnionId or not $Key);
    my %Union = get_Type($Tid_TDid{$UnionId}, $UnionId);
    my @MembStr = ();
    my ($Headers, $Declarations) = ([], "");
    foreach my $Member_Pos (sort {int($a)<=>int($b)} keys(%{$Union{"Memb"}}))
    {#create member types string
        my $Member_Name = $Union{"Memb"}{$Member_Pos}{"name"};
        my $MemberType_Id = $Union{"Memb"}{$Member_Pos}{"type"};
        my $MemberFType_Id = get_FoundationTypeId($MemberType_Id);
        my $MemberType_Name = "";
        if(isAnon(get_TypeName($MemberFType_Id)))
        {
            my ($FieldEType_Id, $Field_Declarations, $Field_Headers) = get_ExtTypeId($Key, $MemberType_Id);
            $Headers = addHeaders($Field_Headers, $Headers);
            $Declarations .= $Field_Declarations;
            $MemberType_Name = get_TypeName($FieldEType_Id);
        }
        else
        {
            $MemberType_Name = get_TypeName($MemberFType_Id);
        }
        my $MembDecl = create_member_decl($MemberType_Name, $Member_Name);
        @MembStr = (@MembStr, $MembDecl);
    }
    my $Type_Name = select_type_name("union_type_".$Key);
    $Declarations .= "//auxilliary union type\nunion ".$Type_Name;
    $Declarations .= "{\n    ".join(";\n    ", @MembStr).";};\n\n";
    $AuxType{$UnionId} = "union ".$Type_Name;
    $TName_Tid{$AuxType{$UnionId}} = $UnionId;
    $TName_Tid{$Type_Name} = $UnionId;
    $TypeDescr{$Tid_TDid{$UnionId}}{$UnionId}{"Name_Old"} = $Union{"Name"};
    $TypeDescr{$Tid_TDid{$UnionId}}{$UnionId}{"Name"} = $AuxType{$UnionId};
    return ($Declarations, $Headers);
}

sub declare_anon_struct($$)
{
    my ($Key, $StructId) = @_;
    return () if($AuxType{$StructId} or not $StructId or not $Key);
    my %Struct = get_Type($Tid_TDid{$StructId}, $StructId);
    my @MembStr = ();
    my ($Headers, $Declarations) = ([], "");
    foreach my $Member_Pos (sort {int($a)<=>int($b)} keys(%{$Struct{"Memb"}}))
    {
        my $Member_Name = $Struct{"Memb"}{$Member_Pos}{"name"};
        my $MemberType_Id = $Struct{"Memb"}{$Member_Pos}{"type"};
        my $MemberFType_Id = get_FoundationTypeId($MemberType_Id);
        my $MemberType_Name = "";
        if(isAnon(get_TypeName($MemberFType_Id)))
        {
            my ($FieldEType_Id, $Field_Declarations, $Field_Headers) = get_ExtTypeId($Key, $MemberType_Id);
            $Headers = addHeaders($Field_Headers, $Headers);
            $Declarations .= $Field_Declarations;
            $MemberType_Name = get_TypeName($FieldEType_Id);
        }
        else
        {
            $MemberType_Name = get_TypeName($MemberFType_Id);
        }
        my $MembDecl = create_member_decl($MemberType_Name, $Member_Name);
        @MembStr = (@MembStr, $MembDecl);
    }
    my $Type_Name = select_type_name("struct_type_".$Key);
    $Declarations .= "//auxiliary struct type\nstruct ".$Type_Name;
    $Declarations .= "{\n    ".join(";\n    ", @MembStr).";};\n\n";
    $AuxType{$StructId} = "struct ".$Type_Name;
    $TName_Tid{$AuxType{$StructId}} = $StructId;
    $TName_Tid{$Type_Name} = $StructId;
    $TypeDescr{$Tid_TDid{$StructId}}{$StructId}{"Name_Old"} = $Struct{"Name"};
    $TypeDescr{$Tid_TDid{$StructId}}{$StructId}{"Name"} = $AuxType{$StructId};
    
    return ($Declarations, $Headers);
}

sub create_member_decl($$)
{
    my ($TName, $Member) = @_;
    
    if($TName=~/\([\*]+\)/)
    {
        $TName=~s/\(([\*]+)\)/\($1$Member\)/;
        return $TName;
    }
    else
    {
        my @ArraySizes = ();
        while($TName=~s/(\[[^\[\]]*\])\Z//)
        {
            push(@ArraySizes, $1);
        }
        return $TName." ".$Member.join("", @ArraySizes);
    }
}

sub assembleStruct(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    my %Type = get_Type($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $Type_PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $StructId = get_FoundationTypeId($Init_Desc{"TypeId"});
    my $StructName = get_TypeName($StructId);
    return () if($OpaqueTypes{$StructName});
    my %Struct = get_Type($Tid_TDid{$StructId}, $StructId);
    return () if(not keys(%{$Struct{"Memb"}}));
    my $Global_State = save_state();
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    if($Type_PointerLevel>0 or ($Type{"Type"} eq "Ref") or not $Init_Desc{"InLine"})
    {
        $Block_Variable{$CurrentBlock}{$Var} = 1;
    }
    $Type_Init{"Headers"} = addHeaders([$Struct{"Header"}], $Type_Init{"Headers"});
    my @ParamStr = ();
    foreach my $Member_Pos (sort {int($a)<=>int($b)} keys(%{$Struct{"Memb"}}))
    {#initialize members
        my $Member_Name = $Struct{"Memb"}{$Member_Pos}{"name"};
        if(getIntLang($TestedInterface) eq "C")
        {
            if($Member_Name eq "c_class"
            and $StructName=~/\A(struct |)(XWindowAttributes|Visual|XVisualInfo)\Z/)
            {#for X11
                $Member_Name = "class";
            }
            elsif($Member_Name eq "c_explicit"
            and $StructName=~/\A(struct |)(_XkbServerMapRec)\Z/)
            {#for X11
                $Member_Name = "explicit";
            }
            elsif($Member_Name eq "fds_bits"
            and $StructName=~/\A(fd_set)\Z/
            and (not defined $Constants{"__USE_XOPEN"} or not $Constants{"__USE_XOPEN"}{"Value"}))
            {#for libc
                $Member_Name = "__fds_bits";
            }
            elsif($Member_Name eq "__fds_bits"
            and $StructName=~/\A(fd_set)\Z/
            and defined $Constants{"__USE_XOPEN"} and $Constants{"__USE_XOPEN"}{"Value"})
            {#for libc
                $Member_Name = "fds_bits";
            }
        }
        my $MemberType_Id = $Struct{"Memb"}{$Member_Pos}{"type"};
        my $MemberFType_Id = get_FoundationTypeId($MemberType_Id);
        if(get_TypeType($MemberFType_Id) eq "Array")
        {
            my $ArrayElemType_Id = get_FoundationTypeId(get_OneStep_BaseTypeId($Tid_TDid{$MemberFType_Id}, $MemberFType_Id));
            if(get_TypeType($ArrayElemType_Id)=~/\A(Intrinsic|Enum)\Z/)
            {
                if(get_TypeSize($MemberFType_Id)>1024)
                {
                    next;
                }
            }
            else
            {
                if(get_TypeSize($MemberFType_Id)>256)
                {
                    next;
                }
            }
        }
        my $Member_Access = $Struct{"Memb"}{$Member_Pos}{"access"};
        #return () if($Member_Access eq "private" or $Member_Access eq "protected");
        my $Memb_Key = "";
        if($Member_Name)
        {
            $Memb_Key = ($Init_Desc{"Key"})?$Init_Desc{"Key"}."_".$Member_Name:$Member_Name;
        }
        else
        {
            $Memb_Key = ($Init_Desc{"Key"})?$Init_Desc{"Key"}."_".($Member_Pos+1):"m".($Member_Pos+1);
        }
        my %Memb_Init = initializeParameter((
        "TypeId" => $MemberType_Id,
        "Key" => $Memb_Key,
        "InLine" => 1,
        "Value" => "no value",
        "ValueTypeId" => 0,
        "TargetTypeId" => 0,
        "CreateChild" => 0,
        "Usage" => "Common",
        "ParamName" => $Member_Name,
        "OuterType_Type" => "Struct",
        "OuterType_Id" => $StructId));
        if(not $Memb_Init{"IsCorrect"})
        {
            restore_state($Global_State);
            return ();
        }
        $Type_Init{"Code"} .= $Memb_Init{"Code"};
        $Type_Init{"Headers"} = addHeaders($Memb_Init{"Headers"}, $Type_Init{"Headers"});
        $Memb_Init{"Call"} = alignCode($Memb_Init{"Call"}, get_paragraph($Memb_Init{"Call"}, 1)."    ", 1);
        if(getIntLang($TestedInterface) eq "C++")
        {
            @ParamStr = (@ParamStr, $Memb_Init{"Call"});
        }
        else
        {
            @ParamStr = (@ParamStr, "\.$Member_Name = ".$Memb_Init{"Call"});
        }
        $Type_Init{"Init"} .= $Memb_Init{"Init"};
        $Type_Init{"Destructors"} .= $Memb_Init{"Destructors"};
    }
    if(my $Typedef_Id = get_type_typedef($StructId))
    {
        $StructName = get_TypeName($Typedef_Id);
    }
    #initialization
    if($Type_PointerLevel==0 and ($Type{"Type"} ne "Ref") and $Init_Desc{"InLine"})
    {
        my $Conversion = (isNotAnon($StructName) and isNotAnon($Struct{"Name_Old"}))?"(".$Type{"Name"}.") ":"";
        $Type_Init{"Call"} = $Conversion."{".create_list(\@ParamStr, "    ")."}";
        $Type_Init{"TypeName"} = $Type{"Name"};
    }
    else
    {
        if(not defined $DisableReuse)
        {
            $ValueCollection{$CurrentBlock}{$Var} = $StructId;
        }
        if(isAnon($StructName))
        {
            my ($AnonStruct_Declarations, $AnonStruct_Headers) = declare_anon_struct($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $StructId);
            $Type_Init{"Code"} .= $AnonStruct_Declarations;
            $Type_Init{"Headers"} = addHeaders($AnonStruct_Headers, $Type_Init{"Headers"});
            $Type_Init{"Init"} .= get_TypeName($StructId)." $Var = {".create_list(\@ParamStr, "    ")."};\n";
            $Type_Init{"TypeName"} = get_TypeName($StructId);
            foreach (1 .. $Type_PointerLevel)
            {
                $Type_Init{"TypeName"} .= "*";
            }
        }
        else
        {
            $Type_Init{"Init"} .= $StructName." $Var = {".create_list(\@ParamStr, "    ")."};\n";
            $Type_Init{"TypeName"} = $Type{"Name"};
        }
        #create call
        my ($Call, $TmpPreamble) =
        convert_familiar_types((
            "InputTypeName"=>get_TypeName($StructId),
            "InputPointerLevel"=>0,
            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Param",
            "MustConvert"=>0));
        $Type_Init{"Init"} .= $TmpPreamble;
        $Type_Init{"Call"} = $Call;
        #create call for constraint
        if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
        {
            $Type_Init{"TargetCall"} = $Type_Init{"Call"};
        }
        else
        {
            my ($TargetCall, $Target_TmpPreamble) =
            convert_familiar_types((
                "InputTypeName"=>get_TypeName($StructId),
                "InputPointerLevel"=>0,
                "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Target",
                "MustConvert"=>0));
            $Type_Init{"TargetCall"} = $TargetCall;
            $Type_Init{"Init"} .= $Target_TmpPreamble;
        }
        #ref handler
        if($Type{"Type"} eq "Ref")
        {
            my $BaseRefId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
            if(get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId) > 0)
            {
                my $BaseRefName = get_TypeName($BaseRefId);
                $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
                $Type_Init{"Call"} = $Var."_ref";
                $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
                }
            }
        }
    }
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub getSomeEnumMember($)
{
    my $EnumId = $_[0];
    my %Enum = get_Type($Tid_TDid{$EnumId}, $EnumId);
    return "" if(not keys(%{$Enum{"Memb"}}));
    my @Members = ();
    foreach my $MembPos (sort{int($a)<=>int($b)} keys(%{$Enum{"Memb"}}))
    {
        push(@Members, $Enum{"Memb"}{$MembPos}{"name"});
    }
    if($RandomCode)
    {
        @Members = mix_array(@Members);
    }
    my @ValidMembers = ();
    foreach my $Member (@Members)
    {
        if(is_valid_constant($Member))
        {
            push(@ValidMembers, $Member);
        }
    }
    my $MemberName = $Members[0];
    if($#ValidMembers>=0)
    {
        $MemberName = $ValidMembers[0];
    }
    if($Enum{"NameSpace"} and $MemberName and getIntLang($TestedInterface) eq "C++")
    {
        $MemberName = $Enum{"NameSpace"}."::".$MemberName;
    }
    return $MemberName;
}

sub getEnumMembers($)
{
    my $EnumId = $_[0];
    my %Enum = get_Type($Tid_TDid{$EnumId}, $EnumId);
    return () if(not keys(%{$Enum{"Memb"}}));
    my @Members = ();
    foreach my $MembPos (sort{int($a)<=>int($b)} keys(%{$Enum{"Memb"}}))
    {
        push(@Members, $Enum{"Memb"}{$MembPos}{"name"});
    }
    return \@Members;
}

sub add_NullSpecType(@)
{
    my %Init_Desc = @_;
    my %NewInit_Desc = %Init_Desc;
    my $PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $TypeName = get_TypeName($Init_Desc{"TypeId"});
    if(($TypeName=~/\&/) or (not $Init_Desc{"InLine"}))
    {
        $NewInit_Desc{"InLine"} = 0;
    }
    else
    {
        $NewInit_Desc{"InLine"} = 1;
    }
    if($PointerLevel>=1)
    {
        if($Init_Desc{"OuterType_Type"}!~/\A(Struct|Union|Array)\Z/
        and (isOutParam_NoUsing($Init_Desc{"TypeId"}, $Init_Desc{"ParamName"}, $Init_Desc{"Interface"})
        or $Interface_OutParam{$Init_Desc{"Interface"}}{$Init_Desc{"ParamName"}}
        or $Interface_OutParam_NoUsing{$Init_Desc{"Interface"}}{$Init_Desc{"ParamName"}} or $PointerLevel>=2))
        {
            $NewInit_Desc{"InLine"} = 0;
            $NewInit_Desc{"ValueTypeId"} = reduce_pointer_level($Init_Desc{"TypeId"});
            if($PointerLevel>=2)
            {
                $NewInit_Desc{"Value"} = get_null();
            }
            else
            {
                $NewInit_Desc{"OnlyDecl"} = 1;
            }
        }
        else
        {
            $NewInit_Desc{"Value"} = get_null();
            $NewInit_Desc{"ValueTypeId"} = $Init_Desc{"TypeId"};
            $NewInit_Desc{"ByNull"}=1;
        }
    }
    else
    {
        $NewInit_Desc{"Value"} = "no value";
    }
    return %NewInit_Desc;
}

sub initializeIntrinsic(@)
{
    my %Init_Desc = @_;
    $Init_Desc{"StrongTypeCompliance"} = 1;
    my %Type_Init = initializeByInterface(%Init_Desc);
    if($Type_Init{"IsCorrect"})
    {
        return %Type_Init;
    }
    else
    {
        return initializeByInterface_OutParam(%Init_Desc);
    }
}

sub initializeRetVal(@)
{
    my %Init_Desc = @_;
    return () if(get_TypeName($Init_Desc{"TypeId"}) eq "void*");
    my %Type_Init = initializeByInterface(%Init_Desc);
    if($Type_Init{"IsCorrect"})
    {
        return %Type_Init;
    }
    else
    {
        return initializeByInterface_OutParam(%Init_Desc);
    }
}

sub initializeEnum(@)
{
    my %Init_Desc = @_;
    return initializeByInterface(%Init_Desc);
}

sub is_geometry_body($)
{
    my $TypeId = $_[0];
    return 0 if(not $TypeId);
    my $StructId = get_FoundationTypeId($TypeId);
    my %Struct = get_Type($Tid_TDid{$StructId}, $StructId);
    return 0 if($Struct{"Name"}!~/rectangle|line/i);
    return 0 if($Struct{"Type"} ne "Struct");
    foreach my $Member_Pos (sort {int($a)<=>int($b)} keys(%{$Struct{"Memb"}}))
    {
        if(get_TypeType(get_FoundationTypeId($Struct{"Memb"}{$Member_Pos}{"type"}))!~/\A(Intrinsic|Enum)\Z/)
        {
            return 0;
        }
    }
    return 1;
}

sub initializeUnion(@)
{
    my %Init_Desc = @_;
    $Init_Desc{"Strong"}=1;
    my %Type_Init = initializeByInterface_OutParam(%Init_Desc);
    if($Type_Init{"IsCorrect"})
    {
        return %Type_Init;
    }
    else
    {
        delete($Init_Desc{"Strong"});
        %Type_Init = initializeByInterface(%Init_Desc);
        if($Type_Init{"IsCorrect"})
        {
            return %Type_Init;
        }
        else
        {
            %Type_Init = assembleUnion(%Init_Desc);
            if($Type_Init{"IsCorrect"})
            {
                return %Type_Init;
            }
            else
            {
                return initializeByInterface_OutParam(%Init_Desc);
            }
        }
    }
}

sub initializeStruct(@)
{
    my %Init_Desc = @_;
    if(is_geometry_body($Init_Desc{"TypeId"}))
    {# GdkRectangle
        return assembleStruct(%Init_Desc);
    }
    $Init_Desc{"Strong"}=1;
    my %Type_Init = initializeByInterface_OutParam(%Init_Desc);
    if($Type_Init{"IsCorrect"})
    {
        return %Type_Init;
    }
    else
    {
        delete($Init_Desc{"Strong"});
        $Init_Desc{"OnlyReturn"}=1;
        %Type_Init = initializeByInterface(%Init_Desc);
        if($Type_Init{"IsCorrect"})
        {
            return %Type_Init;
        }
        else
        {
            return () if($Init_Desc{"OnlyByInterface"});
            delete($Init_Desc{"OnlyReturn"});
            %Type_Init = initializeByInterface_OutParam(%Init_Desc);
            if($Type_Init{"IsCorrect"})
            {
                return %Type_Init;
            }
            else
            {
                $Init_Desc{"OnlyData"}=1;
                %Type_Init = initializeByInterface(%Init_Desc);
                if($Type_Init{"IsCorrect"})
                {
                    return %Type_Init;
                }
                else
                {
                    delete($Init_Desc{"OnlyData"});
                    %Type_Init = initializeSubClass_Struct(%Init_Desc);
                    if($Type_Init{"IsCorrect"})
                    {
                        return %Type_Init;
                    }
                    else
                    {
                        if($Init_Desc{"DoNotAssembly"})
                        {
                            %Type_Init = initializeByAlienInterface(%Init_Desc);
                            if($Type_Init{"IsCorrect"})
                            {
                                return %Type_Init;
                            }
                            else
                            {
                                return initializeByField(%Init_Desc);
                            }
                        }
                        else
                        {
                            %Type_Init = initializeByAlienInterface(%Init_Desc);
                            if($Type_Init{"IsCorrect"})
                            {
                                return %Type_Init;
                            }
                            else
                            {
                                %Type_Init = assembleStruct(%Init_Desc);
                                if($Type_Init{"IsCorrect"})
                                {
                                    return %Type_Init;
                                }
                                else
                                {
                                    %Type_Init = assembleClass(%Init_Desc);
                                    if($Type_Init{"IsCorrect"})
                                    {
                                        return %Type_Init;
                                    }
                                    else
                                    {
                                        return initializeByField(%Init_Desc);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

sub initializeByAlienInterface(@)
{# GtkWidget*  gtk_plug_new (GdkNativeWindow socket_id)
 # return GtkPlug*
    my %Init_Desc = @_;
    if($Init_Desc{"ByInterface"} = find_alien_interface($Init_Desc{"TypeId"}))
    {
        my %Type_Init = initializeByInterface(%Init_Desc);
        if(not $Type_Init{"ByNull"})
        {
            return %Type_Init;
        }
    }
    return ();
}

sub find_alien_interface($)
{
    my $TypeId = $_[0];
    return "" if(not $TypeId);
    return "" if(get_PointerLevel($Tid_TDid{$TypeId}, $TypeId)!=1);
    my $StructId = get_FoundationTypeId($TypeId);
    return "" if(get_TypeType($StructId) ne "Struct");
    my $Desirable = get_TypeName($StructId);
    $Desirable=~s/\Astruct //g;
    $Desirable=~s/\A[_]+//g;
    while($Desirable=~s/([a-z]+)([A-Z][a-z]+)/$1_$2/g){};
    $Desirable = lc($Desirable);
    my @Cnadidates = ($Desirable."_new", $Desirable."_create");
    foreach my $Candiate (@Cnadidates)
    {
        if(defined $CompleteSignature{$Candiate}
        and $CompleteSignature{$Candiate}{"Header"}
        and get_PointerLevel($Tid_TDid{$CompleteSignature{$Candiate}{"Return"}}, $CompleteSignature{$Candiate}{"Return"})==1)  {
            return $Candiate;
        }
    }
    return "";
}

sub initializeByField(@)
{# FIXME: write body of this function
    my %Init_Desc = @_;
    my @Structs = keys(%{$Member_Struct{$Init_Desc{"TypeId"}}});
    return ();
}

sub initializeSubClass_Struct(@)
{
    my %Init_Desc = @_;
    $Init_Desc{"TypeId_Changed"} = $Init_Desc{"TypeId"} if(not $Init_Desc{"TypeId_Changed"});
    my $StructId = get_FoundationTypeId($Init_Desc{"TypeId"});
    my $StructName = get_TypeName($StructId);
    my $PLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    return () if(get_TypeType($StructId) ne "Struct" or $PLevel==0);
    foreach my $SubClassId (keys(%{$Struct_SubClasses{$StructId}}))
    {
        $Init_Desc{"TypeId"} = get_TypeId($SubClassId, $PLevel);
        next if(not $Init_Desc{"TypeId"});
        $Init_Desc{"DoNotAssembly"} = 1;
        my %Type_Init = initializeType(%Init_Desc);
        if($Type_Init{"IsCorrect"})
        {
            return %Type_Init;
        }
    }
    if(my $ParentId = get_TypeId($Struct_Parent{$StructId}, $PLevel))
    {
        $Init_Desc{"TypeId"} = $ParentId;
        $Init_Desc{"DoNotAssembly"} = 1;
        $Init_Desc{"OnlyByInterface"} = 1;
        $Init_Desc{"KeyWords"} = $StructName;
        $Init_Desc{"KeyWords"}=~s/\Astruct //;
        my %Type_Init = initializeType(%Init_Desc);
        if($Type_Init{"IsCorrect"} and (not $Type_Init{"Interface"} or get_word_coinsidence($Type_Init{"Interface"}, $Init_Desc{"KeyWords"})>0))
        {
            return %Type_Init;
        }
    }
}

sub get_TypeId($$)
{
    my ($BaseTypeId, $PLevel) = @_;
    return 0 if(not $BaseTypeId);
    if(my @DerivedTypes = sort {length($a)<=>length($b)} keys(%{$BaseType_PLevel_Type{$BaseTypeId}{$PLevel}}))
    {
        return $DerivedTypes[0];
    }
    elsif(my $NewTypeId = register_new_type($BaseTypeId, $PLevel))
    {
        return $NewTypeId;
    }
    else
    {
        return 0;
    }
}

sub assembleUnion(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    my %Type = get_Type($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $Type_PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    my $UnionId = get_FoundationTypeId($Init_Desc{"TypeId"});
    my %UnionType = get_Type($Tid_TDid{$UnionId}, $UnionId);
    my $UnionName = $UnionType{"Name"};
    return () if($OpaqueTypes{$UnionName});
    return () if(not keys(%{$UnionType{"Memb"}}));
    my $Global_State = save_state();
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    if($Type_PointerLevel>0 or ($Type{"Type"} eq "Ref") or not $Init_Desc{"InLine"})
    {
        $Block_Variable{$CurrentBlock}{$Var} = 1;
    }
    $Type_Init{"Headers"} = addHeaders([$UnionType{"Header"}], $Type_Init{"Headers"});
    my (%Memb_Init, $SelectedMember_Name) = ();
    foreach my $Member_Pos (sort {int($a)<=>int($b)} keys(%{$UnionType{"Memb"}}))
    {#initialize members
        my $Member_Name = $UnionType{"Memb"}{$Member_Pos}{"name"};
        my $MemberType_Id = $UnionType{"Memb"}{$Member_Pos}{"type"};
        my $Memb_Key = "";
        if($Member_Name)
        {
            $Memb_Key = ($Init_Desc{"Key"})?$Init_Desc{"Key"}."_".$Member_Name:$Member_Name;
        }
        else
        {
            $Memb_Key = ($Init_Desc{"Key"})?$Init_Desc{"Key"}."_".($Member_Pos+1):"m".($Member_Pos+1);
        }
        %Memb_Init = initializeParameter((
        "TypeId" => $MemberType_Id,
        "Key" => $Memb_Key,
        "InLine" => 1,
        "Value" => "no value",
        "ValueTypeId" => 0,
        "TargetTypeId" => 0,
        "CreateChild" => 0,
        "Usage" => "Common",
        "ParamName" => $Member_Name,
        "OuterType_Type" => "Union",
        "OuterType_Id" => $UnionId));
        next if(not $Memb_Init{"IsCorrect"});
        $SelectedMember_Name = $Member_Name;
        last;
    }
    if(not $Memb_Init{"IsCorrect"})
    {
        restore_state($Global_State);
        return ();
    }
    $Type_Init{"Code"} .= $Memb_Init{"Code"};
    $Type_Init{"Headers"} = addHeaders($Memb_Init{"Headers"}, $Type_Init{"Headers"});
    $Type_Init{"Init"} .= $Memb_Init{"Init"};
    $Type_Init{"Destructors"} .= $Memb_Init{"Destructors"};
    $Memb_Init{"Call"} = alignCode($Memb_Init{"Call"}, get_paragraph($Memb_Init{"Call"}, 1)."    ", 1);
    if(my $Typedef_Id = get_type_typedef($UnionId))
    {
        $UnionName = get_TypeName($Typedef_Id);
    }
    #initialization
    if($Type_PointerLevel==0 and ($Type{"Type"} ne "Ref") and $Init_Desc{"InLine"})
    {
        my $Conversion = (isNotAnon($UnionName) and isNotAnon($UnionType{"Name_Old"}))?"(".$Type{"Name"}.") ":"";
        if($TestedInterface=~/\A_Z/)
        {#C++
            $Type_Init{"Call"} = $Conversion."{".$Memb_Init{"Call"}."}";
        }
        else
        {
            $Type_Init{"Call"} = $Conversion."{\.$SelectedMember_Name = ".$Memb_Init{"Call"}."}";
        }
        $Type_Init{"TypeName"} = $Type{"Name"};
    }
    else
    {
        if(not defined $DisableReuse)
        {
            $ValueCollection{$CurrentBlock}{$Var} = $UnionId;
        }
        if(isAnon($UnionName))
        {
            my ($AnonUnion_Declarations, $AnonUnion_Headers) = declare_anon_union($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $UnionId);
            $Type_Init{"Code"} .= $AnonUnion_Declarations;
            $Type_Init{"Headers"} = addHeaders($AnonUnion_Headers, $Type_Init{"Headers"});
            if($TestedInterface=~/\A_Z/)
            {#C++
                $Type_Init{"Init"} .= get_TypeName($UnionId)." $Var = {".$Memb_Init{"Call"}."};\n";
            }
            else
            {
                $Type_Init{"Init"} .= get_TypeName($UnionId)." $Var = {\.$SelectedMember_Name = ".$Memb_Init{"Call"}."};\n";
            }
            $Type_Init{"TypeName"} = "union ".get_TypeName($UnionId);
            foreach (1 .. $Type_PointerLevel)
            {
                $Type_Init{"TypeName"} .= "*";
            }
        }
        else
        {
            if($TestedInterface=~/\A_Z/)
            {#C++
                $Type_Init{"Init"} .= $UnionName." $Var = {".$Memb_Init{"Call"}."};\n";
            }
            else
            {
                $Type_Init{"Init"} .= $UnionName." $Var = {\.$SelectedMember_Name = ".$Memb_Init{"Call"}."};\n";
            }
            $Type_Init{"TypeName"} = $Type{"Name"};
        }
        #create call
        my ($Call, $TmpPreamble) =
        convert_familiar_types((
            "InputTypeName"=>get_TypeName($UnionId),
            "InputPointerLevel"=>0,
            "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
            "Value"=>$Var,
            "Key"=>$Var,
            "Destination"=>"Param",
            "MustConvert"=>0));
        $Type_Init{"Init"} .= $TmpPreamble;
        $Type_Init{"Call"} = $Call;
        #create call in constraint
        if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
        {
            $Type_Init{"TargetCall"} = $Type_Init{"Call"};
        }
        else
        {
            my ($TargetCall, $Target_TmpPreamble) =
            convert_familiar_types((
                "InputTypeName"=>get_TypeName($UnionId),
                "InputPointerLevel"=>0,
                "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Target",
                "MustConvert"=>0));
            $Type_Init{"TargetCall"} = $TargetCall;
            $Type_Init{"Init"} .= $Target_TmpPreamble;
        }
        #ref handler
        if($Type{"Type"} eq "Ref")
        {
            my $BaseRefId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
            if(get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId) > 0)
            {
                my $BaseRefName = get_TypeName($BaseRefId);
                $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
                $Type_Init{"Call"} = $Var."_ref";
                $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
                }
            }
        }
    }
    $Type_Init{"IsCorrect"} = 1;
    return %Type_Init;
}

sub initializeClass(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    if($Init_Desc{"CreateChild"})
    {
        $Init_Desc{"InheritingPriority"} = "High";
        return assembleClass(%Init_Desc);
    }
    else
    {
        if((get_TypeType($Init_Desc{"TypeId"}) eq "Typedef")
        and get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"})>=1)
        {#try to initialize typedefs by interface return value
            %Type_Init = initializeByInterface(%Init_Desc);
            if($Type_Init{"IsCorrect"})
            {
                return %Type_Init;
            }
        }
        $Init_Desc{"InheritingPriority"} = "Low";
        %Type_Init = assembleClass(%Init_Desc);
        if($Type_Init{"IsCorrect"})
        {
            return %Type_Init;
        }
        else
        {
            if(isAbstractClass(get_FoundationTypeId($Init_Desc{"TypeId"})))
            {
                $Init_Desc{"InheritingPriority"} = "High";
                %Type_Init = assembleClass(%Init_Desc);
                if($Type_Init{"IsCorrect"})
                {
                    return %Type_Init;
                }
                else
                {
                    return initializeByInterface(%Init_Desc);
                }
            }
            else
            {
                %Type_Init = initializeByInterface(%Init_Desc);
                if($Type_Init{"IsCorrect"})
                {
                    return %Type_Init;
                }
                else
                {
                    $Init_Desc{"InheritingPriority"} = "High";
                    %Type_Init = assembleClass(%Init_Desc);
                    if($Type_Init{"IsCorrect"})
                    {
                        return %Type_Init;
                    }
                    else
                    {
                        return initializeByInterface_OutParam(%Init_Desc);
                    }
                }
            }
        }
    }
}

sub has_public_destructor($$)
{
    my ($ClassId, $DestrType) = @_;
    my $ClassName = get_TypeName($ClassId);
    return $Cache{"has_public_destructor"}{$ClassId}{$DestrType} if($Cache{"has_public_destructor"}{$ClassId}{$DestrType});
    foreach my $Destructor (sort keys(%{$Class_Destructors{$ClassId}}))
    {
        if($Destructor=~/\Q$DestrType\E/)
        {
            if(not $CompleteSignature{$Destructor}{"Protected"})
            {
                $Cache{"has_public_destructor"}{$ClassId}{$DestrType} = $Destructor;
                return $Destructor;
            }
            else
            {
                return "";
            }
        }
    }
    $Cache{"has_public_destructor"}{$ClassId}{$DestrType} = "Default";
    return "Default";
}

sub findConstructor($$)
{
    my ($ClassId, $Key) = @_;
    return () if(not $ClassId);
    foreach my $Constructor (get_CompatibleInterfaces($ClassId, "Construct", ""))
    {
        my %Interface_Init = callInterfaceParameters((
            "Interface"=>$Constructor,
            "Key"=>$Key,
            "ObjectCall"=>"no object"));
        if($Interface_Init{"IsCorrect"})
        {
            $Interface_Init{"Interface"} = $Constructor;
            return %Interface_Init;
        }
    }
    return ();
}

sub assembleClass(@)
{
    my %Init_Desc = @_;
    my %Type_Init = ();
    my $Global_State = save_state();
    my $CreateDestructor = 1;
    $Type_Init{"TypeName"} = get_TypeName($Init_Desc{"TypeId"});
    my $ClassId = get_FoundationTypeId($Init_Desc{"TypeId"});
    my $ClassName = get_TypeName($ClassId);
    my $PointerLevel = get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    my $Var = $Init_Desc{"Var"};
    $Block_Variable{$CurrentBlock}{$Var} = 1;
    my %Obj_Init = findConstructor($ClassId, $Init_Desc{"Key"});
    if(not $Obj_Init{"IsCorrect"})
    {
        restore_state($Global_State);
        return ();
    }
    $Type_Init{"Init"} = $Obj_Init{"Init"};
    $Type_Init{"Destructors"} = $Obj_Init{"Destructors"};
    $Type_Init{"Code"} = $Obj_Init{"Code"};
    $Type_Init{"Headers"} = addHeaders($Obj_Init{"Headers"}, $Type_Init{"Headers"});
    my $NeedToInheriting = (isAbstractClass($ClassId) or $Init_Desc{"CreateChild"} or isNotInCharge($Obj_Init{"Interface"}) or $CompleteSignature{$Obj_Init{"Interface"}}{"Protected"});
    if($NeedToInheriting and ($Init_Desc{"InheritingPriority"} eq "Low"))
    {
        restore_state($Global_State);
        return ();
    }
    my $HeapStack = (($PointerLevel eq 0) and has_public_destructor($ClassId, "D1") and not $Init_Desc{"ObjectInit"} and (not $Init_Desc{"RetVal"} or get_TypeType($Init_Desc{"TypeId"}) ne "Ref"))?"Stack":"Heap";
    my $ChildName = getSubClassName($ClassName);
    if($NeedToInheriting)
    {
        if($Obj_Init{"Call"}=~/\A(\Q$ClassName\E([\n]*)\()/)
        {
            substr($Obj_Init{"Call"}, index($Obj_Init{"Call"}, $1), pos($1) + length($1)) = $ChildName.$2."(";
        }
        $UsedConstructors{$ClassId}{$Obj_Init{"Interface"}} = 1;
        $IntSubClass{$TestedInterface}{$ClassId} = 1;
        $Create_SubClass{$ClassId} = 1;
        $SubClass_Instance{$Var} = 1;
        $SubClass_ObjInstance{$Var} = 1 if($Init_Desc{"ObjectInit"});
    }
    my %AutoFinalCode_Init = ();
    my $Typedef_Id = detect_typedef($Init_Desc{"TypeId"});
    if(get_TypeName($ClassId)=~/list/i or get_TypeName($Typedef_Id)=~/list/i)
    {#auto final code
        %AutoFinalCode_Init = get_AutoFinalCode($Obj_Init{"Interface"}, ($HeapStack eq "Stack")?$Var:"*".$Var);
        if($AutoFinalCode_Init{"IsCorrect"})
        {
            $Init_Desc{"InLine"} = 0;
        }
    }
    if($Obj_Init{"PreCondition"} or $Obj_Init{"PostCondition"})
    {
        $Init_Desc{"InLine"} = 0;
    }
    # check precondition
    if($Obj_Init{"PreCondition"})
    {
        $Type_Init{"Init"} .= $Obj_Init{"PreCondition"}."\n";
    }
    if($HeapStack eq "Stack")
    {
        $CreateDestructor = 0;
        if($Init_Desc{"InLine"} and ($PointerLevel eq 0))
        {
            $Type_Init{"Call"} = $Obj_Init{"Call"};
            $Type_Init{"TargetCall"} = $Type_Init{"Call"};
            delete($Block_Variable{$CurrentBlock}{$Var});
        }
        else
        {
            if(not defined $DisableReuse)
            {
                $ValueCollection{$CurrentBlock}{$Var} = $ClassId;
            }
            #$Type_Init{"Init"} .= "//parameter initialization\n";
            my $ConstructedName = ($NeedToInheriting)?$ChildName:$ClassName;
            $Type_Init{"Init"} .= correct_init_stmt($ConstructedName." $Var = ".$Obj_Init{"Call"}.";\n", $ConstructedName, $Var);
            #create call
            my ($Call, $TmpPreamble) =
            convert_familiar_types((
                "InputTypeName"=>$ConstructedName,
                "InputPointerLevel"=>0,
                "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Param",
                "MustConvert"=>0));
            $Type_Init{"Init"} .= $TmpPreamble;
            $Type_Init{"Call"} = $Call;
            #call to constraint
            if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
            {
                $Type_Init{"TargetCall"} = $Type_Init{"Call"};
            }
            else
            {
                my ($TargetCall, $Target_TmpPreamble) =
                convert_familiar_types((
                    "InputTypeName"=>$ConstructedName,
                    "InputPointerLevel"=>0,
                    "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
                    "Value"=>$Var,
                    "Key"=>$Var,
                    "Destination"=>"Target",
                    "MustConvert"=>0));
                $Type_Init{"TargetCall"} = $TargetCall;
                $Type_Init{"Init"} .= $Target_TmpPreamble;
            }
        }
    }
    elsif($HeapStack eq "Heap")
    {
        if($Init_Desc{"InLine"} and ($PointerLevel eq 1))
        {
            $Type_Init{"Call"} = "new ".$Obj_Init{"Call"};
            $Type_Init{"TargetCall"} = $Type_Init{"Call"};
            $CreateDestructor = 0;
            delete($Block_Variable{$CurrentBlock}{$Var});
        }
        else
        {
            if(not defined $DisableReuse)
            {
                $ValueCollection{$CurrentBlock}{$Var} = get_TypeIdByName("$ClassName*");
            }
            #$Type_Init{"Init"} .= "//parameter initialization\n";
            if($NeedToInheriting)
            {
                if($Init_Desc{"ConvertToBase"})
                {
                    $Type_Init{"Init"} .= $ClassName."* $Var = ($ClassName*)new ".$Obj_Init{"Call"}.";\n";
                }
                else
                {
                    $Type_Init{"Init"} .= $ChildName."* $Var = new ".$Obj_Init{"Call"}.";\n";
                }
            }
            else
            {
                $Type_Init{"Init"} .= $ClassName."* $Var = new ".$Obj_Init{"Call"}.";\n";
            }
            #create call
            my ($Call, $TmpPreamble) =
            convert_familiar_types((
                "InputTypeName"=>"$ClassName*",
                "InputPointerLevel"=>1,
                "OutputTypeId"=>($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"},
                "Value"=>$Var,
                "Key"=>$Var,
                "Destination"=>"Param",
                "MustConvert"=>0));
            $Type_Init{"Init"} .= $TmpPreamble;
            $Type_Init{"Call"} = $Call;
            #call to constraint
            if($Init_Desc{"TargetTypeId"}==$Init_Desc{"TypeId"})
            {
                $Type_Init{"TargetCall"} = $Type_Init{"Call"};
            }
            else
            {
                my ($TargetCall, $Target_TmpPreamble) =
                convert_familiar_types((
                    "InputTypeName"=>"$ClassName*",
                    "InputPointerLevel"=>1,
                    "OutputTypeId"=>$Init_Desc{"TargetTypeId"},
                    "Value"=>$Var,
                    "Key"=>$Var,
                    "Destination"=>"Target",
                    "MustConvert"=>0));
                $Type_Init{"TargetCall"} = $TargetCall;
                $Type_Init{"Init"} .= $Target_TmpPreamble;
            }
        }
        #destructor for object
        if($CreateDestructor) #mayCallDestructors($ClassId)
        {
            if($HeapStack eq "Heap")
            {
                if($NeedToInheriting)
                {
                    if(has_public_destructor($ClassId, "D2"))
                    {
                        $Type_Init{"Destructors"} .= "delete($Var);\n";
                    }
                }
                else
                {
                    if(has_public_destructor($ClassId, "D0"))
                    {
                        $Type_Init{"Destructors"} .= "delete($Var);\n";
                    }
                }
            }
        }
    }
    # check postcondition
    if($Obj_Init{"PostCondition"})
    {
        $Type_Init{"Init"} .= $Obj_Init{"PostCondition"}."\n";
    }
    if($Obj_Init{"ReturnRequirement"})
    {
        if($HeapStack eq "Stack")
        {
            $Obj_Init{"ReturnRequirement"}=~s/(\$0|\$obj)/$Var/gi;
        }
        else
        {
            $Obj_Init{"ReturnRequirement"}=~s/(\$0|\$obj)/*$Var/gi;
        }
        $Type_Init{"Init"} .= $Obj_Init{"ReturnRequirement"}."\n";
    }
    if($Obj_Init{"FinalCode"})
    {
        $Type_Init{"Init"} .= "//final code\n";
        $Type_Init{"Init"} .= $Obj_Init{"FinalCode"}."\n";
    }
    if(get_TypeType($Init_Desc{"TypeId"}) eq "Ref")
    {#obsolete
        my $BaseRefId = get_OneStep_BaseTypeId($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"});
        if($HeapStack eq "Heap")
        {
            if(get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId)>1)
            {
                my $BaseRefName = get_TypeName($BaseRefId);
                $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
                $Type_Init{"Call"} = $Var."_ref";
                $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
                }
            }
        }
        else
        {
            if(get_PointerLevel($Tid_TDid{$BaseRefId}, $BaseRefId)>0)
            {
                my $BaseRefName = get_TypeName($BaseRefId);
                $Type_Init{"Init"} .= $BaseRefName." ".$Var."_ref = ".$Type_Init{"Call"}.";\n";
                $Type_Init{"Call"} = $Var."_ref";
                $Block_Variable{$CurrentBlock}{$Var."_ref"} = 1;
                if(not defined $DisableReuse)
                {
                    $ValueCollection{$CurrentBlock}{$Var."_ref"} = $Init_Desc{"TypeId"};
                }
            }
        }
    }
    $Type_Init{"IsCorrect"} = 1;
    if($Typedef_Id)
    {
        $Type_Init{"Headers"} = addHeaders(getTypeHeaders($Typedef_Id), $Type_Init{"Headers"});
        foreach my $Elem ("Call", "Init")
        {
            $Type_Init{$Elem} = cover_by_typedef($Type_Init{$Elem}, $ClassId, $Typedef_Id);
        }
    }
    else
    {
        $Type_Init{"Headers"} = addHeaders(getTypeHeaders($ClassId), $Type_Init{"Headers"});
    }
    if($AutoFinalCode_Init{"IsCorrect"})
    {
        $Type_Init{"Init"} = $AutoFinalCode_Init{"Init"}.$Type_Init{"Init"}.$AutoFinalCode_Init{"PreCondition"}.$AutoFinalCode_Init{"Call"}.";\n".$AutoFinalCode_Init{"FinalCode"}.$AutoFinalCode_Init{"PostCondition"};
        $Type_Init{"Code"} .= $AutoFinalCode_Init{"Code"};
        $Type_Init{"Destructors"} .= $AutoFinalCode_Init{"Destructors"};
        $Type_Init{"Headers"} = addHeaders($AutoFinalCode_Init{"Headers"}, $Type_Init{"Headers"});
    }
    return %Type_Init;
}

sub cover_by_typedef($$$)
{
    my ($Code, $Type_Id, $Typedef_Id) = @_;
    if($Class_SubClassTypedef{$Type_Id})
    {
        $Typedef_Id = $Class_SubClassTypedef{$Type_Id};
    }
    return "" if(not $Code or not $Type_Id or not $Typedef_Id);
    return $Code if(not $Type_Id or not $Typedef_Id or (get_TypeType($Type_Id)!~/\A(Class|Struct)\Z/));
    my $Type_Name = get_TypeName($Type_Id);
    my $Typedef_Name = get_TypeName($Typedef_Id);
    my $Child_Name_Old = getSubClassName($Type_Name);
    my $Child_Name_New = getSubClassName($Typedef_Name);
    $Class_SubClassTypedef{$Type_Id}=$Typedef_Id;
    $Code=~s/(\W|\A)\Q$Child_Name_Old\E(\W|\Z)/$1$Child_Name_New$2/g;
    $Code=~s/(\W|\A)\Q$Type_Name\E(\W|\Z)/$1$Typedef_Name$2/g;
    if($Type_Name=~/\W\Z/)
    {
        $Code=~s/(\W|\A)\Q$Type_Name\E/$1$Typedef_Name /g;
    }
    return $Code;
}

sub get_type_typedef($)
{
    my $ClassId = $_[0];
    if($Class_SubClassTypedef{$ClassId})
    {
        return $Class_SubClassTypedef{$ClassId};
    }
    my @Types = (keys(%{$Type_Typedef{$ClassId}}));
    @Types = sort {lc(get_TypeName($a)) cmp lc(get_TypeName($b))} @Types;
    @Types = sort {length(get_TypeName($a)) <=> length(get_TypeName($b))} @Types;
    if($#Types==0)
    {
        return $Types[0];
    }
    else
    {
        return 0;
    }
}

sub is_used_var($$)
{
    my ($Block, $Var) = @_;
    return ($Block_Variable{$Block}{$Var} or $ValueCollection{$Block}{$Var}
    or not is_allowed_var_name($Var));
}

sub select_var_name($$)
{
    my ($Var_Name, $SuffixCandidate) = @_;
    my $OtherVarPrefix = 1;
    my $Candidate = $Var_Name;
    if($Var_Name=~/\Ap\d+\Z/)
    {
        $Var_Name = "p";
        while(is_used_var($CurrentBlock, $Candidate))
        {
            $Candidate = $Var_Name.$OtherVarPrefix;
            $OtherVarPrefix += 1;
        }
    }
    else
    {
        if($SuffixCandidate)
        {
            $Candidate = $Var_Name."_".$SuffixCandidate;
            if(not is_used_var($CurrentBlock, $Candidate))
            {
                return $Candidate;
            }
        }
        while(is_used_var($CurrentBlock, $Candidate))
        {
            $Candidate = $Var_Name."_".$OtherVarPrefix;
            $OtherVarPrefix += 1;
        }
    }
    return $Candidate;
}

sub select_type_name($)
{
    my $Type_Name = $_[0];
    my $OtherPrefix = 1;
    my $NameCandidate = $Type_Name;
    while($TName_Tid{$NameCandidate})
    {
        $NameCandidate = $Type_Name."_".$OtherPrefix;
        $OtherPrefix += 1;
    }
    return $NameCandidate;
}

sub select_func_name($)
{
    my $FuncName = $_[0];
    my $OtherFuncPrefix = 1;
    my $Candidate = $FuncName;
    while(is_used_func_name($Candidate))
    {
        $Candidate = $FuncName."_".$OtherFuncPrefix;
        $OtherFuncPrefix += 1;
    }
    return $Candidate;
}

sub is_used_func_name($)
{
    my $FuncName = $_[0];
    return 1 if($FuncNames{$FuncName});
    foreach my $FuncTypeId (keys(%AuxFunc))
    {
        if($AuxFunc{$FuncTypeId} eq $FuncName)
        {
            return 1;
        }
    }
    return 0;
}

sub get_TypeStackId($)
{
    my $TypeId = $_[0];
    my $FoundationId = get_FoundationTypeId($TypeId);
    if(get_TypeType($FoundationId) eq "Intrinsic")
    {
        my %BaseTypedef = goToFirst($Tid_TDid{$TypeId}, $TypeId, "Typedef");
        if(get_TypeType($BaseTypedef{"Tid"}) eq "Typedef")
        {
            return $BaseTypedef{"Tid"};
        }
        else
        {
            return $FoundationId;
        }
    }
    else
    {
        return $FoundationId;
    }
}

sub initializeType(@)
{
    my %Init_Desc = @_;
    return () if(not $Init_Desc{"TypeId"});
    my %Type_Init = ();
    my $Global_State = save_state();
    my $SpecValue = $Init_Desc{"Value"};
    %Init_Desc = add_VirtualSpecType(%Init_Desc);
    $Init_Desc{"Var"} = select_var_name($LongVarNames?$Init_Desc{"Key"}:$Init_Desc{"ParamName"}, $Init_Desc{"ParamNameExt"});
    if((get_TypeName($Init_Desc{"TypeId"}) eq "...") and (($Init_Desc{"Value"} eq "no value") or ($Init_Desc{"Value"} eq "")))
    {
        $Type_Init{"IsCorrect"} = 1;
        $Type_Init{"Call"} = "";
        return %Type_Init;
    }
    my $FoundationId = get_FoundationTypeId($Init_Desc{"TypeId"});
    if(not $Init_Desc{"FoundationType_Type"})
    {
        $Init_Desc{"FoundationType_Type"} = get_TypeType($FoundationId);
    }
    my $TypeStackId = get_TypeStackId($Init_Desc{"TypeId"});
    if(isCyclical(\@RecurTypeId, $TypeStackId))
    {#initialize by null for cyclical types
        if(($Init_Desc{"Value"} ne "no value") and ($Init_Desc{"Value"} ne ""))
        {
            return () if(get_TypeType($TypeStackId) eq "Typedef");
            %Type_Init = initializeByValue(%Init_Desc);
            $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
            return %Type_Init;
        }
        else
        {
            %Init_Desc = add_NullSpecType(%Init_Desc);
            if($Init_Desc{"OnlyDecl"})
            {
                %Type_Init = emptyDeclaration(%Init_Desc);
                $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
                return %Type_Init;
            }
            elsif(($Init_Desc{"Value"} ne "no value") and ($Init_Desc{"Value"} ne ""))
            {
                %Type_Init = initializeByValue(%Init_Desc);
                $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
                return %Type_Init;
            }
            else
            {
                return ();
            }
        }
    }
    else
    {
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            push(@RecurTypeId, $TypeStackId);
        }
    }
    if(not $Init_Desc{"TargetTypeId"})
    {#repair target type
        $Init_Desc{"TargetTypeId"} = $Init_Desc{"TypeId"};
    }
    if($Init_Desc{"RetVal"} and get_PointerLevel($Tid_TDid{$Init_Desc{"TypeId"}}, $Init_Desc{"TypeId"})>=1
    and not $Init_Desc{"TypeType_Changed"} and get_TypeName($Init_Desc{"TypeId"})!~/(\W|\Z)const(\W|\Z)/)
    {#return value
        if(($Init_Desc{"Value"} ne "no value") and ($Init_Desc{"Value"} ne ""))
        {#try to initialize type by value
            %Type_Init = initializeByValue(%Init_Desc);
            if($Type_Init{"IsCorrect"})
            {
                if($Init_Desc{"FoundationType_Type"} ne "Array")
                {
                    pop(@RecurTypeId);
                }
                $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
                return %Type_Init;
            }
        }
        else
        {
            %Type_Init = initializeRetVal(%Init_Desc);
            if($Type_Init{"IsCorrect"})
            {
                if($Init_Desc{"FoundationType_Type"} ne "Array")
                {
                    pop(@RecurTypeId);
                }
                $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
                return %Type_Init;
            }
        }
    }
    if($Init_Desc{"OnlyDecl"})
    {
        %Type_Init = emptyDeclaration(%Init_Desc);
        $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            pop(@RecurTypeId);
        }
        return %Type_Init;
    }
    my $RealTypeId = ($Init_Desc{"TypeId_Changed"})?$Init_Desc{"TypeId_Changed"}:$Init_Desc{"TypeId"};
    my $RealFTypeType = get_TypeType(get_FoundationTypeId($RealTypeId));
    if(($RealFTypeType eq "Intrinsic") and not $SpecValue and not $Init_Desc{"Reuse"} and not $Init_Desc{"OnlyByValue"} and $Init_Desc{"ParamName"}!~/num|width|height/i)
    {#initializing intrinsics by the interface
        my %BaseTypedef = goToFirst($Tid_TDid{$RealTypeId}, $RealTypeId, "Typedef");
        if(get_TypeType($BaseTypedef{"Tid"}) eq "Typedef"
            and $BaseTypedef{"Name"}!~/(int|short|long|error|real|float|double|bool|boolean|pointer|count|byte)\d*(_t|)\Z/i
            and $BaseTypedef{"Name"}!~/char|str|size|enum/i
            and $BaseTypedef{"Name"}!~/(\A|::)u(32|64)/i)
        {#try to initialize typedefs to intrinsic types
            my $Global_State1 = save_state();
            my %Init_Desc_Copy = %Init_Desc;
            $Init_Desc_Copy{"InLine"} = 0 if($Init_Desc{"ParamName"}!~/\Ap\d+\Z/);
            $Init_Desc_Copy{"TypeId"} = $RealTypeId;
            restore_state($Global_State);
            %Type_Init = initializeIntrinsic(%Init_Desc_Copy);
            if($Type_Init{"IsCorrect"})
            {
                if($Init_Desc{"FoundationType_Type"} ne "Array")
                {
                    pop(@RecurTypeId);
                }
                return %Type_Init;
            }
            else
            {
                restore_state($Global_State1);
            }
        }
    }
    if(($Init_Desc{"Value"} ne "no value") and ($Init_Desc{"Value"} ne ""))
    {#try to initialize type by value
        %Type_Init = initializeByValue(%Init_Desc);
        $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            pop(@RecurTypeId);
        }
        return %Type_Init;
    }
    else
    {
        %Type_Init = selectInitializingWay(%Init_Desc);
    }
    if($Type_Init{"IsCorrect"})
    {
        $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            pop(@RecurTypeId);
        }
        return %Type_Init;
    }
    else
    {
        restore_state($Global_State);
    }
    if($Init_Desc{"TypeId_Changed"})
    {
        $Init_Desc{"TypeId"} = $Init_Desc{"TypeId_Changed"};
        %Init_Desc = add_VirtualSpecType(%Init_Desc);
        if(($Init_Desc{"Value"} ne "no value") and ($Init_Desc{"Value"} ne ""))
        {#try to initialize type by value
            %Type_Init = initializeByValue(%Init_Desc);
            $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
            if($Init_Desc{"FoundationType_Type"} ne "Array")
            {
                pop(@RecurTypeId);
            }
            return %Type_Init;
        }
    }
    #finally initializing by null (0)
    %Init_Desc = add_NullSpecType(%Init_Desc);
    if($Init_Desc{"OnlyDecl"})
    {
        %Type_Init = emptyDeclaration(%Init_Desc);
        $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            pop(@RecurTypeId);
        }
        return %Type_Init;
    }
    elsif(($Init_Desc{"Value"} ne "no value") and ($Init_Desc{"Value"} ne ""))
    {
        %Type_Init = initializeByValue(%Init_Desc);
        $Type_Init{"Headers"} = addHeaders($Init_Desc{"Headers"}, $Type_Init{"Headers"});
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            pop(@RecurTypeId);
        }
        return %Type_Init;
    }
    else
    {
        if($Init_Desc{"FoundationType_Type"} ne "Array")
        {
            pop(@RecurTypeId);
        }
        return ();
    }
}

sub selectInitializingWay(@)
{
    my %Init_Desc = @_;
    if($Init_Desc{"FoundationType_Type"} eq "Class")
    {
        return initializeClass(%Init_Desc);
    }
    elsif($Init_Desc{"FoundationType_Type"} eq "Intrinsic")
    {
        return initializeIntrinsic(%Init_Desc);
    }
    elsif($Init_Desc{"FoundationType_Type"} eq "Struct")
    {
        return initializeStruct(%Init_Desc);
    }
    elsif($Init_Desc{"FoundationType_Type"} eq "Union")
    {
        return initializeUnion(%Init_Desc);
    }
    elsif($Init_Desc{"FoundationType_Type"} eq "Enum")
    {
        return initializeEnum(%Init_Desc);
    }
    elsif($Init_Desc{"FoundationType_Type"} eq "Array")
    {
        return initializeArray(%Init_Desc);
    }
    elsif($Init_Desc{"FoundationType_Type"} eq "FuncPtr")
    {
        return initializeFuncPtr(%Init_Desc);
    }
    else
    {
        return ();
    }
}

sub is_const_type($)
{
    my $TypeName = uncover_typedefs($_[0]);
    return ($TypeName=~/(\W|\A)const(\W|\Z)/);
}

sub clearSyntax($)
{
    my $Expression = $_[0];
    $Expression=~s/\*\&//g;
    $Expression=~s/\&\*//g;
    $Expression=~s/\(\*(\w+)\)\./$1\-\>/ig;
    $Expression=~s/\(\&(\w+)\)\-\>/$1\./ig;
    $Expression=~s/\*\(\&(\w+)\)/$1/ig;
    $Expression=~s/\*\(\(\&(\w+)\)\)/$1/ig;
    $Expression=~s/\&\(\*(\w+)\)/$1/ig;
    $Expression=~s/\&\(\(\*(\w+)\)\)/$1/ig;
    $Expression=~s/(?<=[\s()])\(([a-z_]\w*)\)[ ]*,/$1,/ig;
    $Expression=~s/,(\s*)\(([a-z_]\w*)\)[ ]*(\)|,)/,$1$2/ig;
    $Expression=~s/(?<=[^\$])\(\(([a-z_]\w*)\)\)/\($1\)/ig;
    return $Expression;
}

sub get_random_number($)
{
    my $Range = $_[0];
    return `echo \$(( (\`od -An -N2 -i /dev/random\` )\%$Range ))`;
}

sub apply_default_value($$)
{
    my ($Interface, $ParamPos) = @_;
    if(replace_c2c1($Interface) ne replace_c2c1($TestedInterface)
        and not defined $DisableDefaultValues
        and defined $CompleteSignature{$Interface}{"Param"}{$ParamPos}
        and $CompleteSignature{$Interface}{"Param"}{$ParamPos}{"default"})
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

sub sort_AppendInsert(@)
{
    my @Interfaces = @_;
    my (@Add, @Append, @Push, @Init, @Insert) = ();
    foreach my $Interface (@Interfaces)
    {
        if($CompleteSignature{$Interface}{"ShortName"}=~/add/i)
        {
            push(@Add, $Interface);
        }
        elsif($CompleteSignature{$Interface}{"ShortName"}=~/append/i)
        {
            push(@Append, $Interface);
        }
        elsif($CompleteSignature{$Interface}{"ShortName"}=~/push/i)
        {
            push(@Push, $Interface);
        }
        elsif($CompleteSignature{$Interface}{"ShortName"}=~/init/i)
        {
            push(@Init, $Interface);
        }
        elsif($CompleteSignature{$Interface}{"ShortName"}=~/insert/)
        {
            push(@Insert, $Interface);
        }
    }
    return (@Add, @Append, @Push, @Init, @Insert);
}

sub get_AutoFinalCode($$)
{
    my ($Interface, $ObjectCall) = @_;
    my (@AddMethods, @AppendMethods, @PushMethods, @InitMethods, @InsertMethods) = ();
    if($CompleteSignature{$Interface}{"Constructor"})
    {
        my $ClassId = $CompleteSignature{$Interface}{"Class"};
        my @Methods = sort_AppendInsert(keys(%{$Class_Method{$ClassId}}));
        return () if($#Methods<0);
        foreach my $Method (@Methods)
        {
            my %Method_Init = callInterface((
            "Interface"=>$Method,
            "ObjectCall"=>$ObjectCall,
            "DoNotReuse"=>1,
            "InsertCall"));
            if($Method_Init{"IsCorrect"})
            {
                return %Method_Init;
            }
        }
        return ();
    }
    else
    {
        return ();
    }
}

sub initializeParameter(@)
{
    my %ParamDesc = @_;
    my $ParamPos = $ParamDesc{"ParamPos"};
    my ($TypeOfSpecType, $SpectypeCode, $SpectypeValue);
    my (%Param_Init, $PreCondition, $PostCondition, $InitCode);
    my $ObjectCall = $ParamDesc{"AccessToParam"}->{"obj"};
    if((not $ParamDesc{"SpecType"}) and ($ObjectCall ne "create object")
    and not $Interface_OutParam_NoUsing{$ParamDesc{"Interface"}}{$ParamDesc{"ParamName"}}
    and not $Interface_OutParam{$ParamDesc{"Interface"}}{$ParamDesc{"ParamName"}})
    {
        $ParamDesc{"SpecType"} = chooseSpecType($ParamDesc{"TypeId"}, "common_param", $ParamDesc{"Interface"});
    }
    if($ParamDesc{"SpecType"} and not isCyclical(\@RecurSpecType, $ParamDesc{"SpecType"}))
    {
        $IntSpecType{$TestedInterface}{$ParamDesc{"SpecType"}} = 1;
        $SpectypeCode = $SpecType{$ParamDesc{"SpecType"}}{"GlobalCode"} if(not $SpecCode{$ParamDesc{"SpecType"}});
        $SpecCode{$ParamDesc{"SpecType"}} = 1;
        push(@RecurSpecType, $ParamDesc{"SpecType"});
        $TypeOfSpecType = get_TypeIdByName($SpecType{$ParamDesc{"SpecType"}}{"DataType"});
        $InitCode = $SpecType{$ParamDesc{"SpecType"}}{"InitCode"};
        if($InitCode)
        {
            $InitCode .= "\n";
            if($InitCode=~/\$0/ or $InitCode=~/\$$ParamPos(\Z|\D)/)
            {
                $ParamDesc{"InLine"} = 0;
            }
        }
        $Param_Init{"FinalCode"} = $SpecType{$ParamDesc{"SpecType"}}{"FinalCode"};
        if($Param_Init{"FinalCode"})
        {
            $Param_Init{"FinalCode"} .= "\n";
            if($Param_Init{"FinalCode"}=~/\$0/
            or $Param_Init{"FinalCode"}=~/\$$ParamPos(\Z|\D)/)
            {
                $ParamDesc{"InLine"} = 0;
            }
        }
        $PreCondition = $SpecType{$ParamDesc{"SpecType"}}{"PreCondition"};
        if($PreCondition=~/\$0/ or $PreCondition=~/\$$ParamPos(\Z|\D)/)
        {
            $ParamDesc{"InLine"} = 0;
        }
        $PostCondition = $SpecType{$ParamDesc{"SpecType"}}{"PostCondition"};
        if($PostCondition=~/\$0/ or $PostCondition=~/\$$ParamPos(\Z|\D)/)
        {
            $ParamDesc{"InLine"} = 0;
        }
        $SpectypeValue = $SpecType{$ParamDesc{"SpecType"}}{"Value"};
        foreach my $Lib (keys(%{$SpecType{$ParamDesc{"SpecType"}}{"Libs"}}))
        {
            $SpecLibs{$Lib} = 1;
        }
    }
    elsif(apply_default_value($ParamDesc{"Interface"}, $ParamDesc{"ParamPos"}))
    {
        $Param_Init{"IsCorrect"} = 1;
        $Param_Init{"Call"} = "";
        return %Param_Init;
    }
    if(($ObjectCall ne "no object") and ($ObjectCall ne "create object"))
    {
        if(($ObjectCall=~/\A\*/) or ($ObjectCall=~/\A\&/))
        {
            $ObjectCall = "(".$ObjectCall.")";
        }
        $SpectypeValue=~s/\$obj/$ObjectCall/g;
        $SpectypeValue = clearSyntax($SpectypeValue);
    }
    if(($ParamDesc{"Value"} ne "") and ($ParamDesc{"Value"} ne "no value"))
    {
        $SpectypeValue = $ParamDesc{"Value"};
    }
    if($SpectypeValue=~/\$[^\(\[]/)
    {# access to other parameters
        foreach my $ParamKey (keys(%{$ParamDesc{"AccessToParam"}}))
        {
            my $AccessToParam_Value = $ParamDesc{"AccessToParam"}->{$ParamKey};
            $SpectypeValue=~s/\$\Q$ParamKey\E([^0-9]|\Z)/$AccessToParam_Value$1/g;
        }
    }
    if($SpectypeValue)
    {
        my %ParsedValueCode = parseCode($SpectypeValue, "Value");
        if(not $ParsedValueCode{"IsCorrect"})
        {
            pop(@RecurSpecType);
            return ();
        }
        $Param_Init{"Init"} .= $ParsedValueCode{"CodeBefore"};
        $Param_Init{"FinalCode"} .= $ParsedValueCode{"CodeAfter"};
        $SpectypeValue = $ParsedValueCode{"Code"};
        $Param_Init{"Headers"} = addHeaders($ParsedValueCode{"Headers"}, $ParsedValueCode{"Headers"});
        $Param_Init{"Code"} .= $ParsedValueCode{"NewGlobalCode"};
    }
    my $FoundationType_Id = get_FoundationTypeId($ParamDesc{"TypeId"});
    if(get_TypeType($FoundationType_Id)=~/\A(Struct|Class|Union)\Z/i
    and $CompleteSignature{$ParamDesc{"Interface"}}{"Constructor"}
    and get_PointerLevel($Tid_TDid{$ParamDesc{"TypeId"}}, $ParamDesc{"TypeId"})==0)
    {
        $ParamDesc{"InLine"} = 0;
    }
    if($ParamDesc{"Usage"} eq "Common")
    {
        my %Type_Init = initializeType((
        "Interface" => $ParamDesc{"Interface"},
        "TypeId" => $ParamDesc{"TypeId"},
        "Key" => $ParamDesc{"Key"},
        "InLine" => $ParamDesc{"InLine"},
        "Value" => $SpectypeValue,
        "ValueTypeId" => $TypeOfSpecType,
        "TargetTypeId" => $TypeOfSpecType,
        "CreateChild" => $ParamDesc{"CreateChild"},
        "ParamName" => $ParamDesc{"ParamName"},
        "ParamPos" => $ParamDesc{"ParamPos"},
        "ConvertToBase" => $ParamDesc{"ConvertToBase"},
        "StrongConvert" => $ParamDesc{"StrongConvert"},
        "ObjectInit" => $ParamDesc{"ObjectInit"},
        "DoNotReuse" => $ParamDesc{"DoNotReuse"},
        "RetVal" => $ParamDesc{"RetVal"},
        "ParamNameExt" => $ParamDesc{"ParamNameExt"},
        "MaxParamPos" => $ParamDesc{"MaxParamPos"},
        "OuterType_Id" => $ParamDesc{"OuterType_Id"},
        "OuterType_Type" => $ParamDesc{"OuterType_Type"},
        "Index" => $ParamDesc{"Index"},
        "InLineArray" => $ParamDesc{"InLineArray"},
        "IsString" => $ParamDesc{"IsString"},
        "FuncPtrName" => $ParamDesc{"FuncPtrName"},
        "FuncPtrTypeId" => $ParamDesc{"FuncPtrTypeId"}));
        if(not $Type_Init{"IsCorrect"})
        {
            pop(@RecurSpecType);
            return ();
        }
        $Param_Init{"Init"} .= $Type_Init{"Init"};
        $Param_Init{"Call"} .= $Type_Init{"Call"};
        $Param_Init{"TargetCall"} = $Type_Init{"TargetCall"};
        $Param_Init{"Code"} .= $Type_Init{"Code"};
        $Param_Init{"Destructors"} .= $Type_Init{"Destructors"};
        $Param_Init{"FinalCode"} .= $Type_Init{"FinalCode"};
        $Param_Init{"PreCondition"} .= $Type_Init{"PreCondition"};
        $Param_Init{"PostCondition"} .= $Type_Init{"PostCondition"};
        $Param_Init{"Headers"} = addHeaders($Type_Init{"Headers"}, $Param_Init{"Headers"});
        $Param_Init{"ByNull"} = $Type_Init{"ByNull"};
    }
    else
    {
        $Param_Init{"Headers"} = addHeaders(getTypeHeaders($ParamDesc{"TypeId"}), $Param_Init{"Headers"});
        if(my $Target = $ParamDesc{"AccessToParam"}->{"0"})
        {
            $Param_Init{"TargetCall"} = $Target;
        }
    }
    my $TargetCall = $Param_Init{"TargetCall"};
    if(($TargetCall=~/\A\*/) or ($TargetCall=~/\A\&/))
    {
        $TargetCall = "(".$TargetCall.")";
    }
    if($SpectypeCode)
    {
        my $PreviousBlock = $CurrentBlock;
        $CurrentBlock = $CurrentBlock."_code_".$ParamDesc{"SpecType"};
        my %ParsedCode = parseCode($SpectypeCode, "Code");
        $CurrentBlock = $PreviousBlock;
        if(not $ParsedCode{"IsCorrect"})
        {
            pop(@RecurSpecType);
            return ();
        }
        foreach my $Header (@{$ParsedCode{"Headers"}})
        {
            $SpecTypeHeaders{get_FileName($Header)}=1;
        }
        $Param_Init{"Headers"} = addHeaders($ParsedCode{"Headers"}, $Param_Init{"Headers"});
        $Param_Init{"Code"} .= $ParsedCode{"NewGlobalCode"}.$ParsedCode{"Code"};
    }
    if($ObjectCall eq "create object")
    {
        $ObjectCall = $Param_Init{"Call"};
        if(($ObjectCall=~/\A\*/) or ($ObjectCall=~/\A\&/))
        {
            $ObjectCall = "(".$ObjectCall.")";
        }
    }
    if($InitCode)
    {
        if($ObjectCall ne "no object")
        {
            $InitCode=~s/\$obj/$ObjectCall/g;
        }
        $InitCode=~s/\$0/$TargetCall/g;
        my %ParsedCode = parseCode($InitCode, "Code");
        if(not $ParsedCode{"IsCorrect"})
        {
            pop(@RecurSpecType);
            return ();
        }
        $InitCode = clearSyntax($InitCode);
        $Param_Init{"Headers"} = addHeaders($ParsedCode{"Headers"}, $Param_Init{"Headers"});
        $Param_Init{"Code"} .= $ParsedCode{"NewGlobalCode"};
        $InitCode = $ParsedCode{"Code"};
        $Param_Init{"Init"} .= "//init code\n".$InitCode."\n";
    }
    if($Param_Init{"FinalCode"})
    {
        if($ObjectCall ne "no object")
        {
            $Param_Init{"FinalCode"}=~s/\$obj/$ObjectCall/g;
        }
        $Param_Init{"FinalCode"}=~s/\$0/$TargetCall/g;
        my %ParsedCode = parseCode($Param_Init{"FinalCode"}, "Code");
        if(not $ParsedCode{"IsCorrect"})
        {
            pop(@RecurSpecType);
            return ();
        }
        $Param_Init{"FinalCode"} = clearSyntax($Param_Init{"FinalCode"});
        $Param_Init{"Headers"} = addHeaders($ParsedCode{"Headers"}, $Param_Init{"Headers"});
        $Param_Init{"Code"} .= $ParsedCode{"NewGlobalCode"};
        $Param_Init{"FinalCode"} = $ParsedCode{"Code"};
    }
    if(not defined $Template2Code or $ParamDesc{"Interface"} eq $TestedInterface)
    {
        $Param_Init{"PreCondition"} .= constraint_for_parameter($ParamDesc{"Interface"}, $SpecType{$ParamDesc{"SpecType"}}{"DataType"}, "precondition", $PreCondition, $ObjectCall, $TargetCall);
        $Param_Init{"PostCondition"} .= constraint_for_parameter($ParamDesc{"Interface"}, $SpecType{$ParamDesc{"SpecType"}}{"DataType"}, "postcondition", $PostCondition, $ObjectCall, $TargetCall);
    }
    pop(@RecurSpecType);
    $Param_Init{"IsCorrect"} = 1;
    return %Param_Init;
}

sub constraint_for_parameter($$$$$$)
{
    my ($Interface, $DataType, $ConditionType, $Condition, $ObjectCall, $TargetCall) = @_;
    return "" if(not $Interface or not $ConditionType or not $Condition);
    my $Condition_Comment = $Condition;
    $Condition_Comment=~s/\$obj/$ObjectCall/g if($ObjectCall ne "no object" and $ObjectCall ne "");
    $Condition_Comment=~s/\$0/$TargetCall/g if($TargetCall ne "");
    $Condition_Comment = clearSyntax($Condition_Comment);
    $Condition = $Condition_Comment;
    while($Condition_Comment=~s/([^\\])"/$1\\\"/g){}
    $ConstraintNum{$Interface}+=1;
    my $ParameterObject = ($ObjectCall eq "create object")?"object":"parameter";
    $RequirementsCatalog{$Interface}{$ConstraintNum{$Interface}} = "$ConditionType for the $ParameterObject: \'$Condition_Comment\'";
    my $ReqId = short_interface_name($Interface).".".normalize_num($ConstraintNum{$Interface});
    if(my $Format = is_printable($DataType))
    {
        my $Comment = "$ConditionType for the $ParameterObject failed: \'$Condition_Comment\', parameter value: $Format";
        $TraceFunc{"REQva"}=1;
        return "REQva(\"$ReqId\",\n    $Condition,\n    \"$Comment\", $TargetCall);\n";
    }
    else
    {
        my $Comment = "$ConditionType for the $ParameterObject failed: \'$Condition_Comment\'";
        $TraceFunc{"REQ"}=1;
        return "REQ(\"$ReqId\", \"$Comment\", $Condition);\n";
    }
}

sub constraint_for_interface($$$)
{
    my ($Interface, $ConditionType, $Condition) = @_;
    return "" if(not $Interface or not $ConditionType or not $Condition);
    my $Condition_Comment = $Condition;
    $Condition_Comment = clearSyntax($Condition_Comment);
    $Condition = $Condition_Comment;
    while($Condition_Comment=~s/([^\\])"/$1\\\"/g){}
    $ConstraintNum{$Interface}+=1;
    $RequirementsCatalog{$Interface}{$ConstraintNum{$Interface}} = "$ConditionType for the interface: \'$Condition_Comment\'";
    my $ReqId = short_interface_name($Interface).".".normalize_num($ConstraintNum{$Interface});
    my $Comment = "$ConditionType for the interface failed: \'$Condition_Comment\'";
    $TraceFunc{"REQ"}=1;
    return "REQ(\"$ReqId\", \"$Comment\", $Condition);\n";
}

sub create_spec_type($$)
{
    my ($Value, $TypeName) = @_;
    return if($Value eq "" or not $TypeName);
    $ST_ID += 1;
    $SpecType{$ST_ID}{"Value"} = $Value;
    $SpecType{$ST_ID}{"DataType"} = $TypeName;
    return $ST_ID;
}

sub is_array_count($$)
{
    my ($ParamName_Prev, $ParamName_Next) = @_;
    return ($ParamName_Next=~/\A(\Q$ParamName_Prev\E|)[_]*(n|l|c|s)[_]*(\Q$ParamName_Prev\E|)\Z/i
    or $ParamName_Next=~/len|size|amount|count|num|number/i);
}

sub add_VirtualProxy($$$$)
{
    my ($Interface, $OutParamPos, $Order, $Step) = @_;
    return if(keys(%{$CompleteSignature{$Interface}{"Param"}})<$Step+1);
    foreach my $Param_Pos (sort {($Order eq "forward")?int($a)<=>int($b):int($b)<=>int($a)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        my $Prev_Pos = ($Order eq "forward")?$Param_Pos-$Step:$Param_Pos+$Step;
        next if(($Order eq "forward")?$Prev_Pos<0:$Prev_Pos>keys(%{$CompleteSignature{$Interface}{"Param"}})-1);
        my $ParamName = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"name"};
        my $ParamTypeId = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"type"};
        my $ParamTypeName = get_TypeName($ParamTypeId);
        my $ParamName_Prev = $CompleteSignature{$Interface}{"Param"}{$Prev_Pos}{"name"};
        my $ParamTypeId_Prev = $CompleteSignature{$Interface}{"Param"}{$Prev_Pos}{"type"};
        my $ParamTypeName_Prev = get_TypeName($ParamTypeId_Prev);
        if(not $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos})
        {
            next if($OutParamPos ne "" and $OutParamPos==$Prev_Pos);
            my $ParamFTypeId = get_FoundationTypeId($ParamTypeId);
            my $ParamFTypeId_Previous = get_FoundationTypeId($ParamTypeId_Prev);
            my $ParamTypePLevel_Previous = get_PointerLevel($Tid_TDid{$ParamTypeId_Prev}, $ParamTypeId_Prev);
            if(isIntegerType(get_TypeName($ParamFTypeId)) and get_PointerLevel($Tid_TDid{$ParamTypeId}, $ParamTypeId)==0
            and get_PointerLevel($Tid_TDid{$ParamTypeId_Prev}, $ParamTypeId_Prev)>=1 and $ParamName_Prev
            and is_array_count($ParamName_Prev, $ParamName) and not isOutParam_NoUsing($ParamTypeId_Prev, $ParamName_Prev, $Interface)
            and not $OutParamInterface_Pos{$Interface}{$Prev_Pos} and not $OutParamInterface_Pos_NoUsing{$Interface}{$Prev_Pos})
            {
                if(isArray($ParamTypeId_Prev, $ParamName_Prev, $Interface))
                {
                    $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos} = create_spec_type($DEFAULT_ARRAY_AMOUNT, get_TypeName($ParamTypeId));
                }
                elsif(isBuffer($ParamTypeId_Prev, $ParamName_Prev, $Interface))
                {
                    $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos} = create_spec_type($BUFF_SIZE, get_TypeName($ParamTypeId));
                }
                elsif(isString($ParamTypeId_Prev, $ParamName_Prev, $Interface))
                {
                    if($ParamName_Prev=~/file|src|uri|buf|dir|url/i)
                    {
                        $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos} = create_spec_type("1", get_TypeName($ParamTypeId));
                    }
                    elsif($ParamName_Prev!~/\Ap\d+\Z/i)
                    {
                        $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos} = create_spec_type(length($ParamName_Prev), get_TypeName($ParamTypeId));
                    }
                }
                elsif($ParamName_Prev=~/buf/i)
                {
                    $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos} = create_spec_type("1", get_TypeName($ParamTypeId));
                }
            }
            elsif($Order eq "forward" and isString($ParamTypeId_Prev, $ParamName_Prev, $Interface)
            and ($ParamName_Prev=~/\A[_0-9]*(format|fmt)[_0-9]*\Z/i) and ($ParamTypeName eq "..."))
            {
                $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos-1} = create_spec_type("\"\%d\"", get_TypeName($ParamTypeId_Prev));
                $InterfaceSpecType{$Interface}{"SpecParam"}{$Param_Pos} = create_spec_type("1", "int");
            }
        }
    }
}

sub isExactValueAble($)
{
    my $TypeName = $_[0];
    return $TypeName=~/\A(char const\*|wchar_t const\*|wint_t|int|bool|double|float|long double|char|long|long long|long long int|long int)\Z/;
}

sub select_obj_name($$)
{
    my ($Key, $ClassId) = @_;
    my $ClassName = get_TypeName($ClassId);
    if(my $NewName = getParamNameByTypeName($ClassName))
    {
        return $NewName;
    }
    else
    {
        return (($Key)?"src":"obj");
    }
}

sub getParamNameByTypeName($)
{
    my $TypeName = get_type_short_name(remove_quals($_[0]));
    return "" if(not $TypeName or $TypeName=~/\(|\)|<|>/);
    while($TypeName=~s/\A\w+\:\://g){ };
    while($TypeName=~s/(\*|\&|\[|\])//g){ };
    $TypeName=~s/(\A\s+|\s+\Z)//g;
    return "Db" if($TypeName eq "sqlite3");
    return "tif" if($TypeName eq "TIFF");
    if(my $Prefix = getPrefix($TypeName))
    {
        if($Library_Prefixes{$Prefix}>=10)
        {
            $TypeName=~s/\A\Q$Prefix\E//;
            if(is_allowed_var_name(lc($TypeName)))
            {
                return lc($TypeName);
            }
        }
    }
    if($TypeName=~/[A-Z]+/)
    {
        if(is_allowed_var_name(lc($TypeName)))
        {
            return lc($TypeName);
        }
    }
    return "";
}

sub is_allowed_var_name($)
{
    my $Candidate = $_[0];
    return (not $IsKeyword{$Candidate} and not $TName_Tid{$Candidate}
    and not $NameSpaces{$Candidate} and not $EnumMembers{$Candidate}
    and not $GlobVarNames{$Candidate} and not $FuncNames{$Candidate});
}

sub callInterfaceParameters_m(@)
{
    my %Init_Desc = @_;
    my (@ParamList, %ParametersOrdered, %Params_Init, $IsWrapperCall);
    my ($Interface, $Key, $ObjectCall) = ($Init_Desc{"Interface"}, $Init_Desc{"Key"}, $Init_Desc{"ObjectCall"});
    add_VirtualProxy($Interface, $Init_Desc{"OutParam"},  "forward", 1);
    add_VirtualProxy($Interface, $Init_Desc{"OutParam"},  "forward", 2);
    add_VirtualProxy($Interface, $Init_Desc{"OutParam"}, "backward", 1);
    add_VirtualProxy($Interface, $Init_Desc{"OutParam"}, "backward", 2);
    my (%KeyTable, %AccessToParam, %TargetAccessToParam, %InvOrder, %Interface_Init, $SubClasses_Before) = ();
    $AccessToParam{"obj"} = $ObjectCall;
    $TargetAccessToParam{"obj"} = $ObjectCall;
    return () if(needToInherit($Interface) and isInCharge($Interface));
    $Interface_Init{"Headers"} = addHeaders([$CompleteSignature{$Interface}{"Header"}], $Interface_Init{"Headers"}) if(not $CompleteSignature{$Interface}{"Constructor"});
    $Interface_Init{"Headers"} = addHeaders(getTypeHeaders($CompleteSignature{$Interface}{"Return"}), $Interface_Init{"Headers"});
    my $ShortName = $CompleteSignature{$Interface}{"ShortName"};
    if($CompleteSignature{$Interface}{"Constructor"})
    {
        $Interface_Init{"Call"} .= get_TypeName($CompleteSignature{$Interface}{"Class"});
    }
    else
    {
        $Interface_Init{"Call"} .= $ShortName;
    }
    my $IsWrapperCall = (($CompleteSignature{$Interface}{"Protected"}) and (not $CompleteSignature{$Interface}{"Constructor"}));
    if($IsWrapperCall)
    {
        $Interface_Init{"Call"} .= "_Wrapper";
        $Interface_Init{"Call"} = cleanName($Interface_Init{"Call"});
        @{$SubClasses_Before}{keys %Create_SubClass} = values %Create_SubClass;
        %Create_SubClass = ();
    }
    my $NumOfParams = getNumOfParams($Interface);
    #determine inline parameters
    my %InLineParam = detectInLineParams($Interface);
    my %Order = detectParamsOrder($Interface);
    @InvOrder{values %Order} = keys %Order;
    foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
    {
        $ParametersOrdered{$Order{$Param_Pos + 1} - 1}{"type"} = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"type"};
        $ParametersOrdered{$Order{$Param_Pos + 1} - 1}{"name"} = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"name"};
    }
    #initializing parameters
    if(keys(%{$CompleteSignature{$Interface}{"Param"}})>0
    and defined $CompleteSignature{$Interface}{"Param"}{0})
    {
        my $MaxParamPos = keys(%{$CompleteSignature{$Interface}{"Param"}}) - 1;
        foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
        {
            next if($Param_Pos eq "");
            my $TypeId = $ParametersOrdered{$Param_Pos}{"type"};
            my $FTypeId = get_FoundationTypeId($TypeId);
            my $TypeName = get_TypeName($TypeId);
            my $Param_Name = $ParametersOrdered{$Param_Pos}{"name"};
            if($Param_Name=~/\Ap\d+\Z/ and (my $NewParamName = getParamNameByTypeName($TypeName)))
            {
                $Param_Name = $NewParamName;
            }
            my $Param_Name_Ext = "";
            if(is_used_var($CurrentBlock, $Param_Name) and not $LongVarNames
            and ($Key=~/(_|\A)\Q$Param_Name\E(_|\Z)/))
            {
                if($TypeName=~/string/i)
                {
                    $Param_Name_Ext="str";
                }
                elsif($TypeName=~/char/i)
                {
                    $Param_Name_Ext="ch";
                }
            }
            $Param_Name = "p".($InvOrder{$Param_Pos + 1} - 1) if(not $Param_Name);
            my $TypeType = get_TypeType($TypeId);
            my $TypeName_Uncovered = uncover_typedefs($TypeName);
            my $InLine = $InLineParam{$InvOrder{$Param_Pos + 1}};
            my $StrongConvert = 0;
            if($OverloadedInterface{$Interface})
            {
                if(not isExactValueAble($TypeName_Uncovered) and ($TypeType ne "Enum"))
                {
                    #$InLine = 0;
                    $StrongConvert = 1;
                }
            }
            $InLine = 0 if(uncover_typedefs($TypeName)=~/\&/);
            $InLine = 0 if(get_TypeType($FTypeId)!~/\A(Intrinsic|Enum)\Z/ and $Param_Name!~/\Ap\d+\Z/
                and not isCyclical(\@RecurTypeId, get_TypeStackId($TypeId)));
            my $NewKey = ($Param_Name)? (($Key)?$Key."_".$Param_Name:$Param_Name) : ($Key)?$Key."_".$InvOrder{$Param_Pos + 1}:"p".$InvOrder{$Param_Pos + 1};
            my $SpecTypeId = $InterfaceSpecType{$Interface}{"SpecParam"}{$InvOrder{$Param_Pos + 1} - 1};
            #initialize parameter
            if(($Init_Desc{"OutParam"} ne "") and $Param_Pos==$Init_Desc{"OutParam"})
            {# initializing out-parameter
                $AccessToParam{$InvOrder{$Param_Pos + 1}} = $Init_Desc{"OutVar"};
                $TargetAccessToParam{$InvOrder{$Param_Pos + 1}} = $Init_Desc{"OutVar"};
                if($SpecTypeId and ($SpecType{$SpecTypeId}{"InitCode"}.$SpecType{$SpecTypeId}{"FinalCode"}.$SpecType{$SpecTypeId}{"PreCondition"}.$SpecType{$SpecTypeId}{"PostCondition"})=~/\$0/)
                {
                    if(is_equal_types(get_TypeName($TypeId), $SpecType{$SpecTypeId}{"DataType"}))
                    {
                        $AccessToParam{"0"} = $Init_Desc{"OutVar"};
                        $TargetAccessToParam{"0"} = $Init_Desc{"OutVar"};
                    }
                    else
                    {
                        my ($TargetCall, $Preamble)=
                        convert_familiar_types((
                            "InputTypeName"=>get_TypeName($TypeId),
                            "InputPointerLevel"=>get_PointerLevel($Tid_TDid{$TypeId}, $TypeId),
                            "OutputTypeId"=>get_TypeIdByName($SpecType{$SpecTypeId}{"DataType"}),
                            "Value"=>$Init_Desc{"OutVar"},
                            "Key"=>$NewKey,
                            "Destination"=>"Target",
                            "MustConvert"=>0));
                        $Params_Init{"Init"} .= $Preamble;
                        $AccessToParam{"0"} = $TargetCall;
                        $TargetAccessToParam{"0"} = $TargetCall;
                    }
                }
                my %Param_Init = initializeParameter((
                    "Interface" => $Interface,
                    "AccessToParam" => \%TargetAccessToParam,
                    "TypeId" => $TypeId,
                    "Key" => $NewKey,
                    "SpecType" => $SpecTypeId,
                    "Usage" => "OnlySpecType",
                    "ParamName" => $Param_Name,
                    "ParamPos" => $InvOrder{$Param_Pos + 1} - 1));
                $Params_Init{"Init"} .= $Param_Init{"Init"};
                $Params_Init{"Code"} .= $Param_Init{"Code"};
                $Params_Init{"FinalCode"} .= $Param_Init{"FinalCode"};
                $Params_Init{"PreCondition"} .= $Param_Init{"PreCondition"};
                $Params_Init{"PostCondition"} .= $Param_Init{"PostCondition"};
                $Interface_Init{"Headers"} = addHeaders($Param_Init{"Headers"}, $Interface_Init{"Headers"});
            }
            else
            {
                my $CreateChild = ($ShortName eq "operator="
                    and get_TypeName($FTypeId) eq get_TypeName($CompleteSignature{$Interface}{"Class"}) and $CompleteSignature{$Interface}{"Protected"});
                if($IsWrapperCall and $CompleteSignature{$Interface}{"Class"})
                {
                    push(@RecurTypeId, $CompleteSignature{$Interface}{"Class"});
                }
                my %Param_Init = initializeParameter((
                    "Interface" => $Interface,
                    "AccessToParam" => \%TargetAccessToParam,
                    "TypeId" => $TypeId,
                    "Key" => $NewKey,
                    "InLine" => $InLine,
                    "Value" => "no value",
                    "CreateChild" => $CreateChild,
                    "SpecType" => $SpecTypeId,
                    "Usage" => "Common",
                    "ParamName" => $Param_Name,
                    "ParamPos" => $InvOrder{$Param_Pos + 1} - 1,
                    "StrongConvert" => $StrongConvert,
                    "DoNotReuse" => $Init_Desc{"DoNotReuse"},
                    "ParamNameExt" => $Param_Name_Ext,
                    "MaxParamPos" => $MaxParamPos));
                if($IsWrapperCall and $CompleteSignature{$Interface}{"Class"})
                {
                    pop(@RecurTypeId);
                }
                if(not $Param_Init{"IsCorrect"})
                {
                    foreach my $ClassId (keys(%{$SubClasses_Before}))
                    {
                        $Create_SubClass{$ClassId} = 1;
                    }
                    return ();
                }
                my $RetParam = $Init_Desc{"RetParam"};
                if($Param_Init{"ByNull"} and ($Interface ne $TestedInterface)
                and (($CompleteSignature{$Interface}{"ShortName"}=~/(\A|_)\Q$RetParam\E(\Z|_)/i and $Param_Name!~/out|error/i)
                or is_transit_function($CompleteSignature{$Interface}{"ShortName"})))
                {
                    return ();
                }
                if($Param_Init{"ByNull"} and $Param_Init{"InsertCall"})
                {
                    return ();
                }
                $Params_Init{"Init"} .= $Param_Init{"Init"};
                $Params_Init{"Code"} .= $Param_Init{"Code"};
                $Params_Init{"Destructors"} .= $Param_Init{"Destructors"};
                $Params_Init{"FinalCode"} .= $Param_Init{"FinalCode"};
                $Params_Init{"PreCondition"} .= $Param_Init{"PreCondition"};
                $Params_Init{"PostCondition"} .= $Param_Init{"PostCondition"};
                $Interface_Init{"Headers"} = addHeaders($Param_Init{"Headers"}, $Interface_Init{"Headers"});
                $AccessToParam{$InvOrder{$Param_Pos + 1}} = $Param_Init{"Call"};
                $TargetAccessToParam{$InvOrder{$Param_Pos + 1}} = $Param_Init{"TargetCall"};
            }
        }
        foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$CompleteSignature{$Interface}{"Param"}}))
        {
            next if($Param_Pos eq "");
            my $Param_Call = $AccessToParam{$Param_Pos + 1};
            my $ParamType_Id = $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"type"};
            if((get_TypeName($ParamType_Id) ne "..." and not $CompleteSignature{$Interface}{"Param"}{$Param_Pos}{"default"})
                or ($Param_Call ne ""))
            {
                push(@ParamList, $Param_Call);
            }
        }
        my $LastParamPos = keys(%{$CompleteSignature{$Interface}{"Param"}})-1;
        my $LastTypeId = $CompleteSignature{$Interface}{"Param"}{$LastParamPos}{"type"};
        my $LastParamCall = $AccessToParam{$LastParamPos+1};
        if(get_TypeName($LastTypeId) eq "..." and $LastParamCall ne "0" and $LastParamCall ne "NULL")
        {# add sentinel to function call
         # http://www.linuxonly.nl/docs/2/2_GCC_4_warnings_about_sentinels.html
            push(@ParamList, "(char*)0");
        }
        my $Parameters_Call = "(".create_list(\@ParamList, "    ").")";
        if($IsWrapperCall)
        {
            $Interface_Init{"Call"} .= "()";
            $Wrappers{$Interface}{"Init"} = $Params_Init{"Init"};
            $Wrappers{$Interface}{"Code"} = $Params_Init{"Code"};
            $Wrappers{$Interface}{"Destructors"} = $Params_Init{"Destructors"};
            $Wrappers{$Interface}{"FinalCode"} = $Params_Init{"FinalCode"};
            $Wrappers{$Interface}{"PreCondition"} = $Params_Init{"PreCondition"};
            foreach my $PreCondition (keys(%{$Interface_PreCondition{$Interface}}))
            {
                $Wrappers{$Interface}{"PreCondition"} .= constraint_for_interface($Interface, "precondition", $PreCondition);
            }
            $Wrappers{$Interface}{"PostCondition"} = $Params_Init{"PostCondition"};
            foreach my $PostCondition (keys(%{$Interface_PostCondition{$Interface}}))
            {
                $Wrappers{$Interface}{"PostCondition"} .= constraint_for_interface($Interface, "postcondition", $PostCondition);
            }
            $Wrappers{$Interface}{"Parameters_Call"} = $Parameters_Call;
            foreach my $ClassId (keys(%Create_SubClass))
            {
                $Wrappers_SubClasses{$Interface}{$ClassId} = 1;
            }
        }
        else
        {
            $Interface_Init{"Call"} .= $Parameters_Call;
            $Interface_Init{"Init"} .= $Params_Init{"Init"};
            $Interface_Init{"Code"} .= $Params_Init{"Code"};
            $Interface_Init{"Destructors"} .= $Params_Init{"Destructors"};
            $Interface_Init{"FinalCode"} .= $Params_Init{"FinalCode"};
            $Interface_Init{"PreCondition"} .= $Params_Init{"PreCondition"};
            foreach my $PreCondition (keys(%{$Interface_PreCondition{$Interface}}))
            {
                $Interface_Init{"PreCondition"} .= constraint_for_interface($Interface, "precondition", $PreCondition);
            }
            $Interface_Init{"PostCondition"} .= $Params_Init{"PostCondition"};
            foreach my $PostCondition (keys(%{$Interface_PostCondition{$Interface}}))
            {
                $Interface_Init{"PostCondition"} .= constraint_for_interface($Interface, "postcondition", $PostCondition);
            }
        }
    }
    elsif($CompleteSignature{$Interface}{"Data"})
    {
        if($IsWrapperCall)
        {
            $Interface_Init{"Call"} .= "()";
        }
    }
    else
    {
        $Interface_Init{"Call"} .= "()";
        $Wrappers{$Interface}{"Parameters_Call"} = "()";
    }
    if($IsWrapperCall)
    {
        foreach my $ClassId (keys(%{$SubClasses_Before}))
        {
            $Create_SubClass{$ClassId} = 1;
        }
    }
    #check requirement for return value
    my $SpecReturnType = $InterfaceSpecType{$Interface}{"SpecReturn"};
    if(not $SpecReturnType)
    {
        $SpecReturnType = chooseSpecType($CompleteSignature{$Interface}{"Return"}, "common_retval", $Interface);
    }
    $Interface_Init{"ReturnRequirement"} = requirementReturn($Interface, $CompleteSignature{$Interface}{"Return"}, $SpecReturnType, $ObjectCall);
    if($SpecReturnType)
    {
        if($Init_Desc{"GetReturn"}
        and $Interface_Init{"ReturnFinalCode"} = $SpecType{$SpecReturnType}{"FinalCode"})
        {
            if($Interface_Init{"ReturnFinalCode"})
            {
                my $LastId = pop(@RecurTypeId);
                $ValueCollection{$CurrentBlock}{"\$retval"} = $CompleteSignature{$Interface}{"Return"};
                my %ParsedCode = parseCode($Interface_Init{"ReturnFinalCode"}, "Code");
                delete($ValueCollection{$CurrentBlock}{"\$retval"});
                push(@RecurTypeId, $LastId);
                if($ParsedCode{"IsCorrect"})
                {
                    $Interface_Init{"Headers"} = addHeaders($ParsedCode{"Headers"}, $Interface_Init{"Headers"});
                    $Interface_Init{"Code"} .= $ParsedCode{"NewGlobalCode"};
                    $Interface_Init{"ReturnFinalCode"} = $ParsedCode{"Code"};
                }
                else
                {
                    $Interface_Init{"ReturnFinalCode"} = "";
                }
            }
        }
    }
    foreach my $ParamId (keys %AccessToParam)
    {
        if($TargetAccessToParam{$ParamId} and ($TargetAccessToParam{$ParamId} ne "no object"))
        {
            my $AccessValue = $TargetAccessToParam{$ParamId};
            foreach my $Attr (keys(%Interface_Init))
            {
                $Interface_Init{$Attr}=~s/\$\Q$ParamId\E([^0-9]|\Z)/$AccessValue$1/g;
            }
        }
    }
    $Interface_Init{"IsCorrect"} = 1;
    return %Interface_Init;
}

sub parse_param_name($$)
{
    my ($String, $Place) = @_;
    if($String=~/(([a-z_]\w+)[ ]*\(.+\))/i)
    {
        my $Interface_ShortName = $2;
        my $Pos = 0;
        foreach my $Part (get_Signature_Parts($1, 0))
        {
            $Part=~s/(\A\s+|\s+\Z)//g;
            if($Part eq $Place)
            {
                if($CompleteSignature{$Interface_ShortName})
                {
                    return ($CompleteSignature{$Interface_ShortName}{"Param"}{$Pos}{"name"}, $Pos, $Interface_ShortName);
                }
                else
                {
                    return (0, 0, "");
                }
            }
            $Pos+=1;
        }
    }
    return (0, 0, "");
}

sub parseCode_m($$)
{
    my ($Code, $Mode) = @_;
    return ("IsCorrect"=>1) if(not $Code or not $Mode);
    my ($Bracket_Num, $Bracket2_Num, $Code_Inlined, $NotEnded) = (0, 0, "", 0);
    foreach my $Line (split(/\n/, $Code))
    {
        foreach my $Pos (0 .. length($Line) - 1)
        {
            my $Symbol = substr($Line, $Pos, 1);
            $Bracket_Num += 1 if($Symbol eq "(");
            $Bracket_Num -= 1 if($Symbol eq ")");
        }
        if($NotEnded)
        {
            $Line=~s/\A\s+/ /g;
        }
        $Code_Inlined .= $Line;
        if($Bracket_Num==0 and $Bracket2_Num==0)
        {
            $Code_Inlined .= "\n";
        }
        else
        {
            $NotEnded = 1;
        }
    }
    $Code = $Code_Inlined;
    my ($AllSubCode, $ParsedCode, $Headers) = ();
    $Block_InsNum{$CurrentBlock} = 1 if(not defined $Block_InsNum{$CurrentBlock});
    if($Mode eq "Value")
    {
        $Code=~s/\n//g;
    }
    foreach my $String (split(/\n/, $Code))
    {
        if($String=~/\#[ \t]*include[ \t]*\<[ \t]*([^ \t]+)[ \t]*\>/)
        {
            $Headers = addHeaders($Headers, [$1]);
            next;
        }
        my ($CodeBefore, $CodeAfter) = ();
        while($String=~/(\$\(([^\$\(\)]+)\))/)
        {#parsing $(Type) constructions
            my $Replace = $1;
            my $TypeName = $2;
            my $TypeId = get_TypeIdByName($TypeName);
            my $FTypeId = get_FoundationTypeId($TypeId);
            my $NewKey = "_var".$Block_InsNum{$CurrentBlock};
            my ($FuncParamName, $FuncParamPos, $InterfaceShortName) = parse_param_name($String, $Replace);
            if($FuncParamName)
            {
                $NewKey = $FuncParamName;
            }
            my $InLine = 1;
            $InLine = 0 if(uncover_typedefs($TypeName)=~/\&/);
            $InLine = 0 if(get_TypeType($FTypeId)!~/\A(Intrinsic|Enum)\Z/ and $FuncParamName and $FuncParamName!~/\Ap\d+\Z/
                and not isCyclical(\@RecurTypeId, get_TypeStackId($TypeId)));
            my %Param_Init = initializeParameter((
            "AccessToParam" => {"obj"=>"no object"},
            "TypeId" => $TypeId,
            "Key" => $NewKey,
            "InLine" => $InLine,
            "Value" => "no value",
            "CreateChild" => 0,
            "SpecType" => ($FuncParamName and $InterfaceShortName)?$InterfaceSpecType{$InterfaceShortName}{"SpecParam"}{$FuncParamPos}:0,
            "Usage" => "Common",
            "ParamName" => $NewKey,
            "Interface" => $InterfaceShortName));
            return () if(not $Param_Init{"IsCorrect"} or $Param_Init{"ByNull"});
            $Block_InsNum{$CurrentBlock} += 1 if(($Param_Init{"Init"}.$Param_Init{"FinalCode"}.$Param_Init{"Code"})=~/\Q$NewKey\E/);
            $Param_Init{"Init"} = alignCode($Param_Init{"Init"}, $String, 0);
            $Param_Init{"PreCondition"} = alignCode($Param_Init{"PreCondition"}, $String, 0);
            $Param_Init{"PostCondition"} = alignCode($Param_Init{"PostCondition"}, $String, 0);
            $Param_Init{"Call"} = alignCode($Param_Init{"Call"}, $String, 1);
            substr($String, index($String, $Replace), pos($Replace) + length($Replace)) = $Param_Init{"Call"};
            $String = clearSyntax($String);
            $AllSubCode .= $Param_Init{"Code"};
            $Headers = addHeaders($Param_Init{"Headers"}, $Headers);
            $CodeBefore .= $Param_Init{"Init"}.$Param_Init{"PreCondition"};
            $CodeAfter .= $Param_Init{"PostCondition"}.$Param_Init{"FinalCode"};
        }
        while($String=~/(\$\[([^\$\[\]]+)\])/)
        {#parsing $[Interface] constructions
            my $Replace = $1;
            my $InterfaceName = $2;
            my $RetvalName = "";
            if($InterfaceName=~/\A(.+):(\w+?)\Z/)
            {# $[al_create_display:allegro_display]
                ($InterfaceName, $RetvalName) = ($1, $2);
            }
            my $NewKey = "_var".$Block_InsNum{$CurrentBlock};
            my %Interface_Init = ();
            return () if(not $InterfaceName or not $CompleteSignature{$InterfaceName});
            if($CompleteSignature{$InterfaceName}{"Constructor"})
            {
                push(@RecurTypeId, $CompleteSignature{$InterfaceName}{"Class"});
                %Interface_Init = callInterface((
                    "Interface"=>$InterfaceName, 
                    "Key"=>$NewKey));
                pop(@RecurTypeId);
            }
            else
            {
                %Interface_Init = callInterface((
                    "Interface"=>$InterfaceName, 
                    "Key"=>$NewKey));
            }
            return () if(not $Interface_Init{"IsCorrect"});
            $Block_InsNum{$CurrentBlock} += 1 if(($Interface_Init{"Init"}.$Interface_Init{"FinalCode"}.$Interface_Init{"Code"})=~/\Q$NewKey\E/);
            if(($CompleteSignature{$InterfaceName}{"Constructor"}) and (needToInherit($InterfaceName)))
            {#for constructors in abstract classes
                    my $ClassName = get_TypeName($CompleteSignature{$InterfaceName}{"Class"});
                    my $ClassNameChild = getSubClassName($ClassName);
                    if($Interface_Init{"Call"}=~/\A(\Q$ClassName\E([\n]*)\()/)
                    {
                        substr($Interface_Init{"Call"}, index($Interface_Init{"Call"}, $1), pos($1) + length($1)) = $ClassNameChild.$2."(";
                    }
                    $UsedConstructors{$CompleteSignature{$InterfaceName}{"Class"}}{$InterfaceName} = 1;
                    $IntSubClass{$TestedInterface}{$CompleteSignature{$InterfaceName}{"Class"}} = 1;
                    $Create_SubClass{$CompleteSignature{$InterfaceName}{"Class"}} = 1;
            }
            $Interface_Init{"Init"} = alignCode($Interface_Init{"Init"}, $String, 0);
            $Interface_Init{"PreCondition"} = alignCode($Interface_Init{"PreCondition"}, $String, 0);
            $Interface_Init{"PostCondition"} = alignCode($Interface_Init{"PostCondition"}, $String, 0);
            $Interface_Init{"FinalCode"} = alignCode($Interface_Init{"FinalCode"}, $String, 0);
            $Interface_Init{"Call"} = alignCode($Interface_Init{"Call"}, $String, 1);
            if($RetvalName)
            {
                $Block_Variable{$CurrentBlock}{$RetvalName} = 1;
                $ValueCollection{$CurrentBlock}{$RetvalName} = $CompleteSignature{$InterfaceName}{"Return"};
                $UseVarEveryWhere{$CurrentBlock}{$RetvalName} = 1;
                $Interface_Init{"Call"} = get_TypeName($CompleteSignature{$InterfaceName}{"Return"})." $RetvalName = ".$Interface_Init{"Call"};
            }
            substr($String, index($String, $Replace), pos($Replace) + length($Replace)) = $Interface_Init{"Call"};
            $AllSubCode .= $Interface_Init{"Code"};
            $Headers = addHeaders($Interface_Init{"Headers"}, $Headers);
            $CodeBefore .= $Interface_Init{"Init"}.$Interface_Init{"PreCondition"};
            $CodeAfter .= $Interface_Init{"PostCondition"}.$Interface_Init{"FinalCode"};
        }
        $ParsedCode .= $CodeBefore.$String."\n".$CodeAfter;
        if($Mode eq "Value")
        {
            return ("NewGlobalCode" => $AllSubCode,
            "Code" => $String,
            "CodeBefore" => $CodeBefore,
            "CodeAfter" => $CodeAfter,
            "Headers" => $Headers,
            "IsCorrect" => 1);
        }
    }
    return ("NewGlobalCode" => $AllSubCode, "Code" => clearSyntax($ParsedCode), "Headers" => $Headers, "IsCorrect" => 1);
}

sub callInterface_m(@)
{
    my %Init_Desc = @_;
    my ($Interface, $Key) = ($Init_Desc{"Interface"}, $Init_Desc{"Key"});
    my $SpecObjectType = $InterfaceSpecType{$Interface}{"SpecObject"};
    my $SpecReturnType = $InterfaceSpecType{$Interface}{"SpecReturn"};
    my %Interface_Init = ();
    my $ClassName = get_TypeName($CompleteSignature{$Interface}{"Class"});
    my ($CreateChild, $CallAsGlobalData, $MethodToInitObj) = (0, 0, "Common");
    
    if(needToInherit($Interface) and isInCharge($Interface))
    {#inpossible testing
        return ();
    }
    if($CompleteSignature{$Interface}{"Protected"})
    {
        if(not $CompleteSignature{$Interface}{"Constructor"})
        {
            $UsedProtectedMethods{$CompleteSignature{$Interface}{"Class"}}{$Interface} = 1;
        }
        $IntSubClass{$TestedInterface}{$CompleteSignature{$Interface}{"Class"}} = 1;
        $Create_SubClass{$CompleteSignature{$Interface}{"Class"}} = 1;
        $CreateChild = 1;
    }
    if(($CompleteSignature{$Interface}{"Static"}) and (not $CompleteSignature{$Interface}{"Protected"}))
    {
        $MethodToInitObj = "OnlySpecType";
        $CallAsGlobalData = 1;
    }
    if($SpecReturnType and not isCyclical(\@RecurSpecType, $SpecReturnType))
    {
        my $SpecReturnCode = $SpecType{$SpecReturnType}{"Code"};
        if($SpecReturnCode)
        {
            push(@RecurSpecType, $SpecReturnType);
        }
        my $PreviousBlock = $CurrentBlock;
        $CurrentBlock = $CurrentBlock."_code_".$SpecReturnType;
        my %ParsedCode = parseCode($SpecType{$SpecReturnType}{"Code"}, "Code");
        $CurrentBlock = $PreviousBlock;
        if(not $ParsedCode{"IsCorrect"})
        {
            if($SpecReturnCode)
            {
                pop(@RecurSpecType);
            }
            return ();
        }
        $SpecCode{$SpecReturnType} = 1 if($ParsedCode{"Code"});
        $Interface_Init{"Code"} .= $ParsedCode{"NewGlobalCode"}.$ParsedCode{"Code"};
        $Interface_Init{"Headers"} = addHeaders($ParsedCode{"Headers"}, $Interface_Init{"Headers"});
        if($SpecReturnCode)
        {
            pop(@RecurSpecType);
        }
    }
    if($CompleteSignature{$Interface}{"Class"}
    and not $CompleteSignature{$Interface}{"Constructor"})
    {
        #initialize object
        my $ParamName = select_obj_name($Key, $CompleteSignature{$Interface}{"Class"});
        my $NewKey = ($Key)?$Key."_".$ParamName:$ParamName;
        my %Obj_Init = (not $Init_Desc{"ObjectCall"})?initializeParameter((
        "ParamName" => $ParamName,
        "Interface" => $Interface,
        "AccessToParam" => {"obj"=>"create object"},
        "TypeId" => $CompleteSignature{$Interface}{"Class"},
        "Key" => $NewKey,
        "InLine" => 0,
        "Value" => "no value",
        "CreateChild" => $CreateChild,
        "SpecType" => $SpecObjectType,
        "Usage" => $MethodToInitObj,
        "ConvertToBase" => (not $CompleteSignature{$Interface}{"Protected"}),
        "ObjectInit" =>1 )):("IsCorrect"=>1, "Call"=>$Init_Desc{"ObjectCall"});
        return () if(not $Obj_Init{"IsCorrect"});
        $Obj_Init{"Call"} = "no object" if($CallAsGlobalData);
        #initialize parameters
        pop(@RecurInterface);
        $Init_Desc{"ObjectCall"} = $Obj_Init{"Call"} if(not $Init_Desc{"ObjectCall"});
        my %Params_Init = callInterfaceParameters(%Init_Desc);
        push(@RecurInterface, $Interface);
        return () if(not $Params_Init{"IsCorrect"});
        $Interface_Init{"ReturnRequirement"} .= $Params_Init{"ReturnRequirement"};
        $Interface_Init{"ReturnFinalCode"} .= $Params_Init{"ReturnFinalCode"};
        $Interface_Init{"Init"} .= $Obj_Init{"Init"}.$Params_Init{"Init"};
        $Interface_Init{"Destructors"} .= $Params_Init{"Destructors"}.$Obj_Init{"Destructors"};
        $Interface_Init{"Headers"} = addHeaders($Params_Init{"Headers"}, $Interface_Init{"Headers"});
        $Interface_Init{"Headers"} = addHeaders($Obj_Init{"Headers"}, $Interface_Init{"Headers"});
        $Interface_Init{"Code"} .= $Obj_Init{"Code"}.$Params_Init{"Code"};
        $Interface_Init{"PreCondition"} .= $Obj_Init{"PreCondition"}.$Params_Init{"PreCondition"};
        $Interface_Init{"PostCondition"} .= $Obj_Init{"PostCondition"}.$Params_Init{"PostCondition"};
        $Interface_Init{"FinalCode"} .= $Obj_Init{"FinalCode"}.$Params_Init{"FinalCode"};
        #target call
        if($CallAsGlobalData)
        {
            $Interface_Init{"Call"} = $ClassName."::".$Params_Init{"Call"};
        }
        else
        {
            if(($Obj_Init{"Call"}=~/\A\*/) or ($Obj_Init{"Call"}=~/\A\&/))
            {
                $Obj_Init{"Call"} = "(".$Obj_Init{"Call"}.")";
            }
            $Interface_Init{"Call"} = $Obj_Init{"Call"}.".".$Params_Init{"Call"};
            $Interface_Init{"Call"}=~s/\(\*(\w+)\)\./$1\-\>/;
            $Interface_Init{"Call"}=~s/\(\&(\w+)\)\-\>/$1\./;
        }
        #simplify operators
        $Interface_Init{"Call"} = simplifyOperator($Interface_Init{"Call"});
        $Interface_Init{"IsCorrect"} = 1;
        return %Interface_Init;
    }
    else
    {
        pop(@RecurInterface);
        $Init_Desc{"ObjectCall"} = "no object";
        my %Params_Init = callInterfaceParameters(%Init_Desc);
        push(@RecurInterface, $Interface);
        return () if(not $Params_Init{"IsCorrect"});
        $Interface_Init{"ReturnRequirement"} .= $Params_Init{"ReturnRequirement"};
        $Interface_Init{"ReturnFinalCode"} .= $Params_Init{"ReturnFinalCode"};
        $Interface_Init{"Init"} .= $Params_Init{"Init"};
        $Interface_Init{"Destructors"} .= $Params_Init{"Destructors"};
        $Interface_Init{"Headers"} = addHeaders($Params_Init{"Headers"}, $Interface_Init{"Headers"});
        $Interface_Init{"Code"} .= $Params_Init{"Code"};
        $Interface_Init{"PreCondition"} .= $Params_Init{"PreCondition"};
        $Interface_Init{"PostCondition"} .= $Params_Init{"PostCondition"};
        $Interface_Init{"FinalCode"} .= $Params_Init{"FinalCode"};
        $Interface_Init{"Call"} = $Params_Init{"Call"};
        if($CompleteSignature{$Interface}{"NameSpace"} and not $CompleteSignature{$Interface}{"Class"})
        {
            $Interface_Init{"Call"} = $CompleteSignature{$Interface}{"NameSpace"}."::".$Interface_Init{"Call"};
        }
        $Interface_Init{"IsCorrect"} = 1;
        return %Interface_Init;
    }
}

sub simplifyOperator($)
{
    my $String = $_[0];
    if($String!~/\.operator/)
    {
        return $String;
    }
    return $String if($String!~/(.*)\.operator[ ]*([^()]+)\((.*)\)/);
    my $Target = $1;
    my $Operator = $2;
    my $Params = $3;
    if($Params eq "")
    {
        #prefix operator
        if($Operator=~/[a-z]/i)
        {
            return $String;
        }
        else
        {
            return $Operator.$Target;
        }
    }
    else
    {
        #postfix operator
        if($Params!~/\,/)
        {
            $Params = "" if(($Operator eq "++") or ($Operator eq "--"));
            if($Operator eq "[]")
            {
                return $Target."[$Params]";
            }
            else
            {
                return $Target.$Operator."$Params";
            }
        }
        else
        {
            return $Target.$Operator."($Params)";
        }
    }
}

sub callInterface(@)
{
    my %Init_Desc = @_;
    my $Interface = $Init_Desc{"Interface"};
    return () if(not $Interface);
    return () if($SkipInterfaces{$Interface});
    foreach my $SkipPattern (keys(%SkipInterfaces_Pattern))
    {
        return () if($Interface=~/$SkipPattern/);
    }
    if(defined $MakeIsolated and $Interface_Library{$Interface}
    and keys(%InterfacesList) and not $InterfacesList{$Interface})
    {
        return ();
    }
    my $Global_State = save_state();
    return () if(isCyclical(\@RecurInterface, $Interface));
    push(@RecurInterface, $Interface);
    $UsedInterfaces{$Interface} = 1;
    my %Interface_Init = callInterface_m(%Init_Desc);
    if(not $Interface_Init{"IsCorrect"})
    {
        pop(@RecurInterface);
        restore_state($Global_State);
        return ();
    }
    pop(@RecurInterface);
    $Interface_Init{"ReturnTypeId"} = $CompleteSignature{$Interface}{"Return"};
    return %Interface_Init;
}

sub get_REQ_define($)
{
    my $Interface = $_[0];
    my $Code = "#define REQ(id, failure_comment, constraint) { \\\n";
    $Code .= "    if(!(constraint)) { \\\n";
    $Code .= "        printf(\"\%s: \%s\\n\", id, failure_comment); \\\n    } \\\n";
    $Code .= "}\n";
    $FuncNames{"REQ"} = 1;
    $Block_Variable{"REQ"}{"id"} = 1;
    $Block_Variable{"REQ"}{"failure_comment"} = 1;
    $Block_Variable{"REQ"}{"constraint"} = 1;
    return $Code;
}

sub get_REQva_define($)
{
    my $Interface = $_[0];
    my $Code = "#define REQva(id, constraint, failure_comment_fmt, ...) { \\\n";
    $Code .= "    if(!(constraint)) { \\\n";
    $Code .= "        printf(\"\%s: \"failure_comment_fmt\"\\n\", id, __VA_ARGS__); \\\n    } \\\n";
    $Code .= "}\n";
    $FuncNames{"REQva"} = 1;
    $Block_Variable{"REQva"}{"id"} = 1;
    $Block_Variable{"REQva"}{"failure_comment"} = 1;
    $Block_Variable{"REQva"}{"constraint"} = 1;
    return $Code;
}

sub add_line_numbers($$)
{
    my ($Code, $AdditionalLines) = @_;
    my @Lines = split(/\n/, $Code);
    my $NewCode_LineNumbers = "\@LT\@table\@SP\@class='line_numbers'\@SP\@border='0'\@SP\@cellpadding='0'\@SP\@cellspacing='0'\@GT\@";
    my $NewCode_Lines = "\@LT\@table\@SP\@class='code_lines'\@SP\@border='0'\@SP\@cellpadding='0'\@SP\@cellspacing='0'\@GT\@";
    my $NewCode = "\@LT\@table\@SP\@border='0'\@SP\@cellpadding='0'\@SP\@cellspacing='0'\@GT\@";
    my $MaxLineNum = 0;
    foreach my $LineNum (0 .. $#Lines)
    {
        my $Line = $Lines[$LineNum];
        $Line = "    " if(not $Line);
        $NewCode_LineNumbers .= "\@LT\@tr\@GT\@\@LT\@td\@GT\@".($LineNum+1)."\@LT\@/td\@GT\@\@LT\@/tr\@GT\@";
        $NewCode_Lines .= "\@LT\@tr\@GT\@\@LT\@td\@GT\@$Line\@LT\@/td\@GT\@\@LT\@/tr\@GT\@";
        $MaxLineNum = $LineNum;
    }
    foreach my $LineNum (1 .. $AdditionalLines)
    {
        $NewCode_LineNumbers .= "\@LT\@tr\@GT\@\@LT\@td\@GT\@".($MaxLineNum+$LineNum+1)."\@LT\@/td\@GT\@\@LT\@/tr\@GT\@";
    }
    $NewCode_LineNumbers .= "\@LT\@/table\@GT\@";
    $NewCode_Lines .= "\@LT\@/table\@GT\@";
    $NewCode .= "\@LT\@tr\@GT\@\@LT\@td\@SP\@valign='top'\@GT\@$NewCode_LineNumbers\@LT\@/td\@GT\@\@LT\@td\@SP\@valign='top'\@GT\@$NewCode_Lines\@LT\@/td\@GT\@\@LT\@/tr\@GT\@\@LT\@/table\@GT\@";
    return $NewCode;
}

sub parse_variables($)
{
    my $Code = $_[0];
    return () if(not $Code);
    my $Code_Copy = $Code;
    my (%Variables, %LocalFuncNames, %LocalMethodNames) = ();
    while($Code=~s/([a-z_]\w*)[ ]*\([^;{}]*\)[ \n]*\{//io)
    {
        $LocalFuncNames{$1} = 1;
    }
    $Code = $Code_Copy;
    while($Code=~s/\:\:([a-z_]\w*)[ ]*\([^;{}]*\)[ \n]*\{//io)
    {
        $LocalMethodNames{$1} = 1;
    }
    foreach my $Block (sort keys(%Block_Variable))
    {
        foreach my $Variable (sort {length($b)<=>length($a)} keys(%{$Block_Variable{$Block}}))
        {
            next if(not $Variable);
            if($Code_Copy=~/\W$Variable[ ]*(,|(\n[ ]*|)\))/)
            {
                $Variables{$Variable}=1;
            }
            else
            {
                next if(is_not_variable($Variable, $Code_Copy));
                next if($LocalFuncNames{$Variable} and ($Code_Copy=~/\W\Q$Variable\E[ ]*\(/ or $Code_Copy=~/\&\Q$Variable\E\W/));
                next if($LocalMethodNames{$Variable} and $Code_Copy=~/\W\Q$Variable\E[ ]*\(/);
                $Variables{$Variable}=1;
            }
        }
    }
    while($Code=~s/[ ]+([a-z_]\w*)([ ]*=|;)//io)
    {
        my $Variable = $1;
        next if(is_not_variable($Variable, $Code_Copy));
        next if($LocalFuncNames{$Variable} and ($Code_Copy=~/\W\Q$Variable\E[ ]*\(/ or $Code_Copy=~/\&\Q$Variable\E\W/));
        next if($LocalMethodNames{$Variable} and $Code_Copy=~/\W\Q$Variable\E[ ]*\(/);
        $Variables{$Variable}=1;
    }
    while($Code=~s/(\(|,)[ ]*([a-z_]\w*)[ ]*(\)|,)//io)
    {
        my $Variable = $2;
        next if(is_not_variable($Variable, $Code_Copy));
        next if($LocalFuncNames{$Variable} and ($Code_Copy=~/\W\Q$Variable\E[ ]*\(/ or $Code_Copy=~/\&\Q$Variable\E\W/));
        next if($LocalMethodNames{$Variable} and $Code_Copy=~/\W\Q$Variable\E[ ]*\(/);
        $Variables{$Variable}=1;
    }
    my @Variables = keys(%Variables);
    return @Variables;
}

sub is_not_variable($$)
{
    my ($Variable, $Code) = @_;
    return 1 if($Variable=~/\A[A-Z_]+\Z/);
    # FIXME: more appropriate constants check
    return 1 if($TName_Tid{$Variable});
    return 1 if($EnumMembers{$Variable});
    return 1 if($NameSpaces{$Variable}
    and ($Code=~/\W\Q$Variable\E\:\:/ or $Code=~/\s+namespace\s+\Q$Variable\E\s*;/));
    return 1 if($IsKeyword{$Variable} or $Variable=~/\A(\d+)\Z|_SubClass/);
    return 1 if($Constants{$Variable});
    return 1 if($GlobVarNames{$Variable});
    return 1 if($FuncNames{$Variable} and ($Code=~/\W\Q$Variable\E[ ]*\(/ or $Code=~/\&\Q$Variable\E\W/));
    return 1 if($MethodNames{$Variable} and $Code=~/\W\Q$Variable\E[ ]*\(/);
    return 1 if($Code=~/(\-\>|\.|\:\:)\Q$Variable\E[ ]*\(/);
    return 1 if($AllDefines{$Variable});
    return 0;
}

sub highlight_code($$)
{
    my ($Code, $Interface) = @_;
    my $Signature = get_Signature($Interface);
    my %Preprocessor = ();
    my $PreprocessorNum = 1;
    my @Lines = split(/\n/, $Code);
    foreach my $LineNum (0 .. $#Lines)
    {
        my $Line = $Lines[$LineNum];
        if($Line=~/\A[ \t]*(#.+)\Z/)
        {
            my $LineNum_Define = $LineNum;
            my $Define = $1;
            while($Define=~/\\[ \t]*\Z/)
            {
                $LineNum_Define+=1;
                $Define .= "\n".$Lines[$LineNum_Define];
            }
            if($Code=~s/\Q$Define\E/\@PREPROC_$PreprocessorNum\@/)
            {
                $Preprocessor{$PreprocessorNum} = $Define;
                $PreprocessorNum+=1;
            }
        }
    }
    my %Strings_DQ = ();
    my $StrNum_DQ = 1;
    while($Code=~s/((L|)"[^"]*")/\@STR_DQ_$StrNum_DQ\@/)
    {
        $Strings_DQ{$StrNum_DQ} = $1;
        $StrNum_DQ += 1;
    }
    my %Strings = ();
    my $StrNum = 1;
    while($Code=~s/((?<=\W)(L|)'[^']*')/\@STR_$StrNum\@/)
    {
        $Strings{$StrNum} = $1;
        $StrNum += 1;
    }
    my %Comments = ();
    my $CommentNum = 1;
    while($Code=~s/([^:]|\A)(\/\/[^\n]*)\n/$1\@COMMENT_$CommentNum\@\n/)
    {
        $Comments{$CommentNum} = $2;
        $CommentNum += 1;
    }
    if(my $ShortName = ($CompleteSignature{$Interface}{"Constructor"})?get_TypeName($CompleteSignature{$Interface}{"Class"}):$CompleteSignature{$Interface}{"ShortName"})
    {# target interface
        if($CompleteSignature{$Interface}{"Class"})
        {
            while($ShortName=~s/\A\w+\:\://g){ };
            if($CompleteSignature{$Interface}{"Constructor"})
            {
                $Code=~s!(\:| new |\n    )(\Q$ShortName\E)([ \n]*\()!$1\@LT\@span\@SP\@class='targ'\@GT\@$2\@LT\@/span\@GT\@$3!g;
            }
            elsif($CompleteSignature{$Interface}{"Destructor"})
            {
                $Code=~s!(\n    )(delete)([ \n]*\()!$1\@LT\@span\@SP\@class='targ'\@GT\@$2\@LT\@/span\@GT\@$3!g;
            }
            else
            {
                $Code=~s!(\-\>|\.|\:\:| new )(\Q$ShortName\E)([ \n]*\()!$1\@LT\@span\@SP\@class='targ'\@GT\@$2\@LT\@/span\@GT\@$3!g;
            }
        }
        else
        {
            $Code=~s!( )(\Q$ShortName\E)([ \n]*\()!$1\@LT\@span\@SP\@class='targ'\@GT\@$2\@LT\@/span\@GT\@$3!g;
        }
    }
    my %Variables = ();
    foreach my $Variable (parse_variables($Code))
    {
        if($Code=~s#(?<=[^\w\n.:])($Variable)(?=\W)#\@LT\@span\@SP\@class='var'\@GT\@$1\@LT\@/span\@GT\@#g)
        {
            $Variables{$Variable}=1;
        }
    }
    $Code=~s!(?<=[^.\w])(bool|_Bool|_Complex|complex|void|const|int|long|short|float|double|volatile|restrict|char|unsigned|signed)(?=[^\w\=])!\@LT\@span\@SP\@class='type'\@GT\@$1\@LT\@/span\@GT\@!g;
    $Code=~s!(?<=[^.\w])(false|true|namespace|return|struct|enum|union|public|protected|private|delete|typedef)(?=[^\w\=])!\@LT\@span\@SP\@class='keyw'\@GT\@$1\@LT\@/span\@GT\@!g;
    if(not $Variables{"class"})
    {
        $Code=~s!(?<=[^.\w])(class)(?=[^\w\=])!\@LT\@span\@SP\@class='keyw'\@GT\@$1\@LT\@/span\@GT\@!g;
    }
    if(not $Variables{"new"})
    {
        $Code=~s!(?<=[^.\w])(new)(?=[^\w\=])!\@LT\@span\@SP\@class='keyw'\@GT\@$1\@LT\@/span\@GT\@!g;
    }
    $Code=~s!(?<=[^.\w])(for|if|else if)([ \n]*\()(?=[^\w\=])!\@LT\@span\@SP\@class='keyw'\@GT\@$1\@LT\@/span\@GT\@$2!g;
    $Code=~s!(?<=[^.\w])else([ \n\{]+)(?=[^\w\=])!\@LT\@span\@SP\@class='keyw'\@GT\@else\@LT\@/span\@GT\@$1!g;
    $Code=~s!(?<=[^\w\@\$])(\d+(f|L|LL|)|NULL)(?=[^\w\@\$])!\@LT\@span\@SP\@class='num'\@GT\@$1\@LT\@/span\@GT\@!g;
    foreach my $Num (keys(%Comments))
    {
        my $String = $Comments{$Num};
        $Code=~s!\@COMMENT_$Num\@!\@LT\@span\@SP\@class='comm'\@GT\@$String\@LT\@/span\@GT\@!g;
    }
    my $AdditionalLines = 0;
    foreach my $Num (keys(%Preprocessor))
    {
        my $Define = $Preprocessor{$Num};
        while($Define=~s/\n//)
        {
            $AdditionalLines+=1;
        }
    }
    $Code = add_line_numbers($Code, $AdditionalLines);
    #$Code=~s/\n/\@LT\@br\/\@GT\@\n/g;
    foreach my $Num (keys(%Preprocessor))
    {
        my $Define = $Preprocessor{$Num};
        $Code=~s!\@PREPROC_$Num\@!\@LT\@span\@SP\@class='prepr'\@GT\@$Define\@LT\@/span\@GT\@!g;
    }
    foreach my $Num (keys(%Strings_DQ))
    {
        my $String = $Strings_DQ{$Num};
        $Code=~s!\@STR_DQ_$Num\@!\@LT\@span\@SP\@class='str'\@GT\@$String\@LT\@/span\@GT\@!g;
    }
    foreach my $Num (keys(%Strings))
    {
        my $String = $Strings{$Num};
        $Code=~s!\@STR_$Num\@!\@LT\@span\@SP\@class='str'\@GT\@$String\@LT\@/span\@GT\@!g;
    }
    $Code =~ s!\[\]![\@LT\@span\@SP\@style='padding-left:2px;'\@GT\@]\@LT\@/span\@GT\@!g;
    $Code =~ s!\(\)!(\@LT\@span\@SP\@style='padding-left:2px;'\@GT\@)\@LT\@/span\@GT\@!g;
    return $Code;
}

sub is_process_running($) {
    my ($PID, $procname) = @_;
    if (!-e "/proc/$PID") {
        return 0;
    }
    open(FILE, "/proc/$PID/stat") or return 0;
    my $info = <FILE>;
    close(FILE);
    if ($info=~/^\d+\s+\((.*)\)\s+(\S)\s+[^\(\)]+$/) {
        return ($2 ne 'Z');
    }
    else {
        return 0;
    }
}

sub kill_all_childs($)
{
    my $root_pid = $_[0];
    return if(not $root_pid);
    # Build the list of processes to be killed.
    # Sub-tree of this particular process is excluded so that it could finish its work.
    my %children = ();
    my %parent = ();
    # Read list of all currently running processes
    return if(!opendir(PROC_DIR, "/proc"));
    my @all_pids = grep(/^\d+$/, readdir(PROC_DIR));
    closedir(PROC_DIR);
    # Build the parent-child tree and get command lines
    foreach my $pid (@all_pids) {
        if (open(PID_FILE, "/proc/$pid/stat")) {
            my $info = <PID_FILE>;
            close(PID_FILE);
            if ($info=~/^\d+\s+\((.*)\)\s+\S\s+(\d+)\s+[^\(\)]+$/) {
                my $ppid = $2;
                $parent{$pid} = $ppid;
                if (!defined($children{$ppid})) {
                    $children{$ppid} = [];
                }
                push @{$children{$ppid}}, $pid;
            }
        }
    }
    # Get the plain list of processes to kill (breadth-first tree-walk)
    my @kill_list = ($root_pid);
    for (my $i = 0; $i < scalar(@kill_list); ++$i) {
        my $pid = $kill_list[$i];
        if ($children{$pid}) {
            foreach (@{$children{$pid}}) {
                push @kill_list, $_;
            }
        }
    }
    # Send TERM signal to all processes
    foreach (@kill_list) {
        kill("SIGTERM", $_);
    }
    # Try 20 times, waiting 0.3 seconds each time, for all the processes to be really dead.
    my %death_check = map { $_ => 1 } @kill_list;
    for (my $i = 0; $i < 20; ++$i) {
        foreach (keys %death_check) {
            if (!is_process_running($_)) {
                delete $death_check{$_};
            }
        }
        if (scalar(keys %death_check) == 0) {
            last;
        }
        else {
            select(undef, undef, undef, 0.3);
        }
    }
}

sub filt_output($)
{
    my $Output = $_[0];
    return $Output if(not keys(%SkipWarnings) and not keys(%SkipWarnings_Pattern));
    my @NewOutput = ();
    foreach my $Line (split(/\n/, $Output))
    {
        my $IsMatched = 0;
        foreach my $Warning (keys(%SkipWarnings))
        {
            if($Line=~/\Q$Warning\E/)
            {
                $IsMatched = 1;
            }
        }
        foreach my $Warning (keys(%SkipWarnings_Pattern))
        {
            if($Line=~/$Warning/)
            {
                $IsMatched = 1;
            }
        }
        if(not $IsMatched)
        {
            push(@NewOutput, $Line);
        }
    }
    my $FinalOut = join("\n", @NewOutput);
    $FinalOut=~s/\A[\n]+//g;
    return $FinalOut;
}

sub run_sanity_test($)
{
    my $Interface = $_[0];
    my $TestDir = $Interface_TestDir{$Interface};
    if(not $TestDir)
    {
        $ResultCounter{"Run"}{"Fail"} += 1;
        $RunResult{$Interface}{"IsCorrect"} = 0;
        $RunResult{$Interface}{"TestNotExists"} = 1;
        if($TargetInterfaceName)
        {
            print "fail\n";
            print STDERR "ERROR: test was not generated yet\n"
        }
        return 1;
    }
    elsif(not -f $TestDir."/test")
    {
        $ResultCounter{"Run"}{"Fail"} += 1;
        $RunResult{$Interface}{"IsCorrect"} = 0;
        $RunResult{$Interface}{"TestNotExists"} = 1;
        if($TargetInterfaceName)
        {
            print "fail\n";
            print STDERR "ERROR: test was not built yet\n"
        }
        return 1;
    }
    unlink($TestDir."/result");
    my $pid = fork();
    unless($pid)
    {
        open(STDIN,"/dev/null");
        open(STDOUT,"/dev/null");
        open(STDERR,"/dev/null");
        setsid();#for the removing signals printing on the screen of terminal
        system("cd ".esc($TestDir)." && sh run_test.sh 2>stderr");
        writeFile("$TestDir/result", $?.";".$!);
        exit(0);
    }
    my $Hang = 0;
    $SIG{ALRM} = sub {
        $Hang=1;
        kill_all_childs($pid);
    };
    alarm $HANGED_EXECUTION_TIME;
    waitpid($pid, 0);
    alarm 0;
    my $Result = readFile("$TestDir/result");
    unlink($TestDir."/result");
    unlink("$TestDir/output") if(not readFile("$TestDir/output"));
    unlink("$TestDir/stderr") if(not readFile("$TestDir/stderr"));
    $Result=~/(.*);(.*)/;
    my ($R_1, $R_2) = ($1, $2);#checking error codes
    my $ErrorOut = readFile("$TestDir/output");#checking test output
    $ErrorOut = filt_output($ErrorOut);
    if($ErrorOut)
    {#reducing length of the test output
        if(length($ErrorOut)>1200)
        {
            $ErrorOut = substr($ErrorOut, 0, 1200)." ...";
        }
    }
    if($Hang)
    {
        $ResultCounter{"Run"}{"Fail"} += 1;
        $RunResult{$Interface}{"IsCorrect"} = 0;
        $RunResult{$Interface}{"Type"} = "Hanged_Execution";
        $RunResult{$Interface}{"Info"} = "hanged execution (more than $HANGED_EXECUTION_TIME seconds)";
        $RunResult{$Interface}{"Info"} .= "\n".$ErrorOut if($ErrorOut);
    }
    elsif($R_1)
    {
        if ($R_1 == -1) {
            $RunResult{$Interface}{"Info"} = "failed to execute: $R_2\n";
            $RunResult{$Interface}{"Type"} = "Other_Problems";
        }
        elsif ($R_1 & 127) {
            my $Signal_Num = ($R_1 & 127);
            my $Signal_Name = $SigName{$Signal_Num};
            $RunResult{$Interface}{"Info"} = "received signal $Signal_Name, ".(($R_1 & 128)?"with":"without")." coredump\n";
            $RunResult{$Interface}{"Type"} = "Received_Signal";
            $RunResult{$Interface}{"Value"} = ($R_1 & 127);
        }
        else {
            my $Signal_Num = ($R_1 >> 8)-128;
            my $Signal_Name = $SigName{$Signal_Num};
            if($Signal_Name)
            {
                $RunResult{$Interface}{"Info"} = "received signal $Signal_Name\n";
                $RunResult{$Interface}{"Type"} = "Received_Signal";
                $RunResult{$Interface}{"Value"} = $Signal_Name;
            }
            else
            {
                $RunResult{$Interface}{"Info"} = "exited with value ".($R_1 >> 8)."\n";
                $RunResult{$Interface}{"Type"} = "Exited_With_Value";
                $RunResult{$Interface}{"Value"} = ($R_1 >> 8);
            }
        }
        $ResultCounter{"Run"}{"Fail"} += 1;
        $RunResult{$Interface}{"IsCorrect"} = 0;
        $RunResult{$Interface}{"Info"} .= "\n".$ErrorOut if($ErrorOut);
    }
    elsif(readFile($TestDir."/output")=~/(constraint|postcondition|precondition) for the (return value|object|environment|parameter) failed/i)
    {
        $ResultCounter{"Run"}{"Fail"} += 1;
        $RunResult{$Interface}{"IsCorrect"} = 0;
        $RunResult{$Interface}{"Type"} = "Requirement_Failed";
        $RunResult{$Interface}{"Info"} .= "\n".$ErrorOut if($ErrorOut);
    }
    elsif($ErrorOut)
    {
        $ResultCounter{"Run"}{"Fail"} += 1;
        $RunResult{$Interface}{"Unexpected_Output"} = $ErrorOut;
        $RunResult{$Interface}{"Type"} = "Unexpected_Output";
        $RunResult{$Interface}{"Info"} = $ErrorOut;
    }
    else
    {
        $ResultCounter{"Run"}{"Success"} += 1;
        $RunResult{$Interface}{"IsCorrect"} = 1;
    }
    if(not $RunResult{$Interface}{"IsCorrect"})
    {
        return 0 if(not -e $TestDir."/test.c" and not -e $TestDir."/test.cpp");
        my $ReadingStarted = 0;
        foreach my $Line (split(/\n/, readFile($TestDir."/view.html")))
        {
            if($ReadingStarted)
            {
                $RunResult{$Interface}{"Test"} .= $Line;
            }
            if($Line eq "<!--Test-->")
            {
                $ReadingStarted = 1;
            }
            if($Line eq "<!--Test_End-->")
            {
                last;
            }
        }
        my $Test_Info = readFile($TestDir."/info");
        foreach my $Str (split(/\n/, $Test_Info))
        {
            if($Str=~/\A[ ]*([^:]*?)[ ]*\:[ ]*(.*)[ ]*\Z/i)
            {
                my ($Attr, $Value) = ($1, $2);
                if(lc($Attr) eq "header")
                {
                    $RunResult{$Interface}{"Header"} = $Value;
                }
                elsif(lc($Attr) eq "shared object")
                {
                    $RunResult{$Interface}{"SharedObject"} = $Value;
                }
                elsif(lc($Attr) eq "interface")
                {
                    $RunResult{$Interface}{"Signature"} = $Value;
                }
                elsif(lc($Attr) eq "short name")
                {
                    $RunResult{$Interface}{"ShortName"} = $Value;
                }
                elsif(lc($Attr) eq "namespace")
                {
                    $RunResult{$Interface}{"NameSpace"} = $Value;
                }
            }
        }
        $RunResult{$Interface}{"ShortName"} = $Interface if(not $RunResult{$Interface}{"ShortName"});
        # filtering problems
        if($RunResult{$Interface}{"Type"} eq "Exited_With_Value")
        {
            if($RunResult{$Interface}{"ShortName"}=~/exit|die|assert/i)
            {
                skip_problem($Interface);
            }
            else
            {
                mark_as_warning($Interface);
            }
        }
        elsif($RunResult{$Interface}{"Type"} eq "Hanged_Execution")
        {
            if($RunResult{$Interface}{"ShortName"}=~/call|exec|acquire|start|run|loop|blocking|startblock|wait|time|show/i
            or ($Interface=~/internal|private/ and $RunResult{$Interface}{"ShortName"}!~/private(.*)key/i))
            {
                mark_as_warning($Interface);
            }
        }
        elsif($RunResult{$Interface}{"Type"} eq "Received_Signal")
        {
            if($RunResult{$Interface}{"ShortName"}=~/badalloc|bad_alloc|fatal|assert/i)
            {
                skip_problem($Interface);
            }
            elsif($Interface=~/internal|private/ and $RunResult{$Interface}{"ShortName"}!~/private(.*)key/i)
            {
                mark_as_warning($Interface);
            }
            elsif($RunResult{$Interface}{"Value"}!~/\A(SEGV|FPE|BUS|ILL|PIPE|SYS|XCPU|XFSZ)\Z/)
            {
                mark_as_warning($Interface);
            }
        }
        elsif($RunResult{$Interface}{"Type"} eq "Unexpected_Output")
        {
            if($Interface=~/print|debug|warn|message|error|fatal/i)
            {
                skip_problem($Interface);
            }
            else
            {
                mark_as_warning($Interface);
            }
        }
        elsif($RunResult{$Interface}{"Type"} eq "Other_Problems")
        {
            mark_as_warning($Interface);
        }
    }
    return 0;
}

sub mark_as_warning($)
{
    my $Interface = $_[0];
    $RunResult{$Interface}{"Warnings"} = 1;
    $ResultCounter{"Run"}{"Warnings"} += 1;
    $ResultCounter{"Run"}{"Fail"} -= 1;
    $ResultCounter{"Run"}{"Success"} += 1;
    $RunResult{$Interface}{"IsCorrect"} = 1;
}

sub skip_problem($)
{
    my $Interface = $_[0];
    $ResultCounter{"Run"}{"Fail"} -= 1;
    $ResultCounter{"Run"}{"Success"} += 1;
    delete($RunResult{$Interface});
    $RunResult{$Interface}{"IsCorrect"} = 1;
}

sub print_highlight($$)
{
    my ($Str, $Part) = @_;
    $ENV{"GREP_COLOR"}="01;36";
    system("echo \"$Str\"|grep \"$Part\" --color");
}

sub read_scenario()
{
    foreach my $TestCase (split(/\n/, readFile($TEST_SUITE_PATH."/scenario")))
    {
        if($TestCase=~/\A(.*);(.*)\Z/)
        {
            $Interface_TestDir{$1} = $2;
        }
    }
}

sub write_scenario()
{
    my $TestCases = "";
    foreach my $Interface (sort {lc($a) cmp lc($b)} keys(%Interface_TestDir))
    {
        $TestCases .= $Interface.";".$Interface_TestDir{$Interface}."\n";
    }
    writeFile("$TEST_SUITE_PATH/scenario", $TestCases);
}

sub build_sanity_test($)
{
    my $Interface = $_[0];
    my $TestDir = $Interface_TestDir{$Interface};
    if(not $TestDir or not -f "$TestDir/Makefile")
    {
        $BuildResult{$Interface}{"TestNotExists"} = 1;
        if($TargetInterfaceName)
        {
            print "fail\n";
            print STDERR "ERROR: test was not generated yet\n"
        }
        return 0;
    }
    system("cd ".esc($TestDir)." && make clean -f Makefile 1>/dev/null && make -f Makefile 2>build_errors 1>/dev/null");
    if($?)
    {
        $ResultCounter{"Build"}{"Fail"} += 1;
        $BuildResult{$Interface}{"IsCorrect"} = 0;
    }
    else
    {
        $ResultCounter{"Build"}{"Success"} += 1;
        $BuildResult{$Interface}{"IsCorrect"} = 1;
    }
    if(-f "$TestDir/test.o")
    {
        unlink("$TestDir/test.o");
    }
    if(not readFile("$TestDir/build_errors"))
    {
        unlink("$TestDir/build_errors");
    }
    elsif($BuildResult{$Interface}{"IsCorrect"})
    {
        $BuildResult{$Interface}{"Warnings"} = 1;
    }
}

sub clean_sanity_test($)
{
    my $Interface = $_[0];
    my $TestDir = $Interface_TestDir{$Interface};
    if(not $TestDir or not -f "$TestDir/Makefile")
    {
        $BuildResult{$Interface}{"TestNotExists"} = 1;
        if($TargetInterfaceName)
        {
            print "fail\n";
            print STDERR "ERROR: test was not generated yet\n";
        }
        return 0;
    }
    unlink("$TestDir/test.o");
    unlink("$TestDir/test");
    unlink("$TestDir/build_errors");
    unlink("$TestDir/output");
    unlink("$TestDir/stderr");
    rmtree("$TestDir/testdata");
    if($CleanSources)
    {
        foreach my $Path (split(/\n/, `find $TestDir -type f`))
        {
            if(get_FileName($Path) ne "view.html")
            {
                unlink($Path);
            }
        }
    }
    return 1;
}

sub testForDestructor($)
{
    my $Interface = $_[0];
    my $ClassId = $CompleteSignature{$Interface}{"Class"};
    my $ClassName = get_TypeName($ClassId);
    my %Interface_Init = ();
    my $Var = select_obj_name("", $ClassId);
    $Block_Variable{$CurrentBlock}{$Var} = 1;
    if($Interface=~/D2/)
    {
        #push(@RecurTypeId, $ClassId);
        my %Obj_Init = findConstructor($ClassId, "");
        #pop(@RecurTypeId);
        return () if(not $Obj_Init{"IsCorrect"});
        my $ClassNameChild = getSubClassName($ClassName);
        if($Obj_Init{"Call"}=~/\A(\Q$ClassName\E([\n]*)\()/)
        {
            substr($Obj_Init{"Call"}, index($Obj_Init{"Call"}, $1), pos($1) + length($1)) = $ClassNameChild.$2."(";
        }
        $ClassName = $ClassNameChild;
        $UsedConstructors{$ClassId}{$Obj_Init{"Interface"}} = 1;
        $IntSubClass{$TestedInterface}{$ClassId} = 1;
        $Create_SubClass{$ClassId} = 1;
        $Interface_Init{"Init"} .= $Obj_Init{"Init"};
        #$Interface_Init{"Init"} .= "//parameter initialization\n";
        if($Obj_Init{"PreCondition"})
        {
            $Interface_Init{"Init"} .= $Obj_Init{"PreCondition"};
        }
        $Interface_Init{"Init"} .= "$ClassName *$Var = new ".$Obj_Init{"Call"}.";\n";
        if($Obj_Init{"PostCondition"})
        {
            $Interface_Init{"Init"} .= $Obj_Init{"PostCondition"};
        }
        if($Obj_Init{"ReturnRequirement"})
        {
            $Obj_Init{"ReturnRequirement"}=~s/(\$0|\$obj)/*$Var/gi;
            $Interface_Init{"Init"} .= $Obj_Init{"ReturnRequirement"};
        }
        if($Obj_Init{"FinalCode"})
        {
            $Interface_Init{"Init"} .= "//final code\n";
            $Interface_Init{"Init"} .= $Obj_Init{"FinalCode"}."\n";
        }
        $Interface_Init{"Headers"} = addHeaders($Obj_Init{"Headers"}, $Interface_Init{"Headers"});
        $Interface_Init{"Code"} .= $Obj_Init{"Code"};
        $Interface_Init{"Call"} = "delete($Var)";
    }
    elsif($Interface=~/D0/)
    {
        if(isAbstractClass($ClassId))
        {#Impossible to call in-charge-deleting (D0) destructor in abstract class
            return ();
        }
        if($CompleteSignature{$Interface}{"Protected"})
        {#Impossible to call protected in-charge-deleting (D0) destructor
            return ();
        }
        #push(@RecurTypeId, $ClassId);
        my %Obj_Init = findConstructor($ClassId, "");
        #pop(@RecurTypeId);
        return () if(not $Obj_Init{"IsCorrect"});
        if($CompleteSignature{$Obj_Init{"Interface"}}{"Protected"})
        {#Impossible to call in-charge-deleting (D0) destructor in class with protected constructor
            return ();
        }
        $Interface_Init{"Init"} .= $Obj_Init{"Init"};
        if($Obj_Init{"PreCondition"})
        {
            $Interface_Init{"Init"} .= $Obj_Init{"PreCondition"};
        }
        #$Interface_Init{"Init"} .= "//parameter initialization\n";
        $Interface_Init{"Init"} .= $ClassName." *$Var = new ".$Obj_Init{"Call"}.";\n";
        if($Obj_Init{"PostCondition"})
        {
            $Interface_Init{"Init"} .= $Obj_Init{"PostCondition"};
        }
        if($Obj_Init{"ReturnRequirement"})
        {
            $Obj_Init{"ReturnRequirement"}=~s/(\$0|\$obj)/*$Var/gi;
            $Interface_Init{"Init"} .= $Obj_Init{"ReturnRequirement"}
        }
        if($Obj_Init{"FinalCode"})
        {
            $Interface_Init{"Init"} .= "//final code\n";
            $Interface_Init{"Init"} .= $Obj_Init{"FinalCode"}."\n";
        }
        $Interface_Init{"Headers"} = addHeaders($Obj_Init{"Headers"}, $Interface_Init{"Headers"});
        $Interface_Init{"Code"} .= $Obj_Init{"Code"};
        $Interface_Init{"Call"} = "delete($Var)";
    }
    elsif($Interface=~/D1/)
    {
        if(isAbstractClass($ClassId))
        {#Impossible to call in-charge (D1) destructor in abstract class
            return ();
        }
        if($CompleteSignature{$Interface}{"Protected"})
        {#Impossible to call protected in-charge (D1) destructor
            return ();
        }
        #push(@RecurTypeId, $ClassId);
        my %Obj_Init = findConstructor($ClassId, "");
        #pop(@RecurTypeId);
        return () if(not $Obj_Init{"IsCorrect"});
        if($CompleteSignature{$Obj_Init{"Interface"}}{"Protected"})
        {#Impossible to call in-charge (D1) destructor in class with protected constructor
            return ();
        }
        $Interface_Init{"Init"} .= $Obj_Init{"Init"};
        #$Interface_Init{"Init"} .= "//parameter initialization\n";
        if($Obj_Init{"PreCondition"})
        {
            $Interface_Init{"Init"} .= $Obj_Init{"PreCondition"};
        }
        $Interface_Init{"Init"} .= correct_init_stmt("$ClassName $Var = ".$Obj_Init{"Call"}.";\n", $ClassName, $Var);
        if($Obj_Init{"PostCondition"})
        {
            $Interface_Init{"Init"} .= $Obj_Init{"PostCondition"};
        }
        if($Obj_Init{"ReturnRequirement"})
        {
            $Obj_Init{"ReturnRequirement"}=~s/(\$0|\$obj)/$Var/gi;
            $Interface_Init{"Init"} .= $Obj_Init{"ReturnRequirement"}
        }
        if($Obj_Init{"FinalCode"})
        {
            $Interface_Init{"Init"} .= "//final code\n";
            $Interface_Init{"Init"} .= $Obj_Init{"FinalCode"}."\n";
        }
        $Interface_Init{"Headers"} = addHeaders($Obj_Init{"Headers"}, $Interface_Init{"Headers"});
        $Interface_Init{"Code"} .= $Obj_Init{"Code"};
        $Interface_Init{"Call"} = "";#auto call after construction
    }
    $Interface_Init{"Headers"} = addHeaders([$CompleteSignature{$Interface}{"Header"}], $Interface_Init{"Headers"});
    $Interface_Init{"IsCorrect"} = 1;
    my $Typedef_Id = get_type_typedef($ClassId);
    if($Typedef_Id)
    {
        $Interface_Init{"Headers"} = addHeaders(getTypeHeaders($Typedef_Id), $Interface_Init{"Headers"});
        foreach my $Elem ("Call", "Init")
        {
            $Interface_Init{$Elem} = cover_by_typedef($Interface_Init{$Elem}, $ClassId, $Typedef_Id);
        }
    }
    else
    {
        $Interface_Init{"Headers"} = addHeaders(getTypeHeaders($ClassId), $Interface_Init{"Headers"});
    }
    return %Interface_Init;
}

sub testForConstructor($)
{
    my $Interface = $_[0];
    my $Ispecobjecttype = $InterfaceSpecType{$Interface}{"SpecObject"};
    my $PointerLevelTarget = get_PointerLevel($Tid_TDid{$SpecType{$Ispecobjecttype}{"TypeId"}}, $SpecType{$Ispecobjecttype}{"TypeId"});
    my $ClassId = $CompleteSignature{$Interface}{"Class"};
    my $ClassName = get_TypeName($ClassId);
    my $Var = select_obj_name("", $ClassId);
    $Block_Variable{$CurrentBlock}{$Var} = 1;
    if(isInCharge($Interface))
    {
        if(isAbstractClass($ClassId))
        {#Impossible to call in-charge constructor in abstract class
            return ();
        }
        if($CompleteSignature{$Interface}{"Protected"})
        {#Impossible to call in-charge protected constructor
            return ();
        }
    }
    my $HeapStack = ($SpecType{$Ispecobjecttype}{"TypeId"} and ($PointerLevelTarget eq 0))?"Stack":"Heap";
    my $ObjectCall = ($HeapStack eq "Stack")?$Var:"(*$Var)";
    my %Interface_Init = callInterfaceParameters((
            "Interface"=>$Interface,
            "Key"=>"",
            "ObjectCall"=>$ObjectCall));
    return () if(not $Interface_Init{"IsCorrect"});
    my $PreviousBlock = $CurrentBlock;
    $CurrentBlock = $CurrentBlock."_code_".$Ispecobjecttype;
    my %ParsedCode = parseCode($SpecType{$Ispecobjecttype}{"Code"}, "Code");
    $CurrentBlock = $PreviousBlock;
    return () if(not $ParsedCode{"IsCorrect"});
    $SpecCode{$Ispecobjecttype} = 1 if($ParsedCode{"Code"});
    $Interface_Init{"Code"} .= $ParsedCode{"NewGlobalCode"}.$ParsedCode{"Code"};
    $Interface_Init{"Headers"} = addHeaders($ParsedCode{"Headers"}, $Interface_Init{"Headers"});
    if(isAbstractClass($ClassId) or isNotInCharge($Interface) or ($CompleteSignature{$Interface}{"Protected"}))
    {
        my $ClassNameChild = getSubClassName($ClassName);
        if($Interface_Init{"Call"}=~/\A($ClassName([\n]*)\()/)
        {
            substr($Interface_Init{"Call"}, index($Interface_Init{"Call"}, $1), pos($1) + length($1)) = $ClassNameChild.$2."(";
        }
        $ClassName = $ClassNameChild;
        $UsedConstructors{$ClassId}{$Interface} = 1;
        $IntSubClass{$TestedInterface}{$ClassId} = 1;
        $Create_SubClass{$ClassId} = 1;
    }
    if($HeapStack eq "Stack")
    {
        $Interface_Init{"Call"} = correct_init_stmt($ClassName." $Var = ".$Interface_Init{"Call"}, $ClassName, $Var);
    }
    elsif($HeapStack eq "Heap")
    {
        $Interface_Init{"Call"} = $ClassName."* $Var = new ".$Interface_Init{"Call"};
    }
    my $Typedef_Id = get_type_typedef($ClassId);
    if($Typedef_Id)
    {
        $Interface_Init{"Headers"} = addHeaders(getTypeHeaders($Typedef_Id), $Interface_Init{"Headers"});
        foreach my $Elem ("Call", "Init")
        {
            $Interface_Init{$Elem} = cover_by_typedef($Interface_Init{$Elem}, $ClassId, $Typedef_Id);
        }
    }
    else
    {
        $Interface_Init{"Headers"} = addHeaders(getTypeHeaders($ClassId), $Interface_Init{"Headers"});
    }
    if($Ispecobjecttype and my $PostCondition = $SpecType{$Ispecobjecttype}{"PostCondition"}
    and $ObjectCall ne "" and (not defined $Template2Code or $Interface eq $TestedInterface))
    {# postcondition
        $PostCondition=~s/(\$0|\$obj)/$ObjectCall/gi;
        $PostCondition = clearSyntax($PostCondition);
        my $NormalResult = $PostCondition;
        while($PostCondition=~s/([^\\])"/$1\\\"/g){}
        $ConstraintNum{$Interface}+=1;
        my $ReqId = short_interface_name($Interface).".".normalize_num($ConstraintNum{$Interface});
        $RequirementsCatalog{$Interface}{$ConstraintNum{$Interface}} = "postcondition for the object: \'$PostCondition\'";
        my $Comment = "postcondition for the object failed: \'$PostCondition\'";
        $Interface_Init{"ReturnRequirement"} .= "REQ(\"$ReqId\", \"$Comment\", $NormalResult);\n";
        $TraceFunc{"REQ"}=1;
    }
    # init code
    my $InitCode = $SpecType{$Ispecobjecttype}{"InitCode"};
    $Interface_Init{"Init"} .= clearSyntax($InitCode);
    # final code
    my $ObjFinalCode = $SpecType{$Ispecobjecttype}{"FinalCode"};
    $ObjFinalCode=~s/(\$0|\$obj)/$ObjectCall/gi;
    $Interface_Init{"FinalCode"} .= clearSyntax($ObjFinalCode);
    return %Interface_Init;
}

sub add_VirtualTestData($$)
{
    my ($Code, $Path) = @_;
    my $RelPath = test_data_relpath("sample.txt");
    if($Code=~s/TG_TEST_DATA_(PLAIN|TEXT)_FILE/$RelPath/g)
    {#plain text files
        mkpath($Path);
        writeFile($Path."/sample.txt", "Where there's a will there's a way.");
    }
    $RelPath = test_data_abspath("sample", $Path);
    if($Code=~s/TG_TEST_DATA_ABS_FILE/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample", "Where there's a will there's a way.");
    }
    $RelPath = test_data_relpath("sample.xml");
    if($Code=~s/TG_TEST_DATA_XML_FILE/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.xml", getXMLSample());
    }
    $RelPath = test_data_relpath("sample.html");
    if($Code=~s/TG_TEST_DATA_HTML_FILE/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.html", getHTMLSample());
    }
    $RelPath = test_data_relpath("sample.dtd");
    if($Code=~s/TG_TEST_DATA_DTD_FILE/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.dtd", getDTDSample());
    }
    $RelPath = test_data_relpath("sample.db");
    if($Code=~s/TG_TEST_DATA_DB/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.db", "");
    }
    $RelPath = test_data_relpath("sample.audio");
    if($Code=~s/TG_TEST_DATA_AUDIO/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.audio", "");
    }
    $RelPath = test_data_relpath("sample.asoundrc");
    if($Code=~s/TG_TEST_DATA_ASOUNDRC_FILE/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.asoundrc", getASoundRCSample());
    }
    $RelPath = test_data_relpath("");
    if($Code=~s/TG_TEST_DATA_DIRECTORY/$RelPath/g)
    {
        mkpath($Path);
        writeFile($Path."/sample.txt", "Where there's a will there's a way.");
    }
    while($Code=~/TG_TEST_DATA_FILE_([A-Z]+)/)
    {
        my ($Type, $Ext) = ($1, lc($1));
        $RelPath = test_data_relpath("sample.$Ext");
        $Code=~s/TG_TEST_DATA_FILE_$Type/$RelPath/g;
        mkpath($Path);
        writeFile($Path."/sample.$Ext", "");
    }
    return $Code;
}

sub test_data_relpath($)
{
    my $File = $_[0];
    if(defined $Template2Code)
    {
        return "T2C_GET_DATA_PATH(\"$File\")";
    }
    else
    {
        return "\"testdata/$File\"";
    }
}

sub test_data_abspath($$)
{
    my ($File, $Path) = @_;
    if(defined $Template2Code)
    {
        return "T2C_GET_DATA_PATH(\"$File\")";
    }
    else
    {
        return "\"".abs_path("")."/".$Path.$File."\"";
    }
}

sub getXMLSample()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<note>
    <to>Tove</to>
    <from>Jani</from>
    <heading>Reminder</heading>
    <body>Don't forget me this weekend!</body>
</note>";
}

sub getHTMLSample()
{
    return "<html>
<body>
Where there's a will there's a way.
</body>
</html>";
}

sub getDTDSample()
{
    return "<!ELEMENT note (to,from,heading,body)>
<!ELEMENT to (#PCDATA)>
<!ELEMENT from (#PCDATA)>
<!ELEMENT heading (#PCDATA)>
<!ELEMENT body (#PCDATA)>";
}

sub getASoundRCSample()
{
    if(my $Sample = readFile("/usr/share/alsa/alsa.conf"))
    {
        return $Sample;
    }
    elsif(my $Sample = readFile("/etc/asound-pulse.conf"))
    {
        return $Sample;
    }
    elsif(my $Sample = readFile("/etc/asound.conf"))
    {
        return $Sample;
    }
    else
    {
        return "pcm.card0 {
    type hw
    card 0
}
ctl.card0 {
    type hw
    card 0
}";
    }
}

sub add_TestData($$)
{
    my ($Code, $Path) = @_;
    my %CopiedFiles = ();
    if($Code=~/TEST_DATA_PATH/)
    {
        if(not $TestDataPath)
        {
            print STDERR "WARNING: test data directory was not specified\n";
            return $Code;
        }
    }
    while($Code=~s/TEST_DATA_PATH[ ]*\([ ]*"([^\(\)]+)"[ ]*\)/test_data_relpath($1)/ge)
    {
        my $FileName = $1;
        next if($CopiedFiles{$FileName});
        mkpath($Path);
        next if(not -e $TestDataPath."/".$FileName);
        copy($TestDataPath."/".$FileName, $Path);
        $CopiedFiles{$FileName} = 1;
    }
    return $Code;
}

sub constraint_for_environment($$$)
{
    my ($Interface, $ConditionType, $Condition) = @_;
    $ConstraintNum{$Interface}+=1;
    my $ReqId = short_interface_name($Interface).".".normalize_num($ConstraintNum{$Interface});
    $RequirementsCatalog{$Interface}{$ConstraintNum{$Interface}} = "$ConditionType for the environment: \'$Condition\'";
    my $Comment = "$ConditionType for the environment failed: \'$Condition\'";
    $TraceFunc{"REQ"}=1;
    return "REQ(\"$ReqId\", \"$Comment\", $Condition);\n";
}

sub get_env_conditions($$)
{
    my ($Interface, $SpecEnv_Id) = @_;
    my %Conditions = ();
    if(my $InitCode = $SpecType{$SpecEnv_Id}{"InitCode"})
    {
        $Conditions{"Preamble"} .= $InitCode."\n";
    }
    if(my $FinalCode = $SpecType{$SpecEnv_Id}{"FinalCode"})
    {
        $Conditions{"Finalization"} .= $FinalCode."\n";
    }
    if(my $GlobalCode = $SpecType{$SpecEnv_Id}{"GlobalCode"})
    {
        $Conditions{"Env_CommonCode"} .= $GlobalCode."\n";
        $SpecCode{$SpecEnv_Id} = 1;
    }
    if(my $PreCondition = $SpecType{$SpecEnv_Id}{"PreCondition"})
    {
        $Conditions{"Env_PreRequirements"} .= constraint_for_environment($Interface, "precondition", $PreCondition);
    }
    if(my $PostCondition = $SpecType{$SpecEnv_Id}{"PostCondition"})
    {
        $Conditions{"Env_PostRequirements"} .= constraint_for_environment($Interface, "postcondition", $PostCondition);
    }
    foreach my $Lib (keys(%{$SpecType{$SpecEnv_Id}{"Libs"}}))
    {
        $SpecLibs{$Lib} = 1;
    }
    return %Conditions;
}

sub generate_sanity_test($)
{
    my %ResultComponents = ();
    my $Interface = $_[0];
    return () if(not $Interface);
    my $CommonCode = "";
    my %TestComponents = ();
    return () if(not interfaceFilter($Interface));
    $TestedInterface = $Interface;
    $CurrentBlock = "main";
    $ValueCollection{$CurrentBlock}{"argc"} = get_TypeIdByName("int");
    $Block_Param{$CurrentBlock}{"argc"} = get_TypeIdByName("int");
    $Block_Variable{$CurrentBlock}{"argc"} = 1;
    $ValueCollection{$CurrentBlock}{"argv"} = get_TypeIdByName("char**");
    $Block_Param{$CurrentBlock}{"argv"} = get_TypeIdByName("char**");
    $Block_Variable{$CurrentBlock}{"argv"} = 1;
    
    my ($CommonPreamble, $Preamble, $Finalization, $Env_CommonCode, $Env_PreRequirements, $Env_PostRequirements) = ();
    foreach my $SpecEnv_Id (sort {int($a)<=>int($b)} (keys(%Common_SpecEnv)))
    {# common environments
        next if($Common_SpecType_Exceptions{$Interface}{$SpecEnv_Id});
        my %Conditions = get_env_conditions($Interface, $SpecEnv_Id);
        $CommonPreamble .= $Conditions{"Preamble"};# in the direct order
        $Finalization = $Conditions{"Finalization"}.$Finalization;# in the backward order
        $Env_CommonCode .= $Conditions{"Env_CommonCode"};
        $Env_PreRequirements .= $Conditions{"Env_PreRequirements"};# in the direct order
        $Env_PostRequirements = $Conditions{"Env_PostRequirements"}.$Env_PostRequirements;# in the backward order
    }
    
    # parsing of common preamble code for using
    # created variables in the following test case
    my %CommonPreamble_Parsed = parseCode($CommonPreamble, "Code");
    $CommonPreamble = $CommonPreamble_Parsed{"Code"};
    $CommonCode = $CommonPreamble_Parsed{"NewGlobalCode"}.$CommonCode;
    $TestComponents{"Headers"} = addHeaders($CommonPreamble_Parsed{"Headers"}, $TestComponents{"Headers"});
    
    # creating test case
    if($CompleteSignature{$Interface}{"Constructor"})
    {
        %TestComponents = testForConstructor($Interface);
        $CommonCode .= $TestComponents{"Code"};
    }
    elsif($CompleteSignature{$Interface}{"Destructor"})
    {
        %TestComponents = testForDestructor($Interface);
        $CommonCode .= $TestComponents{"Code"};
    }
    else
    {
        %TestComponents = callInterface((
            "Interface"=>$Interface));
        $CommonCode .= $TestComponents{"Code"};
    }
    if(not $TestComponents{"IsCorrect"})
    {
        $ResultCounter{"Gen"}{"Fail"} += 1;
        $GenResult{$Interface}{"IsCorrect"} = 0;
        return ();
    }
    if($TraceFunc{"REQ"} and not defined $Template2Code)
    {
        $CommonCode = get_REQ_define($Interface)."\n".$CommonCode;
    }
    if($TraceFunc{"REQva"} and not defined $Template2Code)
    {
        $CommonCode = get_REQva_define($Interface)."\n".$CommonCode;
    }
    
    foreach my $SpecEnv_Id (sort {int($a)<=>int($b)} (keys(%SpecEnv)))
    {# environments used in the test case
        my %Conditions = get_env_conditions($Interface, $SpecEnv_Id);
        $Preamble .= $Conditions{"Preamble"};# in the direct order
        $Finalization = $Conditions{"Finalization"}.$Finalization;# in the backward order
        $Env_CommonCode .= $Conditions{"Env_CommonCode"};
        $Env_PreRequirements .= $Conditions{"Env_PreRequirements"};# in the direct order
        $Env_PostRequirements = $Conditions{"Env_PostRequirements"}.$Env_PostRequirements;# in the backward order
    }
    
    my %Preamble_Parsed = parseCode($Preamble, "Code");
    $Preamble = $Preamble_Parsed{"Code"};
    $CommonCode = $Preamble_Parsed{"NewGlobalCode"}.$CommonCode;
    $TestComponents{"Headers"} = addHeaders($Preamble_Parsed{"Headers"}, $TestComponents{"Headers"});
    
    my %Finalization_Parsed = parseCode($Finalization, "Code");
    $Finalization = $Finalization_Parsed{"Code"};
    $CommonCode = $Finalization_Parsed{"NewGlobalCode"}.$CommonCode;
    $TestComponents{"Headers"} = addHeaders($Finalization_Parsed{"Headers"}, $TestComponents{"Headers"});
    
    my %Env_ParsedCode = parseCode($Env_CommonCode, "Code");
    $CommonCode = $Env_ParsedCode{"NewGlobalCode"}.$Env_ParsedCode{"Code"}.$CommonCode;
    $TestComponents{"Headers"} = addHeaders($Env_ParsedCode{"Headers"}, $TestComponents{"Headers"});
    foreach my $Header (@{$Env_ParsedCode{"Headers"}})
    {
        $SpecTypeHeaders{get_FileName($Header)}=1;
    }
    #Insert subclasses
    my ($SubClasses_Code, $SubClasses_Headers) = create_SubClasses(keys(%Create_SubClass));
    $TestComponents{"Headers"} = addHeaders($SubClasses_Headers, $TestComponents{"Headers"});
    $CommonCode = $SubClasses_Code.$CommonCode;
    # closing streams
    foreach my $Stream (keys(%{$OpenStreams{"main"}}))
    {
        $Finalization .= "fclose($Stream);\n";
    }
    #Sanity test assembling
    my ($SanityTest, $SanityTestMain, $SanityTestBody) = ();
    if($CommonPreamble.$Preamble)
    {
        $SanityTestMain .= "//preamble\n";
        $SanityTestMain .= $CommonPreamble.$Preamble."\n";
    }
    if($Env_PreRequirements)
    {
        $SanityTestMain .= $Env_PreRequirements."\n";
    }
    if($TestComponents{"Init"})
    {
        $SanityTestBody .= $TestComponents{"Init"};
    }
    # precondition for parameters
    if($TestComponents{"PreCondition"})
    {
        $SanityTestBody .= $TestComponents{"PreCondition"};
    }
    if($TestComponents{"Call"})
    {
        if($TestComponents{"ReturnRequirement"} and $CompleteSignature{$Interface}{"Return"})
        {#Call interface and check return value
            my $ReturnType_Id = $CompleteSignature{$Interface}{"Return"};
            my $ReturnType_Name = $TypeDescr{$Tid_TDid{$ReturnType_Id}}{$ReturnType_Id}{"Name"};
            my $ReturnType_PointerLevel = get_PointerLevel($Tid_TDid{$ReturnType_Id}, $ReturnType_Id);
            my $ReturnFType_Id = get_FoundationTypeId($ReturnType_Id);
            my $ReturnFType_Name = get_TypeName($ReturnFType_Id);
            if($ReturnFType_Name eq "void" and $ReturnType_PointerLevel==1)
            {
                my $RetVal = select_var_name("retval", "");
                $TestComponents{"ReturnRequirement"}=~s/(\$0|\$retval)/$RetVal/gi;
                $SanityTestBody .= "int* $RetVal = (int*)".$TestComponents{"Call"}."; //target call\n";
                $Block_Variable{$CurrentBlock}{$RetVal} = 1;
            }
            elsif($ReturnFType_Name eq "void" and $ReturnType_PointerLevel==0)
            {
                $SanityTestBody .= $TestComponents{"Call"}."; //target call\n";
            }
            else
            {
                my $RetVal = select_var_name("retval", "");
                $TestComponents{"ReturnRequirement"}=~s/(\$0|\$retval)/$RetVal/gi;
                my ($InitializedEType_Id, $Declarations, $Headers) = get_ExtTypeId($RetVal, $ReturnType_Id);
                my $InitializedType_Name = get_TypeName($InitializedEType_Id);
                $TestComponents{"Code"} .= $Declarations;
                $TestComponents{"Headers"} = addHeaders($Headers, $TestComponents{"Headers"});
                my $Break = ((length($InitializedType_Name)>20)?"\n":" ");
                my $InitializedFType_Id = get_FoundationTypeId($ReturnType_Id);
                if(($InitializedType_Name eq $ReturnType_Name))
                {
                    $SanityTestBody .= $ReturnType_Name.$Break.$RetVal." = ".$TestComponents{"Call"}."; //target call\n";
                }
                else
                {
                    $SanityTestBody .= $InitializedType_Name.$Break.$RetVal." = "."(".$InitializedType_Name.")".$TestComponents{"Call"}."; //target call\n";
                }
                $Block_Variable{$CurrentBlock}{$RetVal} = 1;
                $TestComponents{"Headers"} = addHeaders(getTypeHeaders($InitializedFType_Id), $TestComponents{"Headers"});
            }
        }
        else
        {
            $SanityTestBody .= $TestComponents{"Call"}."; //target call\n";
        }
    }
    elsif($CompleteSignature{$Interface}{"Destructor"})
    {
        $SanityTestBody .= "//target interface will be called at the end of main() function automatically\n";
    }
    if($TestComponents{"ReturnRequirement"})
    {
        $SanityTestBody .= $TestComponents{"ReturnRequirement"}."\n";
    }
    # postcondition for parameters
    if($TestComponents{"PostCondition"})
    {
        $SanityTestBody .= $TestComponents{"PostCondition"}."\n";
    }
    if($TestComponents{"FinalCode"})
    {
        $SanityTestBody .= "//final code\n";
        $SanityTestBody .= $TestComponents{"FinalCode"}."\n";
    }
    $SanityTestMain .= $SanityTestBody;
    if($Finalization)
    {
        $SanityTestMain .= "\n//finalization\n";
        $SanityTestMain .= $Finalization."\n";
    }
    if($Env_PostRequirements)
    {
        $SanityTestMain .= $Env_PostRequirements."\n";
    }
    #Clear code syntax
    $SanityTestMain = alignCode($SanityTestMain, "    ", 0);
    if(keys(%ConstraintNum)>0)
    {
        if(getTestLang($Interface) eq "C++")
        {
            $TestComponents{"Headers"} = addHeaders(["iostream"], $TestComponents{"Headers"});
            $AuxHeaders{"iostream"}=1;
        }
        else
        {
            $TestComponents{"Headers"} = addHeaders(["stdio.h"], $TestComponents{"Headers"});
            $AuxHeaders{"stdio.h"}=1;
        }
    }
    foreach my $Header_Path (sort {int($Include_Preamble{$b}{"Position"})<=>int($Include_Preamble{$a}{"Position"})} keys(%Include_Preamble))
    {
        @{$TestComponents{"Headers"}} = ($Header_Path, @{$TestComponents{"Headers"}});
    }
    my %HeadersList = unique_headers_list(add_prefixes(create_headers_list(order_headers_list(@{$TestComponents{"Headers"}}))));
    $ResultComponents{"Headers"} = [];
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%HeadersList))
    {
        $SanityTest .= "#include <".$HeadersList{$Pos}{"Inc"}.">\n";
        @{$ResultComponents{"Headers"}} = (@{$ResultComponents{"Headers"}}, $HeadersList{$Pos}{"Inc"});
    }
    my %UsedNameSpaces = ();
    foreach my $NameSpace (add_namespaces(\$CommonCode), add_namespaces(\$SanityTestMain))
    {
        $UsedNameSpaces{$NameSpace} = 1;
    }
    if(keys(%UsedNameSpaces))
    {
        $SanityTest .= "\n";
        foreach my $NameSpace (sort {get_depth($a)<=>get_depth($b)} keys(%UsedNameSpaces))
        {
            $SanityTest .= "using namespace $NameSpace;\n";
        }
        $SanityTest .= "\n";
    }
    if($CommonCode)
    {
        $SanityTest .= "\n$CommonCode\n\n";
        $ResultComponents{"Code"} = $CommonCode;
    }
    $SanityTest .= "int main(int argc, char *argv[])\n";
    $SanityTest .= "{\n";
    $ResultComponents{"main"} = correct_spaces($SanityTestMain);
    $SanityTestMain .= "    return 0;\n";
    $SanityTest .= $SanityTestMain;
    $SanityTest .= "}\n";
    $SanityTest = correct_spaces($SanityTest); #cleaning code
    if(getTestLang($Interface) eq "C++"
    and getIntLang($Interface) eq "C")
    {#removing extended initializer lists
        $SanityTest=~s/({\s*|\s)\.[a-z_]+\s*=\s*/$1  /ig;
    }
    if(defined $Standalone)
    {#creating stuff for building and running test
        my $TestFileName = (getTestLang($Interface) eq "C++")?"test.cpp":"test.c";
        my $TestPath = getTestPath($Interface);
        if(-e $TestPath)
        {
            rmtree($TestPath);
        }
        mkpath($TestPath);
        $Interface_TestDir{$Interface} = $TestPath;
        $SanityTest = add_VirtualTestData($SanityTest, $TestPath."/testdata/");
        $SanityTest = add_TestData($SanityTest, $TestPath."/testdata/");
        writeFile("$TestPath/$TestFileName", $SanityTest);
        my $SharedObject = $Interface_Library{$Interface};
        $SharedObject = $NeededInterface_Library{$Interface} if(not $SharedObject);
        writeFile("$TestPath/info", "Library: $TargetLibraryName-".$Descriptor{"Version"}."\nInterface: ".get_Signature($Interface)."\nSymbol: $Interface".(($Interface=~/\A_Z/)?"\nShort Name: ".$CompleteSignature{$Interface}{"ShortName"}:"")."\nHeader: ".$CompleteSignature{$Interface}{"Header"}.(($SharedObject ne "WithoutLib")?"\nShared Object: ".get_FileName($SharedObject):"").(get_IntNameSpace($Interface)?"\nNamespace: ".get_IntNameSpace($Interface):""));
        my $Signature = get_Signature($Interface);
        my $NameSpace = get_IntNameSpace($Interface);
        if($NameSpace)
        {
            $Signature=~s/(\W|\A)\Q$NameSpace\E\:\:(\w)/$1$2/g;
        }
        my $TitleSignature = $Signature;
        $TitleSignature=~s/([^:]):[^:].+?\Z/$1/;
        my $Title = "Test for interface ".htmlSpecChars($TitleSignature);
        my $Keywords = $CompleteSignature{$Interface}{"ShortName"}.", test, unit, $TargetLibraryName, runtime";
        writeFile("$TestPath/view.html", composeHTML_Head($Title, $Keywords, "    <style type=\"text/css\">\n    body{font-family:Arial}\n    h3.title3{margin-left:7px;margin-bottom:0px;padding-bottom:0px;margin-top:0px;padding-top:0px;font-family:Verdana;font-size:18px;}\n    span.int{font-weight:bold;margin-left:5px;font-size:16px;font-family:Arial;color:#003E69;}\n    span.int_p{font-weight:normal;}\n    hr{color:Black;background-color:Black;height:1px;border:0;}\n".get_TestView_Style()."\n</style>")."<body>\n".all_tests_link($TestPath)."<h3 class='title3'><span style='color:Black'>Unit Test</span></h3><span style='margin-left:20px;text-align:left;font-size:14px;font-family:Verdana'>Interface </span><span class='int'>".highLight_Signature_Italic_Color(htmlSpecChars($Signature))."</span>".(($Interface=~/\A_Z/)?"<br/><span style='margin-left:20px;text-align:left;font-size:14px;font-family:Arial;'>Symbol </span><span class='int'>$Interface</span>":"").($NameSpace?"<br/><span style='margin-left:20px;text-align:left;font-size:14px;font-family:Arial;'>Namespace </span><span class='int'>$NameSpace</span>":"")."<br/>\n<!--Test-->\n".get_TestView($SanityTest, $Interface)."<!--Test_End-->\n".$TOOL_SIGNATURE."\n</body>\n</html>\n");
        writeFile("$TestPath/Makefile", get_Makefile($Interface, \%HeadersList));
        writeFile("$TestPath/run_test.sh", get_RunScript($Interface));
        chmod(777, $TestPath."/run_test.sh");
    }
    else
    {#t2c
    }
    $GenResult{$Interface}{"IsCorrect"} = 1;
    $ResultCounter{"Gen"}{"Success"} += 1;
    $ResultComponents{"IsCorrect"} = 1;
    return %ResultComponents;
}

sub getTestLang($)
{
    my $Interface = $_[0];
    my $Lang = getIntLang($Interface);
    foreach my $Interface (keys(%UsedInterfaces))
    {
        if($CompleteSignature{$Interface}{"Constructor"}
        or $CompleteSignature{$Interface}{"Destructor"}
        or $Interface=~/\A_Z/ or $CompleteSignature{$Interface}{"Header"}=~/\.(hh|hpp)\Z/i)
        {
            $Lang = "C++";
        }
    }
    return $Lang;
}

sub all_tests_link($)
{
    my $TestPath = $_[0];
    my $BackPath = "";
    foreach (0 .. get_depth($TestPath)-3)
    {
        $BackPath .= "../";
    }
    $BackPath .= "view_tests.html";
    return "<a style='font-size:11px;color:#336699;' href=\'$BackPath\'>all tests</a>";
}

sub add_namespaces($)
{
    my $CodeRef = $_[0];
    my @UsedNameSpaces = ();
    foreach my $NameSpace (sort {get_depth($b)<=>get_depth($a)} keys(%NestedNameSpaces))
    {
        next if($NameSpace eq "std");
        my $NameSpace_InCode = $NameSpace."::";
        my $NameSpace_InSubClass = getSubClassBaseName($NameSpace_InCode);
        if(${$CodeRef}=~s/(\W|\A)(\Q$NameSpace_InCode\E|$NameSpace_InSubClass)(\w)/$1$3/g)
        {
            push(@UsedNameSpaces, $NameSpace);
        }
    }
    return @UsedNameSpaces;
}

sub correct_spaces($)
{
    my $Code = $_[0];
    $Code=~s/\n\n\n\n/\n\n/g;
    $Code=~s/\n\n\n/\n\n/g;
    $Code=~s/\n    \n    /\n\n    /g;
    $Code=~s/\n    \n\n/\n/g;
    $Code=~s/\n\n\};/\n};/g;
    return $Code;
}

sub add_prefixes(@)
{
    my %HeadersToInclude = remove_prefixes(@_);
    foreach my $Pos (sort {int($b) <=> int($a)} keys(%HeadersToInclude))
    {
        if(not get_Directory($HeadersToInclude{$Pos}{"Inc"}))
        {
            my $Prefix = get_FileName(get_Directory($HeadersToInclude{$Pos}{"Path"}));
            if($Prefix!~/include/i)
            {
                $HeadersToInclude{$Pos}{"Inc"} = $Prefix."/".$HeadersToInclude{$Pos}{"Inc"};
            }
        }
    }
    return %HeadersToInclude;
}

sub remove_prefixes(@)
{
    my %HeadersToInclude = @_;
    my %IncDir = get_HeaderDeps_forList(%HeadersToInclude);
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%HeadersToInclude))
    {
        if($HeadersToInclude{$Pos}{"Inc"}=~/\A\//)
        {
            foreach my $Prefix (sort {length($b)<=>length($a)} keys(%IncDir))
            {
                last if($HeadersToInclude{$Pos}{"Inc"}=~s/\A\Q$Prefix\E\///);
            }
        }
    }
    return %HeadersToInclude;
}

sub get_HeaderDeps_forList(@)
{
    my %HeadersToInclude = @_;
    my %IncDir = ();
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%HeadersToInclude))
    {
        foreach my $Dir (get_HeaderDeps($HeadersToInclude{$Pos}{"Path"}))
        {
            $IncDir{$Dir}=1;
        }
        if(my $DepDir = get_Directory($HeadersToInclude{$Pos}{"Path"}))
        {
            if(my $Prefix = get_Directory($HeadersToInclude{$Pos}{"Inc"}))
            {
                $DepDir=~s/[\/]+\Q$Prefix\E\Z//;
            }
            $IncDir{$DepDir} = 1 if(not is_default_include_dir($DepDir) and $DepDir ne "/usr/local/include");
        }
    }
    return %IncDir;
}

sub unique_headers_list(@)
{
    my %HeadersToInclude = @_;
    my (%FullPaths, %NewHeadersToInclude) = ();
    my $NewPos = 0;
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%HeadersToInclude))
    {
        if(not $FullPaths{$HeadersToInclude{$Pos}{"Path"}})
        {
            $NewHeadersToInclude{$NewPos}{"Path"} = $HeadersToInclude{$Pos}{"Path"};
            $NewHeadersToInclude{$NewPos}{"Inc"} = $HeadersToInclude{$Pos}{"Inc"};
            $FullPaths{$HeadersToInclude{$Pos}{"Path"}} = 1;
            $NewPos+=1;
        }
    }
    return %NewHeadersToInclude;
}

sub order_headers_list(@)
{# ordering headers according to descriptor
    my @HeadersToInclude = @_;
    return @HeadersToInclude if(not keys(%Include_RevOrder));
    my @NewHeadersToInclude = ();
    my (%ElemNum, %Replace) = ();
    my $Num = 1;
    foreach my $Elem (@HeadersToInclude)
    {
        $ElemNum{$Elem} = $Num;
        $Num+=1;
    }
    foreach my $Elem (@HeadersToInclude)
    {
        if(my $Preamble = $Include_RevOrder{$Elem})
        {
            if(not $ElemNum{$Preamble})
            {
                push(@NewHeadersToInclude, $Preamble);
                push(@NewHeadersToInclude, $Elem);
            }
            elsif($ElemNum{$Preamble}>$ElemNum{$Elem})
            {
                push(@NewHeadersToInclude, $Preamble);
                $Replace{$Preamble} = $Elem;
            }
            else
            {
                push(@NewHeadersToInclude, $Elem);
            }
        }
        elsif($Replace{$Elem})
        {
            push(@NewHeadersToInclude, $Replace{$Elem});
        }
        else
        {
            push(@NewHeadersToInclude, $Elem);
        }
    }
    return @NewHeadersToInclude;
}

sub create_headers_list(@)
{#recreate full information about headers by its names
    my @InputHeadersList = @_;
    my (%Already_Included_Header, %HeadersToInclude) = ();
    my $IPos=0;
    foreach my $Header_Name (@InputHeadersList)
    {
        next if($Already_Included_Header{$Header_Name});
        my ($Header_Inc, $Header_Path) = identify_header($Header_Name);
        next if(not $Header_Inc);
        detect_recursive_includes($Header_Path);
        @RecurHeader = ();
        if(my $RHeader_Path = $Header_ErrorRedirect{$Header_Path}{"Path"})
        {
            my $RHeader_Inc = $Header_ErrorRedirect{$Header_Path}{"Inc"};
            next if($Already_Included_Header{$RHeader_Inc});
            $Already_Included_Header{$RHeader_Inc} = 1;
            $HeadersToInclude{$IPos}{"Inc"} = $RHeader_Inc;
            $HeadersToInclude{$IPos}{"Path"} = $RHeader_Path;
        }
        elsif($Header_ShouldNotBeUsed{$Header_Path})
        {
            next;
        }
        else
        {
            next if($Already_Included_Header{$Header_Inc});
            $Already_Included_Header{$Header_Inc} = 1;
            $HeadersToInclude{$IPos}{"Inc"} = $Header_Inc;
            $HeadersToInclude{$IPos}{"Path"} = $Header_Path;
        }
        $IPos+=1;
    }
    if(keys(%Headers)==1) #<=2
    {
        my (%HeadersToInclude_New, %Included) = ();
        my $Pos = 0;
        foreach my $HeaderPath ((sort {int($Include_Preamble{$a}{"Position"})<=>int($Include_Preamble{$b}{"Position"})} keys(%Include_Preamble)), (sort {int($Headers{$a}{"Position"})<=>int($Headers{$b}{"Position"})} keys(%Headers)))
        {
            my ($Header_Inc, $Header_Path) = identify_header($HeaderPath);
            next if($Included{$Header_Inc});
            $HeadersToInclude_New{$Pos}{"Inc"} = $Header_Inc;
            $HeadersToInclude_New{$Pos}{"Path"} = $Header_Path;
            $Included{$Header_Inc} = 1;
            $Pos+=1;
        }
        foreach my $IPos (sort {int($a)<=>int($b)} keys(%HeadersToInclude))
        {
            my ($Header_Inc, $Header_Path) = ($HeadersToInclude{$IPos}{"Inc"}, $HeadersToInclude{$IPos}{"Path"});
            if($AuxHeaders{get_FileName($Header_Inc)} or $SpecTypeHeaders{get_FileName($Header_Inc)})
            {
                next if($Included{$Header_Inc});
                $HeadersToInclude_New{$Pos}{"Inc"} = $Header_Inc;
                $HeadersToInclude_New{$Pos}{"Path"} = $Header_Path;
                $Included{$Header_Inc} = 1;
                $Pos+=1;
            }
        }
        %HeadersToInclude_New = optimize_includes(%HeadersToInclude_New);
        return %HeadersToInclude_New;
    }
    elsif($IsHeaderListSpecified)
    {
        my (%HeadersToInclude_New, %Included) = ();
        my $Pos = 0;
        foreach my $IPos (sort {int($a)<=>int($b)} keys(%HeadersToInclude))
        {
            my ($Header_Inc, $Header_Path) = ($HeadersToInclude{$IPos}{"Inc"}, $HeadersToInclude{$IPos}{"Path"});
            if(my ($TopHeader_Inc, $TopHeader_Path) = find_top_header($Header_Path))
            {
                next if($Included{$TopHeader_Inc});
                $HeadersToInclude_New{$Pos}{"Inc"} = $TopHeader_Inc;
                $HeadersToInclude_New{$Pos}{"Path"} = $TopHeader_Path;
                $Included{$TopHeader_Inc} = 1;
                $Included{$Header_Inc} = 1;
                $Pos+=1;
            }
            elsif($AuxHeaders{get_FileName($Header_Inc)} or $SpecTypeHeaders{get_FileName($Header_Inc)})
            {
                next if($Included{$Header_Inc});
                $HeadersToInclude_New{$Pos}{"Inc"} = $Header_Inc;
                $HeadersToInclude_New{$Pos}{"Path"} = $Header_Path;
                $Included{$Header_Inc} = 1;
                $Pos+=1;
            }
            else
            {
                next if($Included{$Header_Inc});
                $HeadersToInclude_New{$Pos}{"Inc"} = $Header_Inc;
                $HeadersToInclude_New{$Pos}{"Path"} = $Header_Path;
                $Included{$Header_Inc} = 1;
                $Pos+=1;
            }
        }
        return %HeadersToInclude_New;
    }
    else
    {
        %HeadersToInclude = optimize_includes(%HeadersToInclude);
        return %HeadersToInclude;
    }
}

sub get_all_header_includes($)
{
    my $AbsPath = $_[0];
    return $Cache{"get_all_header_includes"}{$AbsPath} if(defined $Cache{"get_all_header_includes"}{$AbsPath});
    my $Content = cmd_preprocessor($AbsPath, "", getTestLang($TestedInterface), "#\ [0-9]*\ ");
    my %Includes = ();
    while($Content=~s/#\s+\d+\s+"([^"]+)"[\s\d]*\n//)
    {
        my $IncPath = $1;
        if($IncPath ne "<built-in>")
        {
            if(my ($Header_Inc, $Header_Path) = identify_header($IncPath))
            {
                $Includes{$Header_Path} = 1;
            }
        }
    }
    $Cache{"get_all_header_includes"}{$AbsPath} = \%Includes;
    return \%Includes;
}

sub find_top_header($)
{
    my $AbsPath = $_[0];
    foreach my $Header_Path ((sort {int($Include_Preamble{$a}{"Position"})<=>int($Include_Preamble{$b}{"Position"})} keys(%Include_Preamble)), (sort {int($Headers{$a}{"Position"})<=>int($Headers{$b}{"Position"})} keys(%Headers)))
    {
        my %Includes = %{get_all_header_includes($Header_Path)};
        if($Includes{$AbsPath} or ($Header_Path eq $AbsPath))
        {
            return identify_header($Header_Path);
        }
    }
    return ();
}

sub optimize_includes(@)
{
    my %HeadersToInclude = @_;
    %HeadersToInclude = optimize_recursive_includes(%HeadersToInclude);
    %HeadersToInclude = optimize_src_includes(%HeadersToInclude);
    %HeadersToInclude = optimize_recursive_includes(%HeadersToInclude);
    return %HeadersToInclude;
}

sub optimize_recursive_includes(@)
{
    my %HeadersToInclude = @_;
    my (%HeadersToInclude_New, %DeletedHeaders) = ();
    my $Pos=0;
    foreach my $Pos1 (sort {int($a)<=>int($b)} keys(%HeadersToInclude))
    {
        my $IsNeeded=1;
        my $Header_Inc = $HeadersToInclude{$Pos1}{"Inc"};
        my $Header_Path = $HeadersToInclude{$Pos1}{"Path"};
        if(not defined $Include_Preamble{$Header_Path})
        {
            foreach my $Pos2 (sort {int($a)<=>int($b)} keys(%HeadersToInclude))
            {
                my $Candidate_Inc = $HeadersToInclude{$Pos2}{"Inc"};
                my $Candidate_Path = $HeadersToInclude{$Pos2}{"Path"};
                next if($DeletedHeaders{$Candidate_Inc});
                next if($Header_Inc eq $Candidate_Inc);
                my %Includes = %{get_all_header_includes($Candidate_Path)};
                if($Includes{$Header_Path})
                {
                    $IsNeeded=0;
                    $DeletedHeaders{$Header_Inc}=1;
                    last;
                }
            }
        }
        if($IsNeeded)
        {
            %{$HeadersToInclude_New{$Pos}} = %{$HeadersToInclude{$Pos1}};
            $Pos+=1;
        }
    }
    return %HeadersToInclude_New;
}

sub optimize_src_includes(@)
{
    my %HeadersToInclude = @_;
    my (%HeadersToInclude_New, %Included) = ();
    my $ResPos=0;
    foreach my $Pos (sort {int($a)<=>int($b)} keys(%HeadersToInclude))
    {
        my $Header_Inc = $HeadersToInclude{$Pos}{"Inc"};
        my $Header_Path = $HeadersToInclude{$Pos}{"Path"};
        my $DepDir = get_Directory($Header_Path);
        if(my $Prefix = get_Directory($Header_Inc))
        {
            $DepDir=~s/[\/]+\Q$Prefix\E\Z//;
        }
        if(not $SystemPaths{"include"}{$DepDir} and not $DefaultGccPaths{$DepDir} and $Header_TopHeader{$Header_Path}
        and not defined $Include_Preamble{$Header_Path} and ((keys(%{$Header_Includes{$Header_Path}})<=$Header_MaxIncludes/5
        and keys(%{$Header_Includes{$Header_Path}})<keys(%{$Header_Includes{$Header_TopHeader{$Header_Path}}})) or $STDCXX_TESTING))
        {
            my %Includes = %{get_all_header_includes($Header_TopHeader{$Header_Path})};
            if($Includes{$Header_Path})# additional check
            {
                my ($TopHeader_Inc, $TopHeader_Path) = identify_header($Header_TopHeader{$Header_Path});
                next if($Included{$TopHeader_Inc});
                $HeadersToInclude_New{$ResPos}{"Inc"} = $TopHeader_Inc;
                $HeadersToInclude_New{$ResPos}{"Path"} = $TopHeader_Path;
                $ResPos+=1;
                $Included{$TopHeader_Inc}=1;
                next;
            }
        }
        next if($Included{$Header_Inc});
        $HeadersToInclude_New{$ResPos}{"Inc"} = $Header_Inc;
        $HeadersToInclude_New{$ResPos}{"Path"} = $Header_Path;
        $ResPos+=1;
        $Included{$Header_Inc}=1;
    }
    return %HeadersToInclude_New;
}

sub cut_path_prefix($$)
{
    my ($Path, $Prefix) = @_;
    $Prefix=~s/[\/]+\Z//;
    $Path=~s/\A\Q$Prefix\E[\/]+//;
    return $Path;
}

sub identify_header($)
{
    my $Header = $_[0];
    return @{$Cache{"identify_header"}{$Header}} if(defined $Cache{"identify_header"}{$Header});
    @{$Cache{"identify_header"}{$Header}} = identify_header_m($Header);
    return @{$Cache{"identify_header"}{$Header}};
}

sub identify_header_m($)
{# search for header by absolute path, relative path or name
    my $Header = $_[0];
    if(not $Header or $Header=~/\.tcc\Z/)
    {
        return ("", "");
    }
    elsif(-f $Header)
    {
        $Header = abs_path($Header) if($Header!~/\A\//);
        my $Prefix = get_FileName(get_Directory($Header));
        my $HeaderDir = find_in_dependencies($Prefix."/".get_FileName($Header));
        if($Prefix!~/include/i and $HeaderDir eq get_Directory(get_Directory($Header)))
        {
            $Header = cut_path_prefix($Header, $HeaderDir);
            return ($Header, $HeaderDir."/".$Header);
        }
        elsif(is_default_include_dir(get_Directory($Header)))
        {
            return (get_FileName($Header), $Header);
        }
        else
        {
            return ($Header, $Header);
        }
    }
    elsif($Header!~/\// and $GlibcHeader{$Header}
    and my $HeaderDir = find_in_defaults($Header)
    and not $GLIBC_TESTING)
    {
        return ($Header, $HeaderDir."/".$Header);
    }
    elsif(my $HeaderDir = find_in_dependencies($Header))
    {
        return ($Header, $HeaderDir."/".$Header);
    }
    elsif($Header=~/\// and my $HeaderDir = find_in_dependencies(get_FileName($Header)))
    {
        return (get_FileName($Header), $HeaderDir."/".get_FileName($Header));
    }
    elsif($DependencyHeaders_All{get_FileName($Header)})
    {
        return ($DependencyHeaders_All{get_FileName($Header)}, $DependencyHeaders_All_FullPath{get_FileName($Header)});
    }
    elsif($DefaultGccHeader{get_FileName($Header)})
    {
        return ($DefaultGccHeader{get_FileName($Header)}{"Inc"}, $DefaultGccHeader{get_FileName($Header)}{"Path"});
    }
    elsif(my @Res = search_in_public_dirs($Header))
    {
        return @Res;
    }
    elsif(my $HeaderDir = find_in_defaults($Header))
    {
        return ($Header, $HeaderDir."/".$Header);
    }
    elsif($Header=~/\// and my $AnyPath = selectSystemHeader($Header))
    {
        return ($Header, $AnyPath);
    }
    elsif($Header=~/\// and  my @Res = search_in_public_dirs(get_FileName($Header)))
    {
        return @Res;
    }
    elsif($Header=~/\// and my $HeaderDir = find_in_defaults(get_FileName($Header)))
    {
        return (get_FileName($Header), $HeaderDir."/".get_FileName($Header));
    }
    elsif($DefaultCppHeader{get_FileName($Header)})
    {
        return ($DefaultCppHeader{get_FileName($Header)}{"Inc"}, $DefaultCppHeader{get_FileName($Header)}{"Path"});
    }
    elsif($Header!~/\// and $Header_Prefix{$Header}
    and my $AnyPath = selectSystemHeader($Header_Prefix{$Header}."/".$Header))
    {# gfile.h in glib-2.0/gio/ and poppler/goo
        return ($Header_Prefix{$Header}."/".$Header, $AnyPath);
    }
    elsif(my $AnyPath = selectSystemHeader($Header))
    {
        return (get_FileName($AnyPath), $AnyPath);
    }
    elsif($Header=~/\// and my $AnyPath = selectSystemHeader(get_FileName($Header)))
    {
        return (get_FileName($AnyPath), $AnyPath);
    }
    else
    {
        return ("", "");
    }
}

sub search_in_public_dirs($)
{
    my $Header = $_[0];
    return () if(not $Header);
    my @DefaultDirs = ("sys", "netinet");
    foreach my $Dir (@DefaultDirs)
    {
        if(my $HeaderDir = find_in_defaults($Dir."/".$Header))
        {
            return ($Dir."/".$Header, $HeaderDir."/".$Dir."/".$Header);
        }
    }
    return ();
}

sub selectSystemHeader($)
{
    my $FilePath = $_[0];
    return $FilePath if(-f $FilePath);
    return "" if($FilePath=~/\A\// and not -f $FilePath);
    return "" if($FilePath=~/\A(atomic|config|build|conf|setup)\.h\Z/);
    return $Cache{"selectSystemHeader"}{$FilePath} if(defined $Cache{"selectSystemHeader"}{$FilePath});
    if($FilePath!~/\//)
    {
        foreach my $Path (keys(%{$SystemPaths{"include"}}))
        {# search in default paths
            if(-f $Path."/".$FilePath)
            {
                $Cache{"selectSystemHeader"}{$FilePath} = $Path."/".$FilePath;
                return $Path."/".$FilePath;
            }
        }
    }
    detectSystemHeaders() if(not keys(%SystemHeaders));
    foreach my $Path (sort {get_depth($a)<=>get_depth($b)} sort {cmp_paths($b, $a)}
    keys(%{$SystemHeaders{get_FileName($FilePath)}}))
    {
        if($Path=~/\/\Q$FilePath\E\Z/)
        {
            $Cache{"selectSystemHeader"}{$FilePath} = $Path;
            return $Path;
        }
    }
    $Cache{"selectSystemHeader"}{$FilePath} = "";
    return "";
}

sub cmp_paths($$)
{
    my ($Path1, $Path2) = @_;
    my @Parts1 = split(/\//, $Path1);
    my @Parts2 = split(/\//, $Path2);
    foreach my $Num (0 .. $#Parts1)
    {
        my $Part1 = $Parts1[$Num];
        my $Part2 = $Parts2[$Num];
        if($GlibcDir{$Part1}
        and not $GlibcDir{$Part2})
        {
            return 1;
        }
        elsif($GlibcDir{$Part2}
        and not $GlibcDir{$Part1})
        {
            return -1;
        }
        elsif($Part1=~/glib/
        and $Part2!~/glib/)
        {
            return 1;
        }
        elsif($Part1!~/glib/
        and $Part2=~/glib/)
        {
            return -1;
        }
        elsif(my $CmpRes = ($Part1 cmp $Part2))
        {
            return $CmpRes;
        }
    }
    return 0;
}

sub detectSystemHeaders()
{
    foreach my $DevelPath (keys(%{$SystemPaths{"include"}}))
    {
        if(-d $DevelPath)
        {
            foreach my $Path (cmd_find($DevelPath,"f",""))
            {
                $SystemHeaders{get_FileName($Path)}{$Path}=1;
                if(get_depth($Path)>=3 and my $Prefix = get_FileName(get_Directory($Path)))
                {
                    $SystemHeaders{$Prefix."/".get_FileName($Path)}{$Path}=1;
                }
            }
        }
    }
}

sub detectSystemObjects()
{
    foreach my $DevelPath (keys(%{$SystemPaths{"lib"}}))
    {
        if(-d $DevelPath)
        {
            foreach my $Path (cmd_find($DevelPath,"","*\.so*"))
            {
                $SystemObjects{cut_so_suffix(get_FileName($Path))}{$Path}=1;
                $SystemObjects{cut_lib_suffix(get_FileName($Path))}{$Path}=1;
            }
        }
    }
}

sub cut_lib_suffix($)
{
    my $Name = $_[0];
    return "" if(not $Name);
    $Name = cut_so_suffix($Name);
    $Name=~s/[\d-._]*\.so\Z//g;
    return $Name;
}

sub alignSpaces($)
{
    my $Code = $_[0];
    my $Code_Copy = $Code;
    my ($MinParagraph, $Paragraph);
    while($Code=~s/\A([ ]+)//)
    {
        $MinParagraph = length($1) if(not defined $MinParagraph or $MinParagraph>length($1));
    }
    foreach (1 .. $MinParagraph)
    {
        $Paragraph .= " ";
    }
    $Code_Copy=~s/(\A|\n)$Paragraph/$1/g;
    return $Code_Copy;
}

sub alignCode($$$)
{
    my ($Code, $Code_Align, $Single) = @_;
    return "" if($Code eq "" or $Code_Align eq "");
    my $Paragraph = get_paragraph($Code_Align, 0);
    $Code=~s/\n([^\n])/\n$Paragraph$1/g;
    if(not $Single)
    {
        $Code=~s/\A/$Paragraph/g;
    }
    return $Code;
}

sub get_paragraph($$)
{
    my ($Code, $MaxMin) = @_;
    my ($MinParagraph_Length, $MinParagraph);
    while($Code=~s/\A([ ]+)//)
    {
        if(not defined $MinParagraph_Length or
            (($MaxMin)?$MinParagraph_Length<length($1):$MinParagraph_Length>length($1)))
        {
            $MinParagraph_Length = length($1);
        }
    }
    foreach (1 .. $MinParagraph_Length)
    {
        $MinParagraph .= " ";
    }
    return $MinParagraph;
}

sub appendFile($$)
{
    my ($Path, $Content) = @_;
    return if(not $Path);
    if(my $Dir = get_Directory($Path))
    {
        mkpath($Dir);
    }
    open (FILE, ">>".$Path) || die ("can't open file \'$Path\': $!\n");
    print FILE $Content;
    close(FILE);
}

sub writeFile($$)
{
    my ($Path, $Content) = @_;
    return if(not $Path);
    if(my $Dir = get_Directory($Path))
    {
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
    my $Content = join("", <FILE>);
    close(FILE);
    return toUnix($Content);
}

sub toUnix($)
{
    my $Text = $_[0];
    $Text=~s/\r//g;
    return $Text;
}

sub get_RunScript($)
{
    my $Interface = $_[0];
    my $Lib_Exp = "";
    my %Dir = ();
    foreach my $Lib_Path (sort {length($a)<=>length($b)} keys(%SharedObjects))
    {
        my $Lib_Dir = get_Directory($Lib_Path);
        next if($Dir{$Lib_Dir} or $DefaultLibPaths{$Lib_Dir});
        $Lib_Exp .= ":$Lib_Dir";
        $Dir{$Lib_Dir} = 1;
    }
    if($Lib_Exp)
    {
        $Lib_Exp = "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH\"".$Lib_Exp."\"";
        return "#!/bin/sh\n$Lib_Exp && ./test arg1 arg2 arg3 >output 2>&1\n";
    }
    else
    {
        return "#!/bin/sh\n./test arg1 arg2 arg3 >output 2>&1\n";
    }
}

sub sort_include_paths(@)
{
    return sort {$Header_Dependency{$b}<=>$Header_Dependency{$a}} @_;
}

sub get_HeaderDeps($)
{
    my $AbsPath = $_[0];
    return () if(not $AbsPath);
    return @{$Cache{"get_HeaderDeps"}{$AbsPath}} if(defined $Cache{"get_HeaderDeps"}{$AbsPath});
    my %IncDir = ();
    detect_recursive_includes($AbsPath);
    foreach my $HeaderPath (keys(%{$RecursiveIncludes{$AbsPath}}))
    {
        next if(not $HeaderPath or $HeaderPath=~/\A\Q$MAIN_CPP_DIR\E(\/|\Z)/);
        my $Dir = get_Directory($HeaderPath);
        foreach my $Prefix (keys(%{$Header_Include_Prefix{$AbsPath}{$HeaderPath}}))
        {
            my $Dir_Part = $Dir;
            if($Prefix)
            {
                if(not $Dir_Part=~s/[\/]+\Q$Prefix\E\Z//g
                and not is_default_include_dir($Dir_Part))
                {
                    foreach (0 .. get_depth($Prefix))
                    {
                        $Dir_Part=~s/[\/]+[^\/]+?\Z//g;
                    }
                }
            }
            else
            {
                $Dir_Part=~s/[\/]+\Z//g;
            }
            next if(not $Dir_Part or is_default_include_dir($Dir_Part) or get_depth($Dir_Part)==1
            or ($DefaultIncPaths{get_Directory($Dir_Part)} and $GlibcDir{get_FileName($Dir_Part)}));
            $IncDir{$Dir_Part}=1;
        }
    }
    @{$Cache{"get_HeaderDeps"}{$AbsPath}} = sort_include_paths(sort {get_depth($a)<=>get_depth($b)} sort {$b cmp $a} keys(%IncDir));
    return @{$Cache{"get_HeaderDeps"}{$AbsPath}};
}

sub is_default_include_dir($)
{
    my $Dir = $_[0];
    $Dir=~s/[\/]+\Z//;
    return ($DefaultGccPaths{$Dir} or $DefaultCppPaths{$Dir} or $DefaultIncPaths{$Dir});
}

sub cut_so_suffix($)
{
    my $Name = $_[0];
    $Name=~s/(?<=\.so)\.[0-9.]+\Z//g;
    return $Name;
}

sub get_shared_object_deps($)
{
    my $Path = $_[0];
    return () if(not $Path or not -e $Path);
    my $LibName = get_FileName($Path);
    return if(isCyclical(\@RecurLib, $LibName));
    push(@RecurLib, $LibName);
    my %Deps = ();
    foreach my $Dep (keys(%{$SystemObjects_Needed{$Path}}))
    {
        $Deps{$Dep}=1;
        foreach my $RecurDep (get_shared_object_deps($Dep))
        {
            $Deps{$RecurDep}=1;
        }
    }
    pop(@RecurLib);
    return keys(%Deps);
}

sub get_library_pure_symlink($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    my ($Directory, $Name) = separatePath($Path);
    $Name=~s/[0-9.]+\Z//g;
    $Name=~s/[\-0-9.]+\.so\Z/.so/g;
    my $Candidate = $Directory."/".$Name;
    if(-f $Candidate and resolve_symlink($Candidate) eq $Path)
    {
        return $Candidate;
    }
    else
    {
        return "";
    }
}

sub get_Makefile($$)
{
    my ($Interface, $HeadersList) = @_;
    my (%LibPaths, %LibSuffixes, %SpecLib_Paths) = ();
    foreach my $SpecLib (keys(%SpecLibs))
    {
        if(my $SpecLib_Path = find_solib_path($SpecLib))
        {
            $SpecLib_Paths{$SpecLib_Path} = 1;
        }
    }
    my (%UsedSharedObjects, %UsedSharedObjects_Needed) = ();
    foreach my $Interface (keys(%UsedInterfaces))
    {
        if(my $Path = $Interface_Library{$Interface})
        {
            $UsedSharedObjects{$Path}=1;
        }
        elsif(my $Path = $NeededInterface_Library{$Interface})
        {
            $UsedSharedObjects{$Path}=1;
        }
        else
        {
            foreach my $SoPath (find_symbol_libs($Interface))
            {
                $UsedSharedObjects{$SoPath}=1;
            }
        }
    }
    if(not keys(%UsedSharedObjects))
    {
        %UsedSharedObjects = %SharedObjects;
    }
    foreach my $Path (keys(%UsedSharedObjects))
    {
        foreach my $Dep (get_shared_object_deps($Path))
        {
            $UsedSharedObjects_Needed{$Dep}=1;
        }
    }
    my $Libs = "";
    foreach my $Path (sort (keys(%UsedSharedObjects), keys(%CompilerOptions_Libs), keys(%UsedSharedObjects_Needed), keys(%SpecLib_Paths)))
    {
        if(my $Link = get_library_pure_symlink($Path))
        {
            $Path = $Link;
        }
        if(($Path=~/\.so\Z/ or -f cut_so_suffix($Path)) and $Path=~/\A(.*)\/lib([^\/]+)\.so[^\/]*\Z/)
        {
            $LibPaths{$1} = 1;
            $LibSuffixes{$2} = 1;
        }
        else
        {
            $Libs .= " ".$Path;
        }
    }
    foreach my $Path (keys(%LibPaths))
    {
        next if(not $Path or $DefaultLibPaths{$Path});
        $Libs .= " -L".$Path;
    }
    foreach my $Suffix (keys(%LibSuffixes))
    {
        $Libs .= " -l".$Suffix;
    }
    my %IncDir = get_HeaderDeps_forList(%{$HeadersList});
    my $Headers_Depend = "";
    foreach my $Dir (sort_include_paths(sort {get_depth($a)<=>get_depth($b)} sort {$b cmp $a} keys(%IncDir)))
    {
        $Headers_Depend .= " -I".esc($Dir);
    }
    if(getTestLang($Interface) eq "C++")
    {
        my $Makefile = "CXX      = $GPP_PATH\nCXXFLAGS = -Wall$CompilerOptions_Headers".($Headers_Depend?"\nINCLUDES =$Headers_Depend":"").($Libs?"\nLIBS     =$Libs":"")."\n\nall: test\n\n";
        $Makefile .= "test: test.cpp\n\t\$(CXX) \$(CXXFLAGS)".($Headers_Depend?" \$(INCLUDES)":"")." test.cpp -o test".($Libs?" \$(LIBS)":"")."\n\n";
        $Makefile .= "clean:\n\trm -f test test\.o\n";
        return $Makefile;
    }
    else
    {
        my $Makefile = "CC       = $GCC_PATH\nCFLAGS   = -Wall$CompilerOptions_Headers".($Headers_Depend?"\nINCLUDES =$Headers_Depend":"").($Libs?"\nLIBS     =$Libs":"")."\n\nall: test\n\n";
        $Makefile .= "test: test.c\n\t\$(CC) \$(CFLAGS)".($Headers_Depend?" \$(INCLUDES)":"")." test.c -o test".($Libs?" \$(LIBS)":"")."\n\n";
        $Makefile .= "clean:\n\trm -f test test\.o\n";
        return $Makefile;
    }
}

sub get_one_step_title($$$$$)
{
    my ($Num, $All_Count, $Head, $Success, $Fail)  = @_;
    my $Title = "$Head: $Num/$All_Count [".cut_off_number($Num*100/$All_Count, 3)."%],";
    $Title .= " success/fail: $Success/$Fail";
    return $Title."    ";
}

sub addArrows($)
{
    my $Text = $_[0];
    $Text=~s/\-\>/&minus;&gt;/g;
    return $Text;
}

sub insertIDs($)
{
    my $Text = $_[0];
    
    while($Text=~/CONTENT_ID/)
    {
        if(int($Content_Counter)%2)
        {
            $ContentID -= 1;
        }
        $Text=~s/CONTENT_ID/c_$ContentID/;
        $ContentID += 1;
        $Content_Counter += 1;
    }
    return $Text;
}

sub cut_off_number($$)
{
    my ($num, $digs_to_cut) = @_;
    if($num!~/\./)
    {
        $num .= ".";
        foreach (1 .. $digs_to_cut-1)
        {
            $num .= "0";
        }
    }
    elsif($num=~/\.(.+)\Z/ and length($1)<$digs_to_cut-1)
    {
        foreach (1 .. $digs_to_cut - 1 - length($1))
        {
            $num .= "0";
        }
    }
    elsif($num=~/\d+\.(\d){$digs_to_cut,}/)
    {
      $num=sprintf("%.".($digs_to_cut-1)."f", $num);
    }
    return $num;
}

sub generate_tests()
{
    rmtree($TEST_SUITE_PATH) if(-e $TEST_SUITE_PATH);
    mkpath($TEST_SUITE_PATH);
    ($ResultCounter{"Gen"}{"Success"}, $ResultCounter{"Gen"}{"Fail"}) = (0, 0);
    my %TargetInterfaces = ();
    if($TargetHeaderName)
    {# from the header file
        if(not $DependencyHeaders_All{$TargetHeaderName})
        {
            print STDERR "ERROR: specified header \'$TargetHeaderName\' was not found\n";
            return;
        }
        foreach my $Interface (keys(%Interface_Library))
        {
            if($CompleteSignature{$Interface}{"Header"} eq $TargetHeaderName
            and interfaceFilter($Interface))
            {
                $TargetInterfaces{$Interface} = 1;
            }
        }
    }
    elsif(keys(%InterfacesList))
    {# from the list
        foreach my $Interface (keys(%InterfacesList))
        {
            if(interfaceFilter($Interface))
            {
                $TargetInterfaces{$Interface} = 1;
            }
        }
    }
    elsif(keys(%Interface_Library) and (values(%Interface_Library))[0] ne "WithoutLib")
    {# from the shared objects (default)
        foreach my $Interface (keys(%Interface_Library))
        {
            if(interfaceFilter($Interface))
            {
                $TargetInterfaces{$Interface} = 1;
            }
        }
        foreach my $Interface (keys(%NeededInterface_Library))
        {
            if($RegisteredHeaders_Short{$CompleteSignature{$Interface}{"Header"}}
            and interfaceFilter($Interface))
            {
                $TargetInterfaces{$Interface} = 1;
            }
        }
    }
    elsif($NoLibs and keys(%CompleteSignature))
    {# from the headers
        foreach my $Interface (keys(%CompleteSignature))
        {
            next if($Interface=~/\A__/ or $CompleteSignature{$Interface}{"ShortName"}=~/\A__/);
            next if(not $DependencyHeaders_All{$CompleteSignature{$Interface}{"Header"}});
            if(interfaceFilter($Interface))
            {
                $TargetInterfaces{$Interface} = 1;
            }
        }
    }
    if(not keys(%TargetInterfaces))
    {
        print STDERR "ERROR: specified information is not enough for generating tests\n";
        return;
    }
    unlink($TEST_SUITE_PATH."/scenario");
    open(FAIL_LIST, ">$TEST_SUITE_PATH/gen_fail_list");
    if(defined $Template2Code)
    {
        if(keys(%LibGroups))
        {
            my %LibGroups_Filtered = ();
            my ($Test_Num, $All_Count) = (0, 0);
            foreach my $LibGroup (sort {lc($a) cmp lc($b)} keys(%LibGroups))
            {
                foreach my $Interface (keys(%{$LibGroups{$LibGroup}}))
                {
                    if($TargetInterfaces{$Interface})
                    {
                        $LibGroups_Filtered{$LibGroup}{$Interface} = 1;
                        $All_Count+=1;
                    }
                }
            }
            foreach my $LibGroup (sort {lc($a) cmp lc($b)} keys(%LibGroups_Filtered))
            {
                my @Ints = sort {lc($a) cmp lc($b)} keys(%{$LibGroups_Filtered{$LibGroup}});
                t2c_group_generation($LibGroup, "", \@Ints, 0, \$Test_Num, $All_Count);
            }
            print "\r".get_one_step_title($All_Count, $All_Count, "generating tests", $ResultCounter{"Gen"}{"Success"}, $ResultCounter{"Gen"}{"Fail"})."\n";
        }
        else
        {
            my $TwoComponets = 0;
            my %Header_Class_Interface = ();
            my ($Test_Num, $All_Count) = (0, int(keys(%TargetInterfaces)));
            foreach my $Interface (sort {lc($a) cmp lc($b)} keys(%TargetInterfaces))
            {
                my %Signature = %{$CompleteSignature{$Interface}};
                $Header_Class_Interface{$Signature{"Header"}}{get_type_short_name(get_TypeName($Signature{"Class"}))}{$Interface}=1;
                if($Signature{"Class"})
                {
                    $TwoComponets=1;
                }
            }
            foreach my $Header (sort {lc($a) cmp lc($b)} keys(%Header_Class_Interface))
            {
                foreach my $ClassName (sort {lc($a) cmp lc($b)} keys(%{$Header_Class_Interface{$Header}}))
                {
                    my @Ints = sort {lc($a) cmp lc($b)} keys(%{$Header_Class_Interface{$Header}{$ClassName}});
                    t2c_group_generation($Header, $ClassName, \@Ints, $TwoComponets, \$Test_Num, $All_Count);
                }
            }
            print "\r".get_one_step_title($All_Count, $All_Count, "generating tests", $ResultCounter{"Gen"}{"Success"}, $ResultCounter{"Gen"}{"Fail"})."\n";
        }
        writeFile("$TEST_SUITE_PATH/$TargetLibraryName-t2c/$TargetLibraryName.cfg", "# Custom compiler options\nCOMPILER_FLAGS = -DCHECK_EXT_REQS `pkg-config --cflags ".search_pkgconfig_name($TargetLibraryName)."` -D_GNU_SOURCE\n\n# Custom linker options\nLINKER_FLAGS = `pkg-config --libs ".search_pkgconfig_name($TargetLibraryName)."`\n\n# Maximum time (in seconds) each test is allowed to run\nWAIT_TIME = $HANGED_EXECUTION_TIME\n\n# Copyright holder\nCOPYRIGHT_HOLDER = Linux Foundation\n");
    }
    else
    {#standalone
        my $Test_Num = 0;
        if(keys(%LibGroups))
        {
            foreach my $Interface (keys(%TargetInterfaces))
            {
                if(not $Interface_LibGroup{$Interface})
                {
                    delete($TargetInterfaces{$Interface});
                }
            }
        }
        my $All_Count = keys(%TargetInterfaces);
        foreach my $Interface (sort {lc($a) cmp lc($b)} keys(%TargetInterfaces))
        {
            print "\r".get_one_step_title($Test_Num, $All_Count, "generating tests", $ResultCounter{"Gen"}{"Success"}, $ResultCounter{"Gen"}{"Fail"});
            #resetting global state
            restore_state(());
            @RecurInterface = ();
            @RecurTypeId = ();
            @RecurSpecType = ();
            %SubClass_Created = ();
            my %Result = generate_sanity_test($Interface);
            if(not $Result{"IsCorrect"})
            {
                print FAIL_LIST $Interface."\n";
            }
            $Test_Num += 1;
        }
        write_scenario();
        print "\r".get_one_step_title($All_Count, $All_Count, "generating tests", $ResultCounter{"Gen"}{"Success"}, $ResultCounter{"Gen"}{"Fail"})."\n";
        restore_state(());
    }
    close(FAIL_LIST);
    unlink($TEST_SUITE_PATH."/gen_fail_list") if(not readFile($TEST_SUITE_PATH."/gen_fail_list"));
}

sub get_pkgconfig_name($)
{
    my $Name = $_[0];
    if(my $PkgConfig = get_CmdPath("pkg-config"))
    {
        my %InstalledLibs = ();
        foreach my $Line (split(/\n/, `$PkgConfig --list-all`))
        {
            if($Line=~/\A([^ ]+)[ ]+(.+?)\Z/)
            {
                $InstalledLibs{$1}=$2;
            }
        }
        foreach my $Lib (sort {length($a)<=>length($b)} keys(%InstalledLibs))
        {
            if($Lib=~/([^a-z]|\A)$Name([^a-z]|\Z)/i)
            {
                return $Lib;
            }
        }
        return "";
    }
    else
    {
        foreach my $PkgPath (keys(%{$SystemPaths{"pkgconfig"}}))
        {
            if(-d $PkgPath)
            {
                foreach my $Path (sort {length($a)<=>length($b)} split(/\n/, `find $PkgPath -iname "*$Name*"`))
                {
                    my $LibName = get_FileName($Path);
                    $LibName=~s/\.pc\Z//;
                    if($LibName=~/([^a-z]|\A)\Q$Name\E([^a-z]|\Z)/i)
                    {
                        return $LibName;
                    }
                }
            }
        }
        return "";
    }
}

sub search_pkgconfig_name($)
{
    my $Name = $_[0];
    my $PkgConfigName = get_pkgconfig_name($Name);
    return $PkgConfigName if($PkgConfigName);
    if($Name=~s/[0-9_-]+\Z//)
    {
        if($PkgConfigName = get_pkgconfig_name($Name))
        {
            return $PkgConfigName;
        }
    }
    if($Name=~s/\Alib//i)
    {
        if($PkgConfigName = get_pkgconfig_name($Name))
        {
            return $PkgConfigName;
        }
    }
    else
    {
        $Name = "lib".$Name;
        if($PkgConfigName = get_pkgconfig_name($Name))
        {
            return $PkgConfigName;
        }
    }
    return lc($Name);
}

sub t2c_group_generation($$$$$$)
{
    my ($C1, $C2, $Interfaces, $TwoComponets, $Test_NumRef, $All_Count) = @_;
    my ($SuitePath, $MediumPath, $TestName) = getLibGroupPath($C1, $C2, $TwoComponets);
    my $MaxLength = 0;
    my $LibGroupName = getLibGroupName($C1, $C2);
    my %TestComponents = ();
    #resetting global state for section
    restore_state(());
    foreach my $Interface (@{$Interfaces})
    {
        print "\r".get_one_step_title(${$Test_NumRef}, $All_Count, "generating tests", $ResultCounter{"Gen"}{"Success"}, $ResultCounter{"Gen"}{"Fail"});
        restore_local_state(());
        %IntrinsicNum=(
        "Char"=>64,
        "Int"=>0,
        "Str"=>0,
        "Float"=>0);
        @RecurInterface = ();
        @RecurTypeId = ();
        @RecurSpecType = ();
        %SubClass_Created = ();
        my $Global_State = save_state();
        my %Result = generate_sanity_test($Interface);
        if(not $Result{"IsCorrect"})
        {
            restore_state($Global_State);
            print FAIL_LIST $Interface."\n";
        }
        ${$Test_NumRef} += 1;
        $TestComponents{"Headers"} = addHeaders($TestComponents{"Headers"}, $Result{"Headers"});
        $TestComponents{"Code"} .= $Result{"Code"};
        my ($DefinesList, $ValuesList) = list_t2c_defines();
        $TestComponents{"Blocks"} .= "##=========================================================================\n## ".get_Signature($Interface)."\n\n<BLOCK>\n\n<TARGETS>\n    ".$CompleteSignature{$Interface}{"ShortName"}."\n</TARGETS>\n\n".(($DefinesList)?"<DEFINE>\n".$DefinesList."</DEFINE>\n\n":"")."<CODE>\n".$Result{"main"}."</CODE>\n\n".(($ValuesList)?"<VALUES>\n".$ValuesList."</VALUES>\n\n":"")."</BLOCK>\n\n\n";
        $MaxLength = length($CompleteSignature{$Interface}{"ShortName"}) if($MaxLength<length($CompleteSignature{$Interface}{"ShortName"}));
    }
    #adding test data
    my $TestDataDir = $SuitePath."/testdata/".(($MediumPath)?"$MediumPath/":"")."$TestName/";
    mkpath($TestDataDir);
    $TestComponents{"Blocks"} = add_VirtualTestData($TestComponents{"Blocks"}, $TestDataDir);
    $TestComponents{"Blocks"} = add_TestData($TestComponents{"Blocks"}, $TestDataDir);
    my $Content = "#library $TargetLibraryName\n#libsection $LibGroupName\n\n<GLOBAL>\n\n// Tested here:\n";
    foreach my $Interface (@{$Interfaces})
    {#development progress
        $Content .= "// ".$CompleteSignature{$Interface}{"ShortName"};
        foreach (0 .. $MaxLength - length($CompleteSignature{$Interface}{"ShortName"}) + 2)
        {
            $Content .= " ";
        }
        $Content .= "DONE (shallow)\n";
    }
    $Content .= "\n";
    foreach my $Header (@{$TestComponents{"Headers"}})
    {#includes
        $Content .= "#include <$Header>\n";
    }
    $Content .= "\n".$TestComponents{"Code"}."\n" if($TestComponents{"Code"});
    $Content .= "\n</GLOBAL>\n\n".$TestComponents{"Blocks"};
    writeFile($SuitePath."/src/".(($MediumPath)?"$MediumPath/":"")."$TestName.t2c", $Content);
    writeFile($SuitePath."/reqs/".(($MediumPath)?"$MediumPath/":"")."$TestName.xml", get_requirements_catalog($Interfaces));
}

sub get_requirements_catalog($)
{
    my @Interfaces = @{$_[0]};
    my $Reqs = "";
    foreach my $Interface (@Interfaces)
    {
        foreach my $ReqId (sort {int($a)<=>int($b)} keys(%{$RequirementsCatalog{$Interface}}))
        {
            my $Req = $RequirementsCatalog{$Interface}{$ReqId};
            $Req=~s/&/&amp;/g;
            $Req=~s/>/&gt;/g;
            $Req=~s/</&lt;/g;
            $Reqs .= "<req id=\"".$CompleteSignature{$Interface}{"ShortName"}.".".normalize_num($ReqId)."\">\n    $Req\n</req>\n";
        }
    }
    if(not $Reqs)
    {
        $Reqs = "<req id=\"function.01\">\n    If ... then ...\n</req>\n";
    }
    return "<?xml version=\"1.0\"?>\n<requirements>\n".$Reqs."</requirements>\n";
}

sub list_t2c_defines()
{
    my (%Defines, $DefinesList, $ValuesList) = ();
    my $MaxLength = 0;
    foreach my $Define (sort keys(%Template2Code_Defines))
    {
        if($Define=~/\A(\d+):(.+)\Z/)
        {
            $Defines{$1}{"Name"} = $2;
            $Defines{$1}{"Value"} = $Template2Code_Defines{$Define};
            $MaxLength = length($2) if($MaxLength<length($2));
        }
    }
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%Defines))
    {
        $DefinesList .= "#define ".$Defines{$Pos}{"Name"};
        foreach (0 .. $MaxLength - length($Defines{$Pos}{"Name"}) + 2)
        {
            $DefinesList .= " ";
        }
        $DefinesList .= "<%$Pos%>\n";
        $ValuesList .= "    ".$Defines{$Pos}{"Value"}."\n";
    }
    return ($DefinesList, $ValuesList);
}

sub cut_off($$)
{
    my ($String, $Num) = @_;
    if(length($String)<$Num)
    {
        return $String;
    }
    else
    {
        return substr($String, 0, $Num)."...";
    }
}

sub build_tests()
{
    if(-e $TEST_SUITE_PATH."/build_fail_list")
    {
        unlink($TEST_SUITE_PATH."/build_fail_list");
    }
    ($ResultCounter{"Build"}{"Success"}, $ResultCounter{"Build"}{"Fail"}) = (0, 0);
    read_scenario();
    return if(not keys(%Interface_TestDir));
    my $All_Count = keys(%Interface_TestDir);
    my $Test_Num = 0;
    open(FAIL_LIST, ">$TEST_SUITE_PATH/build_fail_list");
    foreach my $Interface (sort {lc($a) cmp lc($b)} keys(%Interface_TestDir))
    {#building tests
        print "\r".get_one_step_title($Test_Num, $All_Count, "building tests", $ResultCounter{"Build"}{"Success"}, $ResultCounter{"Build"}{"Fail"});
        build_sanity_test($Interface);
        if(not $BuildResult{$Interface}{"IsCorrect"})
        {
            print FAIL_LIST $Interface_TestDir{$Interface}."\n";
        }
        $Test_Num += 1;
    }
    close(FAIL_LIST);
    unlink($TEST_SUITE_PATH."/build_fail_list") if(not readFile($TEST_SUITE_PATH."/build_fail_list"));
    print "\r".get_one_step_title($All_Count, $All_Count, "building tests", $ResultCounter{"Build"}{"Success"}, $ResultCounter{"Build"}{"Fail"})."\n";
}

sub clean_tests()
{
    read_scenario();
    return if(not keys(%Interface_TestDir));
    my $All_Count = keys(%Interface_TestDir);
    my $Test_Num = 0;
    foreach my $Interface (sort {lc($a) cmp lc($b)} keys(%Interface_TestDir))
    {#cleaning tests
        print "\r".get_one_step_title($Test_Num, $All_Count, "cleaning tests", $Test_Num, 0);
        clean_sanity_test($Interface);
        $Test_Num += 1;
    }
    print "\r".get_one_step_title($All_Count, $All_Count, "cleaning tests", $All_Count, 0)."\n";
}

sub run_tests()
{
    if(-f $TEST_SUITE_PATH."/run_fail_list")
    {
        unlink($TEST_SUITE_PATH."/run_fail_list");
    }
    ($ResultCounter{"Run"}{"Success"}, $ResultCounter{"Run"}{"Fail"}) = (0, 0);
    read_scenario();
    if(not keys(%Interface_TestDir))
    {
        print STDERR "ERROR: tests were not generated yet\n";
        return 1;
    }
    my %ForRunning = ();
    foreach my $Interface (keys(%Interface_TestDir))
    {
        if(-f $Interface_TestDir{$Interface}."/test")
        {
            $ForRunning{$Interface} = 1;
        }
    }
    my $All_Count = keys(%ForRunning);
    if($All_Count==0)
    {
        print STDERR "ERROR: tests were not built yet\n";
        return 1;
    }
    my $Test_Num = 0;
    open(FAIL_LIST, ">$TEST_SUITE_PATH/run_fail_list");
    my $XvfbStarted = 0;
    $XvfbStarted = runXvfb() if($UseXvfb);
    foreach my $Interface (sort {lc($a) cmp lc($b)} keys(%ForRunning))
    {#running tests
        print "\r".get_one_step_title($Test_Num, $All_Count, "running tests", $ResultCounter{"Run"}{"Success"}, $ResultCounter{"Run"}{"Fail"});
        run_sanity_test($Interface);
        if(not $RunResult{$Interface}{"IsCorrect"})
        {
            print FAIL_LIST $Interface_TestDir{$Interface}."\n";
        }
        $Test_Num += 1;
    }
    stopXvfb($XvfbStarted) if($UseXvfb);
    close(FAIL_LIST);
    unlink($TEST_SUITE_PATH."/run_fail_list") if(not readFile($TEST_SUITE_PATH."/run_fail_list"));
    print "\r".get_one_step_title($All_Count, $All_Count, "running tests", $ResultCounter{"Run"}{"Success"}, $ResultCounter{"Run"}{"Fail"})."\n";
    return 0;
}

sub detectPointerSize()
{
    mkpath(".tmpdir");
    writeFile(".tmpdir/get_pointer_size.c", "#include <stdio.h>
int main()
{
    printf(\"\%d\", sizeof(int*));
    return 0;
}\n");
    system("$GCC_PATH .tmpdir/get_pointer_size.c -o .tmpdir/get_pointer_size");
    $POINTER_SIZE = `./.tmpdir/get_pointer_size`;
    rmtree(".tmpdir");
}

sub init_signals()
{
    return if(not defined $Config{"sig_name"}
    or not defined $Config{"sig_num"});
    my $No = 0;
    my @Numbers = split(/\s/, $Config{"sig_num"} );
    foreach my $Name (split(/\s/, $Config{"sig_name"})) 
    {
        if(not $SigName{$Numbers[$No]} or $Name=~/\A(SEGV|ABRT)\Z/)
        {
            $SigNo{$Name} = $Numbers[$No];
            $SigName{$Numbers[$No]} = $Name;
        }
        $No+=1;
    }
}

sub genDescriptorTemplate()
{
    writeFile("lib_ver.xml", $Descriptor_Template."\n");
    print "descriptor template 'lib_ver.xml' has been generated into the current directory\n";
}

sub genSpecTypeTemplate()
{
    writeFile("spectypes.xml", $SpecType_Template."\n");
    print "specialized type template 'spectypes.xml' has been generated into the current directory\n";
}

sub testSystem_cpp()
{
    print "testing C++ library API\n";
    my (@DataDefs, @Sources)  = ();
    
    #Simple parameters
    @DataDefs = (@DataDefs, "//Simple parameters\nint func_simple_parameters(\n    int a,\n    float b,\n    double c,\n    long double d,\n    long long e,\n    char f,\n    unsigned int g,\n    const char* h,\n    char* i,\n    unsigned char* j,\n    char** k,\n    const char*& l,\n    const char**& m,\n    char const*const* n,\n    unsigned int* offset\n);");
    @Sources = (@Sources, "//Simple parameters\nint func_simple_parameters(\n    int a,\n    float b,\n    double c,\n    long double d,\n    long long e,\n    char f,\n    unsigned int g,\n    const char* h,\n    char* i,\n    unsigned char* j,\n    char** k,\n    const char*& l,\n    const char**& m,\n    char const*const* n,\n    unsigned int* offset\n)\n{\n    return 1;\n}");
    
    #Initialization by interface
    @DataDefs = (@DataDefs, "//Initialization by interface\nstruct simple_struct {\n    int m;\n};\nstruct simple_struct simple_func(int a, int b);");
    @Sources = (@Sources, "struct simple_struct simple_func(int a, int b){return (struct simple_struct){1};}");
    
    @DataDefs = (@DataDefs, "int func_init_param_by_interface(struct simple_struct p);");
    @Sources = (@Sources, "//Initialization by interface\nint func_init_param_by_interface(struct simple_struct p)\n{\n    return 1;\n}");
    
    #Assembling structure
    @DataDefs = (@DataDefs, "//Assembling structure\nstruct complex_struct {\n    int a;\n    float b;\n    struct complex_struct* c;\n};");
    
    @DataDefs = (@DataDefs, "int func_assemble_param(struct complex_struct p);");
    @Sources = (@Sources, "//Assembling structure\nint func_assemble_param(struct complex_struct p)\n{\n    return 1;\n}");
    
    #Abstract class
    @DataDefs = (@DataDefs, "//Abstract class\nclass abstract_class {\n    public:\n    abstract_class(){};\n    int a;\n    virtual float virt_func(float p) = 0;\n    float func(float p);\n};");
    @Sources = (@Sources, "//Abstract class\nfloat abstract_class::func(float p)\n{\n    return p;\n}");
    
    #Parameter FuncPtr
    @DataDefs = (@DataDefs, "//Parameter FuncPtr\ntypedef int (*funcptr_type)(int a, int b);\nfuncptr_type func_return_funcptr(int a);\nint func_param_funcptr(const funcptr_type** p);");
    @Sources = (@Sources, "//Parameter FuncPtr\nfuncptr_type func_return_funcptr(int a){return 0;}\nint func_param_funcptr(const funcptr_type** p)\n{\n    return 0;\n}");
    
    #Parameter Array
    @DataDefs = (@DataDefs, "//Parameter Array\nint func_param_array(struct complex_struct const ** x);");
    @Sources = (@Sources, "//Parameter Array\nint func_param_array(struct complex_struct const ** x)\n{\n    return 0;\n}");
    
    #Nested classes
    @DataDefs = (@DataDefs, "//Nested classes
class A
{
public:
    virtual bool method1()
    {
        return false;
    };
};

class B: public A { };

class C: public B
{
public:
    C(){};
    virtual bool method1();
};");
    @Sources = (@Sources, "//Nested classes
bool C::method1()
{
    return false;
};");
    
    #Throw class
    @DataDefs = (@DataDefs, "//Exception class\nclass Exception {\n    public:\n    Exception();\n    int a;\n};");
    @Sources = (@Sources, "//Abstract class\nException::Exception()\n{}");
    @DataDefs = (@DataDefs, "//Throw class\nclass throw_class {\n    public:\n    throw_class(){};\n    int a;\n    virtual float virt_func(float p) throw(Exception) = 0;\n    float func(float p);\n};");
    @Sources = (@Sources, "//Throw class\nfloat throw_class::func(float p)\n{\n    return p;\n}");
    
    create_TestSuite("simple_lib_cpp", "C++", join("\n\n", @DataDefs), join("\n\n", @Sources), "type_test_opaque", "_ZN18type_test_internal5func1ES_");
}

sub testSystem_c()
{
    print "\ntesting C library API\n";
    my (@DataDefs, @Sources)  = ();
    
    #Simple parameters
    @DataDefs = (@DataDefs, "int func_simple_parameters(int a, float b, double c, long double d, long long e, char f, unsigned int g, const char* h, char* i, unsigned char* j, char** k);");
    @Sources = (@Sources, "int func_simple_parameters(int a, float b, double c, long double d, long long e, char f, unsigned int g, const char* h, char* i, unsigned char* j, char** k)\n{\n    return 1;\n}");
    
    #Initialization by interface
    @DataDefs = (@DataDefs, "struct simple_struct {int m;};\nstruct simple_struct simple_func(int a, int b);");
    @Sources = (@Sources, "struct simple_struct simple_func(int a, int b){return (struct simple_struct){1};}");
    
    @DataDefs = (@DataDefs, "int func_init_param_by_interface(struct simple_struct p);");
    @Sources = (@Sources, "int func_init_param_by_interface(struct simple_struct p)\n{\n    return 1;\n}");
    
    #Assembling structure
    @DataDefs = (@DataDefs, "struct complex_struct {int a;\nfloat b;\nstruct complex_struct* c;};");
    
    @DataDefs = (@DataDefs, "int func_assemble_param(struct complex_struct p);");
    @Sources = (@Sources, "int func_assemble_param(struct complex_struct p)\n{\n    return 1;\n}");
    
    #Initialization by out parameter
    @DataDefs = (@DataDefs, "struct out_opaque_struct;\nvoid create_out_param(struct out_opaque_struct** out);");
    @Sources = (@Sources, "struct out_opaque_struct {const char* str;};void create_out_param(struct out_opaque_struct** out)\n{\n    struct out_opaque_struct* res = (struct out_opaque_struct*)malloc(sizeof(struct out_opaque_struct));\n    res->str=\"string\";}\n");
    
    @DataDefs = (@DataDefs, "int func_init_param_by_out_param(struct out_opaque_struct* p);");
    @Sources = (@Sources, "int func_init_param_by_out_param(struct out_opaque_struct* p)\n{\n    return 1;\n}");
    
    #Function with out parameter
    @DataDefs = (@DataDefs, "int func_has_out_opaque_param(struct out_opaque_struct* out);");
    @Sources = (@Sources, "int func_has_out_opaque_param(struct out_opaque_struct* out)\n{\n    return 1;\n}");
    
    create_TestSuite("simple_lib_c", "C", join("\n\n", @DataDefs), join("\n\n", @Sources), "type_test_opaque", "func_test_internal");
}

sub create_TestSuite($$$$$$)
{
    my ($Name, $Lang, $DataDefs, $Sources, $Opaque, $Private) = @_;
    my $Ext = ($Lang eq "C++")?"cpp":"c";
    my $Gcc = ($Lang eq "C++")?$GPP_PATH:$GCC_PATH;
    #creating test suite
    rmtree($Name);
    mkpath($Name);
    writeFile($Name."/version", "TEST_1.0 {\n};\nTEST_2.0 {\n};\n");
    writeFile("$Name/simple_lib.h", "#include <stdlib.h>\n".$DataDefs."\n");
    writeFile("$Name/simple_lib.$Ext", "#include \"simple_lib.h\"\n".$Sources."\n");
    writeFile("$Name/descriptor", "<version>\n    1.0.0\n</version>\n\n<headers>\n    ".abs_path($Name)."\n</headers>\n\n<libs>\n    ".abs_path($Name)."\n</libs>\n\n<opaque_types>\n    $Opaque\n</opaque_types>\n\n<skip_interfaces>\n    $Private\n</skip_interfaces>\n\n<include_paths>\n    ".abs_path($Name)."\n</include_paths>\n");
    system("cd $Name && $Gcc -Wl,--version-script version -shared simple_lib.$Ext -o simple_lib.so");
    if($?)
    {
        print STDERR "ERROR: can't compile \'$Name/simple_lib.$Ext\'\n";
        return;
    }
    #running api-sanity-autotest
    system("perl $0 -l $Name -d $Name/descriptor -gen -build -run");
}

sub esc($)
{
    my $Str = $_[0];
    $Str=~s/([()\[\]{}$ &'"`;,<>])/\\$1/g;
    return $Str;
}

sub detect_default_paths()
{
    my $TargetCfg = ($Config{"osname"}=~/\A(haiku|beos)\Z/)?"haiku":"default";
    foreach my $Type (keys(%{$OperatingSystemAddPaths{$TargetCfg}}))
    {# additional search paths
        foreach my $Path (keys(%{$OperatingSystemAddPaths{$TargetCfg}{$Type}}))
        {
            next if(not -d $Path);
            $SystemPaths{$Type}{$Path} = $OperatingSystemAddPaths{$TargetCfg}{$Type}{$Path};
        }
    }
    detect_bin_default_paths();
    foreach my $Path (keys(%DefaultBinPaths))
    {
        $SystemPaths{"bin"}{$Path} = $DefaultBinPaths{$Path};
    }
    foreach my $Path (split(/\n/, `find / -maxdepth 1 -name "*bin*" -type d`))
    {# autodetecting bin directories
        $SystemPaths{"bin"}{$Path} = 1;
    }
    if($Config{"osname"}=~/\A(haiku|beos)\Z/)
    {
        foreach my $Path (split(/:|;/, $ENV{"BEINCLUDES"}))
        {
            if($Path=~/\A\//)
            {
                $DefaultIncPaths{$Path} = 1;
            }
        }
        foreach my $Path (split(/:|;/, $ENV{"BELIBRARIES"}), split(/:|;/, $ENV{"LIBRARY_PATH"}))
        {
            if($Path=~/\A\//)
            {
                $DefaultLibPaths{$Path} = 1;
            }
        }
    }
    else
    {
        foreach my $Var (keys(%ENV))
        {
            if($Var=~/INCLUDE/i)
            {
                foreach my $Path (split(/:|;/, $Var))
                {
                    if($Path=~/\A\//)
                    {
                        $SystemPaths{"include"}{$Path} = 1;
                    }
                }
            }
            elsif($Var=~/(\ALIB)|LIBRAR(Y|IES)/i)
            {
                foreach my $Path (split(/:|;/, $Var))
                {
                    if($Path=~/\A\//)
                    {
                        $SystemPaths{"lib"}{$Path} = 1;
                    }
                }
            }
        }
    }
    detect_solib_default_paths();
    foreach my $Path (keys(%DefaultLibPaths))
    {
        $SystemPaths{"lib"}{$Path} = $DefaultLibPaths{$Path};
    }
    foreach my $Path (keys(%DefaultIncPaths))
    {
        $SystemPaths{"include"}{$Path} = $DefaultIncPaths{$Path};
    }
}

sub detect_default_paths_from_gcc()
{
    $GCC_PATH = search_for_gcc("gcc");
    $GPP_PATH = search_for_gcc("g++");
    exit(1) if(not $GCC_PATH or not $GPP_PATH);
    if($GPP_PATH ne "g++")
    {# search for c++filt in the same directory as gcc
        my $CppFilt = $GPP_PATH;
        if($CppFilt=~s/\/g\+\+\Z/\/c++filt/ and -f $CppFilt)
        {
            $CPP_FILT = $CppFilt;
        }
    }
    detect_include_default_paths();
}

sub search_for_gcc($)
{
    my $Cmd = $_[0];
    return "" if(not $Cmd);
    if(check_gcc_version(get_gcc_version($Cmd), 3))
    {# some systems search for commands not only in the $PATH
        return $Cmd;
    }
    else
    {
        my $SomeGcc_Default = get_CmdPath_Default($Cmd);
        if($SomeGcc_Default
        and check_gcc_version(get_gcc_version($SomeGcc_Default), 3))
        {
            return $Cmd;
        }
        else
        {
            my $SomeGcc_System = get_CmdPath($Cmd);
            if($SomeGcc_System and check_gcc_version(get_gcc_version($SomeGcc_System), 3))
            {
                return $SomeGcc_System;
            }
            else
            {
                my $SomeGcc_Optional = "";
                foreach my $Path (keys(%{$SystemPaths{"gcc"}}))
                {
                    if(-d $Path)
                    {
                        foreach $SomeGcc_Optional (sort {$b cmp $a} cmd_find($Path,"f",$Cmd))
                        {
                            if(check_gcc_version(get_gcc_version($SomeGcc_Optional), 3))
                            {
                                return $SomeGcc_Optional;
                            }
                        }
                    }
                }
                my $SomeGcc = $Cmd if(get_gcc_version($Cmd));
                $SomeGcc = $SomeGcc_Default if($SomeGcc_Default and not $SomeGcc);
                $SomeGcc = $SomeGcc_System if($SomeGcc_System and not $SomeGcc);
                $SomeGcc = $SomeGcc_Optional if($SomeGcc_Optional and not $SomeGcc);
                if(not $SomeGcc)
                {
                    print STDERR "ERROR: can't find $Cmd\n";
                }
                else
                {
                    print STDERR "ERROR: unsupported gcc version ".get_gcc_version($SomeGcc).", needed >= 3.0.0\n";
                }
            }
        }
    }
    return "";
}

sub get_gcc_version($)
{
    my $Cmd = $_[0];
    return "" if(not $Cmd);
    my $Version = `$Cmd -dumpversion 2>/dev/null`;
    chomp($Version);
    return $Version;
}

sub check_gcc_version($$)
{
    my ($SystemVersion, $ReqVersion) = @_;
    return 0 if(not $SystemVersion or not $ReqVersion);
    my ($MainVer, $MinorVer, $MicroVer) = split(/\./, $SystemVersion);
    if($MainVer>=$ReqVersion)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

sub show_time_interval($)
{
    my $Interval = $_[0];
    my $Hr = int($Interval/3600);
    my $Min = int($Interval/60)-$Hr*60;
    my $Sec = $Interval-$Hr*3600-$Min*60;
    if($Hr)
    {
        return "$Hr hr, $Min min, $Sec sec";
    }
    elsif($Min)
    {
        return "$Min min, $Sec sec";
    }
    else
    {
        return "$Sec sec";
    }
}

sub remove_option($$)
{
    my ($OptionsRef, $Option) = @_;
    return if(not $OptionsRef or not $Option);
    $Option=esc($Option);
    my @Result = ();
    foreach my $Arg (@{$OptionsRef})
    {
        if($Arg!~/\A[-]+$Option\Z/)
        {
            push(@Result, $Arg);
        }
    }
    @{$OptionsRef} = @Result;
}

sub scenario()
{
    if(defined $Help)
    {
        HELP_MESSAGE();
        exit(0);
    }
    if(defined $ShowVersion)
    {
        print "API Sanity Autotest $API_SANITY_AUTOTEST_VERSION\nCopyright (C) The Linux Foundation\nCopyright (C) Institute for System Programming, RAS\nLicenses GPL and LGPL <http://www.gnu.org/licenses/>\nThis program is free software: you can redistribute it and/or modify it.\n\nWritten by Andrey Ponomarenko.\n";
        exit(0);
    }
    if(defined $DumpVersion)
    {
        print "$API_SANITY_AUTOTEST_VERSION\n";
        exit(0);
    }
    if(not defined $Template2Code)
    {
        $Standalone = 1;
    }
    if($GenerateDescriptorTemplate)
    {
        genDescriptorTemplate();
        exit(0);
    }
    if($GenerateSpecTypeTemplate)
    {
        genSpecTypeTemplate();
        exit(0);
    }
    detect_default_paths();
    if(defined $TestSystem)
    {
        detect_default_paths_from_gcc();
        testSystem_cpp();
        testSystem_c();
        exit(0);
    }
    if(not defined $TargetLibraryName)
    {
        print STDERR "\nERROR: library name was not selected (option -l <name>)\n";
        exit(1);
    }
    if($TestDataPath and not -d $TestDataPath)
    {
        print STDERR "\nERROR: can't access directory \'$TestDataPath\'\n";
        exit(1);
    }
    if($SpecTypes_PackagePath and not -f $SpecTypes_PackagePath)
    {
        print STDERR "\nERROR: can't access file \'$SpecTypes_PackagePath\'\n";
        exit(1);
    }
    if($InterfacesListPath)
    {
        if(-f $InterfacesListPath)
        {
            foreach my $Interface (split(/\n/, readFile($InterfacesListPath)))
            {
                $InterfacesList{$Interface} = 1;
            }
        }
        else
        {
            print STDERR "\nERROR: can't access file \'$InterfacesListPath\'\n";
            exit(1);
        }
    }
    if(not $Descriptor)
    {
        print STDERR "ERROR: library descriptor was not selected (option -d <path>)\n";
        exit(1);
    }
    if(not $GenerateTests and not $BuildTests
    and not $RunTests and not $CleanTests and not $CleanSources)
    {
        print "specify appropriate option: -gen, -build, -run, -clean\n";
        exit(1);
    }
    if($ParameterNamesFilePath)
    {
        if(-f $ParameterNamesFilePath)
        {
            foreach my $Line (split(/\n/, readFile($ParameterNamesFilePath)))
            {
                if($Line=~s/\A(\w+)\;//)
                {
                    my $Interface = $1;
                    if($Line=~/;(\d+);/)
                    {
                        while($Line=~s/(\d+);(\w+)//)
                        {
                            $AddIntParams{$Interface}{$1}=$2;
                        }
                    }
                    else
                    {
                        my $Num = 0;
                        foreach my $Name (split(/;/, $Line))
                        {
                            $AddIntParams{$Interface}{$Num}=$Name;
                            $Num+=1;
                        }
                    }
                }
            }
        }
        else
        {
            print STDERR "\nERROR: can't access file \'$ParameterNamesFilePath\'\n";
            exit(1);
        }
    }
    if($TargetInterfaceName and defined $Template2Code)
    {
        print STDERR "\nERROR: interface selecting is not supported in the Template2Code format\n";
        exit(1);
    }
    if(($BuildTests or $RunTests or $CleanTests) and defined $Template2Code
    and not defined $GenerateTests)
    {
        print STDERR "\nERROR: see Template2Code technology documentation for building and running tests:\n       http://template2code.sourceforge.net/t2c-doc/index.html\n";
        exit(1);
    }
    if($GenerateTests)
    {
        detect_default_paths_from_gcc();
        my $StartTime_Gen = time;
        readDescriptor($Descriptor);
        $TEST_SUITE_PATH = ((defined $Template2Code)?"tests_t2c":"tests")."/$TargetLibraryName/".$Descriptor{"Version"};
        $LOG_PATH = "logs/$TargetLibraryName/".$Descriptor{"Version"}."/log";
        rmtree(get_Directory($LOG_PATH));
        mkpath(get_Directory($LOG_PATH));
        if(not $NoLibs)
        {
            print "shared object(s) analysis: [0.00%]";
            getSymbols();
            print "\rshared object(s) analysis: [100.00%]\n";
            translateSymbols(keys(%Interface_Library));
        }
        print "header(s) analysis: [0.00%]";
        searchForHeaders();
        print "\rheader(s) analysis: [20.00%]";
        detectPointerSize();
        parseHeaders_AllInOne();
        print "\rheader(s) analysis: [85.00%]";
        prepareInterfaces();
        print "\rheader(s) analysis: [95.00%]";
        initializeClass_PureVirtFunc();
        if($NoLibs)
        {#detecting language
            foreach my $Interface (keys(%CompleteSignature))
            {
                $Interface_Library{$Interface}="WithoutLib";
                if($Interface=~/\A_Z/)
                {
                    $Language{"WithoutLib"}="C++";
                }
            }
            $Language{"WithoutLib"}="C" if(not $Language{"WithoutLib"});
        }
        print "\rheader(s) analysis: [100.00%]\n";
        add_os_spectypes();
        if($SplintAnnotations)
        {
            read_annotations_Splint();
        }
        readSpecTypes(readFile($SpecTypes_PackagePath)) if($SpecTypes_PackagePath);
        if($TargetInterfaceName)
        {
            if(not $CompleteSignature{$TargetInterfaceName})
            {
                print STDERR "ERROR: specified symbol was not found\n";
                if($Func_ShortName_MangledName{$TargetInterfaceName})
                {
                    if(keys(%{$Func_ShortName_MangledName{$TargetInterfaceName}})==1)
                    {
                        print STDERR "did you mean ".(keys(%{$Func_ShortName_MangledName{$TargetInterfaceName}}))[0]." ?\n";
                    }
                    else
                    {
                        print STDERR "candidates are:\n ".join("\n ", keys(%{$Func_ShortName_MangledName{$TargetInterfaceName}}))."\n";
                    }
                }
                exit(1);
            }
            print "generating test for $TargetInterfaceName ... ";
            read_scenario();
            generate_sanity_test($TargetInterfaceName);
            write_scenario();
            if($GenResult{$TargetInterfaceName}{"IsCorrect"})
            {
                print "success\n";
            }
            else
            {
                print "fail\n";
            }
            create_Index();
        }
        else
        {
            generate_tests();
            create_Index() if(not defined $Template2Code);
        }
        print "elapsed time: ".show_time_interval(time - $StartTime_Gen)."\n" if($ShowExpendTime);
        if($ResultCounter{"Gen"}{"Success"}>0)
        {
            if($TargetInterfaceName)
            {
                my $TestPath = getTestPath($TargetInterfaceName);
                print "see generated test in \'$TestPath/\'\n";
            }
            else
            {
                if($Template2Code)
                {
                    print "1. see generated test suite in the directory \'$TEST_SUITE_PATH/\'\n";
                    print "2. see Template2Code technology documentation for building and running tests:\nhttp://template2code.sourceforge.net/t2c-doc/index.html\n";
                }
                else
                {
                    print "1. see generated test suite in the directory \'$TEST_SUITE_PATH/\'\n";
                    print "2. for viewing the tests use \'$TEST_SUITE_PATH/view_tests.html\'\n";
                    print "3. use -build option for building tests\n";
                }
            }
        }
        remove_option(\@INPUT_OPTIONS, "gen");
        remove_option(\@INPUT_OPTIONS, "generate");
    }
    if($BuildTests and $GenerateTests and defined $Standalone)
    {# allocated memory for generating tests must be returned to the system
        system("perl", $0, @INPUT_OPTIONS);
        exit($?>>8);
    }
    elsif($BuildTests and defined $Standalone)
    {
        my $StartTime_Build = time;
        readDescriptor($Descriptor);
        $TEST_SUITE_PATH = "tests/$TargetLibraryName/".$Descriptor{"Version"};
        if(not -e $TEST_SUITE_PATH)
        {
            print STDERR "\nERROR: tests were not generated yet\n";
            exit(1);
        }
        if($TargetInterfaceName)
        {
            print "building test for $TargetInterfaceName ... ";
            read_scenario();
            build_sanity_test($TargetInterfaceName);
            if($BuildResult{$TargetInterfaceName}{"IsCorrect"})
            {
                if($BuildResult{$TargetInterfaceName}{"Warnings"})
                {
                    print "success (Warnings)\n";
                }
                else
                {
                    print "success\n";
                }
            }
            elsif(not $BuildResult{$TargetInterfaceName}{"TestNotExists"})
            {
                print "fail\n";
            }
        }
        else
        {
            build_tests();
        }
        print "elapsed time: ".show_time_interval(time - $StartTime_Build)."\n" if($ShowExpendTime);
        if($ResultCounter{"Build"}{"Success"}>0 and not $TargetInterfaceName and not $RunTests)
        {
            print "use -run option for running tests\n";
        }
        remove_option(\@INPUT_OPTIONS, "build");
        remove_option(\@INPUT_OPTIONS, "make");
    }
    if(($CleanTests or $CleanSources) and defined $Standalone)
    {
        my $StartTime_Clean = time;
        readDescriptor($Descriptor);
        $TEST_SUITE_PATH = "tests/$TargetLibraryName/".$Descriptor{"Version"};
        if(not -e $TEST_SUITE_PATH)
        {
            print STDERR "\nERROR: tests were not generated yet\n";
            exit(1);
        }
        if($TargetInterfaceName)
        {
            print "cleaning test for $TargetInterfaceName ... ";
            read_scenario();
            clean_sanity_test($TargetInterfaceName);
            print "success\n";
        }
        else
        {
            clean_tests();
        }
        print "elapsed time: ".show_time_interval(time - $StartTime_Clean)."\n" if($ShowExpendTime);
        remove_option(\@INPUT_OPTIONS, "clean") if($CleanTests);
        remove_option(\@INPUT_OPTIONS, "view-only") if($CleanSources);
        
    }
    if($RunTests and $GenerateTests and defined $Standalone)
    {#tests running requires creation of two processes, so allocated memory must be returned to the system
        system("perl", $0, @INPUT_OPTIONS);
        exit($?>>8);
    }
    elsif($RunTests and defined $Standalone)
    {
        my $StartTime_Run = time;
        init_signals();
        readDescriptor($Descriptor);
        $TEST_SUITE_PATH = "tests/$TargetLibraryName/".$Descriptor{"Version"};
        $REPORT_PATH = "test_results/$TargetLibraryName/".$Descriptor{"Version"};
        if(not -e $TEST_SUITE_PATH)
        {
            print STDERR "\nERROR: tests were not generated yet\n";
            exit(1);
        }
        my $ErrCode = 0;
        if($TargetInterfaceName)
        {
            read_scenario();
            my $XvfbStarted = 0;
            $XvfbStarted = runXvfb() if($UseXvfb and -f $Interface_TestDir{$TargetInterfaceName}."/test");
            print "running test for $TargetInterfaceName ... ";
            $ErrCode = run_sanity_test($TargetInterfaceName);
            stopXvfb($XvfbStarted) if($UseXvfb);
            if($RunResult{$TargetInterfaceName}{"IsCorrect"})
            {
                if($RunResult{$TargetInterfaceName}{"Warnings"})
                {
                    print "success (Warnings)\n";
                }
                else
                {
                    print "success\n";
                }
            }
            elsif(not $RunResult{$TargetInterfaceName}{"TestNotExists"})
            {
                print "fail (".get_problem_title($RunResult{$TargetInterfaceName}{"Type"}, $RunResult{$TargetInterfaceName}{"Value"}).")\n";
            }
        }
        else
        {
            $ErrCode = run_tests();
        }
        mkpath($REPORT_PATH);
        if((not $TargetInterfaceName or not $RunResult{$TargetInterfaceName}{"TestNotExists"})
        and keys(%Interface_TestDir) and not $ErrCode)
        {
            unlink($REPORT_PATH."/test_results.html");# removing old report
            print "creating report ...\n";
            translateSymbols(keys(%RunResult));
            create_HtmlReport();
            print "elapsed time: ".show_time_interval(time - $StartTime_Run)."\n" if($ShowExpendTime);
            print "see test results in the file:\n  $REPORT_PATH/test_results.html\n";
        }
        exit($ResultCounter{"Run"}{"Fail"});
    }
    exit(0);
}

scenario();
