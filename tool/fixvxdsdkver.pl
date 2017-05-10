#!/usr/bin/perl
# fix SDK version in LE header

if (@ARGV < 3) {
    print STDERR "fixvxdsdkver.pl <LE image> <request> <SDK>\n";
    exit 1;
}

my $ne_file = shift @ARGV;
die unless -f $ne_file;

my $req = shift @ARGV;
$req = oct($req);
my $sdk = shift @ARGV;
$sdk = oct($sdk);

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
    print "Not an LE image\n";
    exit 1;
}

# at offset +0xC0, should be SDK version and driver request
print "Patching to request ".sprintf("0x%X",$req)." and sdk ".sprintf("0x%X",$sdk)."\n";

seek(NE,$ne_offset+0xC0,0);
print NE pack("vv",$req,$sdk);
close(NE);

