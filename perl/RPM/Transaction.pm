package RPM::Transaction;

use strict;
use Exporter;
use RPM;
use RPM::PackageIterator;
use RPM::Header;
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

=head2 $ts->packageiterator

Return a RPM::PackageIterator against rpmdb use in the transaction.

See L<RPM::PackageIterator>

=head2 $ts->add_install($header, $key, $upgrade)

Add a new package to install into the transaction.

=over 4

=item $header is the L<RPM::Header> from the rpm to add

=item $key an arbitrary value that can be set to identify the element

=item $upgrade: (optional, default to 1) the package will upgrade already
installed packages.

=back

=head2 $ts->add_delete($header, $offset)

Add a package to remove from transaction

=over 4

=item $header is the L<RPM::Header> of the rpm to uninstall

=item $offset is the db location of the rpm to uninstall

=back

=head2 $ts->element_count()

Return the count of element in transaction.

=head2 $ts->check()

Check the transaction as no problem, return True on success.

=head2 $ts->order()

Order the transaction, return True on success.

Return as second value the count of unordered element:

    my $result = $ts->order();

or

    my ($result, $unordered) = $ts->order();

=head2 $ts->dbadd($header)

Inject a L<RPM::Header> into the rpmdb.

=head2 $ts->dbremove($offset)

Remove the header at B<$offset> from the rpmdb.

=cut

1;

__END__
