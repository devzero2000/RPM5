use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
	      'NAME'	=> 'RPM',
	      'OBJECT'	=> 'RPM.o',
	      'VERSION_FROM' => 'RPM.pm',		# finds $VERSION
	      'MAKEFILE'=> 'Makefile.perl',
	      'LIBS'	=> ['-lpopt -lneon -lbeecrypt -lpthread  -lneon  '],			# e.g., '-lm' 
	      'CCFLAGS' => '-g -O2 -D_GNU_SOURCE -D_REENTRANT  ',
	      'OPTIMIZE'=> '-g',
	      'INC'	=> join(' ', map { '-I../'. $_ } qw(build lib popt rpmdb rpmio)),
);

