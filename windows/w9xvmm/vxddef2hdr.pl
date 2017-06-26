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
use Data::Dumper;

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
        last if $section eq "endheader";
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

sub reg2gccspec($) {
    my $v = shift @_;

    return "\@ccz" if $v =~ m/^zf$/i;
    return "\@ccnz" if $v =~ m/^\!zf$/i;

    return "\@ccc" if $v =~ m/^cf$/i;
    return "\@ccnc" if $v =~ m/^\!cf$/i;

    return "a" if $v =~ m/^(al|ax|eax)$/i;
    return "b" if $v =~ m/^(bl|bx|ebx)$/i;
    return "c" if $v =~ m/^(cl|cx|ecx)$/i;
    return "d" if $v =~ m/^(dl|dx|edx)$/i;
    return "S" if $v =~ m/^(si|esi)$/i;
    return "D" if $v =~ m/^(di|edi)$/i;

    return "";
}

sub reg2type($) {
    my $v = shift @_;

    $v =~ s/^\!//; # remove !

    return "_Bool" if $v =~ m/^(zf|cf)$/i;

    return "uint8_t" if $v =~ m/^(al|ah|bl|bh|cl|ch|dl|dh)$/i;

    return "uint16_t" if $v =~ m/^(ax|bx|cx|dx|si|di|bp)$/i;

    return "uint32_t" if $v =~ m/^(eax|ebx|ecx|edx|esi|edi|ebp)$/i;

    return "unsigned int";
}

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

# function defs
print "#if defined(__GNUC__) /* GCC only, for now */\n";
print "# if defined(GCC_INLINE_ASM_SUPPORTS_cc_OUTPUT) /* we require GCC 6.1 or higher with support for CPU flags as output */\n";
my %funcdef;
while (my $line = <DEF>) {
    chomp $line;
    $line =~ s/^[ \t]*//; # eat leading whitespace
    $line =~ s/[ \t]*#.*$//; # eat comments
    $line =~ s/[ \t]*$//; # eat trailing whitespace
    next if $line eq "";

    if ($line =~ s/^%[ \t]*//) {
        $section = lc($line); # s/// modified it in place

        if ($section eq "enddef") {
            #print Dumper(\%funcdef);

            # check: we allow '.' as an output struct member IF it's the only output
            if (exists($funcdef{struct})) {
                my %x = %{$funcdef{struct}};

                if (exists($x{'.'})) {
                    die "only one output allowed if default '.' output is defined" if (scalar(keys %x) > 1);
                }
            }

            # okay, generate the code
            my $serviceid = undef;
            my $funcname = undef;
            if (exists($funcdef{byname})) {
                $funcname = $funcdef{byname};

                for ($i=0;$i < @calls;$i++) {
                    my $r = $calls[$i];
                    my @ar = @{$r};

                    if ($ar[2] eq $funcname) {
                        $serviceid = $ar[1];
                        last;
                    }
                }

                die "cannot locate service id for $funcname" unless defined($serviceid);
            }
            else {
                die "Cannot determine name for function";
            }

            # emit
            print "/*-------------------------------------------------------------*/\n";
            print "/* $vxddevname $funcname (VMMCall dev=$deviceid serv=$serviceid) */\n";
            print "\n";
            if (exists($funcdef{description})) {
                my @b = split(/\n/,$funcdef{description});
                print "/* description: */\n";
                for ($i=0;$i < @b;$i++) {
                    print "/*   ".$b[$i]." */\n";
                }
                print "\n";
            }
            if (exists($funcdef{in})) {
                my %f = %{$funcdef{in}};
                print "/* inputs: */\n";
                while (($key,$value) = each %f) {
                    print "/*   ".uc($key)." = ".$value." ";
                    if (exists($funcdef{incomment})) {
                        if (exists($funcdef{incomment}{$key})) {
                            print "(".$funcdef{incomment}{$key}.") ";
                        }
                    }
                    print "*/\n";
                }
                print "\n";
            }
            else {
                print "/* inputs: */\n";
                print "/*   None */\n";
                print "\n";
            }
            if (exists($funcdef{out})) {
                my %f = %{$funcdef{out}};
                print "/* outputs: */\n";
                while (($key,$value) = each %f) {
                    if ($value eq '.') {
                        print "/*   ".uc($key)." = ";
                        if (exists($funcdef{outcomment})) {
                            if (exists($funcdef{outcomment}{$key})) {
                                print $funcdef{outcomment}{$key}." ";
                            }
                        }
                        print "*/\n";
                    }
                    else {
                        print "/*   ".uc($key)." = ".$value." ";
                        if (exists($funcdef{outcomment})) {
                            if (exists($funcdef{outcomment}{$key})) {
                                print "(".$funcdef{outcomment}{$key}.") ";
                            }
                        }
                        print "*/\n";
                    }
                }
                print "\n";
            }
            else {
                print "/* outputs: */\n";
                print "/*   None */\n";
                print "\n";
            }

            if (exists($funcdef{return})) {
                my @f = split(/\n/,$funcdef{return});
                print "/* returns: */\n";
                for ($i=0;$i < @f;$i++) {
                    print "/*   ".$f[$i]." */\n";
                }
                print "\n";
            }

            my $params = "void";
            if (exists($funcdef{in})) {
                my %f = %{$funcdef{in}};
                my $fc = 0;
                while (($key,$value) = each %f) {
                    $params = "" if $fc == 0;

                    $ptype = $funcdef{paramtype}{$value};
                    if (defined($ptype) && $ptype ne "") {
                        $ptype = "const ".$ptype;
                    }
                    else {
                        $ptype = "const ".reg2type($key);
                    }

                    $params .= "," unless $params eq "";
                    $params .= $ptype." ".$value."/*".$key."*/";

                    $fc++;
                }
            }

            $directreg = 0;
            my $rettype = "void";
            if (exists($funcdef{out})) {
                if (exists($funcdef{struct}{'.'})) {
                    $ptype = $funcdef{structtype}{'.'};
                    if (defined($ptype) && $ptype ne "") {
                        $ptype = $ptype;
                    }
                    else {
                        $ptype = reg2type($funcdef{struct}{'.'});
                    }

                    $rettype = $ptype;
                    $directreg = 1;
                }
                else {
                    # declare a struct of the same name, as return value.
                    # GCC is smart enough to optimize access to members down
                    # to direct register access.
                    $structname = $funcname."__response";
                    $directreg = 0;

                    print "typedef struct $structname {\n";

                    my %f = %{$funcdef{struct}};
                    while (($key,$value) = each %f) {
                        $ptype = $funcdef{structtype}{$key};
                        if (defined($ptype) && $ptype ne "") {
                            $ptype = $ptype;
                        }
                        else {
                            $ptype = reg2type($value);
                        }

                        print "    ".$ptype;
                        print " ".$key;
                        print "; /* ".uc($value)." */";
                        print "\n";
                    }

                    print "} $structname;\n";
                    print "\n";

                    $rettype = $structname;
                }
            }

            print "static inline $rettype $funcname($params) {\n";
            print "    register $rettype r;\n";
            print "\n";
            print "    __asm__ (\n";
            print "        VXD_AsmCall(".$vxddevname."_Device_ID,".$vxddevname."_snr_".$funcname.")\n";
            print "        : /* outputs */";
            if ($directreg) {
                print " \"=".reg2gccspec($funcdef{struct}{'.'})."\" (r)";
            }
            elsif (exists($funcdef{out})) {
                my %f = %{$funcdef{out}};
                $fc = 0;
                while (($key,$value) = each %f) {
                    print "," if $fc > 0;
                    print " \"=".reg2gccspec($key)."\" (r.$value)";
                    $fc++;
                }
            }
            print "\n";

            print "        : /* inputs */";
            if (exists($funcdef{in})) {
                my %f = %{$funcdef{in}};
                my $fc = 0;
                while (($key,$value) = each %f) {
                    print "," if $fc > 0;
                    print " \"".reg2gccspec($key)."\" ($value)";
                    $fc++;
                }
            }
            print "\n";

            print "        : /* clobbered */";
            print "\n";
 
            print "    );\n\n";

            print "    return r;\n";
            print "}\n";

            print "\n";

            # start again
            undef %funcdef;
        }

        next;
    }

    if ($section eq "defcall") {
        my @a = split(/[ \t]+/,$line);

        next if @a < 1;

        if ($a[0] eq "return") {
            if (exists($funcdef{return})) {
                $funcdef{return} .= "\n";
            }
            else {
                $funcdef{return} = "";
            }

            for ($i=1;$i < @a;$i++) {
                $funcdef{return} .= " " if $i > 1;
                $funcdef{return} .= $a[$i];
            }
        }
        elsif ($a[0] eq "byname") {
            $funcdef{byname} = $a[1];
        }
        elsif ($a[0] eq "description") {
            if (exists($funcdef{description})) {
                $funcdef{description} .= "\n";
            }
            else {
                $funcdef{description} = "";
            }

            for ($i=1;$i < @a;$i++) {
                $funcdef{description} .= " " if $i > 1;
                $funcdef{description} .= $a[$i];
            }
        }
        elsif ($a[0] eq "in") {
            $i = index($line,';');
            my $comment = '';
            if ($i >= 0) {
                $comment = substr($line,$i+1);
                $comment =~ s/^[ \t]+//;
                $line = substr($line,0,$i);
            }
            my @a = split(/[ \t]+/,$line);

            if (!exists($funcdef{in})) {
                $funcdef{in} = { };
            }

            if (!exists($funcdef{param})) {
                $funcdef{param} = { };
            }

            if (!exists($funcdef{paramcomment})) {
                $funcdef{paramcomment} = { };
            }

            my $type = '';

            $i = index($a[1],'=');
            if ($i >= 0) {
                $type = substr($a[1],$i+1);
                $a[1] = substr($a[1],0,$i);
            }

# the name '.' is allowed to mean no name if it's the ONLY return value
# out             AX        version                                 ; major, minor (example: 0x030A = 3.10)
# out             register  name                                    ; comment
            $a[1] = lc($a[1]);
            $a[2] = lc($a[2]);

            die "register $a[1] already allocated" if exists($funcdef{in}{$a[1]});
            die "param already has name $a[2]" if exists($funcdef{param}{$a[2]});

            $funcdef{in}{$a[1]} = $a[2];
            $funcdef{param}{$a[2]} = $a[1];
            $funcdef{incomment}{$a[1]} = $comment;
            $funcdef{paramcomment}{$a[2]} = $comment;
            $funcdef{paramtype}{$a[2]} = $type;
        }
        elsif ($a[0] eq "out") {
            $i = index($line,';');
            my $comment = '';
            if ($i >= 0) {
                $comment = substr($line,$i+1);
                $comment =~ s/^[ \t]+//;
                $line = substr($line,0,$i);
            }
            my @a = split(/[ \t]+/,$line);

            if (!exists($funcdef{out})) {
                $funcdef{out} = { };
            }

            if (!exists($funcdef{struct})) {
                $funcdef{struct} = { };
            }

            if (!exists($funcdef{structcomment})) {
                $funcdef{structcomment} = { };
            }

            my $type = '';

            $i = index($a[1],'=');
            if ($i >= 0) {
                $type = substr($a[1],$i+1);
                $a[1] = substr($a[1],0,$i);
            }

# the name '.' is allowed to mean no name if it's the ONLY return value
# out             AX        version                                 ; major, minor (example: 0x030A = 3.10)
# out             register  name                                    ; comment
            $a[1] = lc($a[1]);
            $a[2] = lc($a[2]);

            die "register $a[1] already allocated" if exists($funcdef{out}{$a[1]});
            die "struct already has name $a[2]" if exists($funcdef{struct}{$a[2]});

            $funcdef{out}{$a[1]} = $a[2];
            $funcdef{struct}{$a[2]} = $a[1];
            $funcdef{outcomment}{$a[1]} = $comment;
            $funcdef{structcomment}{$a[2]} = $comment;
            $funcdef{structtype}{$a[2]} = $type;
        }
        else {
            die "Unknown defcall $a[0]";
        }
    }
}
print "# endif /*GCC_INLINE_ASM_SUPPORTS_cc_OUTPUT*/\n";
print "#endif /*defined(__GNUC__)*/\n";

close(DEF);

