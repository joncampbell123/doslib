#!/usr/bin/perl
#
# Given a version number and NE image, patch the version number in the image
# (C) 2012 Jonathan Campbell

if (@ARGV < 2) {
    print STDERR "chgnever.pl <version major.minor> <NE image>\n";
    exit 1;
}

my $vmaj,$vmin;
my $version_str = shift @ARGV;
my $ne_file = shift @ARGV;
die unless -f $ne_file;
($vmaj,$vmin) = split(/\./,$version_str);
$vmaj = $vmaj + 0;
$vmin = $vmin + 0;

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

# ok, patch
seek(NE,$ne_offset+0x3E,0);
read(NE,$tmp,2);
($ovminor,$ovmajor) = unpack("CC",$tmp);

print "Old version: v$ovmajor.$ovminor\n";

seek(NE,$ne_offset+0x3E,0);
print NE pack("CC",$vmin,$vmaj);

print "Patching to: v$vmaj.$vmin\n";

close(NE);

