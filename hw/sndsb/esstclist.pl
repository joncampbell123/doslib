#!/usr/bin/perl

for ($i=0;$i < 256;$i++) {
    if ($i >= 128) {
        $r = 795500 / (256 - $i);
    }
    else {
        $r = 397700 / (128 - $i);
    }

    print sprintf("0x%02X: %u\n",$i,$r);
}
