#!/usr/bin/perl
my $file = $ARGV[0];
die unless -f $file;

mkdir("gnuplot");

$filenn = $file;
$filenn =~ s/\.txt$//i;

open(I,"<",$file) || die;

my @sb16;

my $line;
my $ref = undef;
while ($line = <I>) {
    chomp $line;

    if ($line =~ m/^SB16 4\.x single cycle/i) {
        $ref = "sb16";
    }
    #  - Rate 0x0000: expecting 0Hz, 480b/0.096s @ 5000.361Hz
    elsif ($line =~ s/^ *- Rate *//) {
        my @a = split(/ +/,$line);

        $idx = shift(@a);
        next unless $idx =~ m/^0x[0-9a-f]+: */i;

        $idx = hex($idx);
        next unless $idx >= 0 && $idx <= 0xFFFF;

        $x = shift(@a);
        next unless $x =~ m/expecting/i;

        $expect = shift(@a);
        next unless $expect =~ m/\d+Hz,/;
        $expect = int($expect);

        shift(@a); # throw away 4000b/0.985s

        $x = shift(@a);
        next unless $x eq "@";

        $rate = shift(@a);
        next unless (defined($rate) && $rate ne "");
        $rate = $rate + 0.0;

        if ($ref eq "sb16") {
            $sb16[$idx] = $rate;
        }
    }
    else {
        $ref = undef;
    }
}

close(I);

exit 0 unless @sb16 > 0;




$png1 = "gnuplot/$file.sb16.png";
$csv = $ofile = "gnuplot/$file.sb16.csv";
open(O,">",$ofile) || die;

print O "# tc, sb16";
print O "\n";

for ($i=0;$i < 65536;$i++) {
    # tc
    print O "$i";

    # sb16
    print O ", ".$sb16[$i];

    print O "\n";
}

close(O);

$ofile = "gnuplot/$file.sb16.gnuplot";
open(O,">",$ofile) || die;

print O "reset\n";

print O "set term png size 1920,1080\n";
print O "set output '$png1'\n";

print O "set grid\n";
print O "set autoscale\n";
print O "set xrange [0:65535]\n";
print O "set title 'Sound Blaster 16 output rate and Playback rate ($filenn)'\n";
print O "set xlabel 'Output rate'\n";
print O "set ylabel 'Sample rate (Hz)'\n";

print O "plot '$csv' using 1:2 with lines title 'SB16 rate value'\n";

close(O);

system("gnuplot '$ofile'");

