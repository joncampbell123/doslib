#!/usr/bin/perl
# Pack a bunch of files into an extremely simple format single file
# ./dumbpack.pl [in [in [in [...]]]] -- out
my @srcfiles;
my $dstfile;

for ($i=0;$i < @ARGV;$i++) {
    last if ($ARGV[$i] eq "--");
    die "input file does not exist" unless -f $ARGV[$i];
    push(@srcfiles,$ARGV[$i]);
}

die "no output file" if ($i == @ARGV); # because it means we never hit the --
$i++; # skip --
die "no output file" if ($i == @ARGV);
$dstfile = $ARGV[$i];

my @offsets;
my $ofs = 2 + (2 * @srcfiles);

for ($i=0;$i < @srcfiles;$i++) {
    my $sz = -s $srcfiles[$i];
    push(@offsets,$ofs);
    $ofs += $sz;
}

open(OUT,">",$dstfile) || die;
binmode(OUT);

print OUT pack("v",scalar(@srcfiles));
for ($i=0;$i < @srcfiles;$i++) {
    print OUT pack("v",$offsets[$i]);
}

my $buf;

for ($i=0;$i < @srcfiles;$i++) {
    die "offset $i error" unless tell(OUT) eq $offsets[$i];

    open(IN,"<",$srcfiles[$i]) || die;
    binmode(IN);
    read(IN,$buf,0x100000);
    print OUT $buf;
    close(IN);
}

close(OUT);

