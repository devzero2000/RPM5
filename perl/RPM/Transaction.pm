package RPM::Transaction;

use strict;
use Exporter;
use RPM;
use vars qw($AUTOLOAD);

our @ISA = qw(Exporter);

our @EXPORT = qw(
);

=head1 NAME

RPM::Transaction

=head1 DESCRIPTION

=head1 FUNCTIONS

=head2 new

Create a new RPM::Transaction.

=head2 $ts->setrootdir

Set the root path for the transaction. Should be called before any action.

=head2 $ts->vsflags

Return the current vsflags set in the transaction.

An optionnal vsflags can be passed, in this case the new vsflags is set,
old value is returned:

    $ts->vsflag(4);
    my $oldflags = $ts->vsflag(6);
    # $oldflags contains 6
    my $flags = $ts->vsflag();
    # $flags contains 6

=head2 $ts->transflags

Return the current transflags set in the transaction.

An optionnal transflags can be passed, in this case the new transflags is set,
old value is returned:

    $ts->transflag(4);
    my $oldflags = $ts->transflag(6);
    # $oldflags contains 6
    my $flags = $ts->transflag();
    # $flags contains 6

=head2 $ts->dbadd($header)

Inject a L<RPM::Header> into the rpmdb.

=head2 $ts->dbremove($offset)

Remove the header at B<$offset> from the rpmdb.

=cut

1;

__END__
