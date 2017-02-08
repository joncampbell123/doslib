#!/usr/bin/perl
#
# Given a version number and NE image, patch the MS-DOS header to
# include the NE image in the resident portion. For use with
# files targeting Windows 1.x and Windows 2.x
# (C) 2012 Jonathan Campbell

if (@ARGV < 1) {
    print STDERR "win2xstubpatch.pl <NE image>\n";
    exit 1;
}

my $ne_file = shift @ARGV;
die unless -f $ne_file;

my $tmp;
my $ne_offset = 0;

open(NE,"+<",$ne_file) || die;
binmode(NE);
seek(NE,0,0);
read(NE,$tmp,2);

if ($tmp ne "MZ") {
    print "Not MS-DOS executable\n";
    exit 1;
}

seek(NE,0x3C,0);
read(NE,$tmp,4);
$ne_offset = unpack("l",$tmp);
print "Extended header at $ne_offset\n";

seek(NE,$ne_offset,0);
read(NE,$tmp,2);
#if ($tmp ne "NE") {
#    print "Not an NE image\n";
#    exit 1;
#}

# patch MS-DOS header to include the whole file as resident
# TODO: Not all Windows 1.x/2.x files include the whole file.
# USER.EXE appears to include MOST of the file but stop somewhere
# in the resource section (I think...)
seek(NE,0,2);
$length = tell(NE);
print "New resident length: $length\n";

$blocks = int($length / 512);
$blocklast = int($length % 512);
$blocks++ if $blocklast != 0;
print "$blocks x 512   last $blocklast\n";

seek(NE,2,0);
print NE pack("vv",$blocklast,$blocks);

close(NE);

