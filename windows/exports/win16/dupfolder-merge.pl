#!/usr/bin/perl
my @a;

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

	# common errors:
	# X, the
	#   rename to X, The
	if ( $dir =~ m/, +the$/i ) {
		my $dirn = $dir;
		$dirn =~ s/, +the$/, The/;
		if ($dir ne $dirn) {
			print "the -> The: $dir -> $dirn\n";
			if (!rename($dir,$dirn)) {
				$i=2;
				while (!rename($dir,"$dirn\[$i\]")) {
					die if ++$i >= 1000;
				}
			}
			next;
		}
	}

	# The X
	#    rename to X, The
	if ( $dir =~ m/^the +/i ) {
		my $dirn = $dir;
		$dirn =~ s/^the +//i;
		$dirn .= ", The";
		if ($dir ne $dirn) {
			print "the -> The: $dir -> $dirn\n";
			if (!rename($dir,$dirn)) {
				$i=2;
				while (!rename($dir,"$dirn\[$i\]")) {
					die if ++$i >= 1000;
				}
			}
			next;
		}
	}

	# X X, The
	#   rename to X, The[N]
	if ( -d "$dir, The" ) {
		print "X -> X, The: $dir\n";
		$i=2;
		while (!rename($dir,"$dir, The\[$i\]")) {
			die if ++$i >= 1000;
		}
		next;
	}

	# common duplications:

	# X X[2]
	#   merge contents into X
	for ($i=2;$i < 9;$i++) {
		if ( -d "$dir\[$i\]" ) {
			print "Dup [$i]: $dir\n";
			(system("./dupfolder-mergecontents.pl --src ".shellesc("$dir\[$i\]")." --dest ".shellesc($dir)) == 0) || die;
		}
	}
}

system("./dupfolder-caserename.pl");

