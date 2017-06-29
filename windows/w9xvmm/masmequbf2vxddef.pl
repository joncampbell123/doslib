#!/usr/bin/perl
#
# Take in MASM EQUates of a bitfield, spit out a vxddef definition
#
# STDIN = file with EQUates
# STDOUT = vxddef definitions without %defconst...

print  "#                                                           default=1   default=(1<<width)-1\n";
print  "#       constant                                bit number  bit width   value   comment\n";
#       0       8                                       48          60          72      80
#       0       8       16      24      32      40      48      56      64      72      80      88

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

        print substr("def".(' 'x8),0,8);
        print substr($name.(' 'x40),0,40);
        print substr($bitpos.(' 'x12),0,12);
        if ($bitwidth > 1) {
            print substr($bitwidth,(' 'x12),0,12);
            if ($bitval != ((1 << $bitwidth) - 1)) {
                print substr($bitval.(' 'x8),0,8);
            }
        }
        print "\n";

#def     Low_Pri_Device_Boost        4                             ; For events that need timely processing, but are not time critical
    }
}

