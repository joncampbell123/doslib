#!/bin/bash
set QEMU_AUDIO_DRV=oss
qemu-system-x86_64 -M pc -vga std -no-acpi -fda win95.dsk -hda hdd.dsk -m 16 -boot a -soundhw sb16 -localtime -net user -net nic,model=ne2k_pci
