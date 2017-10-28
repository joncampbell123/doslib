#!/usr/bin/perl
my $file = $ARGV[0];
die unless -f $file;

mkdir("gnuplot");

$filenn = $file;
$filenn =~ s/\.txt$//i;

open(I,"<",$file) || die;

my @sb1x;
my @sb2x;
my @ess;
my @ess16;

my $line;
my $ref = undef;
while ($line = <I>) {
    chomp $line;

    if ($line =~ m/^SB 1\.x single cycle/i) {
        $ref = "sb1";
    }
    elsif ($line =~ m/^SB 2\.x single cycle/i) {
        $ref = "sb2";
    }
    elsif ($line =~ m/^ESS688 single cycle.*8 bit/i) {
        $ref = "ess";
    }
    elsif ($line =~ m/^ESS688 single cycle.*16 bit/i) {
        $ref = "ess16";
    }
    #  - TC 0x06: expecting 4000Hz, 4000b/0.985s @ 4060.103Hz
    elsif ($line =~ s/^ *- TC *//) {
        my @a = split(/ +/,$line);

        $idx = shift(@a);
        next unless $idx =~ m/^0x[0-9a-f]+: */i;

        $idx = hex($idx);
        next unless $idx >= 0 && $idx <= 0xFF;

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

        if ($ref eq "sb1") {
            $sb1[$idx] = $rate;
        }
        elsif ($ref eq "sb2") {
            $sb2[$idx] = $rate;
        }
        elsif ($ref eq "ess") {
            $ess[$idx] = $rate;
        }
        elsif ($ref eq "ess16") {
            $ess16[$idx] = $rate;
        }
    }
    else {
        $ref = undef;
    }
}

close(I);





$png1 = "gnuplot/$file.sb1.png";
$png2 = "gnuplot/$file.sb2.png";
$csv = $ofile = "gnuplot/$file.sb.csv";
open(O,">",$ofile) || die;

print O "# tc, sb1";
print O ", sb2" if @sb2 > 0;
print O "\n";

for ($i=0;$i < 256;$i++) {
    next unless (defined($sb1[$i]) || defined($sb2[$i]));

    # tc
    print O "$i";

    # sb1
    print O ", ".$sb1[$i];

    # sb2
    print O ", ".$sb2[$i] if @sb2 > 0;

    print O "\n";
}

close(O);

$ofile = "gnuplot/$file.sb.gnuplot";
open(O,">",$ofile) || die;

print O "reset\n";

print O "set term png size 1920,1080\n";
print O "set output '$png1'\n";

print O "set grid\n";
print O "set autoscale\n";
print O "set xrange [0:255]\n";
print O "set title 'Sound Blaster Time Constant and Playback rate ($filenn)'\n";
print O "set xlabel 'Time constant byte'\n";
print O "set ylabel 'Sample rate (Hz)'\n";

print O "plot '$csv' using 1:2 with lines title 'SB 1.x non-highspeed TC'\n";

if (@sb2 > 0) {

print O "reset\n";

print O "set term png size 1920,1080\n";
print O "set output '$png2'\n";

print O "set grid\n";
print O "set autoscale\n";
print O "set xrange [0:255]\n";
print O "set title 'Sound Blaster Time Constant and Playback rate ($filenn)'\n";
print O "set xlabel 'Time constant byte'\n";
print O "set ylabel 'Sample rate (Hz)'\n";

print O "plot '$csv' using 1:3 with lines title 'SB 2.x highspeed TC'\n";

}

close(O);

system("gnuplot '$ofile'");



if (@ess > 0) {
    $png1 = "gnuplot/$file.ess.png";
    $csv = $ofile = "gnuplot/$file.ess.csv";
    open(O,">",$ofile) || die;

    print O "# tc, ess";
    print O "\n";

    for ($i=0;$i < 256;$i++) {
        next unless defined($ess[$i]);

        # tc
        print O "$i";

        # ess
        print O ", ".$ess[$i];

        print O "\n";
    }

    close(O);

    $ofile = "gnuplot/$file.ess.gnuplot";
    open(O,">",$ofile) || die;

    print O "reset\n";

    print O "set term png size 1920,1080\n";
    print O "set output '$png1'\n";

    print O "set grid\n";
    print O "set autoscale\n";
    print O "set xrange [0:255]\n";
    print O "set title 'ESS688 sample rate divider and Playback rate 8-bit ($filenn)'\n";
    print O "set xlabel 'Sample rate divider byte'\n";
    print O "set ylabel 'Sample rate (Hz)'\n";

    print O "plot '$csv' using 1:2 with lines title 'Sample rate divider byte'\n";

    close(O);

    system("gnuplot '$ofile'");
}

if (@ess16 > 0) {
    $png1 = "gnuplot/$file.ess16.png";
    $csv = $ofile = "gnuplot/$file.ess16.csv";
    open(O,">",$ofile) || die;

    print O "# tc, ess16";
    print O "\n";

    for ($i=0;$i < 256;$i++) {
        next unless defined($ess16[$i]);

        # tc
        print O "$i";

        # ess
        print O ", ".$ess16[$i];

        print O "\n";
    }

    close(O);

    $ofile = "gnuplot/$file.ess16.gnuplot";
    open(O,">",$ofile) || die;

    print O "reset\n";

    print O "set term png size 1920,1080\n";
    print O "set output '$png1'\n";

    print O "set grid\n";
    print O "set autoscale\n";
    print O "set xrange [0:255]\n";
    print O "set title 'ESS688 sample rate divider and Playback rate 16-bit ($filenn)'\n";
    print O "set xlabel 'Sample rate divider byte'\n";
    print O "set ylabel 'Sample rate (Hz)'\n";

    print O "plot '$csv' using 1:2 with lines title 'Sample rate divider byte'\n";

    close(O);

    system("gnuplot '$ofile'");
}

