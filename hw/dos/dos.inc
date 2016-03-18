
; extern definitions for *.c files in this library.

; ---------- DATA segment -----------------
%include "_segdata.inc"

extern _dpmi_entered ; BYTE
extern _dpmi_entry_point ; DWORD
extern _dpmi_private_data_segment ; word
extern _dpmi_rm_entry ; qword
extern _dpmi_pm_entry ; dword
extern _dpmi_pm_cs,_dpmi_pm_ds,_dpmi_pm_es,_dpmi_pm_ss

extern l_lin2fm_params
extern l_lin2fm_param_dst
extern l_lin2fm_param_lsrc
extern l_lin2fm_param_sz
extern l_rm_ret
extern l_rm_reentry
extern l_lin2fm_src_sel
extern l_lin2fm_dst_sel

