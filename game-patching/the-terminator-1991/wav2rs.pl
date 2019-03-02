#!/usr/bin/perl
my $rsfile;
my $wavfile;

die "<input .WAV> <output .RS>" if (@ARGV < 2);

$wavfile = $ARGV[0];
$rsfile = $ARGV[1];

die unless -f $wavfile;

my $WAVBIN;
open(WAV,"<",$wavfile) || die;
binmode(WAV);
read(WAV,$WAVBIN,100000);
close(WAV);

die "Not a WAV file" unless substr($WAVBIN,0,4) eq "RIFF";
die "Not a WAV file" unless substr($WAVBIN,8,4) eq "WAVE";

# locate the data chunk.
# for simplicity the code ASSUMES 8-bit PCM.
$dataofs = undef;
$datalen = undef;
$i = 12; # following the RIFF:WAVE chunk
while ($i < length($WAVBIN)) {
    $type = substr($WAVBIN,$i,4);
    $len = unpack("V",substr($WAVBIN,$i+4,4));
    if ($type eq "data") {
        $dataofs = $i + 8;
        $datalen = $len;
        last;
    }

    $i += $len + 8;
}

die "Cannot find WAV data" unless (defined($dataofs) && defined($datalen));

print "WAV data at $dataofs, length $datalen\n";

open(RS,">",$rsfile) || die;
$hdr = "STEVE" . pack("v",0x4803) . pack("v",$datalen) . (chr(0) x (32 - 9));
die unless length($hdr) == 32;
print RS $hdr;
print RS substr($WAVBIN,$dataofs,$datalen);
close(RS);

