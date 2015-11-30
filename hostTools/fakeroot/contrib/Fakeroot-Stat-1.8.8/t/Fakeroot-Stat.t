use Test;
BEGIN { plan tests => 5 };
END { kill 'TERM', $pid if defined $pid; unlink ".test.file" };

# 1: Can we run it?
use Fakeroot::Stat;
ok(1) or exit 1;

# Start FAKED
open FAKED, "$ENV{FAKED}|" or die "Couldn't start faked: $!";
chomp ($out = <FAKED>);
close FAKED;
($key, $pid) = split(/:/, $out, 2);
die "No key returned from faked" unless length $key;

open TEST, "> .test.file";
print TEST "This file means we tested\n";
close TEST;

# Try it
@realstat = stat(".test.file");
@fakestat = fakestat(undef, ".test.file");

# 2: Make sure we return the same elements as stat
ok(scalar(@fakestat), scalar(@realstat)) or exit 1;

# 3: Make sure we return the same without faked
ok(join(":", @fakestat), join(":",@realstat)) or exit 1;

# 4: Make sure it returns root if we use fakeroot
@teststat = fakestat($key, ".test.file");
ok($teststat[4], 0) or exit 1;
ok($teststat[5], 0) or exit 1;
