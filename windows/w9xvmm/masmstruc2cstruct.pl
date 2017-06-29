#!/usr/bin/perl
#
# Take basic MASM-style struct def and generate C structure
# (C) 2017 Jonathan Campbell
# 
# Take MASM struct def on STDIN and write C struct to STDOUT

sub d2sz($) {
    my $x = shift @_;

    return 1 if $x =~ m/^db$/;
    return 2 if $x =~ m/^dw$/;
    return 4 if $x =~ m/^dd$/;
    return 8 if $x =~ m/^dq$/;

    return 0;
}

sub sztotypedef($) {
    my $x = shift @_;

    return "uint8_t" if $x == 1;
    return "uint16_t" if $x == 2;
    return "uint32_t" if $x == 4;
    return "uint64_t" if $x == 8;

    return "unsigned int";
}

$mode = '';

while (my $line = <STDIN>) {
    chomp $line;

    $line =~ s/^[ \t]+//;
    $line =~ s/[ \t]+$//;

    my @a = split(/[ \t]+/,$line);

    if ($mode eq '') {
        # look for <xxxxx> struc
        # ends at <xxxxx> ends
        if ($a[1] =~ m/struc/i) {
            $name = $a[0];

            die "Invalid struct name $name" unless $name =~ m/^[0-9a-zA-Z_]+$/i;

            print "/* struct $name */\n";

            $offset = 0;

            print "typedef struct $name {\n";

            $mode = 'struc';
        }
    }
    elsif ($mode eq 'struc') {
        $comment = '';

        $i = index($line,';');
        if ($i >= 0) {
            $comment = substr($line,$i+1);
            $comment =~ s/^[ \t]+//;
            $comment =~ s/[ \t]+$//;
            $line = substr($line,0,$i);
        }

        $line =~ s/^[ \t]+//;
        $line =~ s/[ \t]+$//;

        my @a = split(/[ \t]+/,$line);

# Client_Alt_DS dw ?
#
# or
#
# dw	?
        next if @a == 0;

        if ($a[0] =~ m/^d[bwdq]$/i) {
            # unnamed def
            $count = 1;

            if ($a[1] eq "?") {
            }
            elsif ($a[1] =~ m/^\d+$/ && $a[2] eq "dup" && ($a[3] eq "?" || $a[3] eq "(?)")) { # 5 dup ?
                $count = int($a[1]);
            }
            else {
                die "struct member def must be ? in $line" unless $a[1] eq "?";
            }

            do {
                $sz = d2sz($a[0]);
                print "    ".substr(sztotypedef($sz).(' 'x15),0,15)." ".substr("__unnamed_$offset;".(' 'x31),0,31);
                print " /* +".sprintf("0x%04X",$offset)." $comment */";
                print "\n";

                $offset += $sz;
                $count--;
            } while ($count > 0);
        }
        elsif ($a[1] =~ m/^d[bwdq]$/i) {
            # named def
            die "struct member def must be ? in $line" unless $a[2] eq "?";

            die "Invalid struct member ".$a[0] unless $a[0] =~ m/^[0-9a-zA-Z_]+$/i;

            $sz = d2sz($a[1]);
            print "    ".substr(sztotypedef($sz).(' 'x15),0,15)." ".substr($a[0].";".(' 'x31),0,31);
            print " /* +".sprintf("0x%04X",$offset)." $comment */";
            print "\n";

            $offset += $sz;
        }
        elsif ($a[0] eq $name && $a[1] eq "ends") {
            $mode = '';

            print "} $name;\n";

            print "/* end $name */\n";
            print "\n";
        }
        else {
            die "Unexpected line $line";
        }
    }
}

