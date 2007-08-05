#!/bin/env perl

use strict;
use Test::More tests => 16;

use_ok('RPM::Header');
can_ok('RPM::Header',
    qw(
    stream2header
    rpm2header
    )
);

# OO method
can_ok('RPM::Header',
    qw(
    is_source_package
    tagformat
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

    is($header->tagformat('%{NAME}'), 'test-rpm', "tag_format works");
    is($header->tag('NAME'), 'test-rpm', "can get tag name");
    is($header->compare($header), 0, "compare() works");
    is(($header <=> $header), 0, "<=> operator works");
    ok($header->hastag('NAME'), "has_tag is true when exists");
    ok(!$header->hastag(9999), "has_tag is false when not exists");
    isa_ok($header->copy(), 'RPM::Header');
    ok($header->string, 'can get a string from header');
    ok($header->hsize, 'can get header size');
    ok(!$header->is_source_package(), 'package is not a source package');
}

