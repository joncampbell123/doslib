#!/usr/bin/perl
#
# Take in MASM EQUates of a bitfield, spit out a vxddef definition
#
# STDIN = file with EQUates
# STDOUT = vxddef definitions without %defconst...

print "# constant bitpos bitwidth bitval\n";

while (my $line = <STDIN>) {
    chomp $line;
    my @a = split(/[ \t]+/,$line);

    if ($a[1] =~ m/^equ$/i) {
        my $name = $a[0];
        my $value = $a[2];

        die unless $name =~ m/^[0-9a-zA-Z_]+$/i;

        next unless $value =~ m/^[01]+b$/i; # we're looking for 000000b binary defines

        # perl oct() takes 0bxxxxxxx as binary!
        $value =~ s/b$//i;
        $value = "0b".$value;

        $value = oct($value);

        next unless $value != 0;

        $bitpos = 0;
        while ((($value >> $bitpos) & 1) == 0) {
            $bitpos++;
        }

        $bitval = $value >> $bitpos;
        $bitwidth = 1;
        $x = 0;
        while (($x+$bitpos) < 64) {
            $bitwidth = ($x+1) if (($value & (1 << ($x+$bitpos))) != 0);
            $x++;
        }

        print "def $name $bitpos";
        if ($bitwidth > 1) {
            print " $bitwidth";
            if ($bitval != ((1 << $bitwidth) - 1)) {
                print " $bitval";
            }
        }
        print "\n";

#def     Low_Pri_Device_Boost        4                             ; For events that need timely processing, but are not time critical
    }
}

