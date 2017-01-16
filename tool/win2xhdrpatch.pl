#!/usr/bin/perl
#
# Mod the NE header to look more like a Windows 2.x driver
# (C) 2012 Jonathan Campbell

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
if ($tmp ne "NE") {
    print "Not an NE image\n";
    exit 1;
}

# observation 1: program flags (NE+0x0C) sets Application type=0 not 2
seek(NE,$ne_offset+0xC,0);
read(NE,$tmp,2);
$tmp = unpack("v",$tmp);
$tmp &= 0x80FF; # mask off application type, OS/2 family app, errors, etc
seek(NE,$ne_offset+0xC,0);
print NE pack("v",$tmp);

# observation 2: Target Operating System field is zero
seek(NE,$ne_offset+0x36,0);
print NE chr(0);

# observation 3: 0x37-0x3F are zero
seek(NE,$ne_offset+0x37,0);
print NE chr(0) x 9;

close(NE);

