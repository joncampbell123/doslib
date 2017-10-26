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

    if ($line =~ m/^SB 1\.x DMA single cycle DSP/i ||
        $line =~ m/^SB 2\.x DMA single cycle DSP/i ||
        $line =~ m/^SB 16 DMA single cycle DSP/i ||
        $line =~ m/^ESS688 DMA single cycle DSP/i) {
        $test = $line;
        $test =~ s/\.$//g;
        $subtest = undef;
    }
    elsif ($line =~ s/^ - Test at +//i) {
        $subtest = $line;

        $name = "$filenn - $test, $subtest";

        print "Processing $name\n";

        die "bad name $name" if $name =~ m/[^0-9a-z \.\-\,\(\)]/i;

        $csv = "gnuplot/$name.csv";
        $gnuplot = "gnuplot/$name.gnuplot";
        $png = "gnuplot/$name.png";
        $pngc = "gnuplot/$name.start.png";

        open(O,">",$csv) || die;
        print O "# pos, time, irq\n";

        $min_pos = 0;
        $max_pos = 1;

        $min_time = 0;
        $max_time = 0.01;

        $min_irq = 0;
        $max_irq = 1;

        while ($line = <I>) {
            chomp $line;
            $line =~ s/[\x0D\x0A]//g;

            last if $line eq "";

            if ($line =~ s/^ +\>\> +POS +//i) {
                my @a = split(/ +/,$line);

                $pos = $a[0] + 0;
                $time = $a[2] + 0.0;
                $irq = $a[4] + 0;

                $max_pos  = $pos  if $max_pos  < $pos;
                $max_irq  = $irq  if $max_irq  < $irq;
                $max_time = $time if $max_time < $time;

                print O "$pos, $time, $irq\n";
            }
        }

        # expand out a bit, to show the graph in full
        $max_irq  += $max_irq  / 4;
        $max_pos  += $max_pos  / 4;
        $max_time += $max_time / 10;

        close(O);

        # generate gnuplot file
        open(O,">",$gnuplot) || die;

        print O "reset\n";

        print O "set term png size 1920,1080\n";
        print O "set output '$png'\n";

        print O "set multiplot\n";

        print O "set grid\n";

        print O "set size 1.0,0.85\n";
        print O "set origin 0.0,0.0\n";
        print O "set title '$name vs DMA'\n";
        print O "set xrange [$min_time:$max_time]\n";
        print O "set yrange [$min_pos:$max_pos]\n";
        print O "set xlabel 'Time (s)'\n";
        print O "set ylabel 'DMA transfer count (bytes)'\n";
        print O "plot '$csv' using 2:1 with steps title 'DMA transfer count over time'\n";

        print O "set size 1.0,0.15\n";
        print O "set origin 0.0,0.85\n";
        print O "set title '$name vs IRQ'\n";
        print O "set xrange [$min_time:$max_time]\n";
        print O "set yrange [$min_irq:$max_irq]\n";
        print O "set xlabel 'Time (s)'\n";
        print O "set ylabel 'IRQ count'\n";
        print O "plot '$csv' using 2:3 with steps title 'IRQ count over time'\n";

        print O "unset multiplot\n";

        # the initial burst when loading the FIFO is too subtle for the full graph
        print O "reset\n";

        print O "set term png size 1920,1080\n";
        print O "set output '$pngc'\n";

        print O "set grid\n";
        print O "set autoscale\n";
        print O "set title '$name'\n";
        print O "set xrange [0:0.0025]\n";
        print O "set xlabel 'Time (s)'\n";
        print O "set ylabel 'DMA transfer count (bytes)'\n";

        print O "plot '$csv' using 2:1 with steps title 'DMA transfer count over time'\n";
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

