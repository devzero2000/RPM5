#!/bin/env perl

use strict;
use Test::More tests => 6;
use RPM;
use RPM::Header;

use_ok('RPM::Dependencies');
my $header = rpm2header('test-rpm-1.0-1.noarch.rpm');
my $deps = RPM::Dependencies->new($header, "REQUIRENAME");
isa_ok($deps, 'RPM::Dependencies');
ok($deps->count, "can get deps count");
my @depsname = ();
while ($deps->next()) { push @depsname, $deps->name }
is(scalar(@depsname), $deps->count, "Can get all deps using next");
ok(eq_set(\@depsname, [qw(CompressedFileNames PayloadFilesHavePrefix)]), "can get deps name");
$deps->set_index(1);
is($deps->index, 1, "can set and get index");
