
; ---------- CODE segment -----------------
%include "_segdata.inc"

global try_pci_bios1_
global try_pci_bios2_
global _pci_bios_read_dword_16
global _pci_bios_write_dword_16
extern _pci_bios_protmode_entry_point	; 32-bit
extern _pci_bios_hw_characteristics	; 8-bit
extern _pci_bios_last_bus		; 16-bit
extern _pci_bios_interface_level	; 16-bit

