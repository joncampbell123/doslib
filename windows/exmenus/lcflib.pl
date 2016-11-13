#/usr/bin/perl
my $cmd = shift @ARGV; # --filter or --nofilter
my $lib = shift @ARGV; # lib output file path
my $lcf = shift @ARGV;
die unless -f $lcf;
unlink($lib);

#   wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos.obj -+$(SUBDIR)$(HPS)biosext.obj
#   wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)himemsys.obj -+$(SUBDIR)$(HPS)emm.obj
#   wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosbox.obj -+$(SUBDIR)$(HPS)biosmem.obj
#   wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)biosmem3.obj

sub shellesc($) {
    my $x = shift @_;
    $x =~ s/([^a-zA-Z0-9\.])/\\$1/g;
    return $x;
}

open(LCF,"<",$lcf) || die;

my $ex;
foreach my $line (<LCF>) {
    $line =~ s/[\x0D\x0A]+$//g;

    # usually takes the form ++'_HelloMsg@0'.'HELLDLD1.DLL'..'HelloMsg'
    $line =~ s/^ +//;
    $line =~ s/ +$//;
    next unless $line =~ s/^\+\+//;

    if ($cmd eq "--filter" ) {
        # chop off the DLL in the 2nd field
        $line =~ s/\.DLL\'/\'/;
    }

    $ex = "wlib -q -b -c ".shellesc($lib)." ++".shellesc($line);
    print "Running: $ex\n";
    $x = system($ex);
    die unless $x == 0;
}

close(LCF);

