#!/usr/bin/perl
#
# Generate ASM for MS-DOS device driver
#
# (C) 2016 Jonathan Campbell, ALL RIGHTS RESERVED
my $out_make_stack_entry = 0; # FIXME: This isn't quite right
my $out_make_stub_entry = 1;
my $introutine_stub = 0;    # if set, generate interrupt routine stub to load DS and near-call "interrupt" routine
my $introutine_stub_far = 0;# stub should make FAR call
my $def_initbegin = 1;      # generate initbegin
my $out_asmname = undef;
my $ds_is_cs = 0;
my $a;

my $devtype_force = undef;  # allow caller to force-specify device attributes

my $dev_name = undef;       # device name (chardev)

my $devtype = 'c';          # default: character device
my $dev_generic_ioctl = 0;  # default: does not support generic ioctl
my $dev_ioctl_queries = 0;  # default: does not support ioctl queries
my $dev_ioctl_2_3 = 0;      # default: does not ioctl functions 2 and 3

my $dev_c_stdin = 0;        # default: is not STDIN
my $dev_c_stdout = 0;       # default: is not STDOUT
my $dev_c_nul = 0;          # default: is not NUL
my $dev_c_clock = 0;        # default: is not CLOCK$
my $dev_c_int29 = 0;        # default: does not support INT 29h
my $dev_c_openclose = 0;    # default: supports open/close
my $dev_c_output_until_busy = 0; # default: does not support output until busy

my $dev_b_32bit_sectors = 0;# default: does not support 32-bit sector addressing
my $dev_b_open_close_media = 0;# default: does not support open/close media
my $dev_b_media_fat_required = 0;# default: media descriptor byte of FAT required to determine media format

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
        elsif ($a eq "ds-is-cs") {
            $ds_is_cs = 1;
        }
        elsif ($a eq "no-initbegin") {
            $def_initbegin = 0;
        }
        elsif ($a eq "int-stub") {
            $introutine_stub = 1;
        }
        elsif ($a eq "int-stub-far") {
            $introutine_stub = 1;
            $introutine_stub_far = 1;
        }
        elsif ($a eq "stack") {
            $out_make_stack_entry = 1;
        }
        elsif ($a eq "no-stack") {
            $out_make_stack_entry = 0;
        }
        elsif ($a eq "device-attr") {
            $devtype_force = int($ARGV[$i++]+0);
        }
        elsif ($a eq "type") {
            $devtype = $ARGV[$i++];
        }
        elsif ($a eq "c-gioctl") {
            $dev_generic_ioctl = 1;
        }
        elsif ($a eq "c-ioctlq") {
            $dev_ioctl_queries = 1;
        }
        elsif ($a eq "c-ioctl23") {
            $dev_ioctl_2_3 = 1;
        }
        elsif ($a eq "c-stdin") {
            $dev_c_stdin = 1;
        }
        elsif ($a eq "c-stdout") {
            $dev_c_stdout = 1;
        }
        elsif ($a eq "c-nul") {
            $dev_c_nul = 1;
        }
        elsif ($a eq "c-clock") {
            $dev_c_clock = 1;
        }
        elsif ($a eq "c-int29") {
            $dev_c_int29 = 1;
        }
        elsif ($a eq "c-openclose") {
            $dev_c_openclose = 1;
        }
        elsif ($a eq "c-out-busy") {
            $dev_c_output_until_busy = 1;
        }
        elsif ($a eq "b-32bit") {
            $dev_b_32bit_sectors = 1;
        }
        elsif ($a eq "b-openclose") {
            $dev_b_open_close_media = 1;
        }
        elsif ($a eq "b-mfb") {
            $dev_b_media_fat_required = 1;
        }
        elsif ($a eq "name") {
            $dev_name = $ARGV[$i++];
        }
        elsif ($a eq "h" || $a eq "help") {
            print STDERR "MS-DOS device driver header generator.\n";
            print STDERR "(C) 2016 Jonathan Campbell.";
            print STDERR "\n";
            print STDERR "--asm <file>             Output ASM file to generate\n";
            print STDERR "--stub / --no-stub       Generate/Don't generate EXE entry stub\n";
            print STDERR "--device-attr <x>        Force X to be device attribute word (ex. 0xA800)\n";
            print STDERR "--type <x>               Device type: c = character or b = block\n";
            print STDERR "--c-gioctl               Supports generic IOCTL\n";
            print STDERR "--c-ioctlq               Supports IOCTL queries\n";
            print STDERR "--c-ioctl23              Supports IOCTL commands 2 and 3\n";
            print STDERR "--c-stdin                Is STDIN device\n";
            print STDERR "--c-stdout               Is STDOUT device\n";
            print STDERR "--c-nul                  Is NUL device\n";
            print STDERR "--c-clock                Is CLOCK\$ device\n";
            print STDERR "--c-int29                Supports INT 29h\n";
            print STDERR "--c-openclose            Supports open/close\n";
            print STDERR "--c-out-busy             Supports out until busy\n";
            print STDERR "--b-32bit                Supports 32-bit sector addressing\n";
            print STDERR "--b-openclose            Supports media open/close\n";
            print STDERR "--b-mfb                  Requires media descriptor byte to get format\n";
            print STDERR "--name <x>               Device name (up to 8 characters)\n";
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
my $devword = 0;

die unless $devtype eq 'c' || $devtype eq 'b';
die if $devtype eq 'c' and !defined($dev_name);

if (defined($devtype_force)) {
    $devword = $devtype_force;
}
else {
    $devword += 1 << 6 if $dev_generic_ioctl;
    $devword += 1 << 7 if $dev_ioctl_queries;
    $devword += 1 << 14 if $dev_ioctl_2_3;

    if ($devtype eq 'c') {
        $devword += 1 << 15; # is a character device

        $devword += 1 <<  0 if $dev_c_stdin;
        $devword += 1 <<  1 if $dev_c_stdout;
        $devword += 1 <<  2 if $dev_c_nul;
        $devword += 1 <<  3 if $dev_c_clock;
        $devword += 1 <<  4 if $dev_c_int29;
        $devword += 1 << 11 if $dev_c_openclose;
        $devword += 1 << 13 if $dev_c_output_until_busy;
    }
    elsif ($devtype eq 'b') {
        $devword += 1 <<  1 if $dev_b_32bit_sectors;
        $devword += 1 << 11 if $dev_b_open_close_media;
        $devword += 1 << 13 if $dev_b_media_fat_required;
    }
    else {
        die;
    }
}

die if $devword > 0xFFFF;
$dev_name = "" if !defined($dev_name);
die if length($dev_name) > 8;

$dev_name_f = substr(uc($dev_name)."        ",0,8);
die if $dev_name_f =~ m/['"\\]/;

if (!defined($out_asmname) || $out_asmname eq "") {
    die "Need --asm output";
}

open(ASM,">",$out_asmname) || die;
print ASM "; auto-generated code, do not edit\n";
print ASM "bits 16          ; 16-bit real mode\n";
print ASM "\n";
print ASM "section _TEXT class=CODE\n";
print ASM "\n";

if ($introutine_stub) {
    if ($introutine_stub_far) {
        print ASM "extern dosdrv_interrupt_far_            ; <- watcall\n";
    }
    else {
        print ASM "extern dosdrv_interrupt_near_           ; <- watcall\n";
    }
}
else {
    print ASM "extern dosdrv_interrupt_                ; <- watcall\n";
}
print ASM "\n";
print ASM "global _dosdrv_header\n";
print ASM "global _dosdrv_req_ptr\n";
print ASM "\n";

print ASM "; DOS device driver header (must come FIRST)\n";
print ASM "_dosdrv_header:\n";
print ASM "        dw  0xFFFF,0xFFFF               ; next device ptr\n";
print ASM "        dw  ".sprintf("0x%04X",$devword)."                      ; bit 15: 1=character device\n";
print ASM "                                        ; bit 14: 1=supports IOCTL control strings\n";
print ASM "                                        ; bit 13: 1=supports output until busy\n";
print ASM "                                        ; bit 11: 1=understands open/close\n";
print ASM "                                        ; bit  6: 0=does not understand 3.2 functions\n";
print ASM "                                        ; bit 3:0: 0=not NUL, STDIN, STDOUT, CLOCK, etc.\n";
print ASM "        dw  _dosdrv_strategy            ; offset of strategy routine\n";
if ($introutine_stub) {
    print ASM "        dw  dosdrv_interrupt_stub_      ; offset of interrupt routine\n";
}
else {
    print ASM "        dw  dosdrv_interrupt_           ; offset of interrupt routine\n";
}
print ASM "        db  '".$dev_name_f."'                  ; device name (8 chars)\n";
print ASM "        db  0\n";
print ASM "\n";
print ASM "; =================== Strategy routine ==========\n";
print ASM "_dosdrv_strategy:\n";
if ($ds_is_cs) {
    print ASM "        mov     [cs:_dosdrv_req_ptr+0],bx ; BYTE ES:BX+0x00 = length of request header\n";
    print ASM "        mov     [cs:_dosdrv_req_ptr+2],es ; BYTE ES:BX+0x01 = unit number\n";
}
else {
    print ASM "        push    ax\n";
    print ASM "        push    ds\n";
    print ASM "        mov     ax,seg _dosdrv_req_ptr\n";
    print ASM "        mov     ds,ax\n";
    print ASM "        mov     [_dosdrv_req_ptr+0],bx    ; BYTE ES:BX+0x00 = length of request header\n";
    print ASM "        mov     [_dosdrv_req_ptr+2],es    ; BYTE ES:BX+0x01 = unit number\n";
    print ASM "        pop     ds\n";
    print ASM "        pop     ax\n";
}
print ASM "        retf                              ; BYTE ES:BX+0x02 = command code\n";
print ASM "                                          ; WORD ES:BX+0x03 = driver return status word\n";
print ASM "                                          ;      ES:BX+0x05 = reserved??\n";
print ASM "\n";

if ($introutine_stub) {
    print ASM "; =================== Interrupt routine =========\n";
    print ASM "dosdrv_interrupt_stub_:\n";
    print ASM "        push    ax\n";
    print ASM "        push    ds\n";
    if ($ds_is_cs) {
        print ASM "        mov     ax,cs\n";
        print ASM "        mov     ds,ax\n";
    }
    else {
        print ASM "        mov     ax,seg _dosdrv_req_ptr\n";
        print ASM "        mov     ds,ax\n";
    }
    if ($introutine_stub_far) {
        print ASM "        call far dosdrv_interrupt_far_\n";
    }
    else {
        print ASM "        call    dosdrv_interrupt_near_\n";
    }
    print ASM "        pop     ds\n";
    print ASM "        pop     ax\n";
    print ASM "        retf\n";
    print ASM "\n";
}

if ($out_make_stub_entry > 0) {
    print ASM "; EXE entry point points here, in case the user foolishly tries to run this EXE\n";
    print ASM "..start:\n";
    print ASM "_exe_normal_entry:\n";
    print ASM "        mov         ax,4C00h        ; if the user tries to run this EXE, terminate\n";
    print ASM "        int         21h\n";
    print ASM "\n";
}

print ASM "section _DATA class=DATA\n";
print ASM "\n";
print ASM "; var storage\n";
print ASM "_dosdrv_req_ptr:\n";
print ASM "        dd  0\n";
print ASM "\n";
print ASM "; Watcom's _END symbol acts as expected in C when you access it, but then acts funny\n";
print ASM "; when you try to take the address of it. So we have our own.\n";
print ASM "section _END class=END\n";
print ASM "global _dosdrv_end\n";
print ASM "_dosdrv_end:\n";
print ASM "\n";
if ($def_initbegin) {
    print ASM "; begin of init section i.e. the cutoff point once initialization is finished.\n";
    print ASM "section _BEGIN class=INITBEGIN\n";
    print ASM "global _dosdrv_initbegin\n";
    print ASM "_dosdrv_initbegin:\n";
    print ASM "\n";
}

if ($out_make_stack_entry > 0) {
    print ASM "; shut up Watcom\n";
    print ASM "section _STACK class=STACK\n";
    print ASM "\n";
}

close(ASM);

