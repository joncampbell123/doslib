
; extern definitions for *.c files in this library.

; ---------- DATA segment -----------------
%include "_segdata.inc"

%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS
extern _dpmi_pm_cs
extern _dpmi_pm_ds
extern _dpmi_pm_ss
extern _dpmi_pm_es
extern _dpmi_entered
extern _dpmi_pm_entry
extern _dpmi_rm_entry
 %endif
%endif
extern _cpu_flags
extern _cpu_tmp1
extern _cpu_cpuid_max
extern _cpu_cpuid_vendor ; char cpu_cpuid_vendor[13];
extern _cpu_cpuid_features ; struct cpu_cpuid_feature cpu_cpuid_features;
extern _cpu_serial ; struct cpu_serial_number cpu_serial;

; ASM file including this must re-start the segment they were working from

