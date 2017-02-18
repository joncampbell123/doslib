#!/usr/bin/perl
#
# auto-export-dump.pl <path> <subpath> <version>

my $root_path = shift @ARGV;
my $sub_path = shift @ARGV;
my $version = shift @ARGV;
my $top = `pwd`; chomp $top;

$sub_path = "" if $sub_path eq ".";

print "Root path: $root_path\n";
print "Sub path: $sub_path\n";
print "Version: $version\n";
print "Top: $top\n";

die if !defined($root_path);
die if !defined($sub_path);
die if !defined($version);
die unless -d $root_path;
die unless -d "$root_path/$sub_path";

mkdir("new");

chdir("$root_path/$sub_path") || die;
open(SCAN,"find |") || die;
chdir($top) || die;

sub verfmt {
    my $s = shift @_;
    my $a = '',$i;
    my $sb;

    for ($i=0;$i < length($s);$i += 4) {
        $a .= '.' if $i != 0;
        $sb = substr($s,$i,4);
        $sb = hex($sb);
        $a .= $sb;
    }

    return $a;
}

while (my $path = <SCAN>) {
    chomp $path;
    $path =~ s/^\.\///;

    my $fullpath = "$root_path/$sub_path/$path";
    next unless -f $fullpath;

#    next unless $path =~ m/\.(exe|drv|dll|ocx|cpl|acm|qtc)$/i;
    next if $path =~ m/[\'\"\\\!\|\{\}\$]/;

    print "\x1B[m";
    print "$root_path/";
    print "\x1B[36m";
    print "$sub_path/";
    print "\x1B[32m";
    print "$path";
    print "\x1B[m";
    print "\n";

    open(EXE,"../../../hw/dos/linux-host/exenedmp -i \"$fullpath\" 2>/dev/null |") || die;

    my $dwFileVersion,$dwProductVersion;
    my $ProductName,$ProductVersion;
    my $ok = 0;

    while (my $line = <EXE>) {
        chomp $line;

        $line =~ s/^ +//;
        $line =~ s/ +$//;

        if ($line =~ m/^Windows or OS\/2 NE header/) {
            $ok = 1;
        }
        elsif ($line =~ s/^dwFileVersion: *//) {
            $dwFileVersion = $line;
            $dwFileVersion =~ s/(MS 0x|LS 0x)//g;
            $dwFileVersion =~ s/ //g;
            $dwFileVersion = verfmt($dwFileVersion);
        }
        elsif ($line =~ s/^dwProductVersion: *//) {
            $dwProductVersion = $line;
            $dwProductVersion =~ s/(MS 0x|LS 0x)//g;
            $dwProductVersion =~ s/ //g;
            $dwProductVersion = verfmt($dwProductVersion);
        }
        elsif ($line =~ m/^Block: *'ProductName'/) {
            $line = <EXE>;
            chomp $line;
            if ($line =~ m/^ *\".*\" *$/) {
                $line =~ s/^ +//;
                $line =~ s/^\"//;
                $line =~ s/ +$//;
                $line =~ s/\"$//;
                $ProductName = $line;
                $ProductName =~ s/\(TM\)//g;
                $ProductName =~ s/[\x00-\x1F\x7F-\xFF\$\!\\\{\}\(\)\/]+//g;
            }
        }
        elsif ($line =~ m/^Block: *'ProductVersion'/) {
            $line = <EXE>;
            chomp $line;
            if ($line =~ m/^ *\".*\" *$/) {
                $line =~ s/^ +//;
                $line =~ s/^\"//;
                $line =~ s/ +$//;
                $line =~ s/\"$//;
                $ProductVersion = $line;
                $ProductVersion =~ s/\(TM\)//g;
                $ProductVersion =~ s/[\x00-\x1F\x7F-\xFF\$\!\\\{\}\(\)\/]+//g;
            }
        }
    }
    close(EXE);

    if (!$ok) {
        print "Not an NE executable\n";
        next;
    }

    my $autosuf = '';
    my $crc32 = `crc32 \"$fullpath\"`; chomp $crc32;

    $autosuf .= "-$version" if $version ne '';

    $autosuf .= "-sz";
    $autosuf .= -s $fullpath;

    $autosuf .= "-crc";
    $autosuf .= $crc32;

    $ProductName =~ s/ +/-/g;
    $ProductName = substr($ProductName,0,96);

    $ProductVersion =~ s/ +/-/g;
    $ProductVersion = substr($ProductVersion,0,96);

    $autosuf .= "-fv".$dwFileVersion if $dwFileVersion ne '';
    $autosuf .= "-pv".$dwProductVersion if $dwProductVersion ne '';
    $autosuf .= "-pvs".$ProductVersion if $ProductVersion ne '';
    $autosuf .= "-".$ProductName if $ProductName ne '';

    my $fname = "$sub_path/$path";
    $fname =~ s/[\/\\]/-/g;
    $fname =~ s/^-+//;
    $fname .= $autosuf;
    $fname .= ".exports";

    print "  Exporting to new/$fname\n";

    $x = system("../../../hw/dos/linux-host/exeneexp -i \"$fullpath\" >\"new/$fname\"");
    die "Error ".sprintf("0x%x",$x) unless $x == 0;
}

close(SCAN);

