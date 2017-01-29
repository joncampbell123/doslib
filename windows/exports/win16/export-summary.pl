#!/usr/bin/perl

use Data::Dumper;

my %modules;

open(LS,"find -name \*.exports |") || die;
while (my $path = <LS>) {
    chomp $path;

    my $module = '';
    my $modinfo = { };
    my @ordinals = ( );

    open(EX,"<","$path") || next;
    while (my $line = <EX>) {
        chomp $line;

        $line =~ s/^[ \t]+//;
        $line =~ s/[ \t]+$//;

        if ($line =~ s/^MODULE +//) {
            $module = $line;
        }
        elsif ($module ne "") {
            $i = index($line,'=');
            next if $i < 0;
            $name = uc(substr($line,0,$i));
            $value = substr($line,$i+1);

            if ($name eq 'DESCRIPTION') {
                $modinfo{$name} = $value;
            }
            elsif ($name eq 'FILE.NAME') {
                $modinfo{$name} = $value;
            }
            elsif ($name eq 'FILE.SIZE') {
                $modinfo{$name} = int($value);
            }
            elsif ($name =~ s/^ORDINAL\.//) {
                ($ordinal,$ordent) = split(/\./,$name);
                $ordinal = int($ordinal);

                while ((scalar @ordinals) <= $ordinal) {
                    push(@ordinals, { } );
                }

                $ordinals[$ordinal]{$ordent} = $value;
            }
        }
    }
    close(EX);

    if ($module ne "") {
        $modinfo{'filepath'} = $path;
        $modinfo{'ordinals'} = ( \@ordinals );

        if (!(exists $modules{$module})) {
            $modules{$module} = ( );
        }

        push(@{$modules{$module}}, { %modinfo } );
    }
}
close(LS);

sub refsort {
    # try to sort:
    #   named entries first
    #   empty second
    #   not defined third
    my %ra = %{$a};
    my %rb = %{$b};

    # not defined go last
    if (exists($ra{'NAME'}) && exists($rb{'NAME'})) {
        if ($ra{'NAME'} ne '' && $rb{'NAME'} ne '') {
            return $ra{'NAME'} cmp $rb{'NAME'};
        }
        elsif ($ra{'NAME'} ne '') {
            return -1;
        }
        elsif ($rb{'NAME'} ne '') {
            return 1;
        }
    }
    elsif (exists($ra{'NAME'})) {
        return -1;
    }
    elsif (exists($rb{'NAME'})) {
        return 1;
    }

    # try to match 'empty' type vs non-'empty'
    if (($ra{'TYPE'} eq "empty") && ($rb{'TYPE'} eq "empty")) {
        return 0;
    }
    elsif (($ra{'TYPE'} eq "empty")) {
        return -1;
    }
    elsif (($rb{'TYPE'} eq "empty")) {
        return 1;
    }

    return 0;
}

while (my ($module,$modlistr) = each(%modules)) {
    print "MODULE $module\n";

    my $max_ordinals = 0;
    my @modlist = @{$modlistr};
    for ($modi=0;$modi < @modlist;$modi++) {
        my $modinfop = $modlist[$modi];
        my %modinfo = %{$modinfop};

        print "    REFERENCE.$modi.filepath=".$modinfo{'filepath'}."\n" if exists $modinfo{'filepath'};
        print "    REFERENCE.$modi.FILE.NAME=".$modinfo{'FILE.NAME'}."\n" if exists $modinfo{'FILE.NAME'};
        print "    REFERENCE.$modi.FILE.SIZE=".$modinfo{'FILE.SIZE'}."\n" if exists $modinfo{'FILE.SIZE'};
        print "    REFERENCE.$modi.DESCRIPTION=".$modinfo{'DESCRIPTION'}."\n" if exists $modinfo{'DESCRIPTION'};
        if (exists $modinfo{'ordinals'}) {
            my @ordinals = @{$modinfo{'ordinals'}};
            print "    REFERENCE.$modi.ORDINALS=".@ordinals."\n";
            $max_ordinals = @ordinals if $max_ordinals < @ordinals;
        }
        print "\n";
    }

    my $last_refs = 0;
    for ($i=1;$i < $max_ordinals;$i++) {
        my @refs;

        for ($modi=0;$modi < @modlist;$modi++) {
            my $modinfop = $modlist[$modi];
            my %modinfo = %{$modinfop};

            next if (!(exists $modinfo{'ordinals'}));
            my @ordinals = @{$modinfo{'ordinals'}};

            # this is an opportunity to note when ordinals stop
            print "\n    ; ------ At this point, no more ordinals in ".$modinfo{'filepath'}."\n" if ((scalar @ordinals) == $i);

            my $ordref = $ordinals[$i];
            if (defined($ordref)) {
                my %ordent = %{$ordref};
                $ordent{modinfo} = ( );
                push(@{$ordent{modinfo}}, \%modinfo);
                push(@refs,\%ordent);
            }
        }

        # sort
        @refs = sort refsort @refs;

        # combine like items
        for ($j=0;($j+1) < @refs;) {
            my %ra = %{$refs[$j]};
            my %rb = %{$refs[$j+1]};

            my $raname = $ra{'NAME'};
            $raname = 'ZZZZUNDEFINEDxxxxxxx' unless defined($raname);
            my $rbname = $rb{'NAME'};
            $rbname = 'ZZZZUNDEFINEDxxxxxxx' unless defined($rbname);

            my $ratype = $ra{'TYPE'};
            $ratype = '' unless defined($ratype);
            my $rbtype = $rb{'TYPE'};
            $rbtype = '' unless defined($rbtype);
            $ratype =~ s/(nonresident|resident) +//;
            $rbtype =~ s/(nonresident|resident) +//;

            if ($raname eq $rbname && $ratype eq $rbtype) {
                # remove like item, combine $j and $j+1 modinfo
                my @f;

                for ($k=0;$k < @refs;$k++) {
                    if ($k == ($j+1)) {
                        my %ra = %{$refs[$j]}; # previous
                        my %rb = %{$refs[$k]}; # current (one to NOT copy)
                        push(@{$ra{modinfo}},@{$rb{modinfo}});
                    }
                    else {
                        push(@f,$refs[$k]);
                    }
                }

                @refs = @f;
            }
            else {
                $j++;
            }
        }

        # print it out
        my $refc = 0;
        for ($j=0;$j < @refs;$j++) {
            my %ref = %{$refs[$j]};

            next if !exists($ref{'NAME'}) && !exists($ref{'TYPE'});

            if (@refs > 1) {
                my @modinfo = @{$ref{modinfo}};
                for ($ri=0;$ri < @modinfo;$ri++) {
                    my %modinf = %{$modinfo[$ri]};

                    print "\n" if $refc == 0;
                    $refc++;

                    print "    ; Ordinal #".$i." in ";
                    print $modinf{'filepath'} if exists $modinf{'filepath'};
                    print "\n";
                }
            }
            else {
                print "\n" if $last_refs != 0;
            }

            print "    ORDINAL.$i.NAME=".$ref{'NAME'}."\n" if exists $ref{'NAME'};

            if (exists $ref{'TYPE'}) {
                if ($ref{'TYPE'} eq 'empty' || $ref{'TYPE'} =~ m/constant=/) {
                    print "    ORDINAL.$i.TYPE=".$ref{'TYPE'}."\n";
                }
            }
        }
        $last_refs = $refc;

        # remove modinfo
        for ($j=0;$j < @refs;$j++) {
            $refs[$j]{modinfo} = undef;
            delete $refs[$j]{modinfo};
        }
    }
}

