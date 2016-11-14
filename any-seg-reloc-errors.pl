#!/usr/bin/perl
open(X,"find |") || die;
while (my $line = <X>) {
    my $name;
    chomp $line;
    $line =~ s/^\.\///g;
    next if $line =~ m/\.git/i;
    next unless $line =~ m/\.map$/; # map
    next if $line =~ m/[ \t\\\"\']/;

    $c = `grep -c ': segment relocation at ' $line`;
    chomp $c;
    next if $c eq "0";
    print "$line: $c errors\n";
}
close(X);
