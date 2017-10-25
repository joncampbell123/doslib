#!/usr/bin/perl
my $file = $ARGV[0];
die unless -f $file;

mkdir("gnuplot");

$filenn = $file;
$filenn =~ s/\.txt$//i;

open(I,"<",$file) || die;

my $line;
my $test = undef;
my $subtest = undef;
while ($line = <I>) {
    chomp $line;
    $line =~ s/[\x0D\x0A]//g;

    if ($line =~ m/^SB 1\.x DMA single cycle DSP/i) {
        $test = $line;
        $test =~ s/\.$//g;
        $subtest = undef;
    }
    elsif ($line =~ s/^ - Test at +//i) {
        $subtest = $line;

        $name = "$filenn - $test, $subtest";

        print "Processing $name\n";

        die if $name =~ m/[^0-9a-z \.\-\,]/i;

        $csv = "gnuplot/$name.csv";
        $gnuplot = "gnuplot/$name.gnuplot";
        $png = "gnuplot/$name.png";
        $pngc = "gnuplot/$name.start.png";

        open(O,">",$csv) || die;
        print O "# pos, time\n";

        while ($line = <I>) {
            chomp $line;
            $line =~ s/[\x0D\x0A]//g;

            last if $line eq "";

            if ($line =~ s/^ +\>\> +POS +//i) {
                my @a = split(/ +/,$line);

                $pos = $a[0] + 0;
                $time = $a[2] + 0.0;

                print O "$pos, $time\n";
            }
        }

        close(O);

        # generate gnuplot file
        open(O,">",$gnuplot) || die;

        print O "reset\n";

        print O "set term png size 1920,1080\n";
        print O "set output '$png'\n";

        print O "set grid\n";
        print O "set autoscale\n";
        print O "set title '$name'\n";
        print O "set xlabel 'DMA transfer count (bytes)'\n";
        print O "set ylabel 'Time (s)'\n";

        print O "plot '$csv' using 1:2 with lines title 'DMA transfer count over time'\n";

        # the initial burst when loading the FIFO is too subtle for the full graph
        print O "reset\n";

        print O "set term png size 1920,1080\n";
        print O "set output '$pngc'\n";

        print O "set grid\n";
        print O "set autoscale\n";
        print O "set title '$name'\n";
        print O "set xrange [0:288]\n";
        print O "set xlabel 'DMA transfer count (bytes)'\n";
        print O "set ylabel 'Time (s)'\n";

        print O "plot '$csv' using 1:2 with lines title 'DMA transfer count over time'\n";
        # done

        # render
        system("gnuplot '$gnuplot'");
    }
    elsif ($line eq "") {
    }
    else {
        $test = undef;
        $subtest = undef;
    }
}

close(I);

