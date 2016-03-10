 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8250/8250.h>
#include <hw/dos/doswin.h>

uint16_t uart_8250_baud_to_divisor(struct info_8250 *uart,unsigned long rate) {
	if (rate == 0) return 0;
	return (uint16_t)(115200UL / rate);
}

unsigned long uart_8250_divisor_to_baud(struct info_8250 *uart,uint16_t rate) {
	if (rate == 0) return 1;
	return (unsigned long)(115200UL / (unsigned long)rate);
}

