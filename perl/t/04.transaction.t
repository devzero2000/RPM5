#!/bin/env perl

use strict;
use Test::More tests => 17;
use RPM::Header;

use_ok('RPM::Transaction');

isa_ok(RPM::Transaction->new(), 'RPM::Transaction');
RPM::add_macro('_repackage_all_erasures 0');
{
    my $ts = RPM::Transaction->new();
    $ts->vsflags(0);
    is($ts->vsflags(), 0, 'can get vsflags');
    is($ts->vsflags(2), 0, 'can get old vsflags');
    is($ts->vsflags(), 2, 'can change vsflags');
    $ts->transflags(0);
    is($ts->transflags(), 0, 'can get transflags');
    is($ts->transflags(2), 0, 'can get old transflags');
    is($ts->transflags(), 2, 'can change transflags');
}

# playing with DB:
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
# my @rpmcmd = (qw(../rpm --macros ../macros --define), "_dbpath $tempdbpath", '--initdb');
# system(@rpmcmd) and die "Cannot init db in $tempdbpath";

RPM::load_macro_file('../macros');
RPM::add_macro("_dbpath $tempdbpath");
{
my $ts = RPM::Transaction->new();
ok($ts->initdb(), "can initdb()");
ok($ts->closedb(), "can closedb() after init");
ok($ts->opendb(1), "can opendb(), write mode");
ok($ts->closedb(), "can closedb() after open");
my $h = RPM::Header::rpm2header('test-rpm-1.0-1.noarch.rpm');
ok($ts->dbadd($h), "can inject a header");
ok($ts->verifydb(), "can verifydb()");
ok($ts->rebuilddb(), "can rebuilddb()");
ok($ts->closedb(), "can closedb() after rebuild");
ok($ts->dbremove(1), "can remove a header");
}

clean();
