#!/bin/env perl

use strict;
use Test::More tests => 5;

use_ok('RPM::Header');
can_ok('RPM::Header',
    qw(
    stream2header
    rpm2header
    )
);

{
    open(my $fd, '<', 'hdlist-test.hdr') or die "Can open test file hdlist-test.hdr";
    my ($header) = stream2header($fd);
    isa_ok($header, 'RPM::Header');
}
{
    my $header;
    open(my $fd, '<', 'hdlist-test.hdr') or die "Can open test file hdlist-test.hdr";
    stream2header($fd, sub { $header = $_[0] });
    isa_ok($header, 'RPM::Header');
}
{
    my $header = rpm2header("test-rpm-1.0-1.noarch.rpm");
    isa_ok($header, 'RPM::Header');
}
