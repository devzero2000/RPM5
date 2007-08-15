package RPM::Spec;

use strict;
use Exporter;
use RPM;


=head1 NAME

RPM::Spec

=head1 DESCRIPTION

An object to handle operation around specs file

=head1 FUNCTIONS

=new($specfile, %options)

Create a new B<RPM::Spec> object over $specfile spec file.

Optional options are:

=over 4

=item force Don't check all sources files exists

=item passphrase The passphrase to use to sign rpm after build

=item anyarch Don't check architecture compatibility

=item verify

=item root Root directory to use to build the spec file

=back

=cut

# currently everything is in the XS code

1;

__END__

=head1 AUTHOR

Olivier Thauvin <nanardon@nanardon.zarb.org>

=head1 LICENSE

This software is under GPL, refer to rpm license for details.

=cut
