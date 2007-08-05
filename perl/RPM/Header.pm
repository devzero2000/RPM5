package RPM::Header;

use strict;
use Exporter;
use RPM;
use vars qw($AUTOLOAD);

use overload '<=>'  => \&op_spaceship,
             'cmp'  => \&op_spaceship,
             'bool' => \&op_bool;

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

=head1 RPM::Header FUNCTIONS

In addition to the following methods, all tags have simple accessors;
$hdr->epoch() is equivalent to $hdr->tag('epoch').

The <=> and cmp operators can be used to compare versions of two packages.

=cut

# proxify calls to $header->tag()
sub AUTOLOAD {
    my ($header) = @_;

    my $tag = $AUTOLOAD;
    $tag =~ s/.*:://;
    return $header->tag($tag);
}

sub op_bool {
    my ($self) = @_;
    return (defined($self) && ref($self) eq 'RPM::Header');
}

=head2 $hdr->is_source_package()

Returns a true value if the package is a source package, false otherwise.

=cut

1;

__END__

=head1 AUTHOR

Olivier Thauvin <nanardon@nanardon.zarb.org>

=cut
