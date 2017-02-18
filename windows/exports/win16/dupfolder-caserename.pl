#!/usr/bin/perl

sub shellesc($) {
	my $x = shift @_;
	$x =~ s/([^a-zA-Z0-9\.\-])/\\$1/g;
	return $x;
}

open(X,"ls | sort -d |") || die;
@a = <X>;
close(X);

while (@a > 1) {
	my $cur = shift @a; chomp $cur;
	my $nxt = $a[0]; chomp $nxt;

	if (lc($cur) eq lc($nxt)) {
		# NTS: this favors the lowest capitalization
		$i = 2;
		my $nname = "$cur\[$i\]";

		while ( -d $nname ) {
			die if ++$i >= 1000;
			$nname = "$cur\[$i\]";
		}

		(system("mv -vn -- ".shellesc($nxt)." ".shellesc($nname)) == 0) || die;
	}
}

