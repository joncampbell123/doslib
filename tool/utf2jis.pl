#!/usr/bin/perl
# Run GNU iconv to convert UTF-8 to Shift-JIS.
# Then post-process output so Shift-JIS is hex-escaped for use in C source code regardless of host charset.
# The output is still UTF-8. The original text is inserted as a comment one line below the converted text.
# This code filters the input to only process strings (anything between quotation marks), not the remaining source.
# As long as you're not doing fancy C/C++ #define macros with strings you should be OK.
# 
# no command line arguments are needed to run this script.
# just redirect our STDIN to the file to convert and this script will emit converted output to STDOUT.
#
# TODO: this should be extended to allow UTF-8 to ANY code page conversion!
my $quot = "";
my $comment = "";
my $procsum = "";
my $line;
my $out;
my $i,$j;
my $c;

# fixme: proper tempfile processing!
my $tmp1 = ".iconv.tmp1";
my $tmp2 = ".iconv.tmp2";

# make sure it's known that this is the converted copy!
# print "\xEF\xBB\xBF"; # UTF-8 BOM     FIXME: Watcom C does NOT like the UTF-8 BOM!!
print "/* UTF-8 to SHIFT-JIS string converted copy. Do not edit. Modifications will be lost when converted again. */\n";
print "\n";

while ($line = <STDIN>) {
	chomp $line;

	# eat UTF-8 BOM
	$line =~ s/^\xEF\xBB\xBF//;

	$i = 0;
	$out = "";
	while ($i < length($line)) {
		if ($quot ne "") {
			my $proc = "",$procret = "";

			while ($i < length($line)) {
				$c = substr($line,$i,1);
				$proc .= $c;
				$i++;

				if ($c eq $quot) {
					$quot = "";
					last;
				}
			}

			# run the string through iconv, hex escape, then carry on
			open(TI,">",$tmp1) || die;
			print TI $proc;
			close(TI);

			# Use GNU iconv to do charset conversion
			system("iconv -f UTF-8 -t SHIFT-JIS -o '$tmp2' '$tmp1'") == 0 || die;

			open(TO,"<",$tmp2) || die;
			read(TO,$procret,65536);
			close(TO);

			for ($l=0;$l < length($procret);$l++) {
				$c = substr($procret,$l,1);
				if (ord($c) >= 0x80) {
					$out .= sprintf("\\x%02x",ord($c));
				}
				else {
					$out .= $c;
				}
			}

			$procsum .= $proc;
			$procsum .= "\n" if ($quot ne "");

			# leave comments indicating the conversion
			if ($quot eq "") {
				$out .= "/* UTF-8 to Shift-JIS of \"".substr($procsum,0,length($procsum)-1)."\" */";
				$procsum = "";
			}
		}
		else {
			while ($i < length($line)) {
				$c = substr($line,$i,1);
				$out .= $c;
				$i++;

				if ($c eq "\"") { # todo we should allow unicode single char ' ' as well
					if ($comment eq "") {
						$quot = $c;
						last;
					}
				}
				elsif ($c eq "/" && substr($line,$i,1) eq "/") { # C++ style comment begins! // comment
					$comment = "//";
				}
				elsif ($c eq "/" && substr($line,$i,1) eq "*") { # C comment begins /* comment */
					$comment = "/*";
				}
				elsif ($comment eq "/*" && $c eq "*" && substr($line,$i,1) eq "/") { # end of C comment
					$comment = "";
				}
			}
		}
	}

	# C++ comments end at EOL
	if ($comment eq "//") {
		$comment = "";
	}

	print "$out\n";
}
