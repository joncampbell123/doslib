#!/usr/bin/perl
#
# Align the MS-DOS stub and NE header to sit on 512-byte blocks,
# as observed on Windows 1.x/2.x executables. This means making
# the MS-DOS header 512 bytes long as well.
# (C) 2012 Jonathan Campbell

if (@ARGV < 1) {
    print STDERR "win2xalign512.pl <NE image>\n";
    exit 1;
}

my $ne_file = shift @ARGV;
die unless -f $ne_file;

my $tmp;
my $ne_offset = 0;
my $ne_file_tmp = $ne_file.".tmp";

open(NE,"<",$ne_file) || die;
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

print "NE header at $ne_offset\n";
if ($ne_offset == 1024) { # NTS: Shortcut
    print "Nothing to do\n";
    exit 1;
}

# we copy the contents this time, to keep the code simple
open(NETMP,">$ne_file_tmp") || die;
binmode(NETMP);

# copy the MS-DOS header. Assume at least 0x40 bytes. Patch NE offset field to set it to 1024.
seek(NE,0,0);
seek(NETMP,0,0);
read(NE,$tmp,0x40);
$tmp = substr($tmp,0,0x3C).pack("V",1024);
print NETMP $tmp;

# how big is the original MS-DOS header?
($msdos_last,$msdos_blocks,$relocs,$hdr_para) = unpack("vvvv",substr($tmp,2,8));
$hdr_para *= 16;
print "MS-DOS header: blocks=$msdos_blocks last=$msdos_last header=$hdr_para\n";
$msdos_size = $msdos_blocks * 512;
$msdos_size += $msdos_last - 512 if $msdos_last != 0;
print "..size: $msdos_size\n";
$msdos_size = $ne_offset if $msdos_size > $ne_offset;
print "..stub: $msdos_size\n";
die if $msdos_size <= $hdr_para;
$msdos_size -= $hdr_para;
print "..stub without hdr: $msdos_size\n";
die "original header too big" if $hdr_para > (512-0x40);
die "Relocations not supported" if $relocs != 0;
# finish padding header
print NETMP chr(0) x (512 - 0x40);

# now copy the MS-DOS stub
seek(NE,$hdr_para,0);
read(NE,$tmp,$msdos_size);
print NETMP $tmp;
die "stub too big" if $msdos_size > 512;
# and pad
print NETMP chr(0) x (512 - $msdos_size);

# then the rest of the file.
# NTS: Noted that Windows 1.x/2.x binaries use alignment=16.
read(NE,$tmp,0x1000000);

$shift_align = unpack("v",substr($tmp,0x32,2));
die if $shift_align > 9;
print "NE alignment: ".(1 << $shift_align)."\n";

# locate segment table (we're going to patch it).
# you determine where segment table ends by resource table offset.
$segment_offset = unpack("v",substr($tmp,0x22,2));
print "Segment table at: $segment_offset + $ne_offset\n";
$resource_table_offset = unpack("v",substr($tmp,0x24,2));
print "Resource table at: $resource_table_offset + $ne_offset\n";
die if $segment_offset >= $resource_table_offset;

# patch the segment table to reflect our change in offset.
die if $ne_offset > 1024;
$adjust = 1024 - $ne_offset;
print "NE adjustment: $adjust\n";
$adjust_shf = $adjust >> $shift_align;
$adjust_shfm = $adjust - ($adjust_shf << $shift_align);
print "..blocks: $adjust_shf + $adjust_shfm\n";
die "NE realignment is impossible" if $adjust_shfm != 0;

$segtablent = int(($resource_table_offset - $segment_offset) / 8);
die "No segments" if $segtablent == 0;
$segtabl = substr($tmp,$segment_offset,$segtablent * 8);
print "$segtablent segments to patch\n";

my $nsegtabl;
for ($seg=0;$seg < $segtablent;$seg++) {
    $segent = substr($segtabl,$seg * 8,8);
    ($seg_offset_shf,$seg_length,$seg_flags,$seg_minalloc) = unpack("vvvv",$segent);

    print "Segment ".($seg+1)."\n";
    print "  Offset: $seg_offset_shf << $shift_align = ".($seg_offset_shf << $shift_align)."\n";
    print "  Length: ".(($seg_length == 0 && $seg_offset_shf != 0) ? 0x10000 : $seg_length)."\n";
    print "  Flags: ".sprintf("0x%04x",$seg_flags)."\n";
    print "  Min alloc: ".($seg_minalloc == 0 ? 0x10000 : $seg_minalloc)."\n";

    $seg_offset_shf += $adjust_shf;
    print "  New offset: $seg_offset_shf << $shift_align = ".($seg_offset_shf << $shift_align)."\n";

    $nsegtabl .= pack("vvvv",$seg_offset_shf,$seg_length,$seg_flags,$seg_minalloc);
}
$tmp = substr($tmp,0,$segment_offset).$nsegtabl.substr($tmp,$segment_offset+length($nsegtabl));

# patch offset to nonresident name table, it's relative to the start of the file not the NE header
$offset = unpack("V",substr($tmp,0x2C,4));
print "Nonresident name table offset: $offset\n";
$offset += $adjust;
print "..new offset: $offset\n";

$tmp = substr($tmp,0,0x2C).pack("V",$offset).substr($tmp,0x30);

# write out
print NETMP $tmp;

close(NETMP);
close(NE);

# DONE
unlink($ne_file);
rename($ne_file_tmp,$ne_file) || die;

