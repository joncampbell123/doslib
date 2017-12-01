#!/usr/bin/perl
# process a C/C++ file to convert comments from SHIFT-JIS to UTF-8.
# running a whole file through is not advised because the conversion
# will break standard C++ syntax:
#
#   - \ will become Yen
#   - ~ will become an upper bar

# WARNING: This script assumes your Perl interpreter allows strings to be handled byte-wise (no charset encoding enforcement)

# fixme: proper tempfile processing!
my $tmp1 = ".iconv.tmp1";
my $tmp2 = ".iconv.tmp2";

my $procme,$procres;

while ($line = <STDIN>) {
    chomp $line;

    if ($line =~ s/(\/\/.*)$//) { # C++ comment
        $procme = $1;

        open(TI,">",$tmp1);
        print TI $procme;
        close(TI);

        system("iconv -f CP932 -t UTF-8 '$tmp1' >'$tmp2'") == 0 || die;

        open(TO,"<",$tmp2);
        read(TO,$procres,65536);
        close(TO);

        $line .= $procres;
    }
    # TODO: Plain C /* */

    print "$line\n";
}
