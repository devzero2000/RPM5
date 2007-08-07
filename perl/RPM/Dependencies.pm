package RPM::Dependencies;

use strict;
use RPM;


=head1 NAME

RPM::Dependencies

=head1 DESCRIPTION

=head1 SYNOPSIS

=head1 FUNCTIONS

=head2 new($header, $tag)

Create a new B<RPM::Dependencies> set from $header for $tag type dependencies.

=head2 $ds->count

Return the count of dependencies in set

=head2 $ds->index

Return the current internal index pointer

=head2 $ds->set_index($idx)

Set the internal index pointer to $idx

=head2 $ds->init

Initialize the structure for next() call

=head2 $ds->next

Point to next dependencies inside the set. Return the previous index.

The function allways return "True", in the case previous index was 0, it return
"0E0" which is still 0 in an integer context.

=cut

# currently everything is in the XS code

1;

__END__

=head1 AUTHOR

Olivier Thauvin <nanardon@nanardon.zarb.org>

=head1 LICENSE

This software is under GPL, refer to rpm license for details.

=cut
