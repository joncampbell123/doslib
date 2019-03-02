#!/usr/bin/perl
my $rsfile;

die "<.RS file to modify>" if (@ARGV < 1);

$rsfile = $ARGV[0];

die unless -f $rsfile;

my $RSBIN;
open(RS,"<",$rsfile) || die;
binmode(RS);
# the length field seems to be 2-byte so I doubt any sound effects larger than 64KB are supported. This is MS-DOS you know.
read(RS,$RSBIN,65536);
close(RS);

# the header contains "STEVE" followed by an unknown 16-bit unsigned integer 0x03 0x48, and another 16-bit unsigned integer specifying the length.
# the rest of the header is padded with zeros until offset 32.
die "Not an RS audio file" unless substr($RSBIN,0,5) eq "STEVE";

$len = unpack("v",substr($RSBIN,7,2));
die "Invalid length in header" if (($len+32) > length($RSBIN));
warn "Shorter length in header" if (($len+32) < length($RSBIN));

# the "poppy" RS files have samples only in the 0x00-0x3F range.
# if there are samples outside that, then it's probably already been fixed, nothing to be done.
# the other problem to fix is the 0x00 byte at the end, which also causes a "pop"
$newdata = "";
for ($i=32;$i < length($RSBIN);$i++) {
    $c = unpack("C",substr($RSBIN,$i,1));
    if ($c > 0x3F) {
        print "RS file contains samples out of range. It's probably already been converted.\n";
        exit 0;
    }

    # that final byte is often zero.
    # left unfixed, the audio will still pop.
    if ($c == 0 && $i == (length($RSBIN)-1)) {
        $c = 0x20;
    }

    $newdata .= pack("C",$c << 2);
}

open(RS,">",$rsfile) || die;
binmode(RS);
print RS substr($RSBIN,0,32);
print RS $newdata;
close(RS);

