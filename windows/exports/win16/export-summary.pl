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
            $module = uc($line);
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
                $value = uc($value);

                $value =~ s/\.3G_$/.3GR/;
                $value =~ s/\.AC_$/.ACM/;
                $value =~ s/\.CP_$/.CPL/;
                $value =~ s/\.DL_$/.DLL/;
                $value =~ s/\.DR_$/.DRV/;
                $value =~ s/\.EX_$/.EXE/;
                $value =~ s/\.FO_$/.FON/;
                $value =~ s/\.MM_$/.MMH/;
                $value =~ s/\.MO_$/.MOD/;
                $value =~ s/\.OC_$/.OCX/;
                $value =~ s/\.SC_$/.SCR/;
                $value =~ s/\.TS_$/.TSK/;
                $value =~ s/\.VB_$/.VBX/;
                $value =~ s/\.WN_$/.WND/;

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

sub text2html {
    my $s = shift @_;
    my $r = '';
    my $i,$c;

    for ($i=0;$i < length($s);$i++) {
        $c = substr($s,$i,1);

        if ($c eq '<') {
            $r .= "&lt;";
        }
        elsif ($c eq '>') {
            $r .= "&gt;";
        }
        elsif ($c eq '&') {
            $r .= "&amp;";
        }
        else {
            $r .= $c;
        }
    }

    return $r;
}

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
            my $res = ($ra{'NAME'} cmp $rb{'NAME'});
            if ($res == 0) {
                # then sort by constant= field in type
                my $rat = $ra{'TYPE'};
                $rat = '' unless defined($rat);
                my $rbt = $rb{'TYPE'};
                $rbt = '' unless defined($rbt);
                $rat =~ s/(nonresident|resident) +//;
                $rbt =~ s/(nonresident|resident) +//;
                return ($rat cmp $rbt);
            }
            return $res;
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

my %mod_official;

my @modorder = ( );
while (my ($module,$modlistr) = each(%modules)) {
    push(@modorder,$module);
}
@modorder = sort @modorder;

#while (my ($module,$modlistr) = each(%modules)) {
for ($lmodi=0;$lmodi < @modorder;$lmodi++) {
    $module = $modorder[$lmodi];
    $modlistr = $modules{$module};

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
    my $last_nomoreord = 0;
    for ($i=1;$i < $max_ordinals;$i++) {
        my @refs;

        for ($modi=0;$modi < @modlist;$modi++) {
            my $modinfop = $modlist[$modi];
            my %modinfo = %{$modinfop};

            next if (!(exists $modinfo{'ordinals'}));
            my @ordinals = @{$modinfo{'ordinals'}};

            # this is an opportunity to note when ordinals stop
            if ((scalar @ordinals) == $i) {
                print "\n    ; ------ At this point, no more ordinals in ".$modinfo{'filepath'}."\n";
                $last_nomoreord = 1;
            }

            my $ordref = $ordinals[$i];
            if (defined($ordref)) {
                my %ordent = %{$ordref};
                $ordent{modinfo} = ( );
                push(@{$ordent{modinfo}}, \%modinfo);
                push(@refs,\%ordent);
            }
        }

        # announce what is left after no more ordinals
        if ($last_nomoreord) {
            $last_nomoreord = 0;

            print "    ; Modules still to go:\n";
            for ($modi=0;$modi < @modlist;$modi++) {
                my $modinfop = $modlist[$modi];
                my %modinfo = %{$modinfop};

                next if (!(exists $modinfo{'ordinals'}));
                my @ordinals = @{$modinfo{'ordinals'}};

                my $ordref = $ordinals[$i];
                if (defined($ordref)) {
                    print "    ;    ".$modinfo{'filepath'}."\n";
                }
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

        # for the decompiler, come up with the "official" name of the symbol.
        # This code considers Windows 3.1 authoritative on what the names of the symbols are.
        # Older versions have fewer symbols, though the names hardly change.
        # Windows 95 and later added some symbols, and took away the names (leaving the ordinals)
        # of some deprecated APIs.
        $ofname = undef;
        $oftype = undef;

        # Windows 3.1x
        if ((!defined($ofname) && !defined($oftype)) || $ofname eq "" || $ofname eq "__GP" || $ofname eq "___EXPORTEDSTUB" || $ofname eq "WEP" || $oftype eq "empty") {
            for ($j=0;$j < @refs;$j++) {
                my %ref = %{$refs[$j]};

                next if !exists($ref{'NAME'}) && !exists($ref{'TYPE'});

                my $match = 0;

                my @modinfo = @{$ref{modinfo}};
                for ($ri=0;$ri < @modinfo;$ri++) {
                    my %modinf = %{$modinfo[$ri]};

                    if ($modinf{'filepath'} =~ m/-windows-3\.1/i) {
                        $match = 1;
                        last;
                    }
                }

                if ($match) {
                    if ($ref{'TYPE'} ne 'empty') {
                        if (defined $ref{'NAME'} && $ref{'NAME'} ne '') {
                            $ofname = $ref{'NAME'};
                            $oftype = $ref{'TYPE'};
                        }
                    }
                }
            }
        }

        # Windows 3.0x
        if ((!defined($ofname) && !defined($oftype)) || $ofname eq "" || $ofname eq "__GP" || $ofname eq "___EXPORTEDSTUB" || $ofname eq "WEP" || $oftype eq "empty") {
            for ($j=0;$j < @refs;$j++) {
                my %ref = %{$refs[$j]};

                next if !exists($ref{'NAME'}) && !exists($ref{'TYPE'});

                my $match = 0;

                my @modinfo = @{$ref{modinfo}};
                for ($ri=0;$ri < @modinfo;$ri++) {
                    my %modinf = %{$modinfo[$ri]};

                    if ($modinf{'filepath'} =~ m/-windows-3\.0/i) {
                        $match = 1;
                        last;
                    }
                }

                if ($match) {
                    if ($ref{'TYPE'} ne 'empty') {
                        if (defined $ref{'NAME'} && $ref{'NAME'} ne '') {
                            $ofname = $ref{'NAME'};
                            $oftype = $ref{'TYPE'};
                        }
                    }
                }
            }
        }

        # Windows 9x/ME
        if ((!defined($ofname) && !defined($oftype)) || $ofname eq "" || $ofname eq "__GP" || $ofname eq "___EXPORTEDSTUB" || $ofname eq "WEP" || $oftype eq "empty") {
            for ($j=0;$j < @refs;$j++) {
                my %ref = %{$refs[$j]};

                next if !exists($ref{'NAME'}) && !exists($ref{'TYPE'});

                my $match = 0;

                my @modinfo = @{$ref{modinfo}};
                for ($ri=0;$ri < @modinfo;$ri++) {
                    my %modinf = %{$modinfo[$ri]};

                    if ($modinf{'filepath'} =~ m/-windows-(9|ME)/i) {
                        $match = 1;
                        last;
                    }
                }

                if ($match) {
                    if ($ref{'TYPE'} ne 'empty') {
                        if (defined $ref{'NAME'} && $ref{'NAME'} ne '') {
                            $ofname = $ref{'NAME'};
                            $oftype = $ref{'TYPE'};
                        }
                    }
                }
            }
        }

        # Windows 2x
        if ((!defined($ofname) && !defined($oftype)) || $ofname eq "" || $ofname eq "__GP" || $ofname eq "___EXPORTEDSTUB" || $ofname eq "WEP" || $oftype eq "empty") {
            for ($j=0;$j < @refs;$j++) {
                my %ref = %{$refs[$j]};

                next if !exists($ref{'NAME'}) && !exists($ref{'TYPE'});

                my $match = 0;

                my @modinfo = @{$ref{modinfo}};
                for ($ri=0;$ri < @modinfo;$ri++) {
                    my %modinf = %{$modinfo[$ri]};

                    if ($modinf{'filepath'} =~ m/-windows-2\./i) {
                        $match = 1;
                        last;
                    }
                }

                if ($match) {
                    if ($ref{'TYPE'} ne 'empty') {
                        if (defined $ref{'NAME'} && $ref{'NAME'} ne '') {
                            $ofname = $ref{'NAME'};
                            $oftype = $ref{'TYPE'};
                        }
                    }
                }
            }
        }

        # Windows 1x
        if ((!defined($ofname) && !defined($oftype)) || $ofname eq "" || $ofname eq "__GP" || $ofname eq "___EXPORTEDSTUB" || $ofname eq "WEP" || $oftype eq "empty") {
            for ($j=0;$j < @refs;$j++) {
                my %ref = %{$refs[$j]};

                next if !exists($ref{'NAME'}) && !exists($ref{'TYPE'});

                my $match = 0;

                my @modinfo = @{$ref{modinfo}};
                for ($ri=0;$ri < @modinfo;$ri++) {
                    my %modinf = %{$modinfo[$ri]};

                    if ($modinf{'filepath'} =~ m/-windows-1\./i) {
                        $match = 1;
                        last;
                    }
                }

                if ($match) {
                    if ($ref{'TYPE'} ne 'empty') {
                        if (defined $ref{'NAME'} && $ref{'NAME'} ne '') {
                            $ofname = $ref{'NAME'};
                            $oftype = $ref{'TYPE'};
                        }
                    }
                }
            }
        }

        {
            if (!(exists $mod_official{$module})) {
                $mod_official{$module} = ( );
            }

            $ofname = '' unless defined($ofname);
            $oftype = '' unless defined($oftype);

            while ((scalar @{$mod_official{$module}}) <= $i) {
                push(@{$mod_official{$module}},{ });
            }

            $mod_official{$module}[$i] = {
                'NAME' => $ofname,
                'TYPE' => $oftype
            };
        }

        # remove modinfo
        for ($j=0;$j < @refs;$j++) {
            $refs[$j]{modinfo} = undef;
            delete $refs[$j]{modinfo};
        }
    }
}

# final summary, for decompiler
open(OSUM,">summary.exportsymbols") || die;

#while (my ($module,$modlistr) = each(%mod_official)) {
for ($lmodi=0;$lmodi < @modorder;$lmodi++) {
    $module = $modorder[$lmodi];
    $modlistr = $mod_official{$module};

    print OSUM "MODULE $module\n";

    my @modlist = @{$modlistr};
    for ($modi=0;$modi < @modlist;$modi++) {
        my $modinfop = $modlist[$modi];
        my %modinfo = %{$modinfop};

        if ($modinfo{'NAME'} ne '') {
            print OSUM "    ORDINAL.$modi.NAME=".$modinfo{'NAME'}."\n";
        }
        elsif ($modinfo{'TYPE'} ne '') {
            print OSUM "    ORDINAL.$modi.TYPE=".$modinfo{'TYPE'}."\n";
        }
    }
}

close(OSUM);

open(IBMML,">summary.modules.ibmpc") || die;

#while (my ($module,$modlistr) = each(%modules)) {
for ($lmodi=0;$lmodi < @modorder;$lmodi++) {
    $module = $modorder[$lmodi];
    $modlistr = $modules{$module};

    my $max_ordinals = 0;
    my @modlist = @{$modlistr};

    my @ls = ( );
    for ($modi=0;$modi < @modlist;$modi++) {
        my $modinfop = $modlist[$modi];
        my %modinfo = %{$modinfop};
        my @ent;

        push(@ent,uc($module));
        push(@ent,uc($modinfo{'FILE.NAME'}));
        push(@ent,$modinfo{'DESCRIPTION'});

        push(@ls,[@ent]);
    }

    @ls = sort {
        my @sa = @{$a}, @sb = @{$b};
        my $x;

        $x = $sa[0] cmp $sb[0];
        return $x if $x != 0;

        $x = $sa[1] cmp $sb[1];
        return $x if $x != 0;

        $x = $sa[2] cmp $sb[2];
        return $x if $x != 0;

        return 0;
    } @ls;

    # unique
    my @r = ( );
    my $p0 = '',$p1 = '',$p2 = '';
    for ($i=0;$i < @ls;$i++) {
        my @ent = @{$ls[$i]};

        if ($ent[0] ne $p0 || $ent[1] ne $p1 || $ent[2] ne $p2) {
            push(@r,[@ent]);
        }

        $p0 = $ent[0];
        $p1 = $ent[1];
        $p2 = $ent[2];
    }
    @ls = @r;

    # print
    for ($i=0;$i < @ls;$i++) {
        my @ent = @{$ls[$i]};
        my $lin = '';

        # Module: 8 chars
        $lin .= text2html(substr($ent[0].(' ' x 8),0,8));

        # col
        $lin .= ' ';

        # File: 12 chars
        $lin .= text2html(substr($ent[1].(' ' x 11),0,12));

        # col
        $lin .= '  ';

        # description
        $lin .= text2html($ent[2]);

        # done
        print IBMML "$lin\n";
    }
}
close(IBMML);

