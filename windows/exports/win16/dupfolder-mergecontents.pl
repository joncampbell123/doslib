#!/usr/bin/perl
#
# dupfolder-mergecontents.pl --src <src> --dest <dst>
my $src,$dst;

sub shellesc($) {
	my $x = shift @_;
	$x =~ s/([^a-zA-Z0-9\.\-])/\\$1/g;
	return $x;
}

for ($i=0;$i < @ARGV;) {
	my $a = $ARGV[$i++];

	if ( $a =~ s/^-+// ) {
		if ( $a eq "src" ) {
			$src = $ARGV[$i++];
		}
		elsif ( $a eq "dest" ) {
			$dst = $ARGV[$i++];
		}
		else {
			print "Unknown arg $a\n";
			die;
		}
	}
	else {
		die;
	}
}

die if $src eq "";
die unless -d $src;
die if -l $src;

die if $dst eq "";
die unless -d $dst;
die if -l $dst;

print "Merging $src -> $dst\n";

opendir(X,$src) || die;
my @a = grep { $_ ne "." && $_ ne ".." && ( -d "$src/$_" || -f "$src/$_" ) } readdir(X);
closedir(X);

foreach my $name (@a) {
	die unless -e "$src/$name";

	# NOTICE: we do not use rename() here. in the Linux/POSIX environment, renaming over the target file if it exists
	#         WILL OBLITERATE THE FILE, and thus lose data. It's better if we use the system "mv" command with -v and
	#         -n to ensure that -v shows the user what we did, and -n instructs mv not to clobber the destination if
	#         it exists.
	#
	#         FIXME: Shelling out like this: will it work properly with UTF-8 filenames containing foreign chars?

	# does a file of the same name exist in the target? if not, then we can just move it in there without problems
	if ( !( -e "$dst/$name" ) ) {
		(system("mv -vn -- ".shellesc("$src/$name")." ".shellesc("$dst/$name")) == 0) || die;
		next;
	}

	# alright then, we'll just have to rename it
	$i=2;
	if ( -f "$src/$name" ) {
		$j=rindex($name,".");
		$j=length($name) if $j < 0;
		$ext=substr($name,$j);
		$base=substr($name,0,$j);
		while ( -e "$dst/$base\[$i\]$ext" ) {
			die if ++$i >= 1000;
		}

		print "Dup $src/$name\n";
		(system("mv -vn -- ".shellesc("$src/$name")." ".shellesc("$dst/$base\[$i\]$ext")) == 0) || die;
	}
	else {
		while ( -e "$dst/$name\[$i\]" ) {
			die if ++$i >= 1000;
		}

		print "Dup-dir $src/$name\n";
		(system("mv -vn -- ".shellesc("$src/$name")." ".shellesc("$dst/$name\[$i\]")) == 0) || die;
	}
}

rmdir($src);

