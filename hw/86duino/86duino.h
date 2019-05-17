
#ifndef __DOSLIB_HW_86DUINO_86DUINO_H
#define __DOSLIB_HW_86DUINO_86DUINO_H

#define VTX86_SYSCLK            (100)

#define VTX86_INPUT             (0x00)
#define VTX86_OUTPUT            (0x01)
#define VTX86_INPUT_PULLUP      (0x02)
#define VTX86_INPUT_PULLDOWN    (0x03)

#define VTX86_LOW               (0x00)
#define VTX86_HIGH              (0x01)
#define VTX86_CHANGE            (0x02)
#define VTX86_FALLING           (0x03)
#define VTX86_RISING            (0x04)

#define VTX86_MCM_MC            (0)
#define VTX86_MCM_MD            (1)

#define VTX86_GPIO_PINS         (45)

extern uint8_t vtx86_mc_md_inuse[VTX86_GPIO_PINS];

#endif //__DOSLIB_HW_86DUINO_86DUINO_H

