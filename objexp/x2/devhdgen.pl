#!/usr/bin/perl
#
# Generate ASM for MS-DOS device driver
#
# (C) 2016 Jonathan Campbell, ALL RIGHTS RESERVED
my $out_make_stub_entry = 1;
my $out_asmname = undef;
my $a;

for ($i=0;$i < @ARGV;) {
    $a = $ARGV[$i++];

    if ($a =~ s/^-+//) {
        if ($a eq "asm") {
            $out_asmname = $ARGV[$i++];
        }
        elsif ($a eq "stub") {
            $out_make_stub_entry = 1;
        }
        elsif ($a eq "no-stub") {
            $out_make_stub_entry = 0;
        }
        elsif ($a eq "h" || $a eq "help") {
            print STDERR "--asm <file>             Output ASM file to generate\n";
            print STDERR "--stub / --no-stub       Generate/Don't generate EXE entry stub\n";
            exit 1;
        }
        else {
            print STDERR "Unknown switch $a\n";
            exit 1;
        }
    }
    else {
        print STDERR "Unexpected arg $a\n";
        exit 1;
    }
}

my $mode = '';
my @symbols = ( ); # names, index by ordinal. undef means not defined.

if (!defined($out_asmname) || $out_asmname eq "") {
    die "Need --asm output";
}

open(ASM,">",$out_asmname) || die;
print ASM "; auto-generated code, do not edit\n";
print ASM "bits 16			; 16-bit real mode\n";
print ASM "\n";
print ASM "section _TEXT class=CODE\n";
print ASM "\n";

print ASM "extern dosdrv_interrupt_                ; <- watcall\n";
print ASM "\n";
print ASM "global _dosdrv_header\n";
print ASM "global _dosdrv_req_ptr\n";
print ASM "\n";

print ASM "; DOS device driver header (must come FIRST)\n";
print ASM "_dosdrv_header:\n";
print ASM "        dw  0xFFFF,0xFFFF               ; next device ptr\n";
print ASM "        dw  0xA800                      ; bit 15: 1=character device\n";
print ASM "                                        ; bit 14: 1=supports IOCTL control strings\n";
print ASM "                                        ; bit 13: 1=supports output until busy\n";
print ASM "                                        ; bit 11: 1=understands open/close\n";
print ASM "                                        ; bit  6: 0=does not understand 3.2 functions\n";
print ASM "                                        ; bit 3:0: 0=not NUL, STDIN, STDOUT, CLOCK, etc.\n";
print ASM "        dw  _dosdrv_strategy            ; offset of strategy routine\n";
print ASM "        dw  dosdrv_interrupt_           ; offset of interrupt routine\n";
print ASM "        db  'HELLO\$  '                  ; device name (8 chars)\n";
print ASM "        db  0\n";
print ASM "\n";
print ASM "; var storage\n";
print ASM "_dosdrv_req_ptr:\n";
print ASM "        dd  0\n";
print ASM "\n";
print ASM "; =================== Strategy routine ==========\n";
print ASM "_dosdrv_strategy:\n";
print ASM "        mov     [cs:_dosdrv_req_ptr+0],bx       ; BYTE ES:BX+0x00 = length of request header\n";
print ASM "        mov     [cs:_dosdrv_req_ptr+2],es       ; BYTE ES:BX+0x01 = unit number\n";
print ASM "        retf                            ; BYTE ES:BX+0x02 = command code\n";
print ASM "                                        ; WORD ES:BX+0x03 = driver return status word\n";
print ASM "                                        ;      ES:BX+0x05 = reserved??\n";
print ASM "\n";

if ($out_make_stub_entry > 0) {
    print ASM "; EXE entry point points here, in case the user foolishly tries to run this EXE\n";
    print ASM "..start:\n";
    print ASM "_exe_normal_entry:\n";
    print ASM "        mov         ax,4C00h        ; if the user tries to run this EXE, terminate\n";
    print ASM "        int         21h\n";
    print ASM "\n";
}

print ASM "; Watcom's _END symbol acts as expected in C when you access it, but then acts funny\n";
print ASM "; when you try to take the address of it. So we have our own.\n";
print ASM "section _END class=END\n";
print ASM "global _dosdrv_end\n";
print ASM "_dosdrv_end:\n";
print ASM "\n";
print ASM "; begin of init section i.e. the cutoff point once initialization is finished.\n";
print ASM "section _BEGIN class=INITBEGIN\n";
print ASM "global _dosdrv_initbegin\n";
print ASM "_dosdrv_initbegin:\n";
print ASM "\n";

close(ASM);

