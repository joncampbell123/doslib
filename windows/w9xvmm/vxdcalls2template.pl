#!/usr/bin/perl

while (my $line = <STDIN>) {
    chomp $line;

    my @a = split(/[ \t]+/,$line);

# 3.0+     0001H 002BH  Suspend_VM
# [0]      [1]   [2]    [3]

    next if @a < 4;

    print "#--------------------------------------\n";
    print "%defcall\n";
    print "byname          ".$a[3]."\n";
    print "description     \n";
    print "%enddef\n";

    print "\n";
}

