#!/usr/bin/perl
#
# For a given .exe file, change extension to .rex
# if the file is in fact a .rex file. This is easy
# to determine: the first two chars are "MQ" not "MZ".
#
# (C) 2012 Jonathan Campbell
my $file = shift @ARGV;
die unless -f $file;
my $dfile = $file;
$dfile =~ s/\.exe$/.rex/ || die;

my $buf;
open(X,"<",$file) || die;
binmode(X);
seek(X,0,0);
read(X,$buf,2);
close(X);

if ($buf eq "MQ") {
    # it's a .rex file, so it must be named so
    # so wbind can find it
    rename($file,$dfile) || die;
}

exit 0;

