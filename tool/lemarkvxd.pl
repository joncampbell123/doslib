#!/usr/bin/perl
# Mod LE header to indicate VXD (Windows 386 target)

if (@ARGV < 1) {
    print STDERR "win2xhdrpatch.pl <NE image>\n";
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
if ($tmp ne "LE") {
    print "Not an :E image\n";
    exit 1;
}

# change to Windows 386 target
seek(NE,$ne_offset+0xA,0);
print NE pack("v",4);
close(NE);

