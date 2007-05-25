use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $libs, @defines, @incs;

if (-e '../system.h') {
  $libs = "-L../lib/ -lrpm";
  push @incs, '-I../lib';
  push @incs, '-I../rpmdb';
  push @incs, '-I../rpmio';
  push @incs, '-I../popt';
  push @defines, '-DRPM_RPM41';
  $optimize = "-O2";
} else {
  my $extra_libs = $ENV{EXTRALIBS};
  $libs = "$extra_libs -lrpm -lrpmdb -lrpmio -lpopt";
  push @incs, '-I/usr/include/rpm';

  # try to be smart about which shared libs we should load.
  # use the .la file.  probably libtool only.

  my $filename = '/usr/lib/librpm.la';
  $filename = '/usr/lib64/librpm.la' unless -e $filename;

  if (open FH, "<$filename") {
    my ($line, @rest) = grep { /^dependency_libs=/ } <FH>;
    if ($line and not @rest) {
      if ($line =~ /^dependency_libs='(.*)'$/) {
        my $l = $1;
        my @l = split /\s+/, $l;
        @l = grep { /^-l/ } @l;
        $libs = "$libs -lrpm @l";
      }
    }
  }

  # detect which rpm is running.  ugly but necessary... for now.
  if (-e '/usr/include/rpm/rpmts.h') {
    push @defines, '-DRPM_RPM41';
  }
  else {
    push @defines, '-DRPM_RPM40';
  }
  $optimize = "-g";
}

WriteMakefile(
    'NAME'		=> 'RPM',
    'VERSION_FROM'	=> 'RPM.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    'LIBS'		=> [ $libs ], # e.g., '-lm'
    'DEFINE'		=> join(" ", @defines), # e.g., '-DHAVE_SOMETHING'
    'INC'		=> join(" ", @incs), # e.g., '-I/usr/include/other'
    'TYPEMAPS'          => [ 'typemap' ],
    'OPTIMIZE'          => $optimize
);
