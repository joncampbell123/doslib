#!/usr/bin/perl
#
# Generate sin/cos tables for the Woooo intro so the rotozoomer can rotate properly.
# Because you can't assume an FPU if targeting anything below a Pentium.
# For memory constraint reasons only a quarter of a sine wave is rendered.
# The rest of the sine wave can be made by reversing direction past the quarter point
# and flipping sign past the halfway point. Note that only a sine table is needed
# since cosine is just sine shifted by 90 degrees.
use Math::Trig;

my $i;
my $max = 2048;

open(ST,">sin$max.txt");
open(SB,">sin$max.bin");
binmode(SB);

for ($i=0;$i < $max;$i++) {
    $x = sin(($i * (pi/2)) / ($max - 1));
    $xi = int($x * 0x7FFF);
    print ST "$xi, /* $x */\n";
    print SB pack("v",$xi);
}

close(SB);
close(ST);

