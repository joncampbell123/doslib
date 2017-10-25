#!/usr/bin/perl

for ($i=0;$i < 256;$i++) {
    $r = 1000000 / (256 - $i);
    print sprintf("0x%02X: %u\n",$i,$r);
}
