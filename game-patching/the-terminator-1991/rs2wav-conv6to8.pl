#!/usr/bin/perl
#
# This version upscales the audio data by shifting left by 2.
# Audio in my copy appears to be scaled as if 6-bit PCM (0x00 to 0x3F)
# Popping can be eliminated by doing that on conversion.
my $rsfile;
my $wavfile;

die "<input .RS> <output .WAV>" if (@ARGV < 2);

$rsfile = $ARGV[0];
$wavfile = $ARGV[1];

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

$srate = 5524;

open(WAV,">",$wavfile) || die;
binmode(WAV);

$hdr = "RIFF".pack("V",$len + 40)."WAVE";
print WAV $hdr;

$hdr = "fmt ".pack("V",16);
print WAV $hdr;

# WAVEFORMATPCM
$hdr = pack("vvVVvv",1,1,$srate,$srate,1,8); # wFormatTag, nChannels, nSamplesPerSec, nAvgBytesPerSec, nBlockAlign, WBitsPerSample
print WAV $hdr;

$hdr = "data".pack("V",$len);
print WAV $hdr;

$newwav = "";
for ($i=0;$i < $len;$i++) {
    $s = unpack("C",substr($RSBIN,32+$i,1));
    die "Sample out of range" if $s > 0x3F;
    $newwav .= chr($s << 2);
}

print WAV $newwav;

close(WAV);

