#!/usr/bin/perl
#
# Wolfenstien 3D AUDIOT extraction tool
# http://www.shikadi.net/moddingwiki/AudioT_Format
# (C) 2016 Jonathan Campbell
#
# Written to extract IMF files from installed WOLF3D files.
# Must be run in the same directory as the DOS installation of WOLF3D.

use POSIX qw(isatty);

# take index from argv
my $listall = 0;
my $idx = 0;

if (@ARGV > 0) {
    $idx = shift(@ARGV) + 0;
}
else {
    $listall = 1;
}

# TODO: What about .WL6?
open(HED,"<","AUDIOHED.WL1") || die;
binmode(HED);

my $blob;
my $start,$end;
my $len;

if ($listall) {
    $idx = 0;
    while (read(HED,$blob,(4 * 2)) == (4 * 2)) {
        my @a = unpack("VV",$blob);
        $start = $a[0];
        $end = $a[1];
        $len = $end - $start;
        print "Entry $idx: start=$start len=$len\n";
        $idx++;
    }

    exit 0;
}

seek(HED,$idx * 4 * 2,0);
read(HED,$blob,(4 * 2)) == (4 * 2) || die;

my @a = unpack("VV",$blob);
$start = $a[0];
$end = $a[1];
die if $start > $end;
$len = $end - $start;

print STDERR "Entry $idx: $start <= x < $end ($len bytes)\n";

# need to direct our output to a file to extract!
exit 0 if (isatty(1) || $len == 0);

open(T,"<","AUDIOT.WL1") || die;
binmode(T);

seek(T,$start,0);
die "offset $start doesn't exist" if tell(T) != $start;
read(T,$blob,$len);
binmode(STDOUT);
print $blob;

