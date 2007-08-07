#!/bin/env perl

use strict;
use Test::More tests => 2;
use RPM;
use RPM::Header;

use_ok('RPM::Dependencies');
my $header = rpm2header('test-rpm-1.0-1.noarch.rpm');
my $deps = RPM::Dependencies->new($header, "REQUIRENAME");
isa_ok($deps, 'RPM::Dependencies');
