#!/usr/bin/perl
#
# Given the path of WIN386.EXE, list the W3 directory.
# (C) 2017 Jonathan Campbell
#
# Someday we'll even offer to decompress them...
my $win386 = shift @ARGV;
die "need win386 path" unless -f $win386;

open(W3,"<",$win386) || die;
binmode(W3);

my $raw;
my $w3ofs;
seek(W3,0x3C,0);
read(W3,$raw,4);
$w3ofs = unpack("V",$raw);

print "W3 offset at $w3ofs (".sprintf("0x%04x",$w3ofs).")\n";

seek(W3,$w3ofs,0);
read(W3,$raw,16);
die unless substr($raw,0,2) eq "W3";

# appears to take the form:
#
# +0x00   char[2]    "W3"
# +0x02   uint16_t   Windows version (0x0300 = 3.0 or 0x030A = 3.10)
# +0x04   uint16_t   Number of directory elements

my $winver = unpack("v",substr($raw,2,2));
print "This is Windows kernel ".sprintf("%u.%u",$winver >> 8,$winver & 0xFF)."\n";

# must be kernel 3.x
die "Unsupported Windows kernel" unless (($winver >> 8) == 3 || ($winver >> 8) == 4);

my $wincount = unpack("v",substr($raw,4,2));
print "There are $wincount files here\n";

die "Unsupported file count" unless ($wincount > 0 && $wincount < 256); # Windows 3.0 and Windows 3.1 have somewhere between 20 and 30 items

# make fake MS-DOS stub by copying the one in the kernel :)
my $msdos_stub;
seek(W3,0,0);
read(W3,$msdos_stub,$w3ofs);

# read! Each entry is 16 bytes
$fofs = $w3ofs + 16;
for ($i=0;$i < $wincount;$i++,$fofs += 16) {
# +0x00   char[8]    Name of file
# +0x08   uint32_t   offset
# +0x0C   uint32_t   length
    seek(W3,$fofs,0);
    read(W3,$raw,16);

    my $name,$ofs,$len;
    $name = substr($raw,0,8);
    ($ofs,$len) = unpack("VV",substr($raw,8));

    $name =~ s/ +$//;
    print "    \"$name\" at @".sprintf("0x%08x",$ofs).", $len bytes\n";

    next if $len < 64;
    next if $len > (4 * 1024 * 1024); # avoid funny sizes
    next if $ofs < $w3ofs;

    # name should only be alphanumeric
    next unless $name =~ m/^[a-z0-9]+$/i;

    open(OUT,">$name.386") || die;
    binmode(OUT);

    # read
    seek(W3,$ofs,0);
    read(W3,$raw,$len);

    # Fun fact, the images stored in the kernel are just raw LE images.
    # Other unexpected features:
    #
    # - Offset points directly at the "LE" header, no stub
    # - Length is only the length of the LE header
    # - The Data Pages Offset field in the LE header is
    #   an absolute offset from the start of WIN386.EXE
    #
    # So the only way to know how much to extract and where is to
    # read the LE header, determine where the pages are in WIN386.EXE,
    # then use the Page Map to know how much to copy and from where.
    $dpo = unpack("V",substr($raw,0x80,4));
    $obj_pm_ofs = unpack("V",substr($raw,0x48,4));
    $mempages = unpack("V",substr($raw,0x14,4));
    $mempgsz = unpack("V",substr($raw,0x28,4));
    $lastpgsz = unpack("V",substr($raw,0x2C,4));

    print "      * Data Pages Offset: $dpo\n";
    print "      * Object Page Map Table: $obj_pm_ofs (LE-relative)\n";
    print "      * Number of pages: $mempages\n";
    print "      * Memory page size: $mempgsz (last=$lastpgsz)\n";

    # figure it out
    $totalsz = ($mempages * $mempgsz) + $lastpgsz;
    print "      * Total image size: $totalsz\n";

    # sanity check
    die unless $dpo >= ($ofs + $len);
    $ex = $dpo - ($ofs + $len);
    print "      * Extra: $ex\n";
    die unless $ex < 65536;

    my $extra;
    read(W3,$extra,$ex);

    # now we have to hack the Data Pages Offset field
    $nofs = length($msdos_stub) + length($raw) + length($extra);
    print "      * New offset: $nofs\n";

    # also, Non-resident data offset fields seem to not actually point anywhere
    # except to some mystery LE block. when we extract these LE images, the
    # offset + LE header offset points just at the end of file. So hack it to
    # point to our EOF and make a fake non-resident name table
    $nonres_ofs = $nofs + (($mempages - 1) * $mempgsz) + $lastpgsz;
    $nonres_name = uc($name)."_DDB";
    $nonres_fakemod = "W3 extracted $name module";
    $nonres_sz =
        1 + length($nonres_fakemod) + 2 +
        1 + length($nonres_name) + 2 + 1; # length + string + ordinal + 0 length

    print "      * New nonres ofs: $nonres_ofs (sz=$nonres_sz)\n";

    #              1                         length       ordinal (2) 1
    $nonres_data =
        chr(length($nonres_fakemod)).$nonres_fakemod.pack("v",0).
        chr(length($nonres_name)).$nonres_name.pack("v",1).chr(0);

    $raw = substr($raw,0,0x80).pack("V",$nofs).substr($raw,0x84);
    $raw = substr($raw,0,0x88).pack("VV",$nonres_ofs,$nonres_sz).substr($raw,0x90);

    # write header and stub
    print OUT $msdos_stub;
    print OUT $raw;
    print OUT $extra;

    # copy
    for ($page=1;$page <= $mempages;$page++) {
        $sz = ($page == $mempages) ? $lastpgsz : $mempgsz;

        $ent = substr($raw,$obj_pm_ofs+(($page-1)*4),4);
        ($eflags,$eofs) = unpack("vv",$ent);

        $src_page_offset = $dpo + (($eofs - 1) * $mempgsz);
        print "       (from $src_page_offset)\n";

        my $page;
        seek(W3,$src_page_offset,0);
        read(W3,$page,$sz);
        print OUT $page;
    }

    # nonresident name
    print OUT $nonres_data;

    # done
    close(OUT);
}

close(W3);

