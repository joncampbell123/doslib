/* 8042.h
 *
 * Intel 8042 keyboard controller library.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#ifndef __HW_8042_8042_H
#define __HW_8042_8042_H

#include <hw/cpu/cpu.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K8042_STATUS                    0x64    /* (R) */
#define K8042_COMMAND                   0x64    /* (W) */
#define K8042_DATA                      0x60    /* (R/W) */

/* status bits */
#define K8042_STATUS_OUTPUT_FULL        0x01
#define K8042_STATUS_INPUT_FULL         0x02
#define K8042_STATUS_SYSTEM             0x04
#define K8042_STATUS_DATA_WRITE         0x08    /* 1=next write to 0x64  0=next write to 0x60 */
#define K8042_STATUS_INHIBIT            0x10
#define K8042_STATUS_XMIT_TIMEOUT       0x20
#define K8042_STATUS_MOUSE_OUTPUT_FULL  0x20
#define K8042_STATUS_RECV_TIMEOUT       0x40
#define K8042_STATUS_PARITY_ERR         0x80

#define K8042_IRQ                       1
#define K8042_MOUSE_IRQ                 12

int k8042_probe();
int k8042_probe_aux();
void k8042_drain_buffer();
int k8042_write_aux_synaptics_E8(unsigned char c);
int k8042_write_aux_synaptics_mode(unsigned char c);

extern unsigned char                    k8042_flags;
extern unsigned char                    k8042_last_status;

#define K8042_F_AUX                     0x01

unsigned char k8042_wait_for_input_buffer(void);
unsigned char k8042_wait_for_output(void);

static inline unsigned char k8042_is_output_ready() {
    return ((k8042_last_status = inp(K8042_STATUS)) & K8042_STATUS_OUTPUT_FULL);
}

static inline unsigned char k8042_is_mouse_output_ready() {
    return ((k8042_last_status = inp(K8042_STATUS)) & (K8042_STATUS_OUTPUT_FULL|K8042_STATUS_MOUSE_OUTPUT_FULL)) ==
        (K8042_STATUS_OUTPUT_FULL|K8042_STATUS_MOUSE_OUTPUT_FULL);
}

static inline unsigned char k8042_output_was_aux() {
    return (k8042_flags & K8042_F_AUX) != 0 && (k8042_last_status & K8042_STATUS_MOUSE_OUTPUT_FULL) != 0;
}

/* WARNING: caller is expected to use k8042_wait_for_output() prior to calling this */
static inline unsigned char k8042_read_output() {
    return inp(K8042_DATA);
}

static inline int k8042_read_output_wait() {
    if (k8042_wait_for_output() || k8042_wait_for_output() || k8042_wait_for_output())
        return k8042_read_output();
    else
        return -1;
}

static inline unsigned char k8042_write_command(unsigned char c) {
    unsigned char r = k8042_wait_for_input_buffer();
    if (r) outp(K8042_COMMAND,c);
    return r;
}

static inline unsigned char k8042_write_data(unsigned char c) {
    unsigned char r = k8042_wait_for_input_buffer();
    if (r) outp(K8042_DATA,c);
    return r;
}

static inline int k8042_read_command_byte() {
    if (k8042_write_command(0x20))
        return k8042_read_output_wait();
    return -1;
}

static inline unsigned char k8042_write_command_byte(unsigned char c) {
    if (k8042_write_command(0x60) && k8042_write_data(c))
        return 1;

    return 0;
}

static inline void k8042_disable_keyboard() {
    k8042_write_command(0xAD); /* disable keyboard */
}

static inline void k8042_enable_keyboard() {
    k8042_write_command(0xAE); /* enable keyboard */
}

static inline void k8042_disable_aux() {
    k8042_write_command(0xA7); /* disable aux */
}

static inline void k8042_enable_aux() {
    k8042_write_command(0xA8); /* enable aux */
}

static inline unsigned char k8042_write_aux(unsigned char c) {
    return (k8042_write_command(0xD4) && k8042_write_data(c));
}

#ifdef __cplusplus
}
#endif

#endif /* __HW_8042_8042_H */

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */

