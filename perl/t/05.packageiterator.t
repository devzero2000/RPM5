#!/bin/env perl

use strict;
use Test::More tests => 2;
use RPM;

my $tempdbpath = `pwd`;
chomp($tempdbpath);
$tempdbpath .= '/t/tempdb';

END {
    clean();
}

sub clean {
    system(qw(rm -fr), $tempdbpath);
}

clean();

# Creating temp db
mkdir($tempdbpath) or die "Cannot create $tempdbpath";
my @rpmcmd = (qw(../rpm --macros ../macros --define), "_dbpath $tempdbpath", '--initdb');
system(@rpmcmd) and die "Cannot init db in $tempdbpath";

RPM::load_macro_file('../macros');
RPM::add_macro("_dbpath $tempdbpath");
use_ok('RPM::PackageIterator');

isa_ok(RPM::PackageIterator->new(), 'RPM::PackageIterator');

clean();
