#!/bin/env perl

use strict;
use Test::More tests => 2;

use_ok('RPM::PackageIterator');

SKIP: {
skip "Does not work at time", 1;
isa_ok(RPM::PackageIterator->new(), 'RPM::PackageIterator');
}
