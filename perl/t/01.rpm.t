#!/bin/env perl

use strict;

use Test::More tests => 3;

use_ok('RPM');
like(RPM::expand_macro('%{?_rpmversion}%{?!_rpmversion:ver}'), '/^[^%]/', "can expand %_rpmversion macro");
RPM::add_macro('_test_macro I_am_set');
is(RPM::expand_macro('%_test_macro'), 'I_am_set', 'setting a marco works');
