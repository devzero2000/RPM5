package RPM::Files;

use strict;
use Exporter;
use RPM;
use RPM::Header;

=head1 NAME

RPM::Files

=head1 DESCRIPTION

A files set iterator.

=head1 FUNCTIONS

=head2 new($header)

Create a new B<RPM::Files> iterator from $header. $header is a
L<RPM::Header> object.

=head2 count

Return the count of files inside the iterator.

=cut

# currently everything is in the XS code

1;

__END__

=head1 AUTHOR

Olivier Thauvin <nanardon@nanardon.zarb.org>

=head1 LICENSE

This software is under GPL, refer to rpm license for details.

=cut
