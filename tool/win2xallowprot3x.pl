#!/usr/bin/perl
#
# Patch Windows 2.x executable to signal that it's OK to run in Windows 3.x protected mode
#
# NTS: Apparently when the executable is marked Windows 2.x, and target OS type none,
#      Windows does not heed the "I can run in Windows 3.x protected mode" flag.
#
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
if ($tmp ne "NE") {
    print "Not an NE image\n";
    exit 1;
}

seek(NE,$ne_offset+0x37,0);
read(NE,$tmp,1);

$tmp = chr(ord($tmp) | 0x02); # bit 1

seek(NE,$ne_offset+0x37,0);
print NE $tmp;

close(NE);

