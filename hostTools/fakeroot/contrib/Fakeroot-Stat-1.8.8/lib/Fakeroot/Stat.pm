package Fakeroot::Stat;

use 5.006;
use strict;
use warnings;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);

our @EXPORT = qw(
	fakestat
	fakelstat
);
our $VERSION = '1.8.8';

bootstrap Fakeroot::Stat $VERSION;

# Preloaded methods go here.

1;
__END__

=head1 NAME

Fakeroot::Stat - Perl extension for interrogating a faked daemon

=head1 SYNOPSIS

  use Fakeroot::Stat;

  ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
     $atime,$mtime,$ctime,$blksize,$blocks)
         = fakestat($fakerootkey, $filename);

  # fakelstat as above, but does an lstat

=head1 AUTHOR

SCM Team at THUS plc E<lt>scm@thus.netE<gt>

=head1 SEE ALSO

L<perl>.

=cut
