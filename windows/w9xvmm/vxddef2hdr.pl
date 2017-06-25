#!/usr/bin/perl
#
# vxddef2hdr.pl <basename>
#
# example:
#
# vxddef2hdr.pl dev_vxd_dev_vmm
#
# to compile dev_vxd_dev_vmm.vxdcalls and dev_vxd_dev_vmm.vxddef
#
# spits header to stdout
my $basename = shift @ARGV;
die "Need basename" unless defined($basename);

my $defname = "$basename.vxddef";
die "Missing vxddef" unless -f $defname;

my $callsname = "$basename.vxdcalls";
# not required to exist

my $section = undef;

# what to gather (header)
my $vxddevname = undef;
my $deviceid = undef;
my $minwinver = undef;

my $maxnamelen = 0;

# read def
open(DEF,"<",$defname) || die "Unable to open vxddef";

# read header
while (my $line = <DEF>) {
    chomp $line;
    $line =~ s/^[ \t]*//; # eat leading whitespace
    $line =~ s/[ \t]*#.*$//; # eat comments
    $line =~ s/[ \t]*$//; # eat trailing whitespace
    next if $line eq "";

    if ($line =~ s/^%[ \t]*//) {
        $section = lc($line); # s/// modified it in place
        next;
    }

    if ($section eq "header") {
        my @a = split(/[ \t]+/,$line);

        next if @a == 0;

        $try = lc($a[0]);
        if ($try eq "vxddevname") {
            $vxddevname = $a[1];
            die "invalid VXD name $vxddevname" unless $vxddevname =~ m/^[a-zA-Z0-9]+$/i;
        }
        elsif ($try eq "deviceid") {
            $deviceid = $a[1];
            die "invalid VXD device ID $deviceid" unless ($deviceid =~ m/^0x[0-9a-fA-F]+$/i && length($deviceid) <= (2+4)); # 0xABCD
        }
        elsif ($try eq "minwinver") {
            $minwinver = $a[1];
            die "invalid min win ver $minwinver" unless $minwinver =~ m/^[34]\.[0-9]+$/;
        }
    }
}

# required field verification
die "Missing header fields. Required: VXDDEVNAME, DEVICEID, and MINWINVER" unless (defined($vxddevname) && defined($deviceid) && defined($minwinver));

$x = length($vxddevname."_Device_ID");
$maxnamelen = $x if $maxnamelen < $x;

# VXD calls
if (open(CALLS,"<",$callsname)) {
    while (my $line = <CALLS>) {
        chomp $line;

# 3.0+     0001H 0000H  Get_VMM_Version
# version  dev   serv   name
        my @a = split(/[ \t]+/,$line);
        next if @a < 3; # name is optional

        die "invalid win ver $a[0] in call" unless $a[0] =~ m/^[34]\.[0-9]+\+{0,1}$/i;

        die "invalid dev $a[1] in call" unless $a[1] =~ m/^[0-9a-fA-F]{4}H$/i;

        die "invalid srv $a[2] in call" unless $a[2] =~ m/^[0-9a-fA-F]{4}H$/i;

        if (defined($a[3]) && $a[3] ne "") {
            die "invalid name $a[3] in call" unless $a[3] =~ m/^[0-9a-zA-Z_]{1,128}$/i;
        }

        $x = $a[1];
        $x =~ s/H$//i; # remove trailing H
        $x = "0x".$x;
        die "dev $x does not match $deviceid" unless lc($x) eq lc($deviceid);

        my @x;

        $x = $a[2];
        $x =~ s/H$//i; # remove trailing H
        $x = "0x".$x;
        $a[2] = $x;

        $a[3] = "" unless defined($a[3]);

        push(@x,$a[0]);
        push(@x,$a[2]);
        push(@x,$a[3]);

        $x = length($vxddevname."_snr_".$a[3]);
        $maxnamelen = $x if $maxnamelen < $x;

        push(@calls,\@x);
    }
    close(CALLS);
}

my $padname = 8;
$maxnamelen += $padname;

print "/* VXD device ID. Combine with service call ID when using VMMCall/VMMJmp */\n";
$x = $vxddevname."_Device_ID".(' ' x $maxnamelen);
print "#define ".substr($x,0,$maxnamelen)." ".$deviceid."\n";
print "\n";

if (@calls > 0) {
    print "/* VXD services (total: ".@calls.", ".sprintf("0x%04X",@calls - 1).") */\n";
    for ($i=0;$i < @calls;$i++) {
        my $r = $calls[$i];
        my @ar = @{$r};

        if ($ar[2] ne "") {
            $x = $vxddevname."_snr_".$ar[2].(' ' x $maxnamelen);
            print "#define ".substr($x,0,$maxnamelen)."     ".$ar[1]."    ";
        }
        else {
            $x = "no name".(' ' x $maxnamelen);
            print "/*      ".substr($x,0,$maxnamelen)."     ".$ar[1]." */ ";
        }

        print "/* ver $ar[0] */";

        print "\n";
    }

    print "\n";
}

close(DEF);

