#!/usr/bin/perl
# 
# Generate a C array containing uint32_t 16.16 frequencies for all 128 MIDI keys.
# motivated by what appear to be occasional "glitches" in Watcom C's pow() function.
# C array is emitted to STDOUT.
#
# (C) 2014 Jonathan Campbell
#
# Ref C:
#   for (i=0;i < 0x80;i++) {
#     double a = 440.0 * pow(2,((double)(i - 69)) / 12);
#     midi_note_freqs[i] = (uint32_t)(a * 65536UL);
#   }
my $i,$v,$r;

print "static const uint32_t midikeys_freqs[0x80] = {\n";
for ($i=0;$i < 0x80;$i++) {
	$v = (($i - 69) / 12);	# A4 (440Hz) at key 69, 12 semitones per octave
	$a = 440.0 * (2 ** $v); # 440 * (2 ^ $v)
	print "\t".sprintf("0x%08x",int($a * 65536));
	print "," if ($i < 0x7F);
	print "\t/* key ".$i." = ".$a."Hz */";
	print "\n";
}
print "}\n";

