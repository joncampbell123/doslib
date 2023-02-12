
/* TODO: The ESS control register dump should show text on the side that provides a
 *       visual "guide" on what the bits are supposed to mean (slightly different
 *       text for ESS 688 vs ESS 1887, etc.) so the user has more guidance in poking
 *       around those control registers */

/* TODO: Sound Blaster Live! DOS emulation is pretty good Sound Blaster 16 emulation,
 *       though missing some commands. How do we detect that emulation driver and
 *       react appropriately? */

/* Notes:
 *    Sound Blaster Live! EMU10K1 PCI:
 *       - Emulates a Sound Blaster 16
 *       - Doesn't support Direct DAC commands (silently ignored)
 *       - Doesn't support ADPCM playback modes (DMA doesn't even cycle)
 *       - Playing audio by single cycle reveals that the last little part of the
 *         buffer will always be skipped by the DSP.
 *       - Our code thinks (incorrectly) it can change the IRQ/DMA settings of the card.
 *       - Backwards DMA playback/recording doesn't work (why would it?)
 *       - Otherwise, it works!
 *
 *    ESS 688 PnP AudioDrive:
 *       - Plug and Play enumerated from the BIOS on a Sharp laptop (uses IRQ 9?!?)
 *       - Supports most Sound Blaster Pro commands and emulates the mixer
 *       - Nag mode works
 *       - No DSP copyright string
 *       - DSP does not wait if IRQ is not ACKed
 *       - Supports ADPCM single-cycle commands!
 *       - Does NOT support ADPCM auto-init commands
 *       - DSP direct DAC commands are not rate-limited, it's possible to play up to
 *         about 60KHz using direct DAC output!
 *       - Also carries or emulates Adlib (OPL3) and joystick port
 *
 *    Gallant SC-660 clone:
 *       - Does not support auto-init ADPCM, though all single-cycle ADPCM commands are
 *         correctly supported.
 *       - DSP time constant is rate limited, but there's a minor bug in how it does it:
 *
 *            If you submit 4000...25000Hz   -> You get 4000...25000Hz
 *            If you submit 25000...35000Hz  -> You get 25000Hz
 *            If you submit 35000...44100Hz  -> You get 24000Hz?
 *
 *            This bug does not affect DSP 4.xx SB16 commands and does not affect
 *            high-speed DSP commands.
 *       - SB16 commands are supported... kind of. The card is obviously coded to expect
 *         only the auto-init + FIFO form of the DSP 4.xx commands (any other combination,
 *         such as single-cycle with/without FIFO or auto-init without FIFO, is ignored by
 *         the card).
 *       - Nag mode has no effect for DSP 4.xx commands, since single-cycle is impossible.
 *       - Does not stop playback if IRQ not acknowledged.
 *
 *    Sound Blaster 2.0 (Creative):
 *       - DSP 1.xx playback maxes out at 22050Hz (not 23KHz documented by Creative)
 *       - DSP 2.xx playback maxes out at 46000Hz (not 44.1KHz documented by Creative)
 *       - DSP auto-init ADPCM supported
 *       - ADPCM 4-bit maxes out at 12KHz
 *       - ADPCM 2.6-bit maxes out at 12KHz (not 13KHz documented by Creative)
 *       - ADPCM 2-bit maxes out at 15KHz (not 11KHz documented by Creative)
 */

/*=========================================OLD===================================================*/
/* Emulator testing:
 *    Sun/Oracle VirtualBox:
 *       - Works perfectly
 *       - Does not emulate ADPCM playback
 *       - Does not do recording... AT ALL
 *       - Supports changing the SB16 configuration byte, but only the IRQ. Changing the DMA
 *         channel has no effect apparently
 *    Microsoft Virtual PC:
 *       - Works perfectly, except for DSP 1.xx single-cycle playback where
 *         it signals the IRQ immediately after playback begins, and can cause
 *         an IRQ storm that overflows the stack and crashes us
 *       - Does not emulate ADPCM playback
 *       - Does record, using local system mic in
 *       - Supports changing the SB16 configuration byte, but changes apparently have no effect
 *         on the resources assigned to the card. This is understandable since according to
 *         the isapnp test program Virtual PC is emulating a Plug & Play type Sound Blaster.
 *         But for some strange reason, the ISA PnP BIOS and PNP emulation then reports the
 *         I/O port as fixed and only lists one possible IRQ and DMA combination. The PnP
 *         device is also active by default, contrary to actual PnP Sound Blaster cards.
 *    DOSBox 0.74
 *       - Works perfectly (with sbtype = sb1, sb2, sbpro1, sbpro2, and sb16)
 *       - Emulates ADPCM playback perfectly, except for auto-init versions of the command
 *       - Fails to emulate recording commands. Command 0x24 is emulated only enough to
 *         return silence immediately, so DONT play with recording under DOSBox unless you
 *         want your (emulated) C: drive to quickly fill up with silence!
 *       - Changing SB16 configuration works (though for some reason, you get noise and garbage
 *         if you attempt to assign the SB16 to DMA channel 0)
 *
 * Real hardware testing:
 *    VIA EPIA motherboard with BIOS Sound Blaster emulation:
 *       - Emulates a Pro only (DSP v3.xx)
 *       - Actual hardware is PCI device, a proprietary VIA interface
 *       - For some reason, no sound can be heard on the output, even if we play with the mixer
 *       - Audio hardware doesn't record anything except silence. Either it's faking the record
 *         commands by filling the buffer with 0x7F or it takes proprietary commands to connect
 *         the audio source within the chip
 *    Toshiba Satellite 465CDX laptop:
 *       - Chipset is Yamaha OPL3-SA2 PCI chipset that emulates a Sound Blaster
 *       - Emulates a Pro only (DSP v3.1 and CT1345 mixer)
 *       - Mixer seems to ignore almost every setting except stereo bit and master+voice volume
 *         controls, and even then, seems to support either full volume or (if any set to zero)
 *         full silence.
 *       - Has Windows Sound System on 0x5x0 (x=3,4,5,6)
 *       - Supports ADPCM playback, but only the 4-bit version. If asked to do the 2.6 and 3-bit
 *         versions, the card cycles DMA as if playing uncompressed 8-bit but does not make any
 *         noise.
 *       - Does not return copyright string
 *       - Supports auto-init ADPCM playback
 *       - Emulates recording commands, but does not actually record (it "records" samples of 0x00)
 *       - Otherwise works perfectly
 *       - Interesting hardware bug: If you use Sound Blaster Pro stereo and set a time interval to
 *         try to play 8-bit 44010Hz stereo (beyond what an actual Sound Blaster is supposed to be
 *         able to do) and the main loop is polling the DMA channel, the output runs at something
 *         like 320000Hz (like a Sound Blaster) but it has a noisy sound to it as if the constant
 *         polling is causing serious jitter in the hardware. When the main loop stops polling, the
 *         jitter seems to go away.
 *    Old Pentium/386/486 system with Reveal v1.10 SC-6600 card (ISA)
 *       - Marked as Reveal, apparently referred to in Linux kernel as Gallant SC-6600
 *       - Has a Crystal Semiconductor CS4321 chip
 *       - Emulates a weird hybrid of Sound Blaster functions: It reports itself as a Sound Blaster
 *         Pro (DSP v3.5) yet it supports Sound Blaster 16 DSP functions, but unlike a SB16 it only
 *         emulates the Sound Blaster Pro mixer settings and lacks the SB16 "configuration byte"
 *         mixer registers (0x80-0x81)
 *       - Is is "plug and play" in the sense that through the Sound Blaster interface there are
 *         secret DSP commands you can issue to read and set the hardware configuration (such as,
 *         IRQ and DMA channels)
 *       - Emulates recording and playback perfectly
 *       - Emulates ADPCM playback, does NOT emulate auto-init ADPCM commands
 *       - Sample rate maxes out at 44.1KHz even though the DSP command set could conceiveably support
 *         rates up to 64KHz
 *       - Supports nonstandard "flipped sign" 8-bit and 16-bit PCM via DSP 4.xx playback commands
 *       - Supports DSP 4.xx recording commands
 *       - Unlike the SB16, it plays both 8-bit and 16-bit on the same DMA channel (and only on the
 *         same DMA channel)
 *       - Also has Windows Sounds System at 0x530 (or other ports if configured so)
 *       - Raw view of the mixer registers suggests their Pro mixer emulation only decodes the lower
 *         6 bits of the index register. This means for whatever index you write the actual value
 *         is (index % 64).
 *       - Fully supports IRQ and DMA reassignment from this program
 *       - The DSP on this one is slightly slower than normal. In fact this card motivated me to
 *         extend the timeout values in the DSP read/write functions because the previous values
 *         would time out too soon and fail communication with the DSP on this card
 *       - On the Pentium MMX 200MHz system: Apparently DMA channel 3 is not a good choice. It seems
 *         to play 1000-2000 samples or so then stall out completely. Not sure what that's about.
 *         All IRQ choices work though, except of course for IRQ 9 (which is probably taken by the
 *         motherboard or PCI device anyway).
 *    Old Pentium/386/486 system with Creative CT1350B Sound Blaster (ISA 8-bit)
 *       - A Sound Blaster 2.0, but not a Sound Blaster Pro
 *       - DSP v2.2
 *       - No mixer chip. Nothing responds to base+4 and base+5
 *       - The DSP has a hardware bug where attempting to play >= 17KHz without using hispeed mode
 *         causes audible warbling if anything else is using DMA (such as: reading from the floppy
 *         drive). It's almost like the DSP in non-hispeed mode is sloppy on when to initiate
 *         another DMA transfer of a sample, hence, the reason Creative went about implementing
 *         hispeed mode in the first place? Perhaps, if the DSP is a specialized CPU, hispeed mode
 *         means that it goes into a tight loop that focuses only on the playback (instead of a
 *         more general loop that listens for certain DSP commands while playing), which is why
 *         Creative says it needs a DSP reset to break out. Right?
 *       - Supports ADPCM playback and auto-init
 *       - Playback maxes out at 44.1KHz, recording at 22.05Hz
 *       - The old detection code failed on this card, would not work until sndsb.c added code
 *         to reset the DSP per IRQ and DMA test
 *       - Question: If the sound card has a mic in and line in, but no mixer chip to choose, then
 *         how does it pick the input to capture from? Does it amplify the mic in and digitize the
 *         mix of the two? Or does it have one of the jacks wired to cut out the other when something
 *         is inserted? (some equipment has a metal piece that when displaced by the plug, shorts to
 *         another metal piece and that short is sensed by the equipment as a signal that something
 *         is plugged in)
 *       - The mic in works (I accidentally recorded loud distorted music from my radio because
 *         the jacks aren't labeled and had the patch cable in the mic in jack) but it doesn't seem
 *         to like modern computer mics, hmm... Line in works perfectly fine however.
 *       - Lack of input filters definitely means you don't want to record on this DSP at anything
 *         below 8KHz, it will sound like crap. In fact, playback below 8KHz isn't really worth it
 *         either...
 *       - Despite being a genuine Creative product, the DSP copyright command does not return a string
 *       - Does not appear to have MPU401 MIDI I/O, the large plug on the back is most likely only
 *         a joystick port
 *       - This is the older variety of Sound Blaster where for DSP playback (even "direct" mode)
 *         you MUST use DSP commands 0xD1 and 0xD3 (Speaker On/Off) to turn the DSP on for playback
 *         and DSP off when done. Playing sound while the speaker is off seems to yield only quiet
 *         staticky noise from the speakers that somewhat resembles what you are trying to play
 *         (when it's supposed to be silent!).
 *       - Recording seems to cause a high-pitched but quiet whining sound from the speaker output.
 *         This software mitigates that somewhat by shutting off the speakers during capture, but
 *         it's still kind of there.
 *    Old Pentium/386/486 system with Creative CT4170 Sound Blaster 16 ViBRA16XV (Plug & Play)
 *       - PnP ID CTL00F0
 *       - DSP v4.16
 *       - SB16 mixer
 *       - All PnP resources are like a Sound Blaster, except that at boot the card leaves them
 *         disabled until a PnP configuration utility assigns resources and enables them
 *       - Contains DSP copyright string as expected
 *       - Just as documented by Creative, the card is PnP based and the configuration bytes in
 *         the mixer are read only, not changeable
 *       - The DSP appears to max out at 48KHz. Apparently 48KHz is also possible when using the
 *         DSP 2/3.xx "high speed" commands. DSP 1.xx commands however max out at 24KHz.
 *       - Auto-init ADPCM and all ADPCM modes are supported
 *       - The DSP can be instructed to play non-standard "flipped sign" formats (16-bit unsigned
 *         and 8-bit signed) via the DSP 4.xx commands
 *       - Card appears to allow bass/treble controls but they have no noticeable effect on the
 *         sound as far as I can tell
 *       - Speaks PnP protocol to list 0x2x0-0x2xF (x=2,4,6,8) DSP and mixer region, 0x388 OPL3
 *         region. and 0x3x0-0x3x1 (x=0,3) MPU MIDI region, as well as a second "logical" device
 *         that is the game port (0x200-0x207) (CTL7005). According to PnP configuration data,
 *         it supports moving the OPL3 interface to 0x394 (?).
 *       - When Intel/Microsoft's ISA PnP spec said to wait 250us per I/O pair read during
 *         the configuration read, they weren't kidding. The isapnp test program could not
 *         properly read the VIBRA's PnP configuration until the 250 delay was added. So whatever
 *         hardware Creative added is not fast enough for a Pentium Pro to read configuration
 *         from it full-speed?
 *       - There appear to be extra mysterious mixer bytes at the 0xFD-0xFE range. What are they?
 *    Old Pentium/386/486 system with Creative CT4180 Sound Blaster 16 ViBRA16C (Plug & Play)
 *       - PnP ID CTL0070
 *       - DSP v4.13
 *       - Same comments as the ViBRA16X listed above
 *    Old Pentium/386/486 system with Creative CT4500 Sound Blaster AWE64 (Plug & Play)
 *       - PnP ID CTL00C3
 *       - DSP v4.16
 *       - For some interesting reason the hardware mixer defaults to 75% master & PCM volume
 *       - The bass & treble mixer controls actually work!
 *       - The raw mixer dump shows a whole row of mystery bytes at 0x88-0x8E and 0xFD-0xFF
 *       - Supports flipped sign nonstandard PCM formats (8-bit signed, 16-bit unsigned)
 *       - DSP maxes out at 48KHz (4.xx commands and 2/3.xx high-speed mode commands), or
 *         24KHz (1.xx commands)
 *       - Supports ADPCM modes and auto-init
 *       - Also has the AWE WaveTable interface on PnP ID CTL0022 logical device #2 (just after the GamePort)
 *         where the WaveTable interface can be assigned to I/O port 0x6x0-0x6x3 (4 I/O ports long)
 *         where x is 2, 4, 6, or 8
 *    Old Pentium/386/486 system with Creative CT4540 Sound Blaster AWE64 Gold (Plug & Play)
 *       - PnP ID CTL00B2
 *       - DSP v4.16
 *       - Also has the AWE WaveTable interface on PnP ID CTL0023 logical device #2 (just after the GamePort)
 *         where the WaveTable interface can be assigned to I/O port 0x6x0-0x6x3 (4 I/O ports long)
 *         where x is 2, 4, 6, or 8
 *       - Same comments as for AWE64 CT4500 listed above
 *    Old Pentium system with Gravis Ultrasound MAX and SBOS (Sound Blaster emulation)
 *       - DSP v2.1
 *       - Quality is OK, emulation is terrible
 *       - Does not properly emulate auto-init DMA playback
 *       - Sound Blaster 1.x style playback (manual-init via IRQ) shows
 *         DMA playback pointer by too fast before the IRQ actually fires.
 *         So much for any accurate timing, eh?
 *       - Does not emulate Sound Blaster Pro stereo
 *       - Does not emulate ADPCM playback. And if you try, further audio
 *         output is corrupted/unusable.
 *       - Obviously doesn't know the "DSP copyright string" command, we read
 *         back a string of smileys (ascii code 0x01)
 *    Old Pentium system with Gravis Ultrasound MAX and MEGA-EM SoundBlaster emulation
 *       - Can't say for sure, MEGA-EM doesn't recognize DSP command 0xE3.
 *         But rather than kindly emulate a "I dunno" response it chooses
 *         to hard-lock the machine and display an error message complaining
 *         about not understanding the DSP command. Thanks >:(
 *       - But MEGA-EM itself is very crashy. So crashy that if you EMUSET
 *         and then run anything that uses a DOS extender like DOS4G/W your
 *         computer will hard-crash and reset. (FIXME: Or perhaps this is a
 *         problem with using EMM386.EXE from a Windows 95 bootdisk?)
 *       - I was only able to do some semblance of SB testing by temporarily
 *         commenting out the DSP copyright string part of the code.
 *       - MEGA-EM reports itself as DSP version 1.3. Seriously?!?
 *         For a "Sound Blaster compatible" in the 1993-1997 timeframe
 *         that's pretty pathetic!
 *       - MEGA-EM does not emulate anything above Sound Blaster. Not even
 *         Pro commands. It will do the same hard-crash on commands 0x1C and
 *         on SB16 commands like 0x41.
 *       - DMA runs too fast relative to IRQ activity, just like SBOS.
 *       - Does not emulate any mixer chip.
 *       - Doesn't really understand Creative's ADPCM format, but fakes it...
 *         badly. Doesn't decode it right.
 *       - Unloading MEGA-EM doesn't resolve the crashing. But not running
 *         SMARTDRV, and unloading MEGA-EM prior to using a DOS extender
 *         does resolve it. If SMARTDRV is already loaded, you can tell
 *         SMARTDRV to disable caching on all drives and that will have the
 *         same effect. So basically, the hard crash is a combination of
 *         EMM386.EXE virtualization and programs trying to use extended RAM.
 *         Hmm...
 *    Microsoft Windows XP with SB emulation enabled [applies also to Windows Vista & Windows 7]
 *       - Emulates a Sound Blaster 2.1. In other words, the lazy asses at Microsoft
 *         couldn't be bothered to emulate Pro stereo, or 16-bit playback.
 *       - Buffer intervals must be a struct integer subdivision of the total buffer
 *         when doing auto-init playback. Or else, the implementation will just run
 *         off into the weeds. Pathetic.
 *       - Mixer emulation provides the absolute bare minimum of Sound Blaster controls.
 *         Just the master volume, voice control, and CD volume. The lazy asses also
 *         didn't bother to emulate the combined effect of Master and Voice volume
 *         levels, meaning; if you write the master volume, then the voice volume,
 *         the voice volume takes effect, regardless what the master volume was.
 *       - Their implementation makes no attempt at emulating the DMA transfer actually
 *         happening. So if a program were to watch the DMA counter it would see it
 *         abruptly "jump" down by the DMA block size. Pathetic. That's worse than
 *         Gravis's SBOS emulation, at least a program there would see DMA moving!
 *       - You can't make DMA block sizes too small, or their code sputters and
 *         underruns. You can't make it too large (4K or more!) or it audibly drops
 *         samples and the audio seems to play too fast! What the fuck Microsoft?
 *       - Against every attempt otherwise, it manages to play the prior buffer
 *         contents before finally playing what my code is trying to play. Why?
 *       - Under Windows XP, something causes this program to eventually hang. Why?
 *    Microsoft Windows XP with VDMSOUND.EXE
 *       - Works perfectly!
 *       - Although, even VDMSOUND.EXE stutters a bit when this program uses the 'tiny'
 *         IRQ interval setting, but... that's understandable.
 *       - Emulates SB16 mixer byte that stores configuration, but you cause all IRQ
 *         activity to stop when you write to it.
 *
 * Card notes:
 *     I have a Creative ViBRA16X/XV that is apparently unable to use it's
 *     high DMA channel. Independent testing across some Pentium and 486
 *     motherboards shows that the card is at fault, on NONE of them am I
 *     able to use DMA channels 5, 6, or 7 for 16-bit playback.
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <direct.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/sndsb/sb16asp.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>

#if defined(TARGET_PC98)
# include <hw/necpc98/necpc98.h>

nec_pc98_intdc_keycode_state_ext            prev_pc98_keycodes;
nec_pc98_intdc_keycode_state_ext            cur_pc98_keycodes;
#endif

#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
  /* chop features out of the Compact memory model build to ensure all code fits inside 64KB */
  /* don't include FX */
  /* don't include live config */
  /* don't include mixer */
#else
# define INCLUDE_FX
# define SB_MIXER
# define CARD_INFO_AND_CHOOSER
# define LIVE_CFG
# if !defined(TARGET_PC98)
#  define ISAPNP
# endif
#endif

#ifdef ISAPNP
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
static unsigned char dmam = 0;
static   signed char dma128 = 0;
static unsigned char ignore_asp = 0;
static unsigned char force_srate_tc = 0;
static unsigned char force_srate_4xx = 0;
static unsigned char always_reset_dsp_to_stop = 0;
static unsigned char assume_dsp_hispeed_doesnt_block = 0;
static unsigned char dma_autoinit_override = 0;
static unsigned char sb_debug = 0;
#endif

static uint32_t      buffer_limit = 0;
static unsigned char force_dma_probe = 0;
static unsigned char force_irq_probe = 0;
static unsigned char irq_probe_f2 = 1;
static unsigned char irq_probe_80 = 1;
static unsigned char dma_probe_e2 = 1;
static unsigned char dma_probe_14 = 1;

static struct dma_8237_allocation *sb_dma = NULL; /* DMA buffer */

static struct sndsb_ctx*	sb_card = NULL;

enum {
	GOLDRATE_MATCH=0,
	GOLDRATE_DOUBLE,
	GOLDRATE_MAX
};

static unsigned char		animator = 0;
static int			wav_fd = -1;
static char			temp_str[512];
#ifdef ISAPNP
static unsigned char far	devnode_raw[4096];
#endif
static char			wav_file[130] = {0};
static unsigned char		wav_stereo = 0,wav_16bit = 0,wav_bytes_per_sample = 1;
static unsigned long		wav_data_offset = 44,wav_data_length = 0,wav_sample_rate = 8000,wav_position = 0,wav_buffer_filepos = 0;
static unsigned long		wav_sample_rate_by_timer_ticks = 1;
static unsigned long		wav_sample_rate_by_timer = 1;
static unsigned char		dont_sb_idle = 0;
static unsigned char		dont_chain_irq = 0;
static unsigned char		wav_playing = 0;
static unsigned char		wav_record = 0;
static signed char		reduced_irq_interval = 0;
static unsigned char		sample_rate_timer_clamp = 0;
static unsigned char		goldplay_samplerate_choice = GOLDRATE_MATCH;

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
/* fun with Creative ADPCM */
static unsigned char		do_adpcm_ai_warning = 1;
#endif

static volatile unsigned char	IRQ_anim = 0;

#ifdef INCLUDE_FX
/* effects */
static unsigned int		fx_volume = 256;		/* 256 = 100% */
static unsigned int		fx_echo_delay = 0;		/* number of samples to delay echo */
#endif

static inline unsigned char xdigit2int(char c) {
	if (c >= '0' && c <= '9')
		return (unsigned char)(c - '0');
	else if (c >= 'a' && c <= 'f')
		return (unsigned char)(c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
		return (unsigned char)(c - 'A' + 10);
	return 0;
}

#ifdef INCLUDE_FX
/*----------------------------- FX: echo ------------------- */
static uint8_t FAR *fx_echo_buf = NULL;
static unsigned int fx_echo_ptr = 0;
static unsigned int fx_echo_alloc = 0;
static void fx_echo_free() {
	if (fx_echo_buf != NULL) {
#if TARGET_MSDOS == 32
		free(fx_echo_buf);
#else
		_ffree(fx_echo_buf);
#endif
		fx_echo_buf = NULL;
		fx_echo_ptr = 0;
		fx_echo_alloc = 0;
	}
}

static void fx_echo_check() {
	unsigned int ns;

#if TARGET_MSDOS == 16
	if (fx_echo_delay > 14000)
		fx_echo_delay = 14000;
#endif

	ns = fx_echo_delay * (wav_16bit?2:1) * (wav_stereo?2:1);
	if (ns == fx_echo_alloc) return;

	if (fx_echo_buf == NULL) {
#if TARGET_MSDOS == 32
		fx_echo_buf = malloc(ns);
#else
		fx_echo_buf = _fmalloc(ns);
#endif
	}
	else {
#if TARGET_MSDOS == 32
		free(fx_echo_buf);
		fx_echo_buf = malloc(ns);
#else
		_ffree(fx_echo_buf);
		fx_echo_buf = _fmalloc(ns);
#endif
	}

	if (fx_echo_buf == NULL)
		return;

	fx_echo_alloc = ns;
	fx_echo_ptr = 0;

#if TARGET_MSDOS == 32
	memset(fx_echo_buf,wav_16bit ? 0 : 128,ns);
#else
	_fmemset(fx_echo_buf,wav_16bit ? 0: 128,ns);
#endif
}

static void fx_echo16(int16_t FAR *d,unsigned int samp/*samples x channels*/) {
	long L,R;

	if (fx_echo_delay == 0) {
		fx_echo_free();
		return;
	}
	fx_echo_check();
	if (fx_echo_buf == NULL) return;

	if (wav_stereo) {
		samp >>= 1;
		while (samp-- != 0) {
			L = (long)d[0] + (((long) *((int16_t FAR*)(&fx_echo_buf[fx_echo_ptr+0])) )*3)/4;
			if (L < -32768L) L = -32768L; else if (L > 32767L) L = 32767L;
			R = (long)d[1] + (((long) *((int16_t FAR*)(&fx_echo_buf[fx_echo_ptr+2])) )*3)/4;
			if (R < -32768L) R = -32768L; else if (R > 32767L) R = 32767L;
			*d++ = *((int16_t FAR*)(&fx_echo_buf[fx_echo_ptr+0])) = L;
			*d++ = *((int16_t FAR*)(&fx_echo_buf[fx_echo_ptr+2])) = R;
			fx_echo_ptr += 4;
			if (fx_echo_ptr >= fx_echo_alloc) fx_echo_ptr = 0;
		}
	}
	else {
		while (samp-- != 0) {
			L = (long)*d + (((long) *((int16_t FAR*)(&fx_echo_buf[fx_echo_ptr])) )*3)/4;
			if (L < -32768L) L = -32768L; else if (L > 32767L) L = 32767L;
			*d++ = *((int16_t FAR*)(&fx_echo_buf[fx_echo_ptr])) = L;
			fx_echo_ptr += 2;
			if (fx_echo_ptr >= fx_echo_alloc) fx_echo_ptr = 0;
		}
	}
}

static void fx_echo8(unsigned char FAR *d,unsigned int samp/*samples x channels*/) {
	int L,R;

	if (fx_echo_delay == 0) {
		fx_echo_free();
		return;
	}
	fx_echo_check();
	if (fx_echo_buf == NULL) return;

	if (wav_stereo) {
		samp >>= 1;
		while (samp-- != 0) {
			L = ((int)d[0] - 128) + (((int)fx_echo_buf[fx_echo_ptr] - 128)*3)/4;
			if (L < -128) L = -128; else if (L > 127) L = 127;
			R = ((int)d[1] - 128) + (((int)fx_echo_buf[fx_echo_ptr+1] - 128)*3)/4;
			if (R < -128) R = -128; else if (R > 127) R = 127;
			*d++ = fx_echo_buf[fx_echo_ptr++] = L + 128;
			*d++ = fx_echo_buf[fx_echo_ptr++] = R + 128;
			if (fx_echo_ptr >= fx_echo_alloc) fx_echo_ptr = 0;
		}
	}
	else {
		while (samp-- != 0) {
			L = ((int)*d - 128) + (((int)fx_echo_buf[fx_echo_ptr] - 128)*3)/4;
			if (L < -128) L = -128; else if (L > 127) L = 127;
			*d++ = fx_echo_buf[fx_echo_ptr] = L + 128;
			if (++fx_echo_ptr >= fx_echo_alloc) fx_echo_ptr = 0;
		}
	}
}


/*----------------------------- FX: volume ------------------- */
static void fx_vol16(int16_t FAR *d,unsigned int samp/*samples x channels*/) {
	long res;

	if (fx_volume == 256)
		return;

	while (samp-- != 0) {
		res = (((long)*d * (long)fx_volume) >> 8L);
		if (res > 32767)
			*d++ = 32767;
		else if (res < -32768)
			*d++ = -32768;
		else
			*d++ = (int16_t)res;
	}
}

static void fx_vol8(unsigned char FAR *d,unsigned int samp/*samples x channels*/) {
	long res;

	if (fx_volume == 256)
		return;

	while (samp-- != 0) {
		res = ((((long)*d - 128L) * (long)fx_volume) >> 8L);
		if (res > 127)
			*d++ = 255;
		else if (res < -128)
			*d++ = 0;
		else
			*d++ = (unsigned char)(res + 0x80L);
	}
}

/*-----------------------------------------------------------------*/
static void fx_proc(unsigned char FAR *d,unsigned int samp) {
	if (wav_16bit) {
		fx_echo16((int16_t FAR*)d,samp*(wav_stereo?2:1));
		fx_vol16((int16_t FAR*)d,samp*(wav_stereo?2:1));
	}
	else {
		fx_echo8(d,samp*(wav_stereo?2:1));
		fx_vol8(d,samp*(wav_stereo?2:1));
	}
}
#endif /* INCLUDE_FX */

static void stop_play();

static void draw_irq_indicator() {
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram;
	unsigned char i;

#if defined(TARGET_PC98)
	wr[0] = 'S';    wr[0+0x1000] = 0xC1;
	wr[1] = 'B';    wr[1+0x1000] = 0xC1;
	wr[2] = '-';    wr[2+0x1000] = 0xC1;
	wr[3] = 'I';    wr[3+0x1000] = 0xC1;
	wr[4] = 'R';    wr[4+0x1000] = 0xC1;
	wr[5] = 'Q';    wr[5+0x1000] = 0xC1;
	for (i=0;i < 4;i++) {
        wr[i+6] = (uint16_t)(i == IRQ_anim ? 'x' : '-');
        wr[i+6+0x1000] = 0xC1;
    }
#else
	wr[0] = 0x1E00 | 'S';
	wr[1] = 0x1E00 | 'B';
	wr[2] = 0x1E00 | '-';
	wr[3] = 0x1E00 | 'I';
	wr[4] = 0x1E00 | 'R';
	wr[5] = 0x1E00 | 'Q';
	for (i=0;i < 4;i++) wr[i+6] = (uint16_t)(i == IRQ_anim ? 'x' : '-') | 0x1E00;
#endif
}

static uint32_t irq_0_count = 0;
static uint32_t irq_0_adv = 1;
static uint32_t irq_0_max = 1;
#if TARGET_MSDOS == 32
static unsigned char irq_0_had_warned = 0;
#endif

/* IRQ 0 watchdog: when playing audio it is possible to exceed the rate
   that the CPU can possibly handle servicing the interrupt. This results
   in audio that still plays but the UI is "frozen" because no CPU time
   is available. If the UI is not there to reset the watchdog, the ISR
   will auto-stop playback, allowing the user to regain control without
   hitting the RESET button. */
static volatile uint32_t irq_0_watchdog = 0x10000UL;
static void irq_0_watchdog_ack() {
	if (irq_0_watchdog != 0UL) {
		irq_0_watchdog += 0x800UL; /* 1/32 of max. This should trigger even if UI is only reduced to tar speeds by ISR */
		if (irq_0_watchdog > 0x10000UL) irq_0_watchdog = 0x10000UL;
	}
}

static void irq_0_watchdog_reset() {
	irq_0_watchdog = 0x10000UL;
}

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */
static unsigned char old_irq_masked = 0;
static void (interrupt *old_irq_0)() = NULL;
static void interrupt irq_0() { /* timer IRQ */
	/* if we're playing the DSP in direct mode, then it's our job to do the direct DAC/ADC commands */
	if (wav_playing && irq_0_watchdog > 0UL) {
		if (sb_card && sb_card->timer_tick_func != NULL)
			sb_card->timer_tick_func(sb_card);

		if (--irq_0_watchdog == 0UL) {
			/* try to help by setting the timer rate back down */
			write_8254_system_timer(0); /* restore 18.2 tick/sec */
			irq_0_count = 0;
			irq_0_adv = 1;
			irq_0_max = 1;
		}
	}

#if defined(TARGET_PC98)
    // do not chain to BIOS handler, it will confuse the timer tick system in place
    p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
#else
	/* tick rate conversion. we may run the timer at a faster tick rate but the BIOS may have problems
	 * unless we chain through it's ISR at the 18.2 ticks/sec it expects to be called. If we don't,
	 * most BIOS services will work fine but some parts usually involving the floppy drive will have
	 * problems and premature timeouts, or may turn the motor off too soon.  */
	irq_0_count += irq_0_adv;
	if (irq_0_count >= irq_0_max) {
		/* NOTE TO SELF: Apparently the 32-bit protmode version
		   has to chain back to the BIOS or else keyboard input
		   doesn't work?!? */
		irq_0_count -= irq_0_max;
		old_irq_0(); /* call BIOS underneath at 18.2 ticks/sec */
	}
	else {
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
#endif
}

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */
static void (interrupt *old_irq)() = NULL;
static void interrupt sb_irq() {
	unsigned char c;

	sb_card->irq_counter++;
	if (++IRQ_anim >= 4) IRQ_anim = 0;
	draw_irq_indicator();

	/* ack soundblaster DSP if DSP was the cause of the interrupt */
	/* NTS: Experience says if you ack the wrong event on DSP 4.xx it
	   will just re-fire the IRQ until you ack it correctly...
	   or until your program crashes from stack overflow, whichever
	   comes first */
	c = sndsb_interrupt_reason(sb_card);
	sndsb_interrupt_ack(sb_card,c);

	/* FIXME: The sndsb library should NOT do anything in
	   send_buffer_again() if it knows playback has not started! */
	/* for non-auto-init modes, start another buffer */
	if (wav_playing) sndsb_irq_continue(sb_card,c);

	/* NTS: we assume that if the IRQ was masked when we took it, that we must not
	 *      chain to the previous IRQ handler. This is very important considering
	 *      that on most DOS systems an IRQ is masked for a very good reason---the
	 *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
	 *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
	 *      not advised! */
	if (old_irq_masked || old_irq == NULL || dont_chain_irq) {
		/* ack the interrupt ourself, do not chain */
		if (sb_card->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		old_irq();
	}
}

static void save_audio(struct sndsb_ctx *cx,uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	unsigned char FAR *buffer = sb_dma->lin;
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram + 80 - 6;
	unsigned char load=0;
	uint16_t prev[6];
	int rd,i,bufe=0;
	uint32_t how;

	/* caller should be rounding! */
	assert((up_to & 3UL) == 0UL);
	if (up_to >= cx->buffer_size) return;
	if (cx->buffer_size < 32) return;
	if (cx->backwards) return; /* NTS: When we do support backwards DMA recording, this code will need to reverse the audio then write forwards in WAV */
	if (cx->buffer_last_io == up_to) return;
	if (sb_card->dsp_adpcm != 0) return;
	if (max == 0) max = cx->buffer_size/4;
	if (max < 16) return;
	lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	while (max > 0UL) {
		/* the most common "hang" apparently is when IRQ 0 triggers too much
		   and then somehow execution gets stuck here */
		if (irq_0_watchdog < 16UL)
			break;

		if (up_to < cx->buffer_last_io) {
			how = (cx->buffer_size - cx->buffer_last_io); /* from last IO to end of buffer */
			bufe = 1;
		}
		else {
			how = (up_to - cx->buffer_last_io); /* from last IO to up_to */
			bufe = 0;
		}

		if (how > max)
			how = max;
		else if (how > 16384UL)
			how = 16384UL;
		else if (!bufe && how < min)
			break;
		else if (how == 0UL)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'S' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'A' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'V' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'E' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		if (cx->buffer_last_io == 0)
			wav_buffer_filepos = wav_position;

		if (sb_card->audio_data_flipped_sign) {
			if (wav_16bit)
				for (i=0;i < ((int)how-1);i += 2) buffer[cx->buffer_last_io+i+1] ^= 0x80;
			else
				for (i=0;i < (int)how;i++) buffer[cx->buffer_last_io+i] ^= 0x80;
		}

#if TARGET_MSDOS == 32
        rd = _dos_xwrite(wav_fd,buffer + cx->buffer_last_io,how);
#else
        {
            uint32_t o;

            o  = (uint32_t)FP_SEG(buffer) << 4UL;
            o += (uint32_t)FP_OFF(buffer);
            o += cx->buffer_last_io;
            rd = _dos_xwrite(wav_fd,MK_FP(o >> 4UL,o & 0xFUL),how);
        }
#endif

		if (rd <= 0 || (uint32_t)rd < how) {
			fprintf(stderr,"write() failed, %d < %d\n",
				rd,(int)how);
			stop_play();
			break;
		}

		assert((cx->buffer_last_io+((uint32_t)rd)) <= cx->buffer_size);
		wav_position += (uint32_t)rd / wav_bytes_per_sample;
		cx->buffer_last_io += (uint32_t)rd;

		assert(cx->buffer_last_io <= cx->buffer_size);
		if (cx->buffer_last_io == cx->buffer_size) cx->buffer_last_io = 0;
		max -= (uint32_t)rd;
	}

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

static unsigned char adpcm_do_reset_interval=1;
static unsigned long adpcm_reset_interval=0;
static unsigned long adpcm_counter=0;
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
static unsigned char adpcm_tmp[4096];
#endif
static void load_audio(struct sndsb_ctx *cx,uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	unsigned char FAR *buffer = sb_dma->lin;
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram + 80 - 6;
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
	unsigned char c;
#endif
	unsigned char load=0;
	uint16_t prev[6];
	int rd,i,bufe=0;
	uint32_t how;

	/* caller should be rounding! */
	assert((up_to & 3UL) == 0UL);
	if (up_to >= cx->buffer_size) return;
	if (cx->buffer_size < 32) return;
	if (cx->buffer_last_io == up_to) return;

	if (sb_card->dsp_adpcm > 0 && (wav_16bit || wav_stereo)) return;
	if (max == 0) max = cx->buffer_size/4;
	if (max < 16) return;
	lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	while (max > 0UL) {
		/* the most common "hang" apparently is when IRQ 0 triggers too much
		   and then somehow execution gets stuck here */
		if (irq_0_watchdog < 16UL)
			break;

		if (cx->backwards) {
			if (up_to > cx->buffer_last_io) {
				how = cx->buffer_last_io;
				if (how == 0) how = cx->buffer_size - up_to;
				bufe = 1;
			}
			else {
				how = (cx->buffer_last_io - up_to);
				bufe = 0;
			}
		}
		else {
			if (up_to < cx->buffer_last_io) {
				how = (cx->buffer_size - cx->buffer_last_io); /* from last IO to end of buffer */
				bufe = 1;
			}
			else {
				how = (up_to - cx->buffer_last_io); /* from last IO to up_to */
				bufe = 0;
			}
		}

		if (how > 16384UL)
			how = 16384UL;

		if (how == 0UL)
			break;
		else if (how > max)
			how = max;
		else if (!bufe && how < min)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'L' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'O' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'A' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'D' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		if (cx->buffer_last_io == 0)
			wav_buffer_filepos = wav_position;

		if (sb_card->dsp_adpcm > 0) {
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
			break; /* cut */
#else
			unsigned int src;

			/* FIXME: Would be neat to demonstrate ADPCM playback with backwards DMA */
			if (cx->backwards) break;

			/* 16-bit mode: avoid integer overflow below */
			if (how > 2048)
				how = 2048;

			/* ADPCM encoding does mono 8-bit only */
			if (initial) {
				/* reference byte */
				rd = _dos_xread(wav_fd,buffer + cx->buffer_last_io,1);
				sndsb_encode_adpcm_set_reference(buffer[cx->buffer_last_io],sb_card->dsp_adpcm);
				cx->buffer_last_io++;
				adpcm_counter++;
				wav_position++;
				initial = 0;
				max--;
				how--;
			}

			/* number of samples */
			if (sb_card->dsp_adpcm == ADPCM_4BIT)
				src = how * 2;
			else if (sb_card->dsp_adpcm == ADPCM_2_6BIT)
				src = how * 3;
			else if (sb_card->dsp_adpcm == ADPCM_2BIT)
				src = how * 4;

			if (src > sizeof(adpcm_tmp)) {
				src = sizeof(adpcm_tmp);
				if (sb_card->dsp_adpcm == ADPCM_2_6BIT)
					src -= src % 3;
			}

			rd = read(wav_fd,adpcm_tmp,src);
			if (rd == 0 || rd == -1) {
				wav_position = 0;
				lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);
				rd = read(wav_fd,adpcm_tmp,src);
				if (rd == 0 || rd == -1) {
#if TARGET_MSDOS == 32
					memset(adpcm_tmp,128,src);
#else
					_fmemset(adpcm_tmp,128,src);
#endif
					rd = src;
				}
			}

#ifdef INCLUDE_FX
			if (!sb_card->backwards) fx_proc(adpcm_tmp,rd / wav_bytes_per_sample);
#endif
			wav_position += (uint32_t)rd;
			if (sb_card->dsp_adpcm == ADPCM_4BIT) {
				rd /= 2;
				for (i=0;i < rd;i++) {
					c  = sndsb_encode_adpcm_4bit(adpcm_tmp[(i*2)  ]) << 4;
					c |= sndsb_encode_adpcm_4bit(adpcm_tmp[(i*2)+1]);
					buffer[cx->buffer_last_io+i] = c;

					if (adpcm_reset_interval != 0) {
						if (++adpcm_counter >= adpcm_reset_interval) {
							adpcm_counter -= adpcm_reset_interval;
							sndsb_encode_adpcm_reset_wo_ref(sb_card->dsp_adpcm);
						}
					}
				}
			}
			else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) {
				rd /= 3;
				for (i=0;i < rd;i++) {
					c  = sndsb_encode_adpcm_2_6bit(adpcm_tmp[(i*3)  ],0) << 5;
					c |= sndsb_encode_adpcm_2_6bit(adpcm_tmp[(i*3)+1],0) << 2;
					c |= sndsb_encode_adpcm_2_6bit(adpcm_tmp[(i*3)+2],1) >> 1;
					buffer[cx->buffer_last_io+i] = c;

					if (adpcm_reset_interval != 0) {
						if (++adpcm_counter >= adpcm_reset_interval) {
							adpcm_counter -= adpcm_reset_interval;
							sndsb_encode_adpcm_reset_wo_ref(sb_card->dsp_adpcm);
						}
					}
				}
			}
			else if (sb_card->dsp_adpcm == ADPCM_2BIT) {
				rd /= 4;
				for (i=0;i < rd;i++) {
					c  = sndsb_encode_adpcm_2bit(adpcm_tmp[(i*4)  ]) << 6;
					c |= sndsb_encode_adpcm_2bit(adpcm_tmp[(i*4)+1]) << 4;
					c |= sndsb_encode_adpcm_2bit(adpcm_tmp[(i*4)+2]) << 2;
					c |= sndsb_encode_adpcm_2bit(adpcm_tmp[(i*4)+3]);
					buffer[cx->buffer_last_io+i] = c;

					if (adpcm_reset_interval != 0) {
						if (++adpcm_counter >= adpcm_reset_interval) {
							adpcm_counter -= adpcm_reset_interval;
							sndsb_encode_adpcm_reset_wo_ref(sb_card->dsp_adpcm);
						}
					}
				}
			}
			else {
				abort();
			}

			cx->buffer_last_io += (uint32_t)rd;
#endif
		}
		else {
			uint32_t oa,adj;

			oa = cx->buffer_last_io;
			if (cx->backwards) {
				if (cx->buffer_last_io == 0) {
					cx->buffer_last_io = cx->buffer_size - how;
				}
				else if (cx->buffer_last_io >= how) {
					cx->buffer_last_io -= how;
				}
				else {
					abort();
				}

				adj = (uint32_t)how / wav_bytes_per_sample;
				if (wav_position >= adj) wav_position -= adj;
				else if (wav_position != 0UL) wav_position = 0;
				else {
					wav_position = lseek(wav_fd,0,SEEK_END);
					if (wav_position >= adj) wav_position -= adj;
					else if (wav_position != 0UL) wav_position = 0;
					wav_position /= wav_bytes_per_sample;
				}

				lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);
			}

			assert(cx->buffer_last_io <= cx->buffer_size);
#if TARGET_MSDOS == 32
			rd = _dos_xread(wav_fd,buffer + cx->buffer_last_io,how);
#else
            {
                uint32_t o;

                o  = (uint32_t)FP_SEG(buffer) << 4UL;
                o += (uint32_t)FP_OFF(buffer);
                o += cx->buffer_last_io;
                rd = _dos_xread(wav_fd,MK_FP(o >> 4UL,o & 0xFUL),how);
            }
#endif
			if (rd == 0 || rd == -1) {
				if (!cx->backwards) {
					wav_position = 0;
					lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);
					rd = _dos_xread(wav_fd,buffer + cx->buffer_last_io,how);
					if (rd == 0 || rd == -1) {
						/* hmph, fine */
#if TARGET_MSDOS == 32
						memset(buffer+cx->buffer_last_io,128,how);
#else
						_fmemset(buffer+cx->buffer_last_io,128,how);
#endif
						rd = (int)how;
					}
				}
				else {
					rd = (int)how;
				}
			}

			assert((cx->buffer_last_io+((uint32_t)rd)) <= cx->buffer_size);
#ifdef INCLUDE_FX
			if (!sb_card->backwards) fx_proc(buffer + cx->buffer_last_io,rd / wav_bytes_per_sample);
#endif
			if (sb_card->audio_data_flipped_sign) {
				if (wav_16bit)
					for (i=0;i < (rd-1);i += 2) buffer[cx->buffer_last_io+i+1] ^= 0x80;
				else
					for (i=0;i < rd;i++) buffer[cx->buffer_last_io+i] ^= 0x80;
			}

			if (!cx->backwards) {
				cx->buffer_last_io += (uint32_t)rd;
				wav_position += (uint32_t)rd / wav_bytes_per_sample;
			}
		}

		assert(cx->buffer_last_io <= cx->buffer_size);
		if (!cx->backwards) {
			if (cx->buffer_last_io == cx->buffer_size) cx->buffer_last_io = 0;
		}
		max -= (uint32_t)rd;
	}

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

static void rec_vu(uint32_t pos) {
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram + (vga_state.vga_width * (vga_state.vga_height - 1));
	const unsigned int leeway = 256;
	unsigned int x,L=0,R=0,max;
	uint16_t sample;

	if (pos == (~0UL)) {
#if defined(TARGET_PC98)
		for (x=0;x < (unsigned int)vga_state.vga_width;x++) {
            wr[x       ] = 0x20;
            wr[x+0x1000] = 0x25;
        }
#else
		for (x=0;x < (unsigned int)vga_state.vga_width;x++)
			wr[x] = 0x1E00 | 177;
#endif

		return;
	}

	/* caller should be sample-aligning the position! */
	assert((pos & 3UL) == 0UL);

	if ((pos+(leeway*wav_bytes_per_sample)) < sb_card->buffer_size) {
		if (wav_16bit) {
			int16_t FAR *ptr = (int16_t FAR*)(sb_dma->lin + pos);
			max = 32767;
			if (wav_stereo) {
				/* 16-bit PCM stereo */
				for (x=0;x < leeway;x++) {
					sample = ptr[x*2];
					if (sb_card->audio_data_flipped_sign) sample = abs((uint16_t)sample - 32768);
					else sample = abs((int16_t)sample);
					if (L < sample) L = sample;

					sample = ptr[x*2 + 1];
					if (sb_card->audio_data_flipped_sign) sample = abs((uint16_t)sample - 32768);
					else sample = abs((int16_t)sample);
					if (R < sample) R = sample;
				}
			}
			else {
				/* 16-bit PCM mono */
				for (x=0;x < leeway;x++) {
					sample = ptr[x];
					if (sb_card->audio_data_flipped_sign) sample = abs((uint16_t)sample - 32768);
					else sample = abs((int16_t)sample);
					if (L < sample) L = sample;
				}
			}
		}
		else {
			max = 127;
			if (wav_stereo) {
				/* 8-bit PCM stereo */
				for (x=0;x < leeway;x++) {
					sample = sb_dma->lin[x*2 + pos];
					if (sb_card->audio_data_flipped_sign) sample = abs((signed char)sample);
					else sample = abs((int)sample - 128);
					if (L < sample) L = sample;

					sample = sb_dma->lin[x*2 + 1 + pos];
					if (sb_card->audio_data_flipped_sign) sample = abs((signed char)sample);
					else sample = abs((int)sample - 128);
					if (R < sample) R = sample;
				}
			}
			else {
				/* 8-bit PCM mono */
				for (x=0;x < leeway;x++) {
					sample = sb_dma->lin[x + pos];
					if (sb_card->audio_data_flipped_sign) sample = abs((signed char)sample);
					else sample = abs((int)sample - 128);
					if (L < sample) L = sample;
				}
			}
		}

		L = (unsigned int)(((unsigned long)L * 80UL) / (unsigned long)max);
		if (wav_stereo)
			R = (unsigned int)(((unsigned long)R * 80UL) / (unsigned long)max);
		else
			R = L;

		if (L > 80) L = 80;
		if (R > 80) R = 80;
		for (x=0;x < 80;x++) {
			if (x < L && x < R)
				wr[x] = 0x0F00 | 219;
			else if (x < L)
				wr[x] = 0x0F00 | 223;
			else if (x < R)
				wr[x] = 0x0F00 | 220;
			else
				wr[x] = 0x0F00 | 32;
		}
	}
}

#define DMA_WRAP_DEBUG

static void wav_idle() {
	uint32_t pos;
#ifdef DMA_WRAP_DEBUG
	uint32_t pos2;
#endif

	if (!wav_playing || wav_fd < 0)
		return;

	/* if we're playing without an IRQ handler, then we'll want this function
	 * to poll the sound card's IRQ status and handle it directly so playback
	 * continues to work. if we don't, playback will halt on actual Creative
	 * Sound Blaster 16 hardware until it gets the I/O read to ack the IRQ */
	if (!dont_sb_idle) sndsb_main_idle(sb_card);

	_cli();
#ifdef DMA_WRAP_DEBUG
	pos2 = sndsb_read_dma_buffer_position(sb_card);
#endif
	pos = sndsb_read_dma_buffer_position(sb_card);
#ifdef DMA_WRAP_DEBUG
	if (sb_card->backwards) {
		if (pos2 < 0x1000 && pos >= (sb_card->buffer_size-0x1000)) {
			/* normal DMA wrap-around, no problem */
		}
		else {
			if (pos > pos2)	fprintf(stderr,"DMA glitch! 0x%04lx 0x%04lx\n",pos,pos2);
			else		pos = max(pos,pos2);
		}
	}
	else {
		if (pos < 0x1000 && pos2 >= (sb_card->buffer_size-0x1000)) {
			/* normal DMA wrap-around, no problem */
		}
		else {
			if (pos < pos2)	fprintf(stderr,"DMA glitch! 0x%04lx 0x%04lx\n",pos,pos2);
			else		pos = min(pos,pos2);
		}
	}
#endif
	pos &= (~3UL); /* round down */
	_sti();

	if (wav_record) {
		/* read audio samples just behind DMA and render as VU meter */
		rec_vu(pos);

		/* write to disk */
		save_audio(sb_card,pos,min(wav_sample_rate/4,4096)/*min*/,
			sb_card->buffer_size/2/*max*/,0/*first block*/);
	}
	else {
		/* load from disk */
		load_audio(sb_card,pos,min(wav_sample_rate/8,4096)/*min*/,
			sb_card->buffer_size/4/*max*/,0/*first block*/);
	}
}

static void update_cfg();

static unsigned long playback_live_position() {
	signed long xx = (signed long)sndsb_read_dma_buffer_position(sb_card);

	if (sb_card->backwards) {
		if (sb_card->buffer_last_io >= (unsigned long)xx)
			xx += sb_card->buffer_size;

		xx -= sb_card->buffer_size; /* because we started from the end */
	}
	else if (wav_record) {
		/* save_audio() is called to up to just behind the DMA position.
		 * if buffer_last_io wrapped around and it is ahead of the DMA playback position,
		 * then wav_buffer_filepos was updated. */
		if (sb_card->buffer_last_io > (unsigned long)xx)
			xx += sb_card->buffer_size;
	}
	else {
		/* load_audio() is called to always load audio as far ahead as possible.
		 * if buffer_last_io wrapped around and it is behind the DMA playback position,
		 * then wav_buffer_filepos was updated. */
		if (sb_card->buffer_last_io <= (unsigned long)xx)
			xx -= sb_card->buffer_size;
	}

	if (sb_card->dsp_adpcm == ADPCM_4BIT) xx *= 2;
	else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) xx *= 3;
	else if (sb_card->dsp_adpcm == ADPCM_2BIT) xx *= 4;
	xx += wav_buffer_filepos * wav_bytes_per_sample;
	if (xx < 0) xx += wav_data_length;
	return ((unsigned long)xx) / wav_bytes_per_sample;
}

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static unsigned char dos_vm_yield_counter = 0;
#endif

static uint32_t last_dma_position = 1;
static void ui_anim(int force) {
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram + 10;
	const unsigned int width = 70 - 4;
	unsigned long temp,rem,rem2;
	unsigned int pH,pM,pS,pSS;
	const char *msg = "DMA:";
	unsigned int i,cc;

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	/* Under Windows, yield every so often. Under Windows NT/2000/XP this prevents
	 * NTVDM.EXE from pegging the CPU at 100%, allowing the rest of the OS to run
	 * smoother. */
	if (windows_mode == WINDOWS_NT || windows_mode == WINDOWS_ENHANCED) {
		unsigned char do_yield = 1;

		if (sb_card != NULL && wav_playing)
			do_yield = sndsb_recommend_vm_wait(sb_card);

		if (do_yield) {
			if (dos_vm_yield_counter == 0)
				dos_vm_yield();

			if (++dos_vm_yield_counter >= 10)
				dos_vm_yield_counter = 0;
		}
	}
#endif

	wav_idle();

	rem = 0;
	if (sb_card != NULL) rem = sndsb_read_dma_buffer_position(sb_card);
	if (force || last_dma_position != rem) {
		last_dma_position = rem;
		if (rem != 0) rem--;
		rem = (unsigned int)(((unsigned long)rem * (unsigned long)width) / (unsigned long)sb_card->buffer_size);

		rem2 = 0;
		if (sb_card != NULL) rem2 = sb_card->buffer_last_io;
		if (rem2 != 0) rem2--;
		rem2 = (unsigned int)(((unsigned long)rem2 * (unsigned long)width) / (unsigned long)sb_card->buffer_size);

#if defined(TARGET_PC98)
		while (*msg) {
            wr[0x0000] = (uint16_t)(*msg++);
            wr[0x1000] = 0x21;
            wr++;
        }
		for (i=0;i < width;i++) {
			if (i == rem2) {
				wr[i] = (uint16_t)(i == rem ? 'x' : (i < rem ? '-' : ' '));
                wr[i+0x1000] = 0xE5;
            }
			else {
				wr[i] = (uint16_t)(i == rem ? 'x' : (i < rem ? '-' : ' '));
                wr[i+0x1000] = 0xC1;
            }
		}
#else
		while (*msg) *wr++ = (uint16_t)(*msg++) | 0x1E00;
		for (i=0;i < width;i++) {
			if (i == rem2)
				wr[i] = (uint16_t)(i == rem ? 'x' : (i < rem ? '-' : ' ')) | 0x7000;
			else
				wr[i] = (uint16_t)(i == rem ? 'x' : (i < rem ? '-' : ' ')) | 0x1E00;
		}
#endif

		if (wav_playing) temp = playback_live_position();
		else temp = wav_position;
		pSS = (unsigned int)(((temp % wav_sample_rate) * 100UL) / wav_sample_rate);
		temp /= wav_sample_rate;
		pS = (unsigned int)(temp % 60UL);
		pM = (unsigned int)((temp / 60UL) % 60UL);
		pH = (unsigned int)((temp / 3600UL) % 24UL);

		msg = temp_str;
		sprintf(temp_str,"%ub %s %5luHz @%c%u:%02u:%02u.%02u",wav_16bit ? 16 : 8,wav_stereo ? "ST" : "MO",
			wav_sample_rate,wav_playing ? (wav_record ? 'r' : 'p') : 's',pH,pM,pS,pSS);

#if defined(TARGET_PC98)
		for (wr=vga_state.vga_alpha_ram+(80*1),cc=0;cc < 29 && *msg != 0;cc++) {
            wr[0x0000] = ((unsigned char)(*msg++));
            wr[0x1000] = 0xE1;
            wr++;
        }
		for (;cc < 29;cc++) {
            wr[0x0000] = 0x20;
            wr[0x1000] = 0xE1;
            wr++;
        }
		msg = sndsb_dspoutmethod_str[sb_card->dsp_play_method];
		rem = sndsb_dsp_out_method_supported(sb_card,wav_sample_rate,wav_stereo,wav_16bit) ? 0x81 : 0x21;
		for (;cc < 36 && *msg != 0;cc++) {
            wr[0x0000] = ((unsigned char)(*msg++));
            wr[0x1000] = rem;
            wr++;
        }
		if (cc < 36) {
			if (rem == 0x21) {
                wr[0x0000] = '?';
                wr[0x1000] = 0xC1;
                wr++;
            }
			else if (sb_card->reason_not_supported) {
                wr[0x0000] = '?';
                wr[0x1000] = 0x81;
                wr++;
            }
		}
		for (;cc < 36;cc++) {
            wr[0x0000] = 0x20;
            wr[0x1000] = 0xE1;
            wr++;
        }

		if (sb_card->dsp_adpcm != 0) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			msg = sndsb_adpcm_mode_str[sb_card->dsp_adpcm];
			for (;cc < 52 && *msg != 0;cc++) {
                wr[0x0000] = ((unsigned char)(*msg++));
                wr[0x1000] = 0xE1;
                wr++;
            }
#endif
		}
		else if (sb_card->audio_data_flipped_sign) {
			msg = "[flipsign]";
			for (;cc < 52 && *msg != 0;cc++) {
                wr[0x0000] = ((unsigned char)(*msg++));
                wr[0x1000] = 0xE1;
                wr++;
            }
		}

		/* fill */
		for (;cc < 57;cc++) {
            wr[0x0000] = 0x20;
            wr[0x1000] = 0xE1;
            wr++;
        }

		msg = temp_str;
		temp = sndsb_read_dma_buffer_position(sb_card);
		sprintf(temp_str,"%05lx/%05lx @%08lx",
			(unsigned long)temp,
			(unsigned long)sb_card->buffer_size,
            (unsigned long)sb_card->buffer_phys + (unsigned long)temp);
		for (;cc < 80 && *msg != 0;cc++) {
            wr[0x0000] = ((unsigned char)(*msg++));
            wr[0x1000] = 0xE1;
            wr++;
        }

		/* finish */
		for (;cc < 80;cc++) {
            wr[0x0000] = 0x20;
            wr[0x1000] = 0xE1;
            wr++;
        }
#else
		for (wr=vga_state.vga_alpha_ram+(80*1),cc=0;cc < 29 && *msg != 0;cc++) *wr++ = 0x1F00 | ((unsigned char)(*msg++));
		for (;cc < 29;cc++) *wr++ = 0x1F20;
		msg = sndsb_dspoutmethod_str[sb_card->dsp_play_method];
		rem = sndsb_dsp_out_method_supported(sb_card,wav_sample_rate,wav_stereo,wav_16bit) ? 0x1A : 0x1C;
		for (;cc < 36 && *msg != 0;cc++) *wr++ = (rem << 8) | ((unsigned char)(*msg++));
		if (cc < 36) {
			if (rem == 0x1C) *wr++ = 0x1E00 + '?';
			else if (sb_card->reason_not_supported) *wr++ = 0x1A00 + '?';
		}
		for (;cc < 36;cc++) *wr++ = 0x1F20;

		if (sb_card->dsp_adpcm != 0) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			msg = sndsb_adpcm_mode_str[sb_card->dsp_adpcm];
			for (;cc < 52 && *msg != 0;cc++) *wr++ = 0x1F00 | ((unsigned char)(*msg++));
#endif
		}
		else if (sb_card->audio_data_flipped_sign) {
			msg = "[flipsign]";
			for (;cc < 52 && *msg != 0;cc++) *wr++ = 0x1F00 | ((unsigned char)(*msg++));
		}

		/* fill */
		for (;cc < 57;cc++) *wr++ = 0x1F20;

		msg = temp_str;
		temp = sndsb_read_dma_buffer_position(sb_card);
		sprintf(temp_str,"%05lx/%05lx @%08lx",
			(unsigned long)temp,
			(unsigned long)sb_card->buffer_size,
            (unsigned long)sb_card->buffer_phys + (unsigned long)temp);
		for (;cc < 80 && *msg != 0;cc++) *wr++ = 0x1F00 | ((unsigned char)(*msg++));

		/* finish */
		for (;cc < 80;cc++) *wr++ = 0x1F20;
#endif
	}

	irq_0_watchdog_ack();

	{
		static const unsigned char anims[] = {'-','/','|','\\'};
		if (++animator >= 4) animator = 0;
		wr = vga_state.vga_alpha_ram + 80 + 79;
		*wr = anims[animator] | 0x1E00;
	}
}

static void close_wav() {
	if (wav_fd >= 0) {
		close(wav_fd);
		wav_fd = -1;
	}
}

static void open_wav() {
	char tmp[64];

	if (wav_fd < 0) {
		uint32_t riff_length,scan,len;
		unsigned char fmt=0;

		wav_position = 0;
		wav_data_offset = 0;
		wav_data_length = 0;
		wav_sample_rate = 8000;
		wav_stereo = 0;
		wav_16bit = 0;
		wav_bytes_per_sample = 1;
		if (strlen(wav_file) < 1) return;

		wav_fd = open(wav_file,O_RDONLY|O_BINARY);
		if (wav_fd < 0) return;

		if (lseek(wav_fd,0,SEEK_SET) != 0) goto fail;

		/* first, the RIFF:WAVE chunk */
		/* 3 DWORDS: 'RIFF' <length> 'WAVE' */
		if (read(wav_fd,tmp,12) != 12) goto fail;
		if (memcmp(tmp+0,"RIFF",4) || memcmp(tmp+8,"WAVE",4)) goto fail;

		scan = 12;
		riff_length = *((uint32_t*)(tmp+4));
		if (riff_length <= 44) goto fail;
		riff_length -= 4; /* the length includes the 'WAVE' marker */

		while ((scan+8UL) <= riff_length) {
			/* RIFF chunks */
			/* 2 WORDS: <fourcc> <length> */
			if (lseek(wav_fd,scan,SEEK_SET) != scan) goto fail;
			if (read(wav_fd,tmp,8) != 8) goto fail;
			len = *((uint32_t*)(tmp+4));

			/* process! */
			if (!memcmp(tmp,"fmt ",4)) {
				if (len >= 16 && len <= sizeof(tmp)) {
					if (read(wav_fd,tmp,len) == len) {
#if 0
						typedef struct tWAVEFORMATEX {
							WORD  wFormatTag;               /* +0 */
							WORD  nChannels;                /* +2 */
							DWORD nSamplesPerSec;           /* +4 */
							DWORD nAvgBytesPerSec;          /* +8 */
							WORD  nBlockAlign;              /* +12 */
							WORD  wBitsPerSample;           /* +14 */
						} WAVEFORMATEX;
#endif
						wav_sample_rate = *((uint32_t*)(tmp + 4));
						wav_stereo = *((uint16_t*)(tmp + 2)) > 1;
						wav_16bit = *((uint16_t*)(tmp + 14)) > 8;
						wav_bytes_per_sample = (wav_stereo ? 2 : 1) * (wav_16bit ? 2 : 1);
						fmt = 1;
					}
				}
			}
			else if (!memcmp(tmp,"data",4)) {
				wav_data_offset = scan + 8UL;
				wav_data_length = len;
			}

			/* next! */
			scan += len + 8UL;
		}

		if (wav_data_length == 0UL || fmt == 0) goto fail;
	}

	update_cfg();
	return;
fail:
	/* assume wav_fd >= 0 */
	close(wav_fd);
	wav_fd = -1;
}

static void open_wav_unique_name() {
	int patience = 1000;
	char *p,*q;

	if (wav_fd >= 0) close(wav_fd);
	wav_fd = -1;

	do {
		p = strrchr(wav_file,'.');
		if (p == NULL) p = wav_file + strlen(wav_file) - 1;
		else if (p > wav_file) p--;

		if (p == wav_file) {
			strcpy(wav_file,"untitled.wav");
		}
		else if (p >= wav_file) {
			if (!isdigit(*p)) {
				*p = '0';
				break;
			}
			else if (*p == '9') {
				*p = '0';
				/* carry the 1 */
				for (q=p-1;q >= wav_file;q--) {
					if (isdigit(*q)) {
						if (*q == '9') {
							*q = '0';
							continue;
						}
						else {
							(*q)++;
						}
					}
					else {
						*q = '0';
						break;
					}
				}
			}
			else {
				(*p)++;
			}
		}

		/* if the file already exists, then reject the name */
		if ((wav_fd = open(wav_file,O_RDONLY|O_BINARY)) >= 0) {
			close(wav_fd);
			wav_fd = -1;
		}
		/* unless we are able to create the file, we don't want it */
		else if ((wav_fd = open(wav_file,O_RDWR|O_BINARY|O_CREAT|O_EXCL|O_TRUNC,0644)) >= 0) {
			break; /* works for me */
		}

		if (--patience == 0)
			break;
	} while (1);
}

static void free_dma_buffer() {
    if (sb_dma != NULL) {
        dma_8237_free_buffer(sb_dma);
        sb_dma = NULL;
    }
}

static void realloc_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);

    if (ch >= 4)
        choice = sndsb_recommended_16bit_dma_buffer_size(sb_card,buffer_limit);
    else
        choice = sndsb_recommended_dma_buffer_size(sb_card,buffer_limit);

    do {
        if (ch >= 4)
            sb_dma = dma_8237_alloc_buffer_dw(choice,16);
        else
            sb_dma = dma_8237_alloc_buffer_dw(choice,8);

        if (sb_dma == NULL) choice -= 4096UL;
    } while (sb_dma == NULL && choice > 4096UL);

    if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
        return;
    if (sb_dma == NULL)
        return;
}

static void begin_play() {
	unsigned long choice_rate;

	if (wav_playing)
		return;

    if (sb_dma == NULL)
        realloc_dma_buffer();

    {
        int8_t ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);
        if (ch >= 0) {
            if (sb_dma->dma_width != (ch >= 4 ? 16 : 8))
                realloc_dma_buffer();
            if (sb_dma == NULL)
                return;
        }
    }

    if (sb_dma != NULL) {
        if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
            return;
    }

	/* sorry, lock out unimplemented modes */
	{
		const char *why = NULL;
		int i;

		if (sb_card->backwards && sb_card->dsp_adpcm != 0)
			why = "Backwards playback and ADPCM not implemented";
		else if (sb_card->backwards && wav_record) /* NTS: When we do support this, code will need to reverse audio before writing to WAV */
			why = "Backwards recording not implemented";

		if (why) {
			struct vga_msg_box box;
			vga_msg_box_create(&box,why,0,0);
			while (1) {
				ui_anim(0);
				if (kbhit()) {
					i = vga_getch();
					if (i == 13 || i == 27) break;
				}
			}
			vga_msg_box_destroy(&box);

			return;
		}
	}

	if (wav_record)
		open_wav_unique_name();

	if (wav_fd < 0)
		return;

	choice_rate = sample_rate_timer_clamp ? wav_sample_rate_by_timer : wav_sample_rate;
	if (sb_card->goldplay_mode) {
		if (goldplay_samplerate_choice == GOLDRATE_DOUBLE)
			choice_rate *= 2;
		else if (goldplay_samplerate_choice == GOLDRATE_MAX) {
			/* basically the maximum the DSP will run at */
			if (sb_card->dsp_play_method <= SNDSB_DSPOUTMETHOD_200)
				choice_rate = 22050;
			else if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_201)
				choice_rate = 44100;
			else if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx)
				choice_rate = wav_stereo ? 22050 : 44100;
			else if (sb_card->pnp_name == NULL)
				choice_rate = 44100; /* Most clones and non-PnP SB16 cards max out at 44.1KHz */
			else
				choice_rate = 48000; /* SB16 ViBRA (PnP-era) cards max out at 48Khz */
		}
	}

	update_cfg();
	irq_0_watchdog_reset();
	if (!sndsb_prepare_dsp_playback(sb_card,choice_rate,wav_stereo,wav_16bit))
		return;

	sndsb_setup_dma(sb_card);

	if (!wav_record)
		load_audio(sb_card,sb_card->buffer_size/2,0/*min*/,0/*max*/,1/*first block*/);
	else { /* create RIFF structure for recording */
		uint32_t dw;
		uint16_t w;

		write(wav_fd,"RIFF",4);
		dw = 44 - 8;			write(wav_fd,&dw,4);
		write(wav_fd,"WAVEfmt ",8);
		dw = 0x10;			write(wav_fd,&dw,4);

		w = 1; /* PCM */		write(wav_fd,&w,2);
		w = wav_stereo ? 2 : 1;		write(wav_fd,&w,2);
		dw = wav_sample_rate;		write(wav_fd,&dw,4);
		dw = wav_sample_rate * wav_bytes_per_sample;
						write(wav_fd,&dw,4);
		w = wav_bytes_per_sample;	write(wav_fd,&w,2);
		w = wav_16bit ? 16 : 8;		write(wav_fd,&w,2);

		write(wav_fd,"data",4);
		dw = 0;				write(wav_fd,&dw,4);

		wav_bytes_per_sample = (wav_stereo ? 2 : 1) * (wav_16bit ? 2 : 1);
		wav_buffer_filepos = 0;
		wav_data_offset = 44;
		wav_data_length = 0;
		wav_position = 0;
	}

	/* make sure the IRQ is acked */
	if (sb_card->irq >= 8) {
		p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
		p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
	}
	else if (sb_card->irq >= 0) {
		p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
	}
	if (sb_card->irq >= 0)
		p8259_unmask(sb_card->irq);

	if (!sndsb_begin_dsp_playback(sb_card))
		return;

	_cli();
	if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
		unsigned long nr = (unsigned long)sb_card->buffer_rate * 2UL;
		write_8254_system_timer(T8254_REF_CLOCK_HZ / nr);
		irq_0_count = 0;
		irq_0_adv = 182UL;		/* 18.2Hz */
		irq_0_max = nr * 10UL;		/* sample rate */
	}
	else if (sb_card->goldplay_mode) {
		write_8254_system_timer(wav_sample_rate_by_timer_ticks);
		irq_0_count = 0;
		irq_0_adv = 182UL;		/* 18.2Hz */
		irq_0_max = wav_sample_rate_by_timer * 10UL;
	}
	wav_playing = 1;
	_sti();
}

static void stop_play() {
	if (!wav_playing) return;
	draw_irq_indicator();
	if (!wav_record) {
		wav_position = playback_live_position();
		wav_position -= wav_position % (unsigned long)wav_bytes_per_sample;
	}

	_cli();
	if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT || sb_card->goldplay_mode) {
		irq_0_count = 0;
		irq_0_adv = 1;
		irq_0_max = 1;
		write_8254_system_timer(0); /* restore 18.2 tick/sec */
	}
	sndsb_stop_dsp_playback(sb_card);
	wav_playing = 0;
	_sti();

	ui_anim(1);
	if (wav_fd >= 0 && wav_record) {
		uint32_t dw;

		unsigned long len = lseek(wav_fd,0,SEEK_END);

		lseek(wav_fd,4,SEEK_SET);
		dw = len - 8; write(wav_fd,&dw,4);

		lseek(wav_fd,40,SEEK_SET);
		dw = len - 44; write(wav_fd,&dw,4);

		close(wav_fd);
		wav_fd = -1;

		wav_fd = open(wav_file,O_RDONLY|O_BINARY);
		wav_data_offset = 44;
		wav_data_length = len - 44;
		wav_position = 0;
		rec_vu(~0UL);
	}
}

static void vga_write_until(unsigned int x) {
	while (vga_state.vga_pos_x < x)
		vga_writec(' ');
}

static int change_param_idx = 0;

/* NTS: the 13000, 15000, 23000 values come from Creative documentation */
static const unsigned short param_preset_rates[] = {
	4000,	5512,	5675,	6000,
	8000,	11025,	11111,	12000,
	13000,	15000,	16000,	22050,
	22222,	23000,	24000,  32000,
	44100,	48000,	54000,	58000};

#if TARGET_MSDOS == 32
static const char *dos32_irq_0_warning =
	"WARNING: The timer is made to run at the sample rate. Depending on your\n"
	"         DOS extender there may be enough overhead to overwhelm the CPU\n"
	"         and possibly cause a crash.\n"
	"         Enable?";
#endif

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static int change_alias_idx = 0;
static unsigned char dsp_alias_had_warned = 0;
static const char *dsp_alias_warning =
	"WARNING: DSP alias port will not work unless you are using an original\n"
	"         Sound Blaster (DSP 1.xx or 2.xx) and NOT a clone. See the\n"
	"         README file for more information.\n"
	"         Enable anyway?";
#endif

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static void measure_dsp_busy_cycle() {
	unsigned long time1 = 0,time2 = 0,tlimit;
	unsigned char timehits = 0;
	t8254_time_t pr,cr,dl;
	unsigned char pc,cc;
	unsigned int i;
	int c;

	vga_write_color(0x07);
	vga_clear();
	vga_moveto(0,0);
	vga_write("Testing DSP busy cycle... please wait\n");

	_cli();

	pc = inp(sb_card->baseio+SNDSB_BIO_DSP_WRITE_STATUS);
	pr = read_8254(0);
	tlimit = t8254_us2ticks(1000000);

	time1 = 0;
	while (time1 < tlimit) {
		for (i=0;i < 1024;i++) {
			cc = inp(sb_card->baseio+SNDSB_BIO_DSP_WRITE_STATUS);
			if ((cc&0x80) != (pc&0x80)) break;
			pc = cc;
		}

		cr = read_8254(0);
		dl = pr - cr; /* we expect rollover. remember the 8254 counts DOWN */
		pr = cr;
		time1 += dl;

		if ((cc&0x80) != (pc&0x80)) {
			break;
		}
	}

	time1 = 0;
	while (time1 < tlimit) {
		for (i=0;i < 1024;i++) {
			cc = inp(sb_card->baseio+SNDSB_BIO_DSP_WRITE_STATUS);
			if ((cc&0x80) != (pc&0x80)) break;
			pc = cc;
		}

		cr = read_8254(0);
		dl = pr - cr; /* we expect rollover. remember the 8254 counts DOWN */
		pr = cr;
		time1 += dl;

		if ((cc&0x80) != (pc&0x80)) {
			timehits |= 1;
			break;
		}
	}

	pc = cc;
	pr = cr;
	while (time2 < tlimit) {
		for (i=0;i < 1024;i++) {
			cc = inp(sb_card->baseio+SNDSB_BIO_DSP_WRITE_STATUS);
			if ((cc&0x80) != (pc&0x80)) break;
			pc = cc;
		}

		cr = read_8254(0);
		dl = pr - cr; /* we expect rollover. remember the 8254 counts DOWN */
		pr = cr;
		time2 += dl;

		if ((cc&0x80) != (pc&0x80)) {
			timehits |= 2;
			break;
		}
	}

	_sti();

	vga_write_color(0x07);
	vga_write("DSP busy cycle measurements:\n");

	if (timehits & 1) {
		vga_write("Time to first transition: ");
		sprintf(temp_str,"%.6f sec\n",(double)time1 / T8254_REF_CLOCK_HZ);
		vga_write(temp_str);
	}
	else {
		vga_write("Time to first transition could not be measured\n");
	}

	if (timehits & 2) {
		vga_write("Time to second transition: ");
		sprintf(temp_str,"%.6f sec\n",(double)time2 / T8254_REF_CLOCK_HZ);
		vga_write(temp_str);
	}
	else {
		vga_write("Time to second transition could not be measured\n");
	}

	vga_write("\n");
	vga_write("Explanation: On some Creative Sound Blaster cards the DSP's busy bit (port 22Ch)\n");
	vga_write("             cycles on/off by itself. This can be seen by software when polling.\n");
	vga_write_sync();

	do {
		c = vga_getch();
	} while (!(c == 13 || c == 27));
}

static void change_alias_menu() {
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	unsigned char selector=change_alias_idx;
	VGA_ALPHA_PTR vga;
	unsigned int cc;
	char tmp[128];

	while (loop) {
		if (redraw || uiredraw) {
			_cli();
			if (redraw) {
#if defined(TARGET_PC98)
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) {
                    vga[0x0000] = 0x20;
                    vga[0x1000] = 0x25;
                    vga++;
                }
#else
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
#endif
				ui_anim(1);
			}
			vga_moveto(0,4);

			vga_write_color(selector == 0 ? 0x70 : 0x1F);
			sprintf(tmp,"DSP alias:     %u",sb_card->dsp_alias_port);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_write_sync();
			_sti();
			redraw = 0;
			uiredraw = 0;
		}

		if (kbhit()) {
			int c = vga_getch();

			if (c == 27 || c == 13)
				loop = 0;
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
//				if (selector > 0) selector--;
//				else selector=0;
				uiredraw=1;
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
//				if (selector < -1) selector++;
//				else selector=0;
				uiredraw=1;
			}
			else if (c == VGATTY_LEFT_ARROW) { /* left arrow */
				switch (selector) {
					case 0:	/* sample rate */
						sb_card->dsp_alias_port ^= 1;
                        sndsb_update_dspio(sb_card);
                        break;
				};
				update_cfg();
				uiredraw=1;
			}
			else if (c == VGATTY_RIGHT_ARROW) { /* right arrow */
				switch (selector) {
					case 0:	/* sample rate */
						sb_card->dsp_alias_port ^= 1;
                        sndsb_update_dspio(sb_card);
						break;
				};
				update_cfg();
				uiredraw=1;
			}
		}

		ui_anim(0);
	}

	if (!dsp_alias_had_warned && sb_card->dsp_alias_port) {
		/* NOTE TO SELF: It can overwhelm the UI in DOSBox too, but DOSBox seems able to
		   recover if you manage to hit CTRL+F12 to speed up the CPU cycles in the virtual machine.
		   On real hardware, even with the recovery method the machine remains hung :( */
		if (confirm_yes_no_dialog(dsp_alias_warning))
			dsp_alias_had_warned = 1;
		else {
			sb_card->dsp_alias_port = 0;
            sndsb_update_dspio(sb_card);
        }
	}

	change_param_idx = selector;
}
#endif

static void change_param_menu() {
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	unsigned char selector=change_param_idx;
#if TARGET_MSDOS == 32
	unsigned char oldmethod=sb_card->dsp_play_method;
#endif
	unsigned int cc,ra;
	VGA_ALPHA_PTR vga;
	char tmp[128];

	while (loop) {
		if (redraw || uiredraw) {
			_cli();
			if (redraw) {
#if defined(TARGET_PC98)
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) {
                    vga[0x0000] = 0x20;
                    vga[0x1000] = 0x25;
                    vga++;
                }
#else
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
#endif
				ui_anim(1);
			}
			vga_moveto(0,4);

			vga_write_color(selector == 0 ? 0x70 : 0x1F);
			sprintf(tmp,"Sample rate:   %luHz",wav_sample_rate);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_write_color(selector == 1 ? 0x70 : 0x1F);
			sprintf(tmp,"Channels:      %s",wav_stereo ? "stereo" : "mono");
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_write_color(selector == 2 ? 0x70 : 0x1F);
			sprintf(tmp,"Bits:          %u-bit",wav_16bit ? 16 : 8);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_write_color(selector == 3 ? 0x70 : 0x1F);
			vga_write(  "Translation:   ");
			if (sb_card->dsp_adpcm > 0) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
				vga_write(sndsb_adpcm_mode_str[sb_card->dsp_adpcm]);
#endif
			}
			else if (sb_card->audio_data_flipped_sign)
				vga_write("Flip sign");
			else
				vga_write("None");
			vga_write_until(30);
			vga_write("\n");

			vga_write_color(selector == 4 ? 0x70 : 0x1F);
			vga_write(  "DSP mode:      ");
			if (sndsb_dsp_out_method_supported(sb_card,wav_sample_rate,wav_stereo,wav_16bit))
				vga_write_color(selector == 4 ? 0x70 : 0x1F);
			else
				vga_write_color(selector == 4 ? 0x74 : 0x1C);
			vga_write(sndsb_dspoutmethod_str[sb_card->dsp_play_method]);
			vga_write_until(30);
			vga_write("\n");

			sprintf(tmp,"%u",sb_card->dsp_direct_dac_read_after_command);
			vga_write_color(selector == 5 ? 0x70 : 0x1F);
			vga_write(  "DDAC read aft: ");
			if (sndsb_dsp_out_method_supported(sb_card,wav_sample_rate,wav_stereo,wav_16bit))
				vga_write_color(selector == 5 ? 0x70 : 0x1F);
			else
				vga_write_color(selector == 5 ? 0x74 : 0x1C);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			sprintf(tmp,"%u",sb_card->dsp_direct_dac_poll_retry_timeout);
			vga_write_color(selector == 6 ? 0x70 : 0x1F);
			vga_write(  "DDAC poll retry");
			if (sndsb_dsp_out_method_supported(sb_card,wav_sample_rate,wav_stereo,wav_16bit))
				vga_write_color(selector == 6 ? 0x70 : 0x1F);
			else
				vga_write_color(selector == 6 ? 0x74 : 0x1C);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			sprintf(tmp,"%s",sb_card->backwards?"Backwards":"Forwards");
			vga_write_color(selector == 7 ? 0x70 : 0x1F);
			vga_write(  "Direction:     ");
			if (sndsb_dsp_out_method_supported(sb_card,wav_sample_rate,wav_stereo,wav_16bit))
				vga_write_color(selector == 7 ? 0x70 : 0x1F);
			else
				vga_write_color(selector == 7 ? 0x74 : 0x1C);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_moveto(0,13);
#if defined(TARGET_PC98)
			vga_write_color(0x10);
#else
			vga_write_color(0x1F);
#endif
			vga_write_until(80);
			vga_write("\n");
			vga_write_until(80);
			vga_write("\n");
			vga_write_until(80);
			vga_write("\n");
			vga_moveto(0,13);
#if defined(TARGET_PC98)
			vga_write_color(0x0E);
#endif
			if (sb_card->reason_not_supported) vga_write(sb_card->reason_not_supported);

			vga_moveto(0,16);
			vga_write("\n");
			vga_write_sync();
			_sti();
			redraw = 0;
			uiredraw = 0;
		}

		if (kbhit()) {
			int c = vga_getch();

			if (c == 27 || c == 13)
				loop = 0;
			else if (isdigit(c)) {
				if (selector == 0) { /* sample rate, allow typing in sample rate */
					int i=0;
					VGA_ALPHA_PTR sco;
					struct vga_msg_box box;
					vga_msg_box_create(&box,"Custom sample rate",2,0);
					sco = vga_state.vga_alpha_ram + ((box.y+2) * vga_state.vga_width) + box.x + 2;
					sco[i] = c | 0x1E00;
					temp_str[i++] = c;
					while (1) {
						c = vga_getch();

						if (c == 27)
							break;
						else if (c == 13) {
							if (i == 0) break;
							temp_str[i] = 0;
							wav_sample_rate = strtol(temp_str,NULL,0);
                            if (wav_sample_rate == 0UL) wav_sample_rate = 1UL;
							uiredraw=1;
							break;
						}
						else if (isdigit(c)) {
							if (i < 5) {
								sco[i] = c | 0x1E00;
								temp_str[i++] = c;
							}
						}
						else if (c == 8) {
							if (i > 0) i--;
							sco[i] = ' ' | 0x1E00;
						}
					}
					vga_msg_box_destroy(&box);
				}
			}
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
				if (selector > 0) selector--;
				else selector=7;
				uiredraw=1;
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
				if (selector < 7) selector++;
				else selector=0;
				uiredraw=1;
			}
			else if (c == VGATTY_LEFT_ARROW) { /* left arrow */
				switch (selector) {
					case 0:	/* sample rate */
						ra = param_preset_rates[0];
						for (cc=0;cc < (sizeof(param_preset_rates)/sizeof(param_preset_rates[0]));cc++) {
							if (param_preset_rates[cc] < wav_sample_rate)
								ra = param_preset_rates[cc];
						}
						wav_sample_rate = ra;
						break;
					case 1:	/* stereo/mono */
						wav_stereo = !wav_stereo;
						break;
					case 2: /* 8/16-bit */
						wav_16bit = !wav_16bit;
						break;
					case 3: /* translatin */
						if (sb_card->dsp_adpcm == ADPCM_2BIT) {
							sb_card->dsp_adpcm = ADPCM_2_6BIT;
						}
						else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) {
							sb_card->dsp_adpcm = ADPCM_4BIT;
						}
						else if (sb_card->dsp_adpcm == ADPCM_4BIT) {
							sb_card->dsp_adpcm = 0;
						}
						else {
							sb_card->dsp_adpcm = ADPCM_2BIT;
						}
						break;
					case 4: /* DSP mode */
						if (sb_card->dsp_play_method == 0)
							sb_card->dsp_play_method = SNDSB_DSPOUTMETHOD_MAX - 1;
						else
							sb_card->dsp_play_method--;
						break;
					case 5: /* Direct DAC read after command/data */
						if (sb_card->dsp_direct_dac_read_after_command != 0)
							sb_card->dsp_direct_dac_read_after_command--;
						break;
					case 6: /* Direct DAC read poll retry timeout */
						if (sb_card->dsp_direct_dac_poll_retry_timeout != 0)
							sb_card->dsp_direct_dac_poll_retry_timeout--;
						break;
					case 7:
						sb_card->backwards ^= 1;
						break;
				};
				update_cfg();
				uiredraw=1;
			}
			else if (c == VGATTY_RIGHT_ARROW) { /* right arrow */
				switch (selector) {
					case 0:	/* sample rate */
						for (cc=0;cc < ((sizeof(param_preset_rates)/sizeof(param_preset_rates[0]))-1);) {
							if (param_preset_rates[cc] > wav_sample_rate) break;
							cc++;
						}
						wav_sample_rate = param_preset_rates[cc];
						break;
					case 1:	/* stereo/mono */
						wav_stereo = !wav_stereo;
						break;
					case 2: /* 8/16-bit */
						wav_16bit = !wav_16bit;
						break;
					case 3: /* translatin */
						if (sb_card->dsp_adpcm == ADPCM_2BIT) {
							sb_card->dsp_adpcm = 0;
						}
						else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) {
							sb_card->dsp_adpcm = ADPCM_2BIT;
						}
						else if (sb_card->dsp_adpcm == ADPCM_4BIT) {
							sb_card->dsp_adpcm = ADPCM_2_6BIT;
						}
						else {
							sb_card->dsp_adpcm = ADPCM_4BIT;
						}
						break;
					case 4: /* DSP mode */
						if (++sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_MAX)
							sb_card->dsp_play_method = 0;
						break;
					case 5: /* Direct DAC read after command/data */
						if (sb_card->dsp_direct_dac_read_after_command < 255)
							sb_card->dsp_direct_dac_read_after_command++;
						break;
					case 6: /* Direct DAC read poll retry timeout */
						if (sb_card->dsp_direct_dac_poll_retry_timeout < 255)
							sb_card->dsp_direct_dac_poll_retry_timeout++;
						break;
					case 7:
						sb_card->backwards ^= 1;
						break;
				};
				update_cfg();
				uiredraw=1;
			}
		}

		ui_anim(0);
	}

#if TARGET_MSDOS == 32
	if (!irq_0_had_warned && sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
		/* NOTE TO SELF: It can overwhelm the UI in DOSBox too, but DOSBox seems able to
		   recover if you manage to hit CTRL+F12 to speed up the CPU cycles in the virtual machine.
		   On real hardware, even with the recovery method the machine remains hung :( */
		if (confirm_yes_no_dialog(dos32_irq_0_warning))
			irq_0_had_warned = 1;
		else
			sb_card->dsp_play_method = oldmethod;
	}
#endif

#ifdef INCLUDE_FX
	fx_echo_free();
#endif
	change_param_idx = selector;
	wav_bytes_per_sample = (wav_stereo ? 2 : 1) * (wav_16bit ? 2 : 1);
}

#ifdef SB_MIXER
static void play_with_sb16_8051() {
	unsigned char bb;
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	signed short offset=0;
	signed short selector=0x00;
	signed short cc,x,y;
	VGA_ALPHA_PTR vga;
	int bbi;

	if (sb_card->dsp_vmaj < 4) return; /* SB16 only */

	while (loop) {
		if (redraw || uiredraw) {
			_cli();
			if (redraw) {
#if defined(TARGET_PC98)
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) {
                    vga[0x0000] = 0x20;
                    vga[0x1000] = 0x25;
                    vga++;
                }
#else
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
#endif
				ui_anim(1);
			}
			vga_moveto(0,2);
			vga_write_color(0x1F);
			sprintf(temp_str,"x=enter byte value\n");
			vga_write(temp_str);
			vga_write("\n");

			redraw = uiredraw = 0;
			if (selector > 0xFF) selector = 0xFF;
			else if (selector < 0) selector = 0;
			offset = 0;

			for (cc=0;cc < 256;cc++) {
				x = ((cc & 15)*3)+4;
				y = (cc >> 4)+4;
			    _cli();
				bbi = sndsb_sb16_8051_mem_read(sb_card,(unsigned char)cc);
                _sti();
                bb = (unsigned char)bbi;
				vga_moveto(x,y);
				vga_write_color(cc == selector ? 0x70 : 0x1E);
				sprintf(temp_str,"%02X ",bb);
				vga_write(temp_str);

				if ((cc&15) == 0) {
					sprintf(temp_str,"%02x  ",cc&0xF0);
					vga_write_color(0x1F);
					vga_moveto(0,y);
					vga_write(temp_str);
				}
				if (cc <= 15) {
					sprintf(temp_str,"%02x ",cc);
					vga_write_color(0x1F);
					vga_moveto(x,y-1);
					vga_write(temp_str);
				}
			}
		}

		if (kbhit()) {
			int c = vga_getch();

			if (c == 'x') {
				int a,b;

				vga_moveto(0,2);
				vga_write_color(0x1F);
				vga_write("Type hex value:                             \n");
				vga_write_sync();

				a = vga_getch();
				vga_moveto(20,2);
				vga_write_color(0x1E);
				vga_writec((char)a);
				vga_write_sync();

				b = vga_getch();
				vga_moveto(21,2);
				vga_write_color(0x1E);
				vga_writec((char)b);
				vga_write_sync();

				if (isxdigit(a) && isxdigit(b)) {
					unsigned char nb;
					nb = (unsigned char)xdigit2int(a) << 4;
					nb |= (unsigned char)xdigit2int(b);

					_cli();
					sndsb_sb16_8051_mem_write(sb_card,(unsigned char)selector,nb);
					_sti();
				}

				uiredraw = 1;
			}
			else if (c == ' ')
				uiredraw = 1;
			else if (c == 27)
				loop = 0;
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
				selector -= 0x10;
				selector &= 0xFF;
				uiredraw=1;
			}
			else if (c == VGATTY_LEFT_ARROW) { /* left arrow */
				selector--;
				selector &= 0xFF;
				uiredraw=1;
			}
			else if (c == VGATTY_RIGHT_ARROW) { /* right arrow */
				selector++;
				selector &= 0xFF;
				uiredraw=1;
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
				selector += 0x10;
				selector &= 0xFF;
				uiredraw=1;
			}
		}

		ui_anim(0);
	}
}

static unsigned char ess688_not_present_init=1;
static unsigned char ess688_not_present[0x100];
static void play_with_ess() {
	unsigned char bb;
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	signed short offset=0;
	signed short selector=0xA0;
	signed short cc,x,y;
	VGA_ALPHA_PTR vga;
	int bbi;

	if (!sb_card->ess_extensions) return;

	if (ess688_not_present_init) {
		ess688_not_present_init = 0;
		if (sb_card->ess_chipset == SNDSB_ESS_688) {
			for (cc=0x00;cc < 0x100;cc++) ess688_not_present[cc] = 1;
			for (cc=0xA0;cc <  0xBA;cc++) ess688_not_present[cc] = 0;
		}
		else {
			for (cc=0x00;cc < 0x100;cc++) ess688_not_present[cc] = 0;
		}
	}

	while (loop) {
		if (redraw || uiredraw) {
			_cli();
			if (redraw) {
#if defined(TARGET_PC98)
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) {
                    vga[0x0000] = 0x20;
                    vga[0x1000] = 0x25;
                    vga++;
                }
#else
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
#endif
				ui_anim(1);
			}
			vga_moveto(0,2);
			vga_write_color(0x1F);
			sprintf(temp_str,"x=enter byte value p=re-detect\n");
			vga_write(temp_str);
			vga_write("\n");

			redraw = uiredraw = 0;
			if (selector > 0xFF) selector = 0xFF;
			else if (selector < 0) selector = 0;
			offset = 0;

			/* reading ESS controllers involves the DSP so avoid conflicts by clearing interrupts */
			_cli();

			for (cc=0;cc < 256;cc++) {
				x = ((cc & 15)*3)+4;
				y = (cc >> 4)+4;
				if (ess688_not_present[cc])
					bbi = -1;
				else {
					bbi = sndsb_ess_read_controller(sb_card,(unsigned char)cc);
					if (bbi < 0) ess688_not_present[cc] = 1;
				}
				bb = (unsigned char)bbi;
				vga_moveto(x,y);
				vga_write_color(cc == selector ? 0x70 : (ess688_not_present[cc] ? 0x16 : 0x1E));
				sprintf(temp_str,"%02X ",bb);
				vga_write(temp_str);

				if ((cc&15) == 0) {
					sprintf(temp_str,"%02x  ",cc&0xF0);
					vga_write_color(0x1F);
					vga_moveto(0,y);
					vga_write(temp_str);
				}
				if (cc <= 15) {
					sprintf(temp_str,"%02x ",cc);
					vga_write_color(0x1F);
					vga_moveto(x,y-1);
					vga_write(temp_str);
				}
			}

			_sti();
		}

		if (kbhit()) {
			int c = vga_getch();

			if (c == 'p') {
				for (cc=0;cc < 256;cc++) ess688_not_present[cc] = 0;
				redraw = 1;
			}
			else if (c == 'x') {
				int a,b;

				vga_moveto(0,2);
				vga_write_color(0x1F);
				vga_write("Type hex value:                             \n");
				vga_write_sync();

				a = vga_getch();
				vga_moveto(20,2);
				vga_write_color(0x1E);
				vga_writec((char)a);
				vga_write_sync();

				b = vga_getch();
				vga_moveto(21,2);
				vga_write_color(0x1E);
				vga_writec((char)b);
				vga_write_sync();

				if (isxdigit(a) && isxdigit(b)) {
					unsigned char nb;
					nb = (unsigned char)xdigit2int(a) << 4;
					nb |= (unsigned char)xdigit2int(b);

					/* reading ESS controllers involves the DSP so avoid conflicts by clearing interrupts */
					_cli();

					sndsb_ess_write_controller(sb_card,(unsigned char)selector,nb);
					ess688_not_present[selector] = 0;

					_sti();
				}

				uiredraw = 1;
			}
			else if (c == ' ')
				uiredraw = 1;
			else if (c == 27)
				loop = 0;
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
				selector -= 0x10;
				selector &= 0xFF;
				uiredraw=1;
			}
			else if (c == VGATTY_LEFT_ARROW) { /* left arrow */
				selector--;
				selector &= 0xFF;
				uiredraw=1;
			}
			else if (c == VGATTY_RIGHT_ARROW) { /* right arrow */
				selector++;
				selector &= 0xFF;
				uiredraw=1;
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
				selector += 0x10;
				selector &= 0xFF;
				uiredraw=1;
			}
		}

		ui_anim(0);
	}
}

static void play_with_mixer() {
	signed short visrows=25-(4+1);
	signed short visy=4;
	signed char mixer=-1;
	unsigned char bb;
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	signed short offset=0;
	signed short selector=0;
	signed short sb_mixer_items = -1;
	const struct sndsb_mixer_control *sb_mixer;
	const struct sndsb_mixer_control *ent;
	unsigned char rawmode=0;
	signed short cc,x,y;
	VGA_ALPHA_PTR vga;

	sb_mixer = sndsb_get_mixer_controls(sb_card,&sb_mixer_items,mixer);

	while (loop) {
		if (redraw || uiredraw) {
			if (redraw) {
#if defined(TARGET_PC98)
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) {
                    vga[0x0000] = 0x20;
                    vga[0x1000] = 0x25;
                    vga++;
                }
#else
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
#endif
				ui_anim(1);
			}
			vga_moveto(0,2);
			vga_write_color(0x1F);
			if (rawmode) {
				sprintf(temp_str,"Raw mixer: R=leave raw  x=enter byte value\n");
				vga_write(temp_str);
				vga_write("\n");

				if (selector > 0xFF) selector = 0xFF;
				else if (selector < 0) selector = 0;
				offset = 0;
				for (cc=0;cc < 256;cc++) {
					x = ((cc & 15)*3)+4;
					y = (cc >> 4)+4;
                    _cli();
					bb = sndsb_read_mixer(sb_card,(unsigned char)cc);
                    _sti();
					vga_moveto(x,y);
					vga_write_color(cc == selector ? 0x70 : 0x1E);
					sprintf(temp_str,"%02X ",bb);
					vga_write(temp_str);

					if ((cc&15) == 0) {
						sprintf(temp_str,"%02x  ",cc&0xF0);
						vga_write_color(0x1F);
						vga_moveto(0,y);
						vga_write(temp_str);
					}
					if (cc <= 15) {
						sprintf(temp_str,"%02x ",cc);
						vga_write_color(0x1F);
						vga_moveto(x,y-1);
						vga_write(temp_str);
					}
				}
			}
			else {
				sprintf(temp_str,"Mixer: %s as %s  M=toggle mixer R=raw\n",sndsb_mixer_chip_str(sb_card->mixer_chip),
					mixer >= 0 ? sndsb_mixer_chip_str(mixer) : "(same)");
				vga_write(temp_str);
				vga_write("\n");

				if (selector >= sb_mixer_items)
					selector = sb_mixer_items - 1;
				if (offset >= sb_mixer_items)
					offset = sb_mixer_items - 1;
				if (offset < 0)
					offset = 0;

				for (y=0;y < visrows;y++) {
					if ((y+offset) >= sb_mixer_items)
						break;
					if (!sb_mixer)
						break;

					ent = sb_mixer + offset + y;
					vga_moveto(0,y+visy);
					vga_write_color((y+offset) == selector ? 0x70 : 0x1E);
                    _cli();

					if (ent->length == 1)
						x=sprintf(temp_str,"%s     %s",
							sndsb_read_mixer_entry(sb_card,ent) ? "On " : "Off",ent->name);
					else
						x=sprintf(temp_str,"%-3u/%-3u %s",sndsb_read_mixer_entry(sb_card,ent),
							(1 << ent->length) - 1,ent->name);

                    _sti();
					while (x < 80) temp_str[x++] = ' '; temp_str[x] = 0;
					vga_write(temp_str);
				}
			}

			vga_write_sync();
			redraw = 0;
			uiredraw = 0;
		}

		if (kbhit()) {
			int c = vga_getch();

			if (c == 'M' || c == 'm') {
				selector = 0;
				offset = 0;
				mixer++;
				if (mixer == 0) mixer++;
				if (mixer == sb_card->mixer_chip) mixer++;
				if (mixer >= SNDSB_MIXER_MAX) mixer = -1;
				sb_mixer = sndsb_get_mixer_controls(sb_card,&sb_mixer_items,mixer);
				redraw=1;
			}
			else if (isdigit(c) && !rawmode) {
				int i=0;
				char temp_str[7];
				unsigned int val;
				VGA_ALPHA_PTR sco;
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Custom value",2,0);
				sco = vga_state.vga_alpha_ram + ((box.y+2) * vga_state.vga_width) + box.x + 2;
				sco[i] = c | 0x1E00;
				temp_str[i++] = c;
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						c = vga_getch();

						if (c == 27)
							break;
						else if (c == 13) {
							if (i == 0) break;
							temp_str[i] = 0;
							val = (unsigned int)strtol(temp_str,NULL,0);
							val &= (1 << sb_mixer[selector].length) - 1;
							sndsb_write_mixer_entry(sb_card,sb_mixer+selector,val);
							break;
						}
						else if (isdigit(c)) {
							if (i < 5) {
								sco[i] = c | 0x1E00;
								temp_str[i++] = c;
							}
						}
						else if (c == 8) {
							if (i > 0) i--;
							sco[i] = ' ' | 0x1E00;
						}
					}
				}
				vga_msg_box_destroy(&box);
				uiredraw=1;
			}
			else if (c == 'x') {
				int a,b;

				vga_moveto(0,2);
				vga_write_color(0x1F);
				vga_write("Type hex value:                             \n");
				vga_write_sync();

				a = vga_getch();
				vga_moveto(20,2);
				vga_write_color(0x1E);
				vga_writec((char)a);
				vga_write_sync();

				b = vga_getch();
				vga_moveto(21,2);
				vga_write_color(0x1E);
				vga_writec((char)b);
				vga_write_sync();

				if (isxdigit(a) && isxdigit(b)) {
					unsigned char nb;
					nb = (unsigned char)xdigit2int(a) << 4;
					nb |= (unsigned char)xdigit2int(b);
					sndsb_write_mixer(sb_card,(unsigned char)selector,nb);
				}

				uiredraw = 1;
			}
			else if (c == 'r' || c == 'R') {
				rawmode = !rawmode;
				selector = 0;
				offset = 0;
				redraw = 1;
			}
			else if (c == 27)
				loop = 0;
			else if (c == ' ')
				uiredraw = 1;
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
				if (rawmode) {
					selector -= 0x10;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if (selector > 0) {
						uiredraw=1;
						selector--;
						if (offset > selector)
							offset = selector;
					}
				}
			}
			else if (c == VGATTY_LEFT_ARROW) { /* left arrow */
				if (rawmode) {
					selector--;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if (selector >= 0 && selector < sb_mixer_items && sb_mixer != NULL) {
						unsigned char v = sndsb_read_mixer_entry(sb_card,sb_mixer+selector);
						if (v > 0) v--;
						sndsb_write_mixer_entry(sb_card,sb_mixer+selector,v);
						uiredraw=1;
					}
				}
			}
			else if (c == VGATTY_RIGHT_ARROW) { /* right arrow */
				if (rawmode) {
					selector++;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if (selector >= 0 && selector < sb_mixer_items && sb_mixer != NULL) {
						unsigned char v = sndsb_read_mixer_entry(sb_card,sb_mixer+selector);
						if (v < ((1 << sb_mixer[selector].length)-1)) v++;
						sndsb_write_mixer_entry(sb_card,sb_mixer+selector,v);
						uiredraw=1;
					}
				}
			}
			else if (c == 0x4900) { /* page up */
				if (rawmode) {
				}
				else {
					if (selector > 0) {
						selector -= visrows-1;
						if (selector < 0) selector = 0;
						if (selector < offset) offset = selector;
						uiredraw=1;
					}
				}
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
				if (rawmode) {
					selector += 0x10;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if ((selector+1) < sb_mixer_items) {
						uiredraw=1;
						selector++;
						if (selector >= (offset+visrows)) {
							offset = selector-(visrows-1);
						}
					}
				}
			}
			else if (c == 0x5100) { /* page down */
				if (rawmode) {
				}
				else {
					if ((selector+1) < sb_mixer_items) {
						selector += visrows-1;
						if (selector >= sb_mixer_items)
							selector = sb_mixer_items-1;
						if (selector >= (offset+visrows))
							offset = selector-(visrows-1);
						uiredraw=1;
					}
				}
			}
			else if (c == '<') {
				if (rawmode) {
				}
				else {
					if (selector >= 0 && selector < sb_mixer_items && sb_mixer != NULL) {
						sndsb_write_mixer_entry(sb_card,sb_mixer+selector,0);
						uiredraw=1;
					}
				}
			}
			else if (c == '>') {
				if (rawmode) {
				}
				else {
					if (selector >= 0 && selector < sb_mixer_items && sb_mixer != NULL) {
						sndsb_write_mixer_entry(sb_card,sb_mixer+selector,
							(1 << sb_mixer[selector].length) - 1);
						uiredraw=1;
					}
				}
			}
		}

		ui_anim(0);
	}
}
#endif

static const struct vga_menu_item menu_separator =
	{(char*)1,		's',	0,	0};

static const struct vga_menu_item main_menu_file_set =
	{"Set file...",		's',	0,	0};
static const struct vga_menu_item main_menu_file_quit =
	{"Quit",		'q',	0,	0};
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static const struct vga_menu_item main_menu_windows_fullscreen =
	{"Windows fullscreen",	'f',	0,	0};
#endif

static const struct vga_menu_item* main_menu_file[] = {
	&main_menu_file_set,
	&main_menu_file_quit,
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&menu_separator,
	&main_menu_windows_fullscreen,
#endif
	NULL
};

static const struct vga_menu_item main_menu_playback_play =
	{"Play",		'p',	0,	0};
static const struct vga_menu_item main_menu_playback_record =
	{"Record",		'r',	0,	0};
static const struct vga_menu_item main_menu_playback_stop =
	{"Stop",		's',	0,	0};
static const struct vga_menu_item main_menu_playback_params =
	{"Parameters",		'a',	0,	0};
static struct vga_menu_item main_menu_playback_reduced_irq =
	{"xxx",			'i',	0,	0};
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static struct vga_menu_item main_menu_playback_autoinit_adpcm =
	{"xxx",			'd',	0,	0};
#endif
static struct vga_menu_item main_menu_playback_goldplay =
	{"xxx",			'g',	0,	0};
static struct vga_menu_item main_menu_playback_goldplay_mode =
	{"xxx",			'm',	0,	0};
static struct vga_menu_item main_menu_playback_dsp_autoinit_dma =
	{"xxx",			't',	0,	0};
static struct vga_menu_item main_menu_playback_dsp_autoinit_command =
	{"xxx",			'c',	0,	0};
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static struct vga_menu_item main_menu_playback_noreset_adpcm =
	{"xxx",			'n',	0,	0};
static struct vga_menu_item main_menu_playback_timer_clamp =
	{"xxx",			0,	0,	0};
static struct vga_menu_item main_menu_playback_force_hispeed =
	{"xxx",			'h',	0,	0};
static struct vga_menu_item main_menu_playback_flip_sign =
	{"xxx",			'l',	0,	0};
static struct vga_menu_item main_menu_playback_dsp4_fifo_autoinit =
	{"xxx",			'f',	0,	0};
static struct vga_menu_item main_menu_playback_dsp4_fifo_single =
	{"xxx",			'e',	0,	0};
static struct vga_menu_item main_menu_playback_dsp_nag_mode =
	{"xxx",			'o',	0,	0};
static struct vga_menu_item main_menu_playback_dsp_poll_ack_no_irq =
	{"xxx",			'q',	0,	0};
static struct vga_menu_item main_menu_playback_wari_hack_alias =
	{"xxx",			'w',	0,	0};
#endif

static const struct vga_menu_item* main_menu_playback[] = {
	&main_menu_playback_play,
	&main_menu_playback_record,
	&main_menu_playback_stop,
	&menu_separator,
	&main_menu_playback_params,
	&main_menu_playback_reduced_irq,
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&main_menu_playback_autoinit_adpcm,
#endif
	&main_menu_playback_goldplay,
	&main_menu_playback_goldplay_mode,
	&main_menu_playback_dsp_autoinit_dma,
	&main_menu_playback_dsp_autoinit_command,
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&main_menu_playback_noreset_adpcm,
	&main_menu_playback_timer_clamp,
	&main_menu_playback_force_hispeed,
	&main_menu_playback_flip_sign,
	&main_menu_playback_dsp4_fifo_autoinit,
	&main_menu_playback_dsp4_fifo_single,
	&main_menu_playback_dsp_nag_mode,
	&main_menu_playback_dsp_poll_ack_no_irq,
	&main_menu_playback_wari_hack_alias,
#endif
	NULL
};

static const struct vga_menu_item main_menu_device_dsp_reset =
	{"DSP reset",		'r',	0,	0};
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static const struct vga_menu_item main_menu_device_mixer_reset =
	{"Mixer reset",		'r',	0,	0};
#endif
static const struct vga_menu_item main_menu_device_trigger_irq =
	{"IRQ test",		't',	0,	0};
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static const struct vga_menu_item main_menu_device_autoinit_stop =
	{"Exit auto-init",	'x',	0,	0};
static const struct vga_menu_item main_menu_device_autoinit_stopcont =
	{"Exit & continue auto-init",	'o',	0,	0};
static const struct vga_menu_item main_menu_device_haltcont_dma =
	{"Halt & continue DMA",	'h',	0,	0};
#endif
#ifdef SB_MIXER
static const struct vga_menu_item main_menu_device_mixer_controls =
	{"Mixer controls",	'm',	0,	0};
static const struct vga_menu_item main_menu_device_ess_controls =
	{"ESS 688/1868 controls",'e',	0,	0};
static const struct vga_menu_item main_menu_device_sb16_8051 =
	{"SB16 8051 memory",'b',	0,	0};
#endif
#ifdef CARD_INFO_AND_CHOOSER
static const struct vga_menu_item main_menu_device_info =
	{"Information",		'i',	0,	0};
static const struct vga_menu_item main_menu_device_choose_sound_card =
	{"Choose sound card",	'c',	0,	0};
#endif
#ifdef LIVE_CFG
static const struct vga_menu_item main_menu_device_configure_sound_card =
	{"Configure sound card",'o',	0,	0};
#endif
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static struct vga_menu_item main_menu_device_dsp_alias =
	{"Alias ports",		'a',	0,	0};
static struct vga_menu_item main_menu_device_busy_cycle =
	{"Busy cycle",		'b',	0,	0};
static struct vga_menu_item main_menu_device_srate_force =
	{"xxx",		        's',	0,	0};
static struct vga_menu_item main_menu_device_realloc_dma =
	{"Realloc DMA",		'd',	0,	0};
#endif

static const struct vga_menu_item* main_menu_device[] = {
	&main_menu_device_dsp_reset,
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&main_menu_device_mixer_reset,
#endif
	&main_menu_device_trigger_irq,
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&main_menu_device_autoinit_stop,
	&main_menu_device_autoinit_stopcont,
	&main_menu_device_haltcont_dma,
#endif
#ifdef SB_MIXER
	&main_menu_device_mixer_controls,
	&main_menu_device_ess_controls,
	&main_menu_device_sb16_8051,
#endif
#ifdef CARD_INFO_AND_CHOOSER
	&main_menu_device_info,
	&main_menu_device_choose_sound_card,
#endif
#ifdef LIVE_CFG
	&main_menu_device_configure_sound_card,
#endif
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&main_menu_device_dsp_alias,
	&main_menu_device_busy_cycle,
    &main_menu_device_srate_force,
    &main_menu_device_realloc_dma,
#endif
	NULL
};

static const struct vga_menu_item main_menu_help_about =
	{"About",		'r',	0,	0};



#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
static const struct vga_menu_item main_menu_help_dsp_modes =
	{"DSP modes",		'd',	0,	0};
#endif
static const struct vga_menu_item* main_menu_help[] = {
	&main_menu_help_about,
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	&menu_separator,
	&main_menu_help_dsp_modes,
#endif
	NULL
};

#ifdef INCLUDE_FX
static const struct vga_menu_item main_menu_effects_reset =
	{"Reset",		'r',	0,	0};

static const struct vga_menu_item main_menu_effects_vol =
	{"Volume",		'v',	0,	0};

static const struct vga_menu_item main_menu_effects_echo =
	{"Echo",		'e',	0,	0};

static const struct vga_menu_item* main_menu_effects[] = {
	&main_menu_effects_reset,
	&main_menu_effects_vol,
	&main_menu_effects_echo,
	NULL
};
#endif

static const struct vga_menu_bar_item main_menu_bar[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Playback ",		'P',	0x19,	6,	10,	&main_menu_playback}, /* ALT-P */
	{" Device ",		'D',	0x20,	16,	8,	&main_menu_device}, /* ALT-D */
#ifdef INCLUDE_FX
	{" Effects ",		'E',	0x12,	24,	9,	&main_menu_effects}, /* ALT-E */
	{" Help ",		'H',	0x23,	33,	6,	&main_menu_help}, /* ALT-H */
#else
	{" Help ",		'H',	0x23,	24,	6,	&main_menu_help}, /* ALT-H */
#endif
	{NULL,			0,	0x00,	0,	0,	0}
};

static void my_vga_menu_idle() {
	ui_anim(0);
}

static int confirm_quit() {
	/* FIXME: Why does this cause Direct DSP playback to horrifically slow down? */
	return confirm_yes_no_dialog("Are you sure you want to exit to DOS?");
}

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
int adpcm_warning_prompt() {
	return confirm_yes_no_dialog("Most Sound Blaster clones do not support auto-init ADPCM playback.\nIf nothing plays when enabled, your sound card is one of them.\n\nEnable?");
}
#endif

static void update_cfg() {
	unsigned int r;

	wav_sample_rate_by_timer_ticks = T8254_REF_CLOCK_HZ / wav_sample_rate;
	if (wav_sample_rate_by_timer_ticks == 0) wav_sample_rate_by_timer_ticks = 1;
	wav_sample_rate_by_timer = T8254_REF_CLOCK_HZ / wav_sample_rate_by_timer_ticks;

	sb_card->dsp_adpcm = sb_card->dsp_adpcm;
	sb_card->dsp_record = wav_record;
	r = wav_sample_rate;
	if (sb_card->dsp_adpcm == ADPCM_4BIT) r /= 2;
	else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) r /= 3;
	else if (sb_card->dsp_adpcm == ADPCM_2BIT) r /= 4;
	adpcm_counter = 0;
	adpcm_reset_interval = 0;
	if (sb_card->dsp_adpcm > 0) {
		if (sb_card->dsp_adpcm == ADPCM_4BIT)
			sb_card->buffer_irq_interval = wav_sample_rate / 2;
		else if (sb_card->dsp_adpcm == ADPCM_2_6BIT)
			sb_card->buffer_irq_interval = wav_sample_rate / 3;
		else if (sb_card->dsp_adpcm == ADPCM_2BIT)
			sb_card->buffer_irq_interval = wav_sample_rate / 4;

		if (reduced_irq_interval == 2)
			sb_card->buffer_irq_interval = sb_card->buffer_size;
		else if (reduced_irq_interval == 0)
			sb_card->buffer_irq_interval /= 15;
		else if (reduced_irq_interval == -1)
			sb_card->buffer_irq_interval /= 100;

		if (sb_card->dsp_adpcm == ADPCM_4BIT)
			sb_card->buffer_irq_interval &= ~1UL;
		else if (sb_card->dsp_adpcm == ADPCM_2_6BIT)
			sb_card->buffer_irq_interval -=
				sb_card->buffer_irq_interval % 3;
		else if (sb_card->dsp_adpcm == ADPCM_2BIT)
			sb_card->buffer_irq_interval &= ~3UL;

		if (adpcm_do_reset_interval)
			adpcm_reset_interval = sb_card->buffer_irq_interval;
	}
	else {
		sb_card->buffer_irq_interval = r;
		if (reduced_irq_interval == 2)
			sb_card->buffer_irq_interval =
				sb_card->buffer_size / wav_bytes_per_sample;
		else if (reduced_irq_interval == 0)
			sb_card->buffer_irq_interval /= 15;
		else if (reduced_irq_interval == -1)
			sb_card->buffer_irq_interval /= 100;
	}

	if (reduced_irq_interval == 2)
		main_menu_playback_reduced_irq.text =
			"IRQ interval: full length";
	else if (reduced_irq_interval == 1)
		main_menu_playback_reduced_irq.text =
			"IRQ interval: large";
	else if (reduced_irq_interval == 0)
		main_menu_playback_reduced_irq.text =
			"IRQ interval: small";
	else /* -1 */
		main_menu_playback_reduced_irq.text =
			"IRQ interval: tiny";

	if (goldplay_samplerate_choice == GOLDRATE_MATCH)
		main_menu_playback_goldplay_mode.text =
			"Goldplay sample rate: Match";
	else if (goldplay_samplerate_choice == GOLDRATE_DOUBLE)
		main_menu_playback_goldplay_mode.text =
			"Goldplay sample rate: Double";
	else if (goldplay_samplerate_choice == GOLDRATE_MAX)
		main_menu_playback_goldplay_mode.text =
			"Goldplay sample rate: Max";
	else
		main_menu_playback_goldplay_mode.text =
			"?";

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	main_menu_playback_autoinit_adpcm.text =
		sb_card->enable_adpcm_autoinit ? "ADPCM Auto-init: On" : "ADPCM Auto-init: Off";
#endif
	main_menu_playback_goldplay.text =
		sb_card->goldplay_mode ? "Goldplay mode: On" : "Goldplay mode: Off";
	main_menu_playback_dsp_autoinit_dma.text =
		sb_card->dsp_autoinit_dma ? "DMA autoinit: On" : "DMA autoinit: Off";
	main_menu_playback_dsp_autoinit_command.text =
		sb_card->dsp_autoinit_command ? "DSP playback: auto-init" : "DSP playback: single-cycle";
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	main_menu_playback_force_hispeed.text =
		sb_card->force_hispeed ? "Force hispeed: On" : "Force hispeed: Off";
	main_menu_playback_noreset_adpcm.text =
		adpcm_do_reset_interval ? "ADPCM reset step/interval: On" : "ADPCM reset step/interval: Off";
	main_menu_playback_timer_clamp.text =
		sample_rate_timer_clamp ? "Clamp samplerate to timer: On" : "Clamp samplerate to timer: Off";
	main_menu_playback_flip_sign.text =
		sb_card->audio_data_flipped_sign ? "Flipped sign: On" : "Flipped sign: Off";
	main_menu_playback_dsp4_fifo_autoinit.text =
		sb_card->dsp_4xx_fifo_autoinit ? "DSP 4.xx autoinit FIFO: On" : "DSP 4.xx autoinit FIFO: Off";
	main_menu_playback_dsp4_fifo_single.text =
		sb_card->dsp_4xx_fifo_single_cycle ? "DSP 4.xx single FIFO: On" : "DSP 4.xx single FIFO: Off";
	main_menu_playback_dsp_nag_mode.text =
		sb_card->dsp_nag_mode ?
			(sb_card->dsp_nag_hispeed ? "DSP nag mode: All" : "DSP nag mode: Non-highspeed")
			: "DSP nag mode: Off";
	main_menu_playback_dsp_poll_ack_no_irq.text =
		sb_card->poll_ack_when_no_irq ? "Poll ack when no IRQ: On" : "Poll ack when no IRQ: Off";
	main_menu_playback_wari_hack_alias.text =
		sb_card->wari_hack_mode ? "Wari Hack Alias: On" : "Wari Hack Alias: Off";

    if (sb_card->srate_force_dsp_4xx)
        main_menu_device_srate_force.text = "Force srate cmd: Using 4.xx (switch to TC)";
    else if (sb_card->srate_force_dsp_tc)
        main_menu_device_srate_force.text = "Force srate cmd: Using TC (switch to Off)";
    else
        main_menu_device_srate_force.text = "Force srate cmd: Off (switch to 4.xx)";
#endif
}

static void prompt_play_wav(unsigned char rec) {
	unsigned char gredraw = 1;
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
	struct find_t ft;
#endif

	{
		const char *rp;
		char temp[sizeof(wav_file)];
		int cursor = strlen(wav_file),i,c,redraw=1,ok=0;
		memcpy(temp,wav_file,strlen(wav_file)+1);
		while (!ok) {
			if (gredraw) {
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
				char *cwd;
#endif

				gredraw = 0;
				vga_clear();
				vga_moveto(0,4);
				vga_write_color(0x07);
				vga_write("Enter WAV file path:\n");
				vga_write_sync();
				draw_irq_indicator();
				ui_anim(1);
				redraw = 1;

#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
				cwd = getcwd(NULL,0);
				if (cwd) {
					vga_moveto(0,6);
					vga_write_color(0x0B);
					vga_write(cwd);
					vga_write_sync();
				}

				if (_dos_findfirst("*.*",_A_NORMAL|_A_RDONLY,&ft) == 0) {
					int x=0,y=7,cw = 14;
					char *ex;

					do {
						ex = strrchr(ft.name,'.');
						if (!ex) ex = "";

						if (ft.attrib&_A_SUBDIR) {
							vga_write_color(0x0F);
						}
						else if (!strcasecmp(ex,".wav")) {
							vga_write_color(0x1E);
						}
						else {
							vga_write_color(0x07);
						}
						vga_moveto(x,y);
#if defined(TARGET_PC98)
			            vga_writew(ft.name);
#else
			            vga_write(ft.name);
#endif
                        vga_write_until(x+14);
						vga_writec(' ');

						x += cw;
						if ((x+cw) > vga_state.vga_width) {
							x = 0;
							if (y >= vga_state.vga_height) break;
							y++;
						}
					} while (_dos_findnext(&ft) == 0);

					_dos_findclose(&ft);
				}
#endif
			}
			if (redraw) {
				rp = (const char*)temp;
				vga_moveto(0,5);
				vga_write_color(0x0E);
				for (i=0;i < 80;i++) {
					if (*rp != 0)	vga_writec(*rp++);
					else		vga_writec(' ');	
				}
				vga_moveto(cursor,5);
				vga_write_sync();
				redraw=0;
			}

			if (kbhit()) {
				c = vga_getch();

				if (c == 27) {
					ok = -1;
				}
				else if (c == 13) {
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
					ok = 1;
#else
					struct stat st;

					if (isalpha(temp[0]) && temp[1] == ':' && temp[2] == 0) {
						unsigned int total;

						_dos_setdrive(tolower(temp[0])+1-'a',&total);
						temp[0] = 0;
						gredraw = 1;
						cursor = 0;
					}
					else if (stat(temp,&st) == 0) {
						if (S_ISDIR(st.st_mode)) {
							chdir(temp);
							temp[0] = 0;
							gredraw = 1;
							cursor = 0;
						}
						else {
							ok = 1;
						}
					}
					else {
						ok = 1;
					}
#endif
				}
				else if (c == 8) {
					if (cursor != 0) {
						temp[--cursor] = 0;
						redraw = 1;
					}
				}
				else if (c >= 32 && c < 256) {
					if (cursor < 79) {
						temp[cursor++] = (char)c;
						temp[cursor  ] = (char)0;
						redraw = 1;
					}
				}
			}
			else {
				ui_anim(0);
			}
		}

		if (ok == 1) {
			unsigned char wp = wav_playing;
			stop_play();
			close_wav();
			memcpy(wav_file,temp,strlen(temp)+1);
			open_wav();
			if (wp) begin_play();
		}
	}
}

static void help() {
#if TARGET_MSDOS == 16 && defined(__TINY__)
    printf("See source code for options.");
#else
	printf("test [options]\n");
# if !(TARGET_MSDOS == 16 && defined(__COMPACT__)) /* this is too much to cram into a small model EXE */
	printf(" /h /help             This help\n");
	printf(" /nopnp               Don't scan for ISA Plug & Play devices\n");
	printf(" /noprobe             Don't probe ISA I/O ports for non PnP devices\n");
	printf(" /noenv               Don't use BLASTER environment variable\n");
	printf(" /wav=<file>          Open with specified WAV file\n");
	printf(" /play                Automatically start playing WAV file\n");
	printf(" /sc=<N>              Automatically pick Nth sound card (first card=1)\n");
	printf(" /ddac                Force DSP Direct DAC output mode\n");
#  if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
	printf(" /96k /64k /63k /32k  Limit DMA buffer to...\n");
#  else
	printf(" /64k /63k /32k       Limit DMA buffer to...\n");
#  endif
    printf(" /16k /8k /4k         Limit DMA buffer to...\n");
	printf(" /nomirqp             Disable 'manual' IRQ probing\n");
	printf(" /noairqp             Disable 'alt' IRQ probing\n");
	printf(" /nosb16cfg           Don't read configuration from SB16 config byte\n");
	printf(" /nodmap              Disable DMA probing\n");
	printf(" /nohdmap             Disable 16-bit DMA probing\n");
	printf(" /nowinvxd            don't try to identify Windows drivers\n");
	printf(" /nochain             Don't chain to previous IRQ (sound blaster IRQ)\n");
	printf(" /noidle              Don't use sndsb library idle function\n");
	printf(" /adma                Assume DMA controllers are present\n");
	printf(" /noess               Do not use/detect ESS 688/1869 extensions\n");
	printf(" /sbalias:dsp         Use DSP alias port 0x22D by default\n");
	printf(" /ex-ess              Experimentally use ESS extensions for ESS chips\n");
	printf("                      not directly supported.\n");
	printf(" /nif2                Don't use DSP command 0xF2 to probe IRQ\n");
	printf(" /ni80                Don't use DSP command 0x80 to probe IRQ\n");
	printf(" /nde2                Don't use DSP command 0xE2 to probe DMA\n");
	printf(" /nd14                Don't use DSP command 0x14 to probe DMA\n");
	printf(" /fip                 Force IRQ probe\n");
	printf(" /fdp                 Force DMA probe\n");
#  if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
	printf(" /dmaaio              Allow DMA auto-init override (careful!)\n");
	printf(" /stopres             Always reset DSP to stop playback\n");
	printf(" /hinoblk             Assume DSP hispeed modes are non-blocking\n");
    printf(" /srf4xx              Force SB16 sample rate commands\n");
    printf(" /srftc               Force SB/SBPro time constant sample rate\n");
    printf(" /noasp               Don't probe SB16 ASP/CSP chip\n");
    printf(" /dma128              Enable 16-bit DMA to span 128KB\n");
    printf(" /-dma128             Disable 16-bit DMA to span 128KB\n");
    printf(" /dmam:[u|a|p]        Assume 16-bit DMA masking\n");
    printf("                          u=unknown a=address p=page\n");
#  endif
# endif

# if TARGET_MSDOS == 32
	printf("The following option affects hooking the NMI interrupt. Hooking is\n");
	printf("required to work with Gravis Ultrasound SBOS/MEGA-EM SB emulation\n");
	printf("and to work around problems with common DOS extenders. If not specified,\n");
	printf("the program will only hook NMI if SBOS/MEGA-EM is resident.\n");
	printf(" /-nmi or /+nmi       Don't / Do hook NMI interrupt, reflect to real mode.\n");
# endif
#endif
}

#ifdef CARD_INFO_AND_CHOOSER
static void draw_device_info(struct sndsb_ctx *cx,int x,int y,int w,int h) {
	int row = 2;

	/* clear prior contents */
	{
		VGA_ALPHA_PTR p = vga_state.vga_alpha_ram + (y * vga_state.vga_width) + x;
		unsigned int a,b;

		for (b=0;b < h;b++) {
			for (a=0;a < w;a++) {
				*p++ = 0x1E20;
			}
			p += vga_state.vga_width - w;
		}
	}

	vga_write_color(0x1E);

	vga_moveto(x,y + 0);
	sprintf(temp_str,"BASE:%03Xh  MPU:%03Xh  DMA:%-2d DMA16:%-2d IRQ:%-2d ",
		cx->baseio,	cx->mpuio,	cx->dma8,	cx->dma16,	cx->irq);
	vga_write(temp_str);
	if (cx->dsp_ok) {
		sprintf(temp_str,"DSP: v%u.%u  ",
			cx->dsp_vmaj,	cx->dsp_vmin);
		vga_write(temp_str);
	}
	else {
		vga_write("DSP: No  ");
	}

	if (cx->mixer_ok) {
		sprintf(temp_str,"MIXER: %s",sndsb_mixer_chip_str(cx->mixer_chip));
		vga_write(temp_str);
	}
	else {
		vga_write("MIXER: No");
	}

	vga_moveto(x,y + 1);
	if (cx->dsp_ok) {
		sprintf(temp_str,"DSP String: %s",cx->dsp_copyright);
		vga_write(temp_str);
	}

	if (row < h && (cx->is_gallant_sc6600 || cx->mega_em || cx->sbos)) {
		vga_moveto(x,y + (row++));
		if (cx->is_gallant_sc6600) vga_write("SC-6600 ");
		if (cx->mega_em) vga_write("MEGA-EM ");
		if (cx->sbos) vga_write("SBOS ");
	}
#ifdef ISAPNP
	if (row < h) {
		vga_moveto(x,y + (row++));
		if (cx->pnp_name != NULL) {
			isa_pnp_product_id_to_str(temp_str,cx->pnp_id);
			vga_write("ISA PnP: ");
			vga_write(temp_str);
			vga_write(" ");
			vga_write(cx->pnp_name);
		}
	}
#endif
}

static void show_device_info() {
	int c,rows=2,cols=70;
	struct vga_msg_box box;

	if (sb_card->is_gallant_sc6600 || sb_card->mega_em || sb_card->sbos)
		rows++;
	if (sb_card->pnp_id != 0 || sb_card->pnp_name != NULL)
		rows++;

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */
	draw_device_info(sb_card,box.x+2,box.y+1,cols-4,rows);

	while (1) {
		ui_anim(0);
		if (kbhit()) {
			c = vga_getch();

			if (c == 27 || c == 13)
				break;
		}
	}

	vga_msg_box_destroy(&box);
}

static void draw_sound_card_choice(unsigned int x,unsigned int y,unsigned int w,struct sndsb_ctx *cx,int sel) {
	const char *msg = cx->dsp_copyright;

	vga_moveto(x,y);
	if (cx->baseio != 0) {
		vga_write_color(sel ? 0x70 : 0x1F);
		sprintf(temp_str,"%03Xh IRQ%-2d DMA%d DMA%d MPU:%03Xh ",
			cx->baseio,	cx->irq,	cx->dma8,	cx->dma16,	cx->mpuio);
		vga_write(temp_str);
		while (vga_state.vga_pos_x < (x+w) && *msg != 0) vga_writec(*msg++);
	}
	else {
		vga_write_color(sel ? 0x70 : 0x18);
		vga_write("(none)");
	}
	while (vga_state.vga_pos_x < (x+w)) vga_writec(' ');
}
#endif

#ifdef LIVE_CFG
static const signed char sc6600_irq[] = { -1,5,7,9,10,11 };
static const signed char sc6600_dma[] = { -1,0,1,3 };

#if defined(TARGET_PC98)
static const signed char sb16_non_pnp_irq[] = { -1,3,5,10,12 };
static const signed char sb16_non_pnp_dma[] = { -1,0,3 };
static const signed char sb16_non_pnp_dma16[] = { -1,0,3 };
#else
static const signed char sb16_non_pnp_irq[] = { -1,2,5,7,10 };
static const signed char sb16_non_pnp_dma[] = { -1,0,1,3 };
static const signed char sb16_non_pnp_dma16[] = { -1,0,1,3,5,6,7 };
#endif

#ifdef ISAPNP
static const int16_t sb16_pnp_base[] = { 0x220,0x240,0x260,0x280 };
static const signed char sb16_pnp_irq[] = { -1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
static const signed char sb16_pnp_dma[] = { -1,0,1,3 };
static const signed char sb16_pnp_dma16[] = { -1,0,1,3,5,6,7 };
#endif

static const signed char ess_688_irq[] = { -1,5,7,9,10 }; /* NTS: The datasheet says 2/9/"all others". Since PCs map IRQ 2 to IRQ 9 we'll just say "IRQ 9" */
static const signed char ess_688_dma[] = { -1,0,1,3 };

static signed char ess_688_map_to_dma[4] = {-1, 0, 1, 3}; /* NTS: Not sure what the datasheet means by "all others" when value == 0x0 */
static signed char ess_688_map_to_irq[8] = { 9, 5, 7, 10}; /* NTS: Doesn't seem to be a way to say "no IRQ". value == 0x0 is 2/9/"all others" and IRQ 2 on ISA is mapped to IRQ 9, so.. */

struct conf_list_item {
	unsigned char	etype;
	unsigned char	name;
	unsigned char	setting;
	unsigned char	listlen;
	void*		list;
};

enum {
	ET_SCHAR,
	ET_SINTX
};

enum {
	ER_IRQ,		/* 0 */
	ER_DMA,
	ER_DMA16,
	ER_BASE
};

static const char *ER_NAMES[] = {
	"IRQ",		/* 0 */
	"DMA",
	"DMA16",
	"BASE"
};

static struct conf_list_item ess_688[] = {
	{ET_SCHAR,	ER_IRQ,		0,	sizeof(ess_688_irq),		(void*)ess_688_irq},
	{ET_SCHAR,	ER_DMA,		0,	sizeof(ess_688_dma),		(void*)ess_688_dma},
};

static struct conf_list_item sc6600[] = {
	{ET_SCHAR,	ER_IRQ,		0,	sizeof(sc6600_irq),		(void*)sc6600_irq},
	{ET_SCHAR,	ER_DMA,		0,	sizeof(sc6600_dma),		(void*)sc6600_dma},
};

static struct conf_list_item sb16_non_pnp[] = {
	{ET_SCHAR,	ER_IRQ,		0,	sizeof(sb16_non_pnp_irq),	(void*)sb16_non_pnp_irq},
	{ET_SCHAR,	ER_DMA,		0,	sizeof(sb16_non_pnp_dma),	(void*)sb16_non_pnp_dma},
	{ET_SCHAR,	ER_DMA16,	0,	sizeof(sb16_non_pnp_dma16),	(void*)sb16_non_pnp_dma16}
};

#ifdef ISAPNP
static struct conf_list_item sb16_pnp[] = {
	{ET_SINTX,	ER_BASE,	0,	sizeof(sb16_pnp_base)/2,	(void*)sb16_pnp_base},
	{ET_SCHAR,	ER_IRQ,		0,	sizeof(sb16_pnp_irq),		(void*)sb16_pnp_irq},
	{ET_SCHAR,	ER_DMA,		0,	sizeof(sb16_pnp_dma),		(void*)sb16_pnp_dma},
	{ET_SCHAR,	ER_DMA16,	0,	sizeof(sb16_pnp_dma16),		(void*)sb16_pnp_dma16}
};
#endif

static void conf_item_index_lookup(struct conf_list_item *item,int val) {
	if (item->etype == ET_SCHAR) {
		item->setting = 0;
		while (item->setting < item->listlen && ((signed char*)(item->list))[item->setting] != val)
			item->setting++;
	}
	else if (item->etype == ET_SINTX) {
		item->setting = 0;
		while (item->setting < item->listlen && ((int16_t*)(item->list))[item->setting] != val)
			item->setting++;
	}

	if (item->setting == item->listlen)
		item->setting = 0;
}

static int conf_sound_card_list(const char *title,struct conf_list_item *list,const int list_items,int width) {
	struct conf_list_item *li;
	unsigned char redraw = 1;
	unsigned char sel = 0,i;
	struct vga_msg_box box;
	int c;

	vga_msg_box_create(&box,title,2,width);
	do {
		if (redraw) {
			vga_moveto(box.x + 2,box.y + 2);

			for (i=0;i < list_items;i++) {
				li = list + i;
				vga_write_color(0x1E);
				vga_write(ER_NAMES[li->name]);
				vga_writec(':');
				vga_write_color(sel == i ? 0x70 : 0x1E);
				if (li->etype == ET_SCHAR) {
					signed char v = ((signed char*)(li->list))[li->setting];
					if (v >= 0) sprintf(temp_str,"%-2u",v);
					else sprintf(temp_str,"NA");
					vga_write(temp_str);
				}
				else if (li->etype == ET_SINTX) {
					int16_t v = ((int16_t*)(li->list))[li->setting];
					if (v >= 0) sprintf(temp_str,"%03x",v);
					else sprintf(temp_str,"NA ");
					vga_write(temp_str);
				}
				vga_write_color(0x1E);
				vga_writec(' ');
			}

			redraw=0;
		}

		ui_anim(0);
		if (kbhit()) {
			c = vga_getch();

			if (c == 27 || c == 13)
                break;
			else if (c == VGATTY_LEFT_ARROW) { /* left arrow */
				if (sel == 0) sel = list_items-1;
				else sel--;
				redraw=1;
			}
			else if (c == VGATTY_RIGHT_ARROW) { /* right arrow */
				if (sel == (list_items-1)) sel = 0;
				else sel++;
				redraw=1;
			}
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
				do {
					li = list + sel;
					if ((++li->setting) >= li->listlen) li->setting = 0;

					if (li->etype == ET_SCHAR) {
						signed char v = ((signed char*)(li->list))[li->setting];
						if (li->name == ER_DMA || li->name == ER_DMA16) {
							if (v >= 0 && v != sb_card->dma8 && v != sb_card->dma16 && sndsb_by_dma(v) != NULL)
								continue;
						}
						else if (li->name == ER_IRQ) {
							if (v >= 0 && v != sb_card->irq && sndsb_by_irq(v) != NULL)
								continue;
						}
					}
					else if (li->etype == ET_SINTX) {
						int16_t v = ((int16_t*)(li->list))[li->setting];
						if (li->name == ER_BASE) {
							if (v > 0 && v != sb_card->baseio && sndsb_by_base(v) != NULL)
								continue;
						}
					}

					break;
				} while (1);
				redraw = 1;
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
				do {
					li = list + sel;
					if (li->setting == 0) li->setting = li->listlen - 1;
					else li->setting--;

					if (li->etype == ET_SCHAR) {
						signed char v = ((signed char*)(li->list))[li->setting];
						if (li->name == ER_DMA || li->name == ER_DMA16) {
							if (v >= 0 && v != sb_card->dma8 && v != sb_card->dma16 && sndsb_by_dma(v) != NULL)
								continue;
						}
						else if (li->name == ER_IRQ) {
							if (v >= 0 && v != sb_card->irq && sndsb_by_irq(v) != NULL)
								continue;
						}
					}
					else if (li->etype == ET_SINTX) {
						int16_t v = ((int16_t*)(li->list))[li->setting];
						if (li->name == ER_BASE) {
							if (v > 0 && v != sb_card->baseio && sndsb_by_base(v) != NULL)
								continue;
						}
					}

					break;
				} while (1);
				redraw = 1;
			}
		}
	} while (1);

	vga_msg_box_destroy(&box);
	return (c == 13);
}

static void conf_sound_card() {
	/* VDMSOUND emulates the mixer byte that reports config, but it gets confused when you change it */
	if (sb_card->vdmsound) {
	}
	/* Creative SB16 drivers for Windows: Despite the ability to do so, it's probably not a good idea
	 *                                    to change the configuration out from under it */
	else if (sb_card->windows_creative_sb16_drivers) {
	}
	else if (sb_card->ess_extensions && sb_card->ess_chipset == SNDSB_ESS_688) {
		/* ESS control registers also allow changing IRQ/DMA, at least on non-PnP versions.
		 * It's also possible to change the base I/O, even if the PnP protocol is not available,
		 * but that's a bit too iffy at the moment to play with.
		 *
		 * TODO: This configuration method is said not to work if the ESS 688 is a ISA PnP
		 * device. So far the only ESS 688 have is in a laptop where the chip is "embedded"
		 * into the motherboard and is "Plug & Play" only in that the PnP BIOS reports it
		 * in device node enumeration, yet I am apparently able to change IRQ/DMA resources
		 * this way. UPDATE: Well, it turns out the chip lets me change the IRQ, but the
		 * DMA channel seems to be fixed (unchangeable). */
		conf_item_index_lookup(&ess_688[0]/*IRQ*/,sb_card->irq);
		conf_item_index_lookup(&ess_688[1]/*DMA*/,sb_card->dma8);
		if (conf_sound_card_list("ESS 688",ess_688,sizeof(ess_688)/sizeof(ess_688[0]),56)) {
			signed char IRQ = ess_688_irq[ess_688[0].setting];/*IRQ*/
			signed char DMA8 = ess_688_dma[ess_688[1].setting];/*DMA*/
			unsigned char do_stop_start = wav_playing;
			unsigned char irq_i=0,dma_i=0;
			int tmp;

			if (do_stop_start) stop_play();

			if (sb_card->irq != -1) {
				_dos_setvect(irq2int(sb_card->irq),old_irq);
				if (old_irq_masked) p8259_mask(sb_card->irq);
			}

			if (DMA8 > 3) DMA8 = -1;

			while (dma_i < 3 && ess_688_map_to_dma[dma_i] != DMA8) dma_i++;
			while (irq_i < 7 && ess_688_map_to_irq[irq_i] != IRQ) irq_i++;

			/* byte expansion:
			 *
			 *   00 = 0000
			 *   01 = 0101
			 *   10 = 1010
			 *   11 = 1111
			 */
			dma_i = (dma_i & 3) | ((dma_i & 3) << 2);
			irq_i = (irq_i & 3) | ((irq_i & 3) << 2);

			/* write the ESS registers to make the change */
			/* NTS: on later chipsets (PnP) these are readonly */
			tmp = sndsb_ess_read_controller(sb_card,0xB1); /* interrupt control */
			if (tmp >= 0) {
				tmp = (tmp & 0xF0) + irq_i;
				sndsb_ess_write_controller(sb_card,0xB1,(unsigned char)tmp);
			}

			tmp = sndsb_ess_read_controller(sb_card,0xB2); /* DMA control */
			if (tmp >= 0) {
				tmp = (tmp & 0xF0) + dma_i;
				sndsb_ess_write_controller(sb_card,0xB2,(unsigned char)tmp);
			}
			sndsb_reset_dsp(sb_card);

			/* now... some registers might actually be readonly.
			 * we'll find out when we readback and find which ones have changed. */
			tmp = sndsb_ess_read_controller(sb_card,0xB1); /* interrupt control */
			if (tmp >= 0) {
				if ((tmp&3) == ((tmp>>2)&3))
					IRQ = ess_688_map_to_irq[tmp & 3];
			}
			tmp = sndsb_ess_read_controller(sb_card,0xB2); /* DMA control */
			if (tmp >= 0) {
				if ((tmp&3) == ((tmp>>2)&3))
					DMA8 = ess_688_map_to_dma[tmp & 3];
			}

			/* then the library needs to be updated */
			/* TODO: there should be a sndsb_ call to do this! */
			sb_card->dma8 = DMA8;
			sb_card->dma16 = DMA8;
			sb_card->irq = IRQ;

			if (sb_card->irq != -1) {
				old_irq_masked = p8259_is_masked(sb_card->irq);
				old_irq = _dos_getvect(irq2int(sb_card->irq));
				if (vector_is_iret(irq2int(sb_card->irq)))
					old_irq_masked = 1;

				_dos_setvect(irq2int(sb_card->irq),sb_irq);
				p8259_unmask(sb_card->irq);
			}

			if (do_stop_start) begin_play();
		}
		return;
	}
#ifdef ISAPNP
	/* -------- ISA Plug & Play --------- */
	else if (sb_card->pnp_id != 0) {
		if (ISAPNP_ID_FMATCH(sb_card->pnp_id,'C','T','L')) {
			conf_item_index_lookup(&sb16_pnp[0]/*BASE*/,sb_card->baseio);
			conf_item_index_lookup(&sb16_pnp[1]/*IRQ*/,sb_card->irq);
			conf_item_index_lookup(&sb16_pnp[2]/*DMA*/,sb_card->dma8);
			conf_item_index_lookup(&sb16_pnp[3]/*DMA16*/,sb_card->dma16);
			if (conf_sound_card_list("Creative Sound Blaster 16 PnP configuration",sb16_pnp,sizeof(sb16_pnp)/sizeof(sb16_pnp[0]),56)) {
				int16_t BASE = sb16_pnp_base[sb16_pnp[0].setting];/*BASE*/
				signed char IRQ = sb16_pnp_irq[sb16_pnp[1].setting];/*IRQ*/
				signed char DMA8 = sb16_pnp_dma[sb16_pnp[2].setting];/*DMA*/
				signed char DMA16 = sb16_pnp_dma16[sb16_pnp[3].setting];/*DMA16*/
				unsigned char do_stop_start = wav_playing;

				if (do_stop_start) stop_play();
				isa_pnp_init_key();
				isa_pnp_wake_csn(sb_card->pnp_csn);

				if (sb_card->irq != -1) {
					_dos_setvect(irq2int(sb_card->irq),old_irq);
					if (old_irq_masked) p8259_mask(sb_card->irq);
				}

				if (DMA8 > 3) DMA8 = -1;

				/* disable the device IO (0) */
				isa_pnp_write_address(0x07);	/* log device select */
				isa_pnp_write_data(0x00);	/* main device */

				isa_pnp_write_data_register(0x30,0x00);	/* activate: bit 0 */
				isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

				isa_pnp_write_io_resource(0,BASE >= 0 ? BASE : 0);
				isa_pnp_write_dma(0,DMA8 >= 0 ? DMA8 : 4); /* setting the DMA field to "4" is how you unassign the resource */
				isa_pnp_write_dma(1,DMA16 >= 0 ? DMA16 : 4);
				isa_pnp_write_irq(0,IRQ > 0 ? IRQ : 0); /* setting the IRQ field to "0" is how you unassign the resource */
				isa_pnp_write_irq_mode(0,2);	/* edge level high */

				/* enable the device IO */
				isa_pnp_write_data_register(0x30,0x01);	/* activate: bit 0 */
				isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

				isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */

				/* then the library needs to be updated */
				/* TODO: there should be a sndsb_ call to do this! */
				sb_card->baseio = BASE;
				sb_card->dma8 = DMA8;
				sb_card->dma16 = DMA16;
				sb_card->irq = IRQ;

				if (sb_card->irq != -1) {
					old_irq_masked = p8259_is_masked(sb_card->irq);
					old_irq = _dos_getvect(irq2int(sb_card->irq));
					if (vector_is_iret(irq2int(sb_card->irq)))
						old_irq_masked = 1;

					_dos_setvect(irq2int(sb_card->irq),sb_irq);
					p8259_unmask(sb_card->irq);
				}

				if (do_stop_start) begin_play();
			}
			return;
		}
	}
#endif
	/* -------- Non Plug & Play --------- */
	else if (sb_card->dsp_vmaj == 4) {
		/* SB16 non-pnp can reassign resources when
		   writing mixer bytes 0x80, 0x81 */
		conf_item_index_lookup(&sb16_non_pnp[0]/*IRQ*/,sb_card->irq);
		conf_item_index_lookup(&sb16_non_pnp[1]/*DMA*/,sb_card->dma8);
		conf_item_index_lookup(&sb16_non_pnp[2]/*DMA16*/,sb_card->dma16);
		if (conf_sound_card_list("Creative Sound Blaster 16 configuration",sb16_non_pnp,sizeof(sb16_non_pnp)/sizeof(sb16_non_pnp[0]),56)) {
			signed char IRQ = sb16_non_pnp_irq[sb16_non_pnp[0].setting];/*IRQ*/
			signed char DMA8 = sb16_non_pnp_dma[sb16_non_pnp[1].setting];/*DMA*/
			signed char DMA16 = sb16_non_pnp_dma16[sb16_non_pnp[2].setting];/*DMA16*/
			unsigned char do_stop_start = wav_playing,c;
			if (do_stop_start) stop_play();

			if (sb_card->irq != -1) {
				_dos_setvect(irq2int(sb_card->irq),old_irq);
				if (old_irq_masked) p8259_mask(sb_card->irq);
			}

			if (DMA8 < 0) DMA16 = -1;
			if (DMA8 > 3) DMA8 = -1;

			/* as seen on real Creative SB16 hardware:
			   the 16-bit DMA channel must be either 5, 6, 7,
			   or must match the 8-bit DMA channel. */
			if (DMA16 < 4 && DMA16 >= 0)
				DMA16 = DMA8;

			/* apply changes */
			c = 0;
#if defined(TARGET_PC98)
			if (IRQ == 3)			c |= 0x01;
			else if (IRQ == 5)		c |= 0x08;
			else if (IRQ == 10)		c |= 0x02;
			else if (IRQ == 12)		c |= 0x04;
			else				IRQ = -1;
			sndsb_write_mixer(sb_card,0x80,c);

			c = 0;
			if (DMA8 == 0)			c |= 0x01;
			else if (DMA8 == 3)		c |= 0x02;
			else				DMA8 = -1;
#else
			if (IRQ == 2)			c |= 0x01;
			else if (IRQ == 5)		c |= 0x02;
			else if (IRQ == 7)		c |= 0x04;
			else if (IRQ == 10)		c |= 0x08;
			else				IRQ = -1;
			sndsb_write_mixer(sb_card,0x80,c);

			c = 0;
			if (DMA8 == 0)			c |= 0x01;
			else if (DMA8 == 1)		c |= 0x02;
			else if (DMA8 == 3)		c |= 0x08;
			else				DMA8 = -1;
#endif

			/* NTS: From the Creative programming guide:
			 *      "DSP version 4.xx also supports the transfer of 16-bit sound data through
			 *       8-bit DMA channel. To make this possible, set all 16-bit DMA channel bits
			 *       to 0 leaving only 8-bit DMA channel set"
			 *
			 *      Also as far as I can tell there's really no way to assign
			 *      an 8-bit DMA without either assigning to 5,6,7 or matching
			 *      the 8-bit DMA channel */
			if (DMA16 == 5)			c |= 0x20;
			else if (DMA16 == 6)		c |= 0x40;
			else if (DMA16 == 7)		c |= 0x80;
			else				DMA16 = DMA8;
			sndsb_write_mixer(sb_card,0x81,c);

			/* then the library needs to be updated */
			/* TODO: there should be a sndsb_ call to do this! */
			sb_card->dma8 = DMA8;
			sb_card->dma16 = DMA16;
			sb_card->irq = IRQ;

			if (sb_card->irq != -1) {
				old_irq_masked = p8259_is_masked(sb_card->irq);
				old_irq = _dos_getvect(irq2int(sb_card->irq));
				if (vector_is_iret(irq2int(sb_card->irq)))
					old_irq_masked = 1;

				_dos_setvect(irq2int(sb_card->irq),sb_irq);
				p8259_unmask(sb_card->irq);
			}

			if (do_stop_start) begin_play();
		}
		return;
	}
	else if (sb_card->is_gallant_sc6600) {
		/* the Gallant SC-6600 has it's own weird "plug & play"
		   configuration method, although the base I/O is not
		   software configurable */
		conf_item_index_lookup(&sc6600[0]/*IRQ*/,sb_card->irq);
		conf_item_index_lookup(&sc6600[1]/*DMA*/,sb_card->dma8);
		if (conf_sound_card_list("SC-4000 configuration",sc6600,sizeof(sc6600)/sizeof(sc6600[0]),56)) {
			signed char IRQ = sc6600_irq[sc6600[0].setting];/*IRQ*/
			signed char DMA8 = sc6600_dma[sc6600[1].setting];/*DMA*/
			unsigned char do_stop_start = wav_playing;
			unsigned char irq_i=0,dma_i=0,cfg=0;

			if (do_stop_start) stop_play();

			if (sb_card->irq != -1) {
				_dos_setvect(irq2int(sb_card->irq),old_irq);
				if (old_irq_masked) p8259_mask(sb_card->irq);
			}

			if (DMA8 > 3) DMA8 = -1;

			while (dma_i < 3 && gallant_sc6600_map_to_dma[dma_i] != DMA8) dma_i++;
			while (irq_i < 7 && gallant_sc6600_map_to_irq[irq_i] != IRQ) irq_i++;

			cfg &= ~(3 << 0);
			cfg |= dma_i << 0;
			cfg &= ~(7 << 3);
			cfg |= irq_i << 3;

			sndsb_reset_dsp(sb_card);

			/* write config byte */
			sndsb_write_dsp(sb_card,0x50);
			sndsb_write_dsp(sb_card,cfg);
			sndsb_write_dsp(sb_card,0xF2);

			sndsb_write_dsp(sb_card,0x50);
			sndsb_write_dsp(sb_card,cfg);
			sndsb_write_dsp(sb_card,0xE6);

			sndsb_reset_dsp(sb_card);

			sndsb_write_dsp(sb_card,0x50);
			sndsb_write_dsp(sb_card,cfg);
			sndsb_write_dsp(sb_card,0xF2);

			sndsb_write_dsp(sb_card,0x50);
			sndsb_write_dsp(sb_card,cfg);
			sndsb_write_dsp(sb_card,0xE6);

			sndsb_reset_dsp(sb_card);

			/* then the library needs to be updated */
			/* TODO: there should be a sndsb_ call to do this! */
			sb_card->dma8 = DMA8;
			sb_card->dma16 = DMA8;
			sb_card->irq = IRQ;

			if (sb_card->irq != -1) {
				old_irq_masked = p8259_is_masked(sb_card->irq);
				old_irq = _dos_getvect(irq2int(sb_card->irq));
				if (vector_is_iret(irq2int(sb_card->irq)))
					old_irq_masked = 1;

				_dos_setvect(irq2int(sb_card->irq),sb_irq);
				p8259_unmask(sb_card->irq);
			}

			if (do_stop_start) begin_play();
		}
		return;
	}

	{
		int c;
		struct vga_msg_box box;
		vga_msg_box_create(&box,"Your sound card is not software configurable,\nor does not have any method I know of to do so.",0,0);
		do {
			ui_anim(0);
			if (kbhit()) {
				c = vga_getch();
			}
			else {
				c = -1;
			}
		} while (!(c == 13 || c == 10));
		vga_msg_box_destroy(&box);
	}
}

static void choose_sound_card() {
	int c,rows=3+1+SNDSB_MAX_CARDS,cols=70,sel=0,i;
	unsigned char wp = wav_playing;
	struct sndsb_ctx *card;
	struct vga_msg_box box;

	if (sb_card->is_gallant_sc6600 || sb_card->mega_em || sb_card->sbos)
		rows++;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		card = &sndsb_card[i];
		if (card == sb_card) sel = i;
	}

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */
	draw_device_info(sb_card,box.x+2,box.y+1+rows-3,cols,3);
	for (i=0;i < SNDSB_MAX_CARDS;i++)
		draw_sound_card_choice(box.x+2,box.y+1+i,cols,&sndsb_card[i],i == sel);

	card = NULL;
	while (1) {
		ui_anim(0);
		if (kbhit()) {
			c = vga_getch();

			if (c == 27) {
				card = NULL;
				break;
			}
			else if (c == 13) {
				card = &sndsb_card[sel];
				if (card->baseio != 0) break;
				card = NULL;
			}
			else if (c == VGATTY_UP_ARROW) { /* up arrow */
				draw_sound_card_choice(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],0);
				if (sel == 0) sel = SNDSB_MAX_CARDS - 1;
				else sel--;
				draw_sound_card_choice(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],1);
				draw_device_info(&sndsb_card[sel],box.x+2,box.y+1+rows-3,cols,3);
			}
			else if (c == VGATTY_DOWN_ARROW) { /* down arrow */
				draw_sound_card_choice(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],0);
				if (++sel == SNDSB_MAX_CARDS) sel = 0;
				draw_sound_card_choice(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],1);
				draw_device_info(&sndsb_card[sel],box.x+2,box.y+1+rows-3,cols,3);
			}
		}
	}

	if (card != NULL) {
		stop_play();
		if (sb_card->irq != -1) {
			_dos_setvect(irq2int(sb_card->irq),old_irq);
			if (old_irq_masked) p8259_mask(sb_card->irq);
		}

		sb_card = card;
		sndsb_assign_dma_buffer(sb_card,sb_dma);
		if (sb_card->irq != -1) {
			old_irq_masked = p8259_is_masked(sb_card->irq);
			if (vector_is_iret(irq2int(sb_card->irq)))
				old_irq_masked = 1;

			old_irq = _dos_getvect(irq2int(sb_card->irq));
			_dos_setvect(irq2int(sb_card->irq),sb_irq);
			p8259_unmask(sb_card->irq);
		}

		if (wp) begin_play();
	}

	vga_msg_box_destroy(&box);
}
#endif

#ifdef INCLUDE_FX
static void fx_reset() {
	fx_volume = 256;
	fx_echo_delay = 0;
	fx_echo_free();
	if (wav_playing) {
		stop_play();
		begin_play();
	}
}

static void fx_vol_echo() {
	struct vga_msg_box box;
	unsigned char redraw=1;
	int c;

	vga_msg_box_create(&box,
		"Echo effect\n"
		"Use +/- to adjust, R to reset to off",3,0);
	while (1) {
		if (redraw) {
			vga_moveto(box.x+2,box.y+4);
			vga_write_color(0x1E);
			if (fx_echo_delay == 0)
				vga_write("(off)              ");
			else {
				sprintf(temp_str,"%u           ",fx_echo_delay);
				vga_write(temp_str);
			}
			redraw=0;
		}

		ui_anim(0);
		if (kbhit()) {
			c = vga_getch();

			if (c == 27 || c == 13)
				break;
			else if (c == 'r') {
				fx_echo_delay = 0;
				redraw = 1;
			}
			else if (c == '-') {
				if (fx_echo_delay > 0) {
					fx_echo_delay--;
					redraw = 1;
				}
			}
			else if (c == '_') {
				if (fx_echo_delay > 64) {
					fx_echo_delay -= 64;
					redraw = 1;
				}
				else if (fx_echo_delay > 0) {
					fx_echo_delay = 0;
					redraw = 1;
				}
			}
			else if (c == '=') {
				fx_echo_delay++;
				redraw = 1;
			}
			else if (c == '+') {
				fx_echo_delay += 128;
				redraw = 1;
			}
		}
	}
	vga_msg_box_destroy(&box);
}

static void fx_vol_dialog() {
	struct vga_msg_box box;
	unsigned char redraw=1;
	int c;

	vga_msg_box_create(&box,
		"Volume effect\n"
		"Use +/- to adjust, R to reset to 100%",3,0);
	while (1) {
		if (redraw) {
			unsigned long i = ((unsigned long)fx_volume * 1000UL) / 256UL;
			vga_moveto(box.x+2,box.y+4);
			vga_write_color(0x1E);
			sprintf(temp_str,"%u.%03u %s    ",
				(unsigned int)(i / 1000UL),
				(unsigned int)(i % 1000UL),
				(fx_volume == 256 ? "(disable)" : "         "));
			vga_write(temp_str);
			redraw=0;
		}

		ui_anim(0);
		if (kbhit()) {
			c = vga_getch();

			if (c == 27 || c == 13)
				break;
			else if (c == 'r') {
				fx_volume = 256;
				redraw = 1;
			}
			else if (c == '-') {
				if (fx_volume > 0) {
					fx_volume--;
					redraw = 1;
				}
			}
			else if (c == '_') {
				if (fx_volume > 32) {
					fx_volume -= 32;
					redraw = 1;
				}
				else if (fx_volume > 0) {
					fx_volume = 0;
					redraw = 1;
				}
			}
			else if (c == '=') {
				fx_volume++;
				redraw = 1;
			}
			else if (c == '+') {
				fx_volume += 64;
				redraw = 1;
			}
		}
	}
	vga_msg_box_destroy(&box);
}
#endif

int main(int argc,char **argv) {
	uint32_t sb_irq_pcounter = 0;
	int i,loop,redraw,bkgndredraw,cc;
	const struct vga_menu_item *mitem = NULL;
	unsigned char assume_dma = 0;
	int disable_probe = 0;
#ifdef ISAPNP
	int disable_pnp = 0;
#endif
	int disable_env = 0;
	int force_ddac = 0;
	VGA_ALPHA_PTR vga;
	int autoplay = 0;
	int sc_idx = -1;

	printf("Sound Blaster test program\n");
	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			unsigned char m = *a++;
			while (*a == m) a++;

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"ex-ess")) {
				sndsb_probe_options.experimental_ess = 1;
			}
			else if (!strcmp(a,"noess")) {
				sndsb_probe_options.disable_ess_extensions = 1;
			}
			else if (!strcmp(a,"adma")) {
				assume_dma = 1;
			}
			else if (!strcmp(a,"sbalias:dsp")) {
				sndsb_probe_options.use_dsp_alias = 1;
			}
			else if (!strcmp(a,"nif2")) {
				irq_probe_f2 = 0;
			}
			else if (!strcmp(a,"ni80")) {
				irq_probe_80 = 0;
			}
			else if (!strcmp(a,"nde2")) {
				dma_probe_e2 = 0;
			}
			else if (!strcmp(a,"nd14")) {
				dma_probe_14 = 0;
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
            else if (!strcmp(a,"srf4xx")) {
                force_srate_4xx = 1;
            }
            else if (!strcmp(a,"dma128")) {
                dma128 = 1;
            }
            else if (!strcmp(a,"-dma128")) {
                dma128 = -1;
            }
            else if (!strncmp(a,"dmam:",5)) {
                a += 5;

                if (*a == 'u' || *a == 'a' || *a == 'p')
                    dmam = *a;
                else
                    return 1;
            }
#endif
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
            else if (!strcmp(a,"noasp")) {
                ignore_asp = 1;
            }
            else if (!strcmp(a,"srftc")) {
                force_srate_tc = 1;
            }
			else if (!strcmp(a,"stopres")) {
				always_reset_dsp_to_stop = 1;
			}
			else if (!strcmp(a,"hinoblk")) {
				assume_dsp_hispeed_doesnt_block = 1;
			}
			else if (!strcmp(a,"dmaaio")) {
				dma_autoinit_override = 1;
			}
#endif
			else if (!strcmp(a,"fip")) {
				force_irq_probe = 1;
			}
			else if (!strcmp(a,"fdp")) {
				force_dma_probe = 1;
			}
			else if (!strcmp(a,"noidle")) {
				dont_sb_idle = 1;
			}
			else if (!strcmp(a,"nochain")) {
				dont_chain_irq = 1;
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			else if (!strcmp(a,"debug")) {
				sb_debug = 1;
			}
            else if (!strcmp(a,"96k")) {
                buffer_limit = 96UL * 1024UL;
            }
#endif
            else if (!strcmp(a,"64k")) {
                buffer_limit = 64UL * 1024UL;
            }
            else if (!strcmp(a,"63k")) {
                buffer_limit = 63UL * 1024UL;
            }
            else if (!strcmp(a,"48k")) {
                buffer_limit = 48UL * 1024UL;
            }
            else if (!strcmp(a,"32k")) {
                buffer_limit = 32UL * 1024UL;
            }
            else if (!strcmp(a,"16k")) {
                buffer_limit = 16UL * 1024UL;
            }
			else if (!strcmp(a,"8k")) {
				buffer_limit = 8UL * 1024UL;
			}
			else if (!strcmp(a,"4k")) {
				buffer_limit = 4UL * 1024UL;
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			else if (!strcmp(a,"-nmi")) {
# if TARGET_MSDOS == 32
				sndsb_nmi_32_hook = 0;
# endif
			}
			else if (!strcmp(a,"+nmi")) {
# if TARGET_MSDOS == 32
				sndsb_nmi_32_hook = 1;
# endif
			}
#endif
#ifdef ISAPNP
			else if (!strcmp(a,"nopnp")) {
				disable_pnp = 1;
			}
#endif
			else if (!strncmp(a,"wav=",4)) {
				a += 4;
				strcpy(wav_file,a);
			}
			else if (!strcmp(a,"play")) {
				autoplay = 1;
			}
			else if (!strncmp(a,"sc=",3)) {
				a += 3;
				sc_idx = strtol(a,NULL,0);
			}
			else if (!strcmp(a,"noprobe")) {
				disable_probe = 1;
			}
			else if (!strcmp(a,"noenv")) {
				disable_env = 1;
			}
			else if (!strcmp(a,"ddac")) {
				force_ddac = 1;
			}
			else if (!strcmp(a,"nomirqp")) {
				sndsb_probe_options.disable_manual_irq_probing = 1;
			}
			else if (!strcmp(a,"noairqp")) {
				sndsb_probe_options.disable_alt_irq_probing = 1;
			}
			else if (!strcmp(a,"nosb16cfg")) {
				sndsb_probe_options.disable_sb16_read_config_byte = 1;
			}
			else if (!strcmp(a,"nodmap")) {
				sndsb_probe_options.disable_manual_dma_probing = 1;
			}
			else if (!strcmp(a,"nohdmap")) {
				sndsb_probe_options.disable_manual_high_dma_probing = 1;
			}
			else if (!strcmp(a,"nowinvxd")) {
				sndsb_probe_options.disable_windows_vxd_checks = 1;
			}
			else {
				help();
				return 1;
			}
		}
	}

	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}

	/* TODO: Make sure this code still runs reliably without DMA (Direct DAC etc) if DMA not available! */
	if (!probe_8237())
		printf("WARNING: Cannot init 8237 DMA\n");

	if (assume_dma)
		d8237_flags |= D8237_DMA_PRIMARY | D8237_DMA_SECONDARY;

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
    if (dma128 == 1)
        d8237_enable_16bit_128kb_dma();
    else if (dma128 == -1)
        d8237_disable_16bit_128kb_dma();

    if (dmam == 'u')
        d8237_16bit_set_unknown_mask();
    else if (dmam == 'a')
        d8237_16bit_set_addr_mask();
    else if (dmam == 'p')
        d8237_16bit_set_page_mask();

	printf("DMA available: 0-3=%s 4-7=%s mask8=0x%lx mask16=0x%lx 128k=%u\n",
		d8237_flags&D8237_DMA_PRIMARY?"yes":"no",
		d8237_flags&D8237_DMA_SECONDARY?"yes":"no",
        d8237_8bit_limit_mask(),
        d8237_16bit_limit_mask(),
        d8237_can_do_16bit_128k()?1:0);
#endif

	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_sndsb()) {
		printf("Cannot init library\n");
		return 1;
	}

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
	/* it's up to us now to tell it certain minor things */
	sndsb_detect_virtualbox();		// whether or not we're running in VirtualBox
	/* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
	sndsb_enable_sb16_support();		// SB16 support
	sndsb_enable_sc400_support();		// SC400 support
	sndsb_enable_ess_audiodrive_support();	// ESS AudioDrive support
#endif

#if TARGET_MSDOS == 32
	if (sndsb_nmi_32_hook > 0) /* it means the NMI hook is taken */
		printf("Sound Blaster NMI hook/reflection active\n");

# if !defined(TARGET_PC98)
	if (gravis_mega_em_detect(&megaem_info)) {
		/* let the user know we're a 32-bit program and MEGA-EM's emulation
		 * won't work with 32-bit DOS programs */
		printf("WARNING: Gravis MEGA-EM detected. Sound Blaster emulation doesn't work\n");
		printf("         with 32-bit protected mode programs (like myself). If you want\n");
		printf("         to test it's Sound Blaster emulation use the 16-bit real mode\n");
		printf("         builds instead.\n");
	}
	if (gravis_sbos_detect() >= 0) {
		printf("WARNING: Gravis SBOS emulation is not 100%% compatible with 32-bit builds.\n");
		printf("         It may work for awhile, but eventually the simulated IRQ will go\n");
		printf("         missing and playback will stall. Please consider using the 16-bit\n");
		printf("         real-mode builds instead. When a workaround is possible, it will\n");
		printf("         be implemented and this warning will be removed.\n");
	}
# endif
#elif TARGET_MSDOS == 16
# if defined(__LARGE__)
#  if !defined(TARGET_PC98)
	if (gravis_sbos_detect() >= 0) {
		printf("WARNING: 16-bit large model builds of the SNDSB program have a known, but not\n");
		printf("         yet understood incompatability with Gravis SBOS emulation. Use the\n");
		printf("         dos86s, dos86m, or dos86c builds.\n");
	}
#  endif
# endif
#endif
#ifdef ISAPNP
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
	if (!disable_pnp) {
		if (find_isa_pnp_bios()) {
			int ret;
			char tmp[192];
			unsigned int j,nodesize=0;
			const char *whatis = NULL;
			unsigned char csn,node=0,numnodes=0xFF,data[192];

			memset(data,0,sizeof(data));
			if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
				struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
				isapnp_probe_next_csn = nfo->total_csn;
				isapnp_read_data = nfo->isa_pnp_port;
			}
			else {
				printf("  ISA PnP BIOS failed to return configuration info\n");
			}

			/* enumerate device nodes reported by the BIOS */
			if (isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize) == 0 && numnodes != 0xFF && nodesize <= sizeof(devnode_raw)) {
				for (node=0;node != 0xFF;) {
					struct isa_pnp_device_node far *devn;
					unsigned char this_node;

					/* apparently, start with 0. call updates node to
					 * next node number, or 0xFF to signify end */
					this_node = node;
					if (isa_pnp_bios_get_sysdev_node(&node,devnode_raw,ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW) != 0) break;

					devn = (struct isa_pnp_device_node far*)devnode_raw;
					if (isa_pnp_is_sound_blaster_compatible_id(devn->product_id,&whatis)) {
						isa_pnp_product_id_to_str(tmp,devn->product_id);
						if ((ret = sndsb_try_isa_pnp_bios(devn->product_id,this_node,devn,sizeof(devnode_raw))) <= 0)
							printf("ISA PnP BIOS: error %d for %s '%s'\n",ret,tmp,whatis);
						else
							printf("ISA PnP BIOS: found %s '%s'\n",tmp,whatis);
					}
				}
			}

			/* enumerate the ISA bus directly */
			if (isapnp_read_data != 0) {
				printf("Scanning ISA PnP devices...\n");
				for (csn=1;csn < 255;csn++) {
					isa_pnp_init_key();
					isa_pnp_wake_csn(csn);

					isa_pnp_write_address(0x06); /* CSN */
					if (isa_pnp_read_data() == csn) {
						/* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
						/* if we don't, then we only read back the resource data */
						isa_pnp_init_key();
						isa_pnp_wake_csn(csn);

						for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();

						if (isa_pnp_is_sound_blaster_compatible_id(*((uint32_t*)data),&whatis)) {
							isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
							if ((ret = sndsb_try_isa_pnp(*((uint32_t*)data),csn)) <= 0)
								printf("ISA PnP: error %d for %s '%s'\n",ret,tmp,whatis);
							else
								printf("ISA PnP: found %s '%s'\n",tmp,whatis);
						}
					}

					/* return back to "wait for key" state */
					isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
				}
			}
		}
	}
#endif
	/* Non-plug & play scan */
	if (!disable_env && sndsb_try_blaster_var() != NULL) {
		printf("Created card ent. for BLASTER variable. IO=%X MPU=%X DMA=%d DMA16=%d IRQ=%d\n",
			sndsb_card_blaster->baseio,
			sndsb_card_blaster->mpuio,
			sndsb_card_blaster->dma8,
			sndsb_card_blaster->dma16,
			sndsb_card_blaster->irq);
		if (!sndsb_init_card(sndsb_card_blaster)) {
			printf("Nope, didn't work\n");
			sndsb_free_card(sndsb_card_blaster);
		}
	}
	if (!disable_probe) {
#if defined(TARGET_PC98)
		if (sndsb_try_base(0xD2))
			printf("Also found one at 0xD2\n");
		if (sndsb_try_base(0xD4))
			printf("Also found one at 0xD4\n");
#else
		if (sndsb_try_base(0x220))
			printf("Also found one at 0x220\n");
		if (sndsb_try_base(0x240))
			printf("Also found one at 0x240\n");
#endif
	}

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	/* There is a known issue with NTVDM.EXE Sound Blaster emulation under Windows XP. Not only
	 * do we get stuttery audio, but at some random point (1-5 minutes of continuous playback)
	 * the DOS VM crashes up for some unknown reason (VM is hung). */
	if (windows_mode == WINDOWS_NT) {
		struct sndsb_ctx *cx = sndsb_index_to_ctx(0);
		if (cx != NULL && cx->baseio != 0) {
			if (cx->windows_emulation && cx->windows_xp_ntvdm) {
				printf("WARNING: Windows XP/Vista/7 NTVDM.EXE emulation detected.\n");
				printf("         There is a known issue with NTVDM.EXE emulation that causes\n");
				printf("         playback to stutter, and if left running long enough, causes\n");
				printf("         this program to lock up and freeze.\n");
				printf("         If you must use this program under Windows XP, please consider\n");
				printf("         installing VDMSOUND and running this program within the VDMSOUND\n");
				printf("         environment.\n");
			}
		}
	}
#endif

	/* init card no longer probes the mixer */
	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
		if (cx->baseio == 0) continue;

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
		if (!cx->mixer_probed)
			sndsb_probe_mixer(cx);
#endif

		if (force_irq_probe) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (sb_debug) printf("SB %03x: Forgetting IRQ, forcing probe\n",cx->baseio);
#endif
			cx->irq = -1;
		}
		if (force_dma_probe) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (sb_debug) printf("SB %03x: Forgetting DMA, forcing probe\n",cx->baseio);
#endif
			cx->dma8 = cx->dma16 = -1;
		}

		if (cx->irq < 0 && irq_probe_f2) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (sb_debug) printf("SB %03x: Probing IRQ (F2)...\n",cx->baseio);
#endif
			sndsb_probe_irq_F2(cx);
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (cx->irq >= 0 && sb_debug) printf("SB %03x: Probing (F2) found IRQ %d\n",cx->baseio,cx->irq);
#endif
		}
		if (cx->irq < 0 && irq_probe_80) {
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (sb_debug) printf("SB %03x: Probing IRQ (80)...\n",cx->baseio);
#endif
			sndsb_probe_irq_80(cx);
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (cx->irq >= 0 && sb_debug) printf("SB %03x: Probing (80) found IRQ %d\n",cx->baseio,cx->irq);
#endif
		}
		if (cx->dma8 < 0 && dma_probe_e2) { // NTS: for some cards, this will also set the 16-bit DMA channel
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (sb_debug) printf("SB %03x: Probing DMA (E2)...\n",cx->baseio);
#endif
			sndsb_probe_dma8_E2(cx);
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (cx->dma8 >= 0 && sb_debug) printf("SB %03x: Probing (E2) found DMA %d\n",cx->baseio,cx->dma8);
#endif
		}
		if (cx->dma8 < 0 && dma_probe_14) { // NTS: for some cards, this will also set the 16-bit DMA channel
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (sb_debug) printf("SB %03x: Probing DMA (14)...\n",cx->baseio);
#endif
			sndsb_probe_dma8_14(cx);
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
			if (cx->dma8 >= 0 && sb_debug) printf("SB %03x: Probing (14) found DMA %d\n",cx->baseio,cx->dma8);
#endif
		}

		// DMA auto-init override.
		// WARNING: Single-cycle DSP commands with auto-init DMA can cause problems with SB clones or emulation.
		//   - Sound Blaster Live! (EMU10K1 PCI cards) will play the audio extra-fast
		//   - Pro Audio Spectrum cards will have occasional pops and crackles (confirmed on PAS16)
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
		if (dma_autoinit_override) cx->dsp_autoinit_dma_override = 1;
		if (assume_dsp_hispeed_doesnt_block) cx->hispeed_blocking = 0;
		if (always_reset_dsp_to_stop) cx->always_reset_dsp_to_stop = 1;
        if (force_srate_4xx) cx->srate_force_dsp_4xx = 1;
        if (force_srate_tc) cx->srate_force_dsp_tc = 1;
#endif

		// having IRQ and DMA changes the ideal playback method and capabilities
		sndsb_update_capabilities(cx);
		sndsb_determine_ideal_dsp_play_method(cx);
	}

	if (sc_idx < 0) {
		int count=0;
		for (i=0;i < SNDSB_MAX_CARDS;i++) {
			const char *ess_str;
			const char *mixer_str;

			struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
			if (cx->baseio == 0) continue;

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			mixer_str = sndsb_mixer_chip_str(cx->mixer_chip);
			ess_str = sndsb_ess_chipset_str(cx->ess_chipset);
#else
			mixer_str = "";
			ess_str = "";
#endif

			printf("  [%u] base=%X mpu=%X dma=%d dma16=%d irq=%d DSP=%u 1.XXAI=%u\n",
					i+1,cx->baseio,cx->mpuio,cx->dma8,cx->dma16,cx->irq,cx->dsp_ok,cx->dsp_autoinit_dma);
			printf("      MIXER=%u[%s] DSPv=%u.%u SC6600=%u OPL=%X GAME=%X AWE=%X\n",
					cx->mixer_ok,mixer_str,(unsigned int)cx->dsp_vmaj,(unsigned int)cx->dsp_vmin,
					cx->is_gallant_sc6600,cx->oplio,cx->gameio,cx->aweio);
			printf("      ESS=%u[%s] use=%u wss=%X OPL3SAx=%X\n",
					cx->ess_chipset,ess_str,cx->ess_extensions,cx->wssio,cx->opl3sax_controlio);
#ifdef ISAPNP
			if (cx->pnp_name != NULL) {
				isa_pnp_product_id_to_str(temp_str,cx->pnp_id);
				printf("      ISA PnP[%u]: %s %s\n",cx->pnp_csn,temp_str,cx->pnp_name);
			}
#endif
			printf("      '%s'\n",cx->dsp_copyright);

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__)))
            if (!ignore_asp) {
                if (sndsb_check_for_sb16_asp(cx)) {
                    printf("      CSP/ASP chip detected: version=0x%02x ",cx->asp_chip_version_id);

                    if (sndsb_sb16_asp_ram_test(cx) > 0)
                        printf("RAM OK\n");
                    else
                        printf("RAM TEST FAILED\n");
                }
            }
#endif

			count++;
		}
		if (count == 0) {
			printf("No cards found.\n");
			return 1;
		}
		printf("-----------\n");
		printf("Select the card you wish to test: "); fflush(stdout);
		i = vga_getch();
		printf("\n");
		if (i == 27) return 0;
		if (i == 13 || i == 10) i = '1';
		sc_idx = i - '0';
	}

	if (sc_idx < 1 || sc_idx > SNDSB_MAX_CARDS) {
		printf("Sound card index out of range\n");
		return 1;
	}

	sb_card = &sndsb_card[sc_idx-1];
	if (sb_card->baseio == 0) {
		printf("No such card\n");
		return 1;
	}

	printf("Allocating sound buffer..."); fflush(stdout);
    realloc_dma_buffer();

	i = int10_getmode();
	if (i != 3) int10_setmode(3);

	/* hook IRQ 0 */
	irq_0_count = 0;
	irq_0_adv = 1;
	irq_0_max = 1;
	old_irq_0 = _dos_getvect(irq2int(0));
	_dos_setvect(irq2int(0),irq_0);
	p8259_unmask(0);

	if (sb_card->irq != -1) {
		old_irq_masked = p8259_is_masked(sb_card->irq);
		if (vector_is_iret(irq2int(sb_card->irq)))
			old_irq_masked = 1;

		old_irq = _dos_getvect(irq2int(sb_card->irq));
		_dos_setvect(irq2int(sb_card->irq),sb_irq);
		p8259_unmask(sb_card->irq);
	}
	
	vga_write_color(0x07);
	vga_clear();

	loop=1;
	redraw=1;
	wav_record=0;
	bkgndredraw=1;
	vga_menu_bar.bar = main_menu_bar;
	vga_menu_bar.sel = -1;
	vga_menu_bar.row = 3;
	vga_menu_idle = my_vga_menu_idle;
	if (force_ddac) sb_card->dsp_play_method = SNDSB_DSPOUTMETHOD_DIRECT;
	reduced_irq_interval=(sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_1xx);
	update_cfg();

	if (!sndsb_assign_dma_buffer(sb_card,sb_dma)) {
		printf("Cannot assign DMA buffer\n");
		return 1;
	}

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	/* please let me know if the user attempts to close my DOS Box */
	if (dos_close_awareness_available()) {
		int d;

		printf("Windows is running, attempting to enable Windows close-awareness\n");

		/* counter-intuitive in typical Microsoft fashion.
		 * When they say "enable/disable" the close command what they apparently mean
		 * is that "enabling" it triggers immediate closing when the user clicks the close
		 * button, and "disabling" means it queues the event and hands it to the DOS
		 * program as a request to shutdown. If their documentation would simply
		 * explain that, I would not have wasted 30 minutes wondering why Windows 9x
		 * would immediately complain about not being able to close this program.
		 *
		 * Sadly, Windows XP, despite being the "merging of Windows NT and 98 codebases"
		 * doesn't provide us with close-awareness. */
		if ((d=dos_close_awareness_enable(0)) != 0)
			printf("Warning, cannot enable Windows 'close-awareness' ret=0x%X\n",d);
		else
			printf("Close-awareness enabled\n");
	}
#endif

#if defined(TARGET_PC98) && TARGET_MSDOS == 16
    /* re-define function and editor keys on PC-98 MS-DOS so we can differentiate
     * between left arrow and backspace, etc.
     *
     * the remapping is based on the patterns used by SEDIT.EXE and VZ.EXE which
     * seems to be perfectly reasonable patterns. */
    pc98_intdc_get_keycode_state_ext(&prev_pc98_keycodes);
    cur_pc98_keycodes = prev_pc98_keycodes;

    /* the VGA GUI expects a specific mapping to work */
    vga_tty_pc98_mapping(&cur_pc98_keycodes);
    pc98_intdc_set_keycode_state_ext(&cur_pc98_keycodes);
#endif

	if (wav_file[0] != 0) open_wav();
	if (autoplay) begin_play();
	while (loop) {
		if ((mitem = vga_menu_bar_keymon()) != NULL) {
			/* act on it */
			if (mitem == &main_menu_file_quit) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
			else if (mitem == &main_menu_file_set) {
				prompt_play_wav(0);
				bkgndredraw = 1;
				wav_record = 0;
				redraw = 1;
			}
			else if (mitem == &main_menu_playback_play) {
				if (wav_record) {
					stop_play();
					wav_record = 0;
					update_cfg();
					sb_card->buffer_last_io = 0;
				}
				if (!wav_playing) {
					begin_play();
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_playback_record) {
				if (!wav_record) {
					stop_play();
					wav_record = 1;
					update_cfg();
					sb_card->buffer_last_io = 0;
				}
				if (!wav_playing) {
					begin_play();
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_playback_stop) {
				if (wav_playing) {
					stop_play();
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_device_dsp_reset) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Resetting DSP...",0,0);
				stop_play();
				sndsb_reset_dsp(sb_card);
				t8254_wait(t8254_us2ticks(1000000));
				vga_msg_box_destroy(&box);
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			else if (mitem == &main_menu_device_mixer_reset) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Resetting mixer...",0,0);
				sndsb_reset_mixer(sb_card);
				t8254_wait(t8254_us2ticks(1000000));
				vga_msg_box_destroy(&box);
			}
#endif
			else if (mitem == &main_menu_help_about) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Sound Blaster test program v1.4 for DOS\n\n(C) 2008-2019 Jonathan Campbell\nALL RIGHTS RESERVED\n"
#if TARGET_MSDOS == 32
					"32-bit protected mode version"
#elif defined(__LARGE__)
					"16-bit real mode (large model) version"
#elif defined(__MEDIUM__)
					"16-bit real mode (medium model) version"
#elif defined(__COMPACT__)
					"16-bit real mode (compact model) version"
#elif defined(__HUGE__)
					"16-bit real mode (huge model) version"
#elif defined(__TINY__)
					"16-bit real mode (tiny model) version"
#else
					"16-bit real mode (small model) version"
#endif
#if defined(TARGET_PC98)
                    "\nFor NEC PC-9801/PC-9821 and compatibles"
#else
                    "\nFor IBM PC/XT/AT and compatibles"
#endif
					,0,0);
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			else if (mitem == &main_menu_help_dsp_modes) {
				int quit = 0;
				struct vga_msg_box box;

				vga_msg_box_create(&box,
					"Explanation of DSP modes:\n"
					"\n"
					"4.xx      Sound Blaster 16 or compatible DSP operation. 16-bit audio.\n"
					"\n"
					"3.xx      Sound Blaster Pro or compatible. Most clones emulate this card.\n"
					"          Creative SB16 cards do not replicate Sound Blaster Pro stereo.\n"
					"\n"
					"2.01      Sound Blaster 2.01 or compatible with auto-init DMA and\n"
					"          high-speed DAC playback modes up to 44100Hz.\n"
					"\n"
					"2.00      Sound Blaster 2.0 or compatible with auto-init DMA.\n"
					"\n"
					"1.xx      Original Sound Blaster or compatible DSP operation.\n"
					"\n"
					"Direct    DSP command 0x10 (Direct DAC output) and system timer.\n"
					"          If DMA is not available, this is your only option. Emulators,\n"
					"          clones, some motherboard & SB16 combos have problems with it.\n"
					"\n"
					"Detailed explanations are available in README.TXT"
					,0,0);
				while (!quit) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) {
							quit = (i == 27);
							break;
						}
					}
				}
				vga_msg_box_destroy(&box);

				vga_msg_box_create(&box,
					"Additional playback modes:\n"
					"\n"
					"Flip sign    Flip sign bit before sending to audio, and instruct SB16 DSP\n"
					"             to play nonstandard format. Clones may produce loud static.\n"
					"\n"
					"ADPCM        Convert audio to Sound Blaster ADPCM format and instruct DSP\n"
					"             to play it. Clones generally do not support this.\n"
					"\n"
					"Auto-init    DSP 2.01 and higher support auto-init ADPCM playback. Clones\n"
					"ADPCM        definitely do not support this.\n"
					"\n"
					"ADPCM reset  On actual Creative SB hardware the DSP resets the ADPCM step\n"
					"per interval size per block. DOSBox, emulators, do not reset ADPCM state.\n"
					"\n"
					"Goldplay     A semi-popular music tracker library, SB support is hacky.\n"
					"             This program can use the same technique for testing purposes\n"
					"\n"
					"Detailed explanations are available in README.TXT"
					,0,0);
				while (!quit) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) {
							quit = (i == 27);
							break;
						}
					}
				}
				vga_msg_box_destroy(&box);

				vga_msg_box_create(&box,
					"Additional things you can play with:\n"
					"\n"
					"IRQ interval ..... DSP block size to play/record with.\n"
					"DMA autoinit ..... Use/don't use auto-init on the DMA controller side.\n"
					"DSP playback ..... Whether to use auto-init or single-cycle DSP commands.\n"
					"Force hispeed .... If set, always use hispeed DSP playback (SB 2.0 and Pro).\n"
					"\n"
					"DSP 4.xx autoinit FIFO . (SB16) use the FIFO for auto-init DSP 4.xx.\n"
					"DSP 4.xx single FIFO ... (SB16) use the FIFO for single cycle DSP 4.xx.\n"
					"DSP nag mode ........... Test 'nagging' the DSP, Crystal Dream style.\n"
					"                         Has no effect unless DSP in single-cycle mode.\n"
					"Poll ack when no IRQ ... If not assigned an IRQ, use polling of the status\n"
					"                         port to prevent halting on SB16.\n"
					"DSP alias port ......... Use alias port 22Dh instead of 22Ch.\n"
					"Backwards .............. Play file backwards by using DMA decrement mode.\n"
					"Wari hack alias ........ Use DSP command 0x15 instead of DSP command 0x14\n"
					"                         to play audio. 'Wari' uses this oddity for audio.\n"
					"\n"
					"Detailed explanations are available in README.TXT"
					,0,0);
				while (!quit) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) {
							quit = (i == 27);
							break;
						}
					}
				}
				vga_msg_box_destroy(&box);
			}
#endif
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			else if (mitem == &main_menu_playback_dsp4_fifo_autoinit) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->dsp_4xx_fifo_autoinit = !sb_card->dsp_4xx_fifo_autoinit;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_device_dsp_alias) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				change_alias_menu();
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
            else if (mitem == &main_menu_device_srate_force) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();

                if (sb_card->srate_force_dsp_4xx) {
                    sb_card->srate_force_dsp_4xx = 0;
                    sb_card->srate_force_dsp_tc = 1;
                }
                else if (sb_card->srate_force_dsp_tc) {
                    sb_card->srate_force_dsp_4xx = 0;
                    sb_card->srate_force_dsp_tc = 0;
                }
                else {
                    sb_card->srate_force_dsp_4xx = 1;
                    sb_card->srate_force_dsp_tc = 0;
                }
				update_cfg();

				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
            }
            else if (mitem == &main_menu_device_realloc_dma) {
                unsigned char wp = wav_playing;
                if (wp) stop_play();
                realloc_dma_buffer();
                if (wp) begin_play();
                bkgndredraw = 1;
                redraw = 1;
            }
            else if (mitem == &main_menu_device_busy_cycle) {
				// NTS: The user may be testing the busy cycle while the card is playing audio. Do NOT stop playback!
				//      The test carried out in this function does not send DSP commands, it only polls the DSP.
				measure_dsp_busy_cycle();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (mitem == &main_menu_playback_wari_hack_alias) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->wari_hack_mode = !sb_card->wari_hack_mode;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_dsp_poll_ack_no_irq) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->poll_ack_when_no_irq = !sb_card->poll_ack_when_no_irq;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_dsp_nag_mode) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				if (sb_card->dsp_nag_mode && sb_card->dsp_nag_hispeed) {
					sb_card->dsp_nag_mode = sb_card->dsp_nag_hispeed = 0;
				}
				else if (sb_card->dsp_nag_mode) {
					sb_card->dsp_nag_mode = sb_card->dsp_nag_hispeed = 1;
				}
				else {
					sb_card->dsp_nag_hispeed = 0;
					sb_card->dsp_nag_mode = 1;
				}
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_dsp4_fifo_single) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->dsp_4xx_fifo_single_cycle = !sb_card->dsp_4xx_fifo_single_cycle;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_force_hispeed) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->force_hispeed = !sb_card->force_hispeed;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_flip_sign) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->audio_data_flipped_sign = !sb_card->audio_data_flipped_sign;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
#endif
			else if (mitem == &main_menu_playback_goldplay) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->goldplay_mode = !sb_card->goldplay_mode;
#if TARGET_MSDOS == 32
				if (!irq_0_had_warned && sb_card->goldplay_mode) {
					/* NOTE TO SELF: It can overwhelm the UI in DOSBox too, but DOSBox seems able to
					   recover if you manage to hit CTRL+F12 to speed up the CPU cycles in the virtual machine.
					   On real hardware, even with the recovery method the machine remains hung :( */
					if (confirm_yes_no_dialog(dos32_irq_0_warning))
						irq_0_had_warned = 1;
					else
						sb_card->goldplay_mode = 0;
				}
#endif
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_goldplay_mode) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				if (++goldplay_samplerate_choice > GOLDRATE_MAX)
					goldplay_samplerate_choice = GOLDRATE_MATCH;
				update_cfg();
				ui_anim(1);
				if (wp) begin_play();
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			else if (mitem == &main_menu_windows_fullscreen) {
				/* NTS: Does not seem to work under Windows XP */
				if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_STANDARD) {
					__asm {
						mov	ax,0x168B
						xor	bx,bx
						int	0x2F
					}
				}
				else {
					struct vga_msg_box box;

					vga_msg_box_create(&box,
						windows_mode == WINDOWS_NONE ?
						"Windows is not running" :
						"Windows NT not supported"
						,0,0);
					while (1) {
						ui_anim(0);
						if (kbhit()) {
							i = vga_getch();
							if (i == 13 || i == 27) break;
						}
					}
					vga_msg_box_destroy(&box);
				}
			}
			else if (mitem == &main_menu_playback_noreset_adpcm) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				adpcm_do_reset_interval = !adpcm_do_reset_interval;
				update_cfg();
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_timer_clamp) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sample_rate_timer_clamp = !sample_rate_timer_clamp;
				update_cfg();
				if (wp) begin_play();
			}
#endif
			else if (mitem == &main_menu_playback_dsp_autoinit_dma) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->dsp_autoinit_dma = !sb_card->dsp_autoinit_dma;
				update_cfg();
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_dsp_autoinit_command) {
				/* NOTES:
				 *
				 *   - Pro Audio Spectrum 16 (PAS16): Despite reporting DSP v2.0, auto-init commands actually do work */
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->dsp_autoinit_command = !sb_card->dsp_autoinit_command;
				update_cfg();
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_reduced_irq) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				if (++reduced_irq_interval >= 3) reduced_irq_interval = -1;
				update_cfg();
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_params) {
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				change_param_menu();
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			else if (mitem == &main_menu_playback_autoinit_adpcm) {
				unsigned char wp = wav_playing;
				if (do_adpcm_ai_warning) {
					if (adpcm_warning_prompt()) {
						do_adpcm_ai_warning = 0;
						if (wp) stop_play();
						sb_card->enable_adpcm_autoinit ^= 1;
						if (wp) begin_play();
					}
				}
				else {
					if (wp) stop_play();
					sb_card->enable_adpcm_autoinit ^= 1;
					if (wp) begin_play();
				}
			}
#endif
			else if (mitem == &main_menu_device_trigger_irq) {
				unsigned char wp = wav_playing;
				struct vga_msg_box box;
				int res;

				if (wp) stop_play();

				vga_msg_box_create(&box,"Issuing DSP command to test IRQ",0,0);
				res = sndsb_irq_test(sb_card); // -1 test inconclusive 0 = failed 1 = pass */
				vga_msg_box_destroy(&box);

				if (wp) begin_play();

				vga_msg_box_create(&box,(res < 0) ? "IRQ test N/A" : ((res > 0) ? "IRQ test success" : "IRQ test failed"),0,0);
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);
			}
#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			else if (mitem == &main_menu_device_haltcont_dma) {
				unsigned char wp = wav_playing;
				struct vga_msg_box box;
				int res;

				vga_msg_box_create(&box,"Asking to halt DMA. Hit enter to then ask to continue DMA",0,0);

				_cli(); // do not conflict with IRQ handler
				res = sndsb_halt_dma(sb_card);
				_sti();
				if (!res) {
					vga_msg_box_destroy(&box);
					vga_msg_box_create(&box,"Failed to halt DMA",0,0);
					stop_play();
					sndsb_reset_dsp(sb_card);
				}

				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);

				_cli(); // do not conflict with IRQ handler
				res = sndsb_continue_dma(sb_card);
				_sti();
				if (!res) {
					vga_msg_box_create(&box,"Failed to continue DMA",0,0);
					stop_play();
					sndsb_reset_dsp(sb_card);

					while (1) {
						ui_anim(0);
						if (kbhit()) {
							i = vga_getch();
							if (i == 13 || i == 27) break;
						}
					}

					vga_msg_box_destroy(&box);
				}

				if (wp) begin_play();
			}
			else if (mitem == &main_menu_device_autoinit_stop || mitem == &main_menu_device_autoinit_stopcont) {
				unsigned char wp = wav_playing;
				struct vga_msg_box box;
				int res;

				vga_msg_box_create(&box,"Asking sound card to exit auto-init mode\nAudio should stop after one IRQ block.\nContinuing auto-init will not work if playback stops",0,0);

				_cli(); // do not conflict with IRQ handler
				res = sndsb_exit_autoinit_mode(sb_card);
				_sti();
				if (!res) {
					vga_msg_box_destroy(&box);
					vga_msg_box_create(&box,"Failed to exit auto-init DMA",0,0);
					stop_play();
					sndsb_reset_dsp(sb_card);
				}

				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = vga_getch();
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);

				if (mitem == &main_menu_device_autoinit_stopcont) {
					vga_msg_box_create(&box,"Asking sound card to reenter auto-init mode\nIf audio already stopped, then this will not restart it.\nHit enter to next use the continue DMA commands.",0,0);

					_cli(); // do not conflict with IRQ handler
					res = sndsb_continue_autoinit_mode(sb_card);
					_sti();
					if (!res) {
						vga_msg_box_destroy(&box);
						vga_msg_box_create(&box,"Failed to continue auto-init DMA",0,0);
						stop_play();
						sndsb_reset_dsp(sb_card);
					}

					while (1) {
						ui_anim(0);
						if (kbhit()) {
							i = vga_getch();
							if (i == 13 || i == 27) break;
						}
					}
					vga_msg_box_destroy(&box);

					vga_msg_box_create(&box,"I sent continue DMA commands. Did they work?",0,0);

					_cli(); // do not conflict with IRQ handler
					res = sndsb_continue_dma(sb_card);
					_sti();
					if (!res) {
						vga_msg_box_destroy(&box);
						vga_msg_box_create(&box,"Failed to continue DMA",0,0);
						stop_play();
						sndsb_reset_dsp(sb_card);
					}

					while (1) {
						ui_anim(0);
						if (kbhit()) {
							i = vga_getch();
							if (i == 13 || i == 27) break;
						}
					}
					vga_msg_box_destroy(&box);
				}

				if (wp) {
					stop_play();
					begin_play();
				}
			}
#endif
#ifdef SB_MIXER
			else if (mitem == &main_menu_device_mixer_controls) {
				play_with_mixer();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (mitem == &main_menu_device_ess_controls) {
				play_with_ess();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (mitem == &main_menu_device_sb16_8051) {
				play_with_sb16_8051();
				bkgndredraw = 1;
				redraw = 1;
			}
#endif
#ifdef CARD_INFO_AND_CHOOSER
			else if (mitem == &main_menu_device_info) {
				show_device_info();
			}
			else if (mitem == &main_menu_device_choose_sound_card) {
				choose_sound_card();
			}
#endif
#ifdef LIVE_CFG
			else if (mitem == &main_menu_device_configure_sound_card) {
				conf_sound_card();
			}
#endif
#ifdef INCLUDE_FX
			else if (mitem == &main_menu_effects_reset) {
				fx_reset();
			}
			else if (mitem == &main_menu_effects_vol) {
				fx_vol_dialog();
			}
			else if (mitem == &main_menu_effects_echo) {
				fx_vol_echo();
			}
#endif
		}

		if (sb_card->irq_counter != sb_irq_pcounter) {
			sb_irq_pcounter = sb_card->irq_counter;
			redraw = 1;
		}

		if (redraw || bkgndredraw) {
			if (!wav_playing) update_cfg();
			if (bkgndredraw) {
#if defined(TARGET_PC98)
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) {
                    vga[0x0000] = 0x20;
                    vga[0x1000] = 0x25;
                    vga++;
                }
#else
				for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
#endif
				vga_menu_bar_draw();
				draw_irq_indicator();
			}
			ui_anim(bkgndredraw);
			_cli();
			vga_moveto(0,2);
			vga_write_color(0x1F);
#if defined(TARGET_PC98)
			for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < 80;cc++) {
                vga[0     ] = 0x20;
                vga[0x1000] = 0x01;
                vga++;
            }
#else
			for (vga=vga_state.vga_alpha_ram+(80*2),cc=0;cc < 80;cc++) *vga++ = 0x1F20;
#endif
			vga_write("File: ");
#if defined(TARGET_PC98)
			vga_writew(wav_file);
#else
			vga_write(wav_file);
#endif
			vga_write_sync();
			bkgndredraw = 0;
			redraw = 0;
			_sti();
		}

		if (kbhit()) {
			i = vga_getch();

			if (i == 27) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
			else if (i == '?') {
				if (sb_card->reason_not_supported) {
					struct vga_msg_box box;

					vga_msg_box_create(&box,sb_card->reason_not_supported,0,0);
					while (1) {
						ui_anim(0);
						if (kbhit()) {
							i = vga_getch();
							if (i == 13 || i == 27) break;
						}
					}
					vga_msg_box_destroy(&box);
				}
			}
			else if (i == ' ') {
				if (!wav_record) {
					if (wav_playing) stop_play();
					else begin_play();
				}
			}
			else if (i == VGATTY_LEFT_ARROW) { /* left arrow */
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				wav_position -= wav_sample_rate * 5UL;
				if ((signed long)wav_position < 0) wav_position = 0;
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == VGATTY_RIGHT_ARROW) { /* right arrow */
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				wav_position += wav_sample_rate * 5UL;
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == VGATTY_SC_Fn(9)) { /* F9 */
				unsigned char wp = wav_playing;
				if (wp) stop_play();
				sb_card->backwards ^= 1;
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == VGATTY_SC_Fn(8)) { /* F8 */
				getch(); /* delibrate pause */
			}
		}

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
		/* Windows "close-awareness".
		 * If the user attempts to close the DOSBox window, Windows will let us know */
		if (dos_close_awareness_available()) {
			int r = dos_close_awareness_query();

			if (r == DOS_CLOSE_AWARENESS_NOT_ACK) {
				/* then ack it, and act as if File -> Quit were selected */
				dos_close_awareness_ack();

				if (confirm_quit())
					break;
				else
					dos_close_awareness_cancel();
			}
			else if (r == DOS_CLOSE_AWARENESS_ACKED) {
				/* then we need to exit */
				break;
			}
		}
#endif

		ui_anim(0);
	}

#if defined(TARGET_PC98) && TARGET_MSDOS == 16
    /* re-define function and editor keys on PC-98 MS-DOS so we can differentiate
     * between left arrow and backspace, etc.
     *
     * the remapping is based on the patterns used by SEDIT.EXE and VZ.EXE which
     * seems to be perfectly reasonable patterns. */
    pc98_intdc_set_keycode_state_ext(&prev_pc98_keycodes);
#endif

	_sti();
	vga_write_sync();
	printf("Stopping playback...\n");
	stop_play();
	printf("Closing WAV...\n");
	close_wav();
	printf("Freeing buffer...\n");
    free_dma_buffer();

	if (sb_card->irq >= 0 && old_irq_masked)
		p8259_mask(sb_card->irq);

	printf("Releasing IRQ...\n");
	if (sb_card->irq != -1)
		_dos_setvect(irq2int(sb_card->irq),old_irq);

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	_dos_setvect(irq2int(0),old_irq_0);
	return 0;
}

