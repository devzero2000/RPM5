Summary: An toy rpm that could cause an "arg list too long" error in build
Name: rpm-arg-max-doc
Version: 1.0
Release: 1
License: LGPL
Group: Development/Tools
URL: http://rpm5.org
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildArch: noarch
%description
%{summary}

Visit this url for some background 
http://www.in-ulm.de/~mascheck/various/argmax/
information

%prep
%setup -c -T
%build
%{__mkdir_p} %{name}-document
cd %{name}-document
ARG_MAX_MEDIUM=20000
[ -x /usr/bin/getconf ] && { ARG_MAX="$(/usr/bin/getconf ARG_MAX 2>&-)" && [ -n "$ARG_MAX" ]  || ARG_MAX=$ARG_MAX_MEDIUM ; }
[ -z $ARG_MAX ] && ARG_MAX=$ARG_MAX_MEDIUM 
[ $ARG_MAX -lt $ARG_MAX_MEDIUM ] && ARG_MAX=$ARG_MAX_MEDIUM
perl -e '
use diagnostics;
use Fcntl;
use POSIX qw(limits_h);
my $FILEHAND="filehandle";
if (@ARGV != 1) {
 print "usage: <arg-max>\n";
 exit;
}
# XXX: ARG_MAX could be undefined
my $my_arg_max=$ARGV[0];
# XXX: 30 is almost the name file lenght
my $my_num_files=$my_arg_max/30;
if ($my_arg_max !~ /^\d+$/) { die "arg-max is not a  number\n" };
my $base_name = sprintf("document-%d-%d-0000", $$, time);
my $count = 0;
until(  $count++ >$my_num_files ) {
       $base_name =~ s/-(\d+)$/"-" . (1 + $1)/e;
       sysopen($FILEHAND, $base_name, O_WRONLY|O_EXCL|O_CREAT);
}
' $ARG_MAX

%install
#empty
rm -rf $RPM_BUILD_ROOT
exit 0

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc %{name}-document/doc*

%changelog
* Tue Mar  2 2010 Elia Pinto <devzero2000@rpm5.org> 1.0-1
- First Built
