#!/usr/bin/perl
# convert Windows 1.x/2.x icon to a Windows 3.x icon

use strict;

my $infile = shift @ARGV;
my $outfile = shift @ARGV;
die if $infile eq "";
die if $outfile eq "";
die unless -f $infile;

# input icon:
# HEADER (14 bytes)
#   +0 BYTE rnType
#   +1 BYTE rnFlags
#   +2 WORD rnZero
#   +4 WORD bmType
#   +6 WORD bmWidth
#   +8 WORD bmHeight
#   +10 WORD bmWidthBytes
#   +12 BYTE bmPlanes
#   +13 BYTE bmBitsPixel
# ICON IMAGE (bmWidthBytes WORD aligned) * bmHeight
# ICON MASK (bmWidthBytes WORD aligned) * bmHeight
#
# I have yet to see any bitmap/icon/cursor from Windows 1.x/2.x that is not 1bpp monochrome.
# Images are top-down, not bottom up.

# output icon:
# ICOONDIR (6 bytes)
#   +0 WORD reserved must be zero
#   +2 WORD image type (1=icon 2=cursor)
#   +4 WORD number of images in the file, numImages
# ICONDIRENTRY[numImages] (16 bytes each)
#   +0 BYTE image width
#   +1 BYTE image height
#   +2 BYTE colors in color palette
#   +3 BYTE reserved 0
#   +4 WORD ICO color planes (0 or 1), CUR X hotspot
#   +6 WORD ICO bits per pixel, CUR y hotspot
#   +8 DWORD image data size in bytes
#   +12 DWORD offset of image data from the beginning of the file
#
# "image data" is misleading, the image data is actually:
# BITMAPINFOHEADER
# ICON IMAGE
# ICON MASK 1bpp

open(IN,"<",$infile) || die;
binmode(IN);

open(OUT,">",$outfile) || die;
binmode(OUT);

my $hdr1;
read(IN,$hdr1,14) == 14 || die;
my ($rnType,$rnFlags,$rnZero,$bmType,$bmWidth,$bmHeight,$bmWidthBytes,$bmPlanes,$bmBitsPixel) = unpack("CCSSSSSCC",$hdr1);
die "Invalid or unsupported icon" unless ($rnType == 1 && $rnZero == 0 && $bmWidth != 0 && $bmHeight != 0 && $bmWidthBytes != 0 && ($bmPlanes == 0 || $bmPlanes == 1) && ($bmBitsPixel == 0 || $bmBitsPixel == 1));

my $in_pitch = ($bmWidthBytes+1)&(~1); # word align
my $out_pitch_raw = ($bmWidth+7)>>3;
die unless $out_pitch_raw >= $in_pitch;
die unless $out_pitch_raw > 0;
my $out_pitch = ($out_pitch_raw+3)&(~3); # dword align
die unless $out_pitch > 0;
my $out_pad = $out_pitch - $in_pitch;
die unless $out_pad >= 0;

my @image_rows;
for (my $y=0;$y < $bmHeight;$y++) {
	my $row;
	read(IN,$row,$in_pitch) == $in_pitch || die;
	push(@image_rows,$row);
	$row .= chr(0) x $out_pad;
}

my @mask_rows;
for (my $y=0;$y < $bmHeight;$y++) {
	my $row;
	read(IN,$row,$in_pitch) == $in_pitch || die;
	push(@mask_rows,$row);
	$row .= chr(0) x $out_pad;
}

print OUT pack("SSS",0,1,1);
print OUT pack("CCCCSSLL",$bmWidth,$bmHeight,2,0,1,1,$out_pitch*$bmHeight*2 + 40,6 + 16);
die unless tell(OUT) == (6 + 16);

# BITMAPINFOHEADER (40 bytes)
#   DWORD biSize;
#   LONG  biWidth;
#   LONG  biHeight;
#   WORD  biPlanes;
#   WORD  biBitCount;
#   DWORD biCompression;
#   DWORD biSizeImage;
#   LONG  biXPelsPerMeter;
#   LONG  biYPelsPerMeter;
#   DWORD biClrUsed;
#   DWORD biClrImportant;
# RGBQUAD palette[biClrUsed];
# IMAGE DATA
print OUT pack("LllSSLLllLL",40,$bmWidth,$bmHeight*2,1,1,0,0,0,0,0,0);
print OUT pack("CCCCCCCC",0,0,0,0,255,255,255,0); # RGBX(0,0,0,0) RGBX(255,255,255,0)   RGBQUAD is { blue, green, red, reserved }

for (my $y=0;$y < $bmHeight;$y++) {
	my $sy = $bmHeight - 1 - $y;
	die unless length($image_rows[$sy]) == $out_pitch;
	print OUT $image_rows[$sy];
}

for (my $y=0;$y < $bmHeight;$y++) {
	my $sy = $bmHeight - 1 - $y;
	die unless length($mask_rows[$sy]) == $out_pitch;
	print OUT $mask_rows[$sy];
}

close(OUT);
close(IN);

