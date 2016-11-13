#!/usr/bin/perl
open(X,"find |") || die;
while (my $line = <X>) {
    my $name;
    chomp $line;
    $line =~ s/^\.\///g;
    next if $line =~ m/\.git/i;
    next unless $line =~ m/\.(c|cpp|h|mak|bat)$/i;
    $i = rindex($line,'/');
    $i = -1 if $i < 0;
    $name = substr($line,$i+1);
    $i = rindex($name,'.');
    next if $i < 0;
    $ext = substr($name,$i+1);
    $name = substr($name,0,$i);
    next if length($ext) <= 3 && length($name) <= 8;
    print "$line\n";
}
close(X);
