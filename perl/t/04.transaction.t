#!/bin/env perl

use strict;
use Test::More tests => 8;

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
