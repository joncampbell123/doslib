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
        $line =~ m/^SB 1\.x DMA ADPCM/i ||
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

        die "bad name $name" if $name =~ m/[^0-9a-zA-Z \.\-\,\!\(\)_]/i;

        $csv = "gnuplot/$name.csv";
        $gnuplot = "gnuplot/$name.gnuplot";
        $png = "gnuplot/$name.png";
        $pngc = "gnuplot/$name.start.png";

        open(O,">",$csv) || die;
        print O "# pos, time\n";

        $min_pos = 0;
        $max_pos = 1;

        $min_time = 0;
        $max_time = 0.01;

        $p_pos = undef;
        $p_time = undef;

        $f_pos = 1;
        $f_time = 0.001;

        while ($line = <I>) {
            chomp $line;
            $line =~ s/[\x0D\x0A]//g;

            last if $line eq "";

            if ($line =~ s/^ +\>\> +POS +//i) {
                my @a = split(/ +/,$line);

                $p_pos = $pos;
                $pos = $a[0] + 0;

                $p_time = $time;
                $time = $a[2] + 0.0;

                if (defined($p_time) && $time > ($p_time + 0.1) && $p_pos == $pos) {
                }
                else {
                    $max_pos  = $pos  if $max_pos  < $pos;
                    $max_time = $time if $max_time < $time;
                }

                if ($f_pos == 1) {
                    if ($time > 0.002 && $pos > 8) {
                        $f_time = $time;
                        $f_pos = $pos;
                    }
                }

                print O "$pos, $time\n";
            }
        }

        # keep original
        $o_max_pos  = $max_pos;
        $o_max_time = $max_time;

        # expand out a bit, to show the graph in full
        $max_pos  += $max_pos  / 4;
        $max_time += $max_time / 100;

        close(O);

        # generate gnuplot file
        open(O,">",$gnuplot) || die;

        print O "reset\n";

        print O "set term png size 1920,1080\n";
        print O "set output '$png'\n";

        print O "set grid\n";

        print O "set title '$name vs DMA'\n";
        print O "set xrange [$min_time:$max_time]\n";
        print O "set yrange [$min_pos:$max_pos]\n";
        print O "set xlabel 'Time (s)'\n";
        print O "set ylabel 'DMA transfer count (bytes)'\n";
        print O "plot '$csv' using 2:1 with steps title 'DMA transfer count over time'\n";

        # the initial burst when loading the FIFO, and ending part, is too subtle for the full graph
        print O "reset\n";

        $time1 = 0;
        $time2 = $f_time + ($f_time / 4);
        $pos1  = 0;
        $pos2  = $f_pos + ($f_pos / 4);

        $time3 = $o_max_time - 0.0025;
        $time4 = $o_max_time + 0.0025;
        $pos3  = $o_max_pos - 144;
        $pos4  = $o_max_pos + 144;

        print O "set term png size 1920,1080\n";
        print O "set output '$pngc'\n";

        print O "set multiplot\n";

        print O "set grid\n";

        print O "set size 1.0,0.5\n";
        print O "set origin 0.0,0.0\n";
        print O "set title '$name vs DMA'\n";
        print O "set xrange [$time1:$time2]\n";
        print O "set yrange [$pos1:$pos2]\n";
        print O "set xlabel 'Time (s)'\n";
        print O "set ylabel 'DMA transfer count (bytes)'\n";
        print O "plot '$csv' using 2:1 with steps title 'DMA transfer count over time'\n";

        print O "set size 1.0,0.5\n";
        print O "set origin 0.0,0.5\n";
        print O "set title '$name vs DMA'\n";
        print O "set xrange [$time3:$time4]\n";
        print O "set yrange [$pos3:$pos4]\n";
        print O "set xlabel 'Time (s)'\n";
        print O "set ylabel 'DMA transfer count (bytes)'\n";
        print O "plot '$csv' using 2:1 with steps title 'DMA transfer count over time'\n";

        print O "unset multiplot\n";

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

