#!/usr/bin/perl
open(X,"find |") || die;
while (my $line = <X>) {
    my $name;
    chomp $line;
    $line =~ s/^\.\///g;
    next if $line =~ m/\.git/i;
    next unless $line =~ m/\.map$/; # map
    next if $line =~ m/[ \t\\\"\'\|]/;

    # NTS: we first grep to NOT match clibs.lib then grep for the error message.
    #      that way we don't count Watcom fpu87 emulation warnings.

    $c = `grep -v 'clibs.lib' $line | grep -c ': segment relocation at '`;
    chomp $c;
    next if $c eq "0";
    print "$line: $c errors\n";
}
close(X);
