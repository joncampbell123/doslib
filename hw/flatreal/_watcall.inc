; defines for watcall handling, including pulling params off the stack

%if TARGET_MSDOS == 16
 %ifidni MMODE,l
  %define retnative retf
  %define cdecl_param_offset 6	; RETF addr + PUSH BP
 %else
  %ifidni MMODE,h
   %define retnative retf
   %define cdecl_param_offset 6	; RETF addr + PUSH BP
  %else
   %ifidni MMODE,m
    %define retnative retf
    %define cdecl_param_offset 6 ; RETF addr + PUSH BP
   %else
    %define retnative ret
    %define cdecl_param_offset 4 ; RET addr + PUSH BP
   %endif
  %endif
 %endif
%else
 %define retnative ret
 %define cdecl_param_offset 8	; RET addr + PUSH EBP
%endif

