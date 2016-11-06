#!/usr/bin/perl
#
# Take .DEF file for a CLSG module, parse it, and generate a header.asm
# to generate the CLSG header in the EXE image.
#
# (C) 2016 Jonathan Campbell, ALL RIGHTS RESERVED
my $out_make_stub_entry = 1;
my $out_enumname = undef;
my $out_enumbase = undef;
my $out_asmname = undef;
my $in_defname = undef;
my $a;

for ($i=0;$i < @ARGV;) {
    $a = $ARGV[$i++];

    if ($a =~ s/^-+//) {
        if ($a eq "def") {
            $in_defname = $ARGV[$i++];
        }
        elsif ($a eq "asm") {
            $out_asmname = $ARGV[$i++];
        }
        elsif ($a eq "stub") {
            $out_make_stub_entry = 1;
        }
        elsif ($a eq "no-stub") {
            $out_make_stub_entry = 0;
        }
        elsif ($a eq "enum") {
            $out_enumname = $ARGV[$i++];
        }
        elsif ($a eq "enum-base") {
            $out_enumbase = $ARGV[$i++];
        }
        elsif ($a eq "h" || $a eq "help") {
            print STDERR "CLSG module header generator.\n";
            print STDERR "(C) 2016 Jonathan Campbell.";
            print STDERR "\n";
            print STDERR "--def <file>             Input DEF file to parse\n";
            print STDERR "--asm <file>             Output ASM file to generate\n";
            print STDERR "--stub / --no-stub       Generate/Don't generate EXE entry stub\n";
            print STDERR "--enum <file>            Output header file to generate\n";
            print STDERR "--enum-base <name>       Prefix to apply to enum names\n";
            exit 1;
        }
        else {
            print STDERR "Unknown switch $a\n";
            exit 1;
        }
    }
    else {
        print STDERR "Unexpected arg $a\n";
        exit 1;
    }
}

if (!defined($in_defname) || $in_defname eq "") {
    print STDERR "Required: --def input file. See -h / --help\n";
    exit 1;
}

my $has_undef_entry = 0;
my $mode = '';
my $library = undef;
my $last_ordinal = undef;
my @symbols = ( ); # names, index by ordinal. undef means not defined.

open(DEF,"<",$in_defname) || die "Cannot oepn DEF";

foreach my $line (<DEF>) {
    chomp $line;
    $line =~ s/ *[;#].*$//;
    next if $line eq "";

    my @a = split(/[ \t]+/,$line);

    if (lc($a[0]) eq "library") {
        $library = $a[1];
        die "invalid LIBRARY value $library" unless $library =~ m/^[a-zA-Z0-9_]+$/i;
    }
    elsif (lc($a[0]) eq "exports") {
        $mode = lc($a[0]);
    }
    elsif ($a[0] eq "") {
        # indent for whatever
        if ($mode eq "exports") {
            # [0] = ""
            # [1] = symbol
            # [2] = "@ordinal" (optional)
            my $ord = undef;
            my $name = undef;
            my $ii = 0;
            shift(@a);

            while (@a > 0) {
                if ($a[0] =~ m/^\@\d+/) {
                    $ord = int(substr($a[0],1)+0);
                }
                else {
                    if ($ii == 0) {
                        $name = $a[0];
                    }
                    else {
                        die "Unknown ii=$ii ".$a[0];
                    }

                    $ii++;
                }

                shift(@a);
            }

            if (!defined($ord)) {
                if (defined($last_ordinal)) {
                    $ord = $last_ordinal + 1;
                }
                else {
                    $ord = 0;
                }
            }

            die unless defined($name) && $name ne "";
            die if $ord < 0 || $ord >= 16384;
            $last_ordinal = $ord;

            # enter it
            while (@symbols <= $ord) {
                push(@symbols,undef);
            }

            if (defined($symbols[$ord])) {
                die "Ordinal $ord already defined";
            }

            $symbols[$ord] = $name;
        }
        else {
            die "unkown indented line $line";
        }
    }
    else {
        die "unknown section ".$a[0];
    }
}

close(DEF);

for ($ord=0;$ord < @symbols;$ord++) {
    $name = $symbols[$ord];
    if (!defined($name)) {
        $has_undef_entry = 1;
    }
}

if (!defined($out_asmname) || $out_asmname eq "") {
    die "Need --asm output";
}

open(ASM,">",$out_asmname) || die;
print ASM "; auto-generated code, do not edit\n";
print ASM "bits 16			; 16-bit real mode\n";
print ASM "\n";
print ASM "section _TEXT class=CODE\n";
print ASM "\n";
if ($out_make_stub_entry > 0) {
    print ASM "global _exe_normal_entry\n";
}
print ASM "global _header_sig\n";
print ASM "global _header_function_count\n";
print ASM "global _header_function_table\n";
print ASM "global _header_fence\n";
print ASM "global _header_end\n";

print ASM "\n";
for ($ord=0;$ord < @symbols;$ord++) {
    $name = $symbols[$ord];
    if (defined($name)) {
        print ASM "extern _$name\n";
    }
}
print ASM "\n";

print ASM "; header. must come FIRST\n";
print ASM "_header_sig:\n";
print ASM "        db          \"CLSG\"          ; Code Loadable Segment Group\n";
print ASM "_header_function_count:\n";
print ASM "        dw          ".@symbols."               ; number of functions\n";
print ASM "_header_function_table:\n";
for ($ord=0;$ord < @symbols;$ord++) {
    $name = $symbols[$ord];
    if (defined($name)) {
        print ASM "        dw          _$name\n";
    }
    else {
        print ASM "        dw          __empty_slot\n";
    }
}
print ASM "_header_fence:\n";
print ASM "        dw          0xFFFF          ; fence\n";
print ASM "_header_end:\n";
print ASM "\n";

if ($has_undef_entry) {
    print ASM "__empty_slot:\n";
    print ASM "        retf\n";
    print ASM "\n";
}

if ($out_make_stub_entry > 0) {
    print ASM "; EXE entry point points here, in case the user foolishly tries to run this EXE\n";
    print ASM "..start:\n";
    print ASM "_exe_normal_entry:\n";
    print ASM "        mov         ax,4C00h        ; if the user tries to run this EXE, terminate\n";
    print ASM "        int         21h\n";
    print ASM "\n";
}

close(ASM);

if (!defined($out_enumbase) && defined($library) && $library ne '') {
    $out_enumbase = $library."_";
}

if (defined($out_enumname) && $out_enumname ne "" && defined($out_enumbase) && $out_enumbase ne '' && defined($library) && $library ne '') {
    my $emit_n = 1;

    open(ENUM,">",$out_enumname) || die;

    print ENUM "/* C enumeration of functions for library $library */\n";

    print ENUM "enum {\n";
    for ($ord=0;$ord < @symbols;$ord++) {
        $name = $symbols[$ord];
        if (defined($name)) {
            print ENUM "\t$out_enumbase$name";
            if ($emit_n) {
                print ENUM "=$ord";
                $emit_n = 0;
            }
            print ENUM ", /* =$ord */\n";
        }
        else {
            $emit_n = 1;
        }
    }
    print ENUM "};\n";

    close(ENUM);
}

