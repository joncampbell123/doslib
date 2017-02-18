#!/usr/bin/perl
my @a;
my $dest;

for ($i=0;$i < @ARGV;) {
	my $a = $ARGV[$i++];

	if ($a =~ s/^-+//) {
		if ($a eq "dest") {
			$dest = $ARGV[$i++];
		}
	}
	else {
		die;
	}
}

die if $dest eq "";
die unless -d $dest;

sub shellesc($) {
	my $x = shift @_;
	$x =~ s/([^a-zA-Z0-9\.\-])/\\$1/g;
	return $x;
}

opendir(X,".") || die;
@a = grep { -d $_ } readdir(X);
closedir(X);

foreach my $dir (@a) {
	next if $dir eq "." || $dir eq "..";
	next if $dir =~ m/\[\d+\]$/;
	die if -f $dir;

	if ( -d "$dest/$dir" ) {
		$i = 2;
		while ( -d ($ddir = "$dest/$dir\[$i\]") ) {
			die if ++$i >= 1000;
		}

		(system("mv -vn -- ".shellesc($dir)." ".shellesc($ddir)) == 0) || die;
	}
	else {
		(system("mv -vn -- ".shellesc($dir)." ".shellesc("$dest/$dir")) == 0) || die;
	}
}

