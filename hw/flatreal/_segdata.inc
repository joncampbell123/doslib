; NASM doesn't support using %macros to do segment definitions, so we do this instead

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
%ifidni segment_use,use32
section _DATA align=4 class=DATA use32
%else
 %ifidni segment_use,use16
segment _DATA align=4 class=DATA use16
 %else
  %error unknown or undefined segment_use
 %endif
%endif

