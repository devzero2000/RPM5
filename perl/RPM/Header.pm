package RPM::Header;

use strict;
use Exporter;
use RPM;

our @ISA = qw(Exporter);

our @EXPORT = qw(
    stream2header
    rpm2header
);

=head1 NAME

RPM::Header

=head1 DESCRIPTION

A module to perform action on rpm b<header>.

The header is a set of data containing information about a rpm archive.

=head1 SINOPSYS

    use RPM::Header;

    my $header = rpm2header('any-1-1.noarch.rpm');
    print $header->header_sprintf('%{NAME}\n');
    # output "any" + carriage return

=cut

# Everythings is in the XS at time

=head1 FUNCTIONS

=head2 rpm2header(rpmfile, vsflags)

Read a rpm archive and return a RPM::Header object on success, return undef
on error.

B<vsflags> (optional) is either an integer value, either an arrayref
containing textual values. See L<RPM::Constant>.

=head2 stream2header(filehandle)

Read all header store into a file from an open file handle.

Two usage:

=over 4

=item stream2header(filehandle)

stream2header return an array b<RPM::Header> object.

=item stream2header(filehandle, $callback)

The function nothing, for each header read, $callback function is called
with 'RPM::Header' passed as argument.

=back

=cut

1;

__END__
