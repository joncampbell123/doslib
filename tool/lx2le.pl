#!/usr/bin/perl
#
# Take a Linear Executable (LX format) and convert to LE format.

if (@ARGV < 1) {
    print STDERR "lx2le.pl <LE image>\n";
    exit 1;
}

my $le_file = shift @ARGV;
die unless -f $le_file;

my $tmp;
my $le_offset = 0;
my $le_file_tmp = $le_file.".tmp";

open(LE,"<",$le_file) || die;
binmode(LE);
seek(LE,0,0);
read(LE,$tmp,2);

if ($tmp ne "MZ") {
    print "Not MS-DOS executable\n";
    exit 1;
}

seek(LE,0x3C,0);
read(LE,$tmp,4);
$le_offset = unpack("l",$tmp);
print "Extended header at $le_offset\n";

seek(LE,$le_offset,0);
read(LE,$tmp,2);
if ($tmp ne "LX") {
    print "Not an LX image\n";
    exit 1;
}

print "LX header at $le_offset\n";

# we copy the contents this time, to keep the code simple
open(LETMP,">$le_file_tmp") || die;
binmode(LETMP);
seek(LE,$le_offset,0);
read(LE,$tmp,0xAC);

# get some key values from the LX header.
my $number_of_memory_pages = unpack("V",substr($tmp,0x14,4));
my $mem_page_size = unpack("V",substr($tmp,0x28,4));
my $page_shift = unpack("V",substr($tmp,0x2C,4)); # NTS this field becomes "bytes in last page" in LE format
my $object_page_map_offset = unpack("V",substr($tmp,0x48,4));
my $data_pages_offset = unpack("V",substr($tmp,0x80,4));

print "Object page map ofs:   $object_page_map_offset\n";
print "No. memory pages:      $number_of_memory_pages\n";
print "Mem page size:         $mem_page_size\n";
print "Page shift:            $page_shift\n";
print "Data pages offset:     $data_pages_offset\n";

# optimized for 4096b pages only, sorry
die "unsupported page size" unless $mem_page_size == 4096;
$mem_page_shift = 12;

# re-read again up to data pages
seek(LE,0,0);
read(LE,$tmp,$data_pages_offset);

# change LX to LE
$tmp = substr($tmp,0,$le_offset)."LE".substr($tmp,$le_offset+2);

# read LX object page map.
# unfortunately, Watcom wlink will generate pages smaller than 4KB.
# which means to convert to LE format we're going to have to move pages around.
# we must copy the size of the last page to fill in the LE "bytes in last page" field.
my $outofs = 0;
my @origpageofs = ( );
my @origpagesz = ( );
my @newpageofs = ( );
my $last_page_size = $mem_page_size;
for ($page=0;$page < $number_of_memory_pages;$page++) {
    my $entry = substr($tmp,$le_offset+$object_page_map_offset+($page * 8),8);
    ($us_data_offset,$data_size,$flags) = unpack("VVv",$entry);
    print "  Page #".($page+1)."\n";
    print "    offset=$us_data_offset << $page_shift + $data_pages_offset = ".(($us_data_offset<<$page_shift)+$data_pages_offset)."\n";
    print "    data_size=$data_size\n";
    print "    flags=".sprintf("0x%04x",$flags)."\n";
    print "    output_offset_after_conversion=$outofs + $data_pages_offset = ".($outofs+$data_pages_offset)."\n";

    push(@origpageofs,($us_data_offset<<$page_shift)+$data_pages_offset);
    push(@origpagesz,$data_size);
    push(@newpageofs,$outofs+$data_pages_offset);
    if ($page == ($number_of_memory_pages-1)) {
        $last_page_size = $data_size;
        $outofs += $data_size;
    }
    else {
        $outofs += $mem_page_size;
    }
}
print "  Last page size=$last_page_size\n";

# patch the bytes in last page field
$tmp = substr($tmp,0,$le_offset+0x2C).pack("V",$last_page_size).substr($tmp,$le_offset+0x30);

# patch the object page map, in LE format.
$b1 = substr($tmp,0,$le_offset+$object_page_map_offset);
$ins = '';
for ($page=0;$page < $number_of_memory_pages;$page++) {
    $ins .= pack("vv",0,int(($newpageofs[$page] - $data_pages_offset) / $mem_page_size) + 1);
}
$padding = chr(0) x ($number_of_memory_pages*4); # LE entries are 4 bytes, LX entries are 8 bytes
$b2 = substr($tmp,$le_offset+$object_page_map_offset+($number_of_memory_pages*8));
$tmp = $b1.$ins.$padding.$b2;

print LETMP $tmp;

# now copy the pages
my $end_of_dest = 0;
my $end_of_data = 0;
for ($page=0;$page < $number_of_memory_pages;$page++) {
    my $sof = $origpageofs[$page];
    my $dof = $newpageofs[$page];
    my $csz = $origpagesz[$page];
    my $wsz = ($page == ($number_of_memory_pages - 1)) ? $csz : $mem_page_size;

    print "Copy page from source at $sof to dest $dof size $csz\n";

    seek(LETMP,$dof,0);
    seek(LE,$sof,0);

    read(LE,$tmp,$csz);
    $tmp .= chr(0) x ($wsz - $csz) if $csz < $wsz;
    print LETMP $tmp;

    $t = $sof + $csz;
    $end_of_data = $t if $end_of_data < $t;

    $t = $dof + $wsz;
    $end_of_dest = $t if $end_of_dest < $t;
}
print "End of data $end_of_data in source\n";
print "End of data $end_of_dest in dest\n";

# copy the rest
seek(LE,$end_of_data,0);
seek(LETMP,$end_of_dest,0);
read(LE,$tmp,0x1000000);
print LETMP $tmp;

print length($tmp)." extra bytes\n";

close(LETMP);
close(LE);

# DONE
unlink($le_file);
rename($le_file_tmp,$le_file) || die;

