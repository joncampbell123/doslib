#!/usr/bin/perl
open(X,"find |") || die;
while (my $line = <X>) {
    my $name;
    chomp $line;
    $line =~ s/^\.\///g;
    next if $line =~ m/\.git/i;
    next if $line =~ m/\.(bat|xxx)$/i;
    next if $line =~ m/[ \t\\\"\']/;

    $c = `grep -c \$'\\t' $line`;
    chomp $c;
    next if $c eq "0";
    print "$line: $c tabs\n";
}
close(X);
