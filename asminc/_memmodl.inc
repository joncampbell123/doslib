
%if TARGET_MSDOS == 16
 %ifndef MMODE
  %error You must specify MMODE variable (memory model) for 16-bit real mode code
 %endif
%endif

; if, by default, function calls are near or far
%if TARGET_MSDOS == 16
 %ifidni MMODE,l
  %define MMODE_CODE_CALL_DEF_FAR
 %else
  %ifidni MMODE,h
   %define MMODE_CODE_CALL_DEF_FAR
  %else
   %ifidni MMODE,m
    %define MMODE_CODE_CALL_DEF_FAR
   %endif
  %endif
 %endif
%endif

; if, by default, data pointers are near or far
%if TARGET_MSDOS == 16
 %ifidni MMODE,l
  %define MMODE_DATA_VAR_DEF_FAR
 %else
  %ifidni MMODE,h
   %define MMODE_DATA_VAR_DEF_FAR
  %else
   %ifidni MMODE,c
    %define MMODE_DATA_VAR_DEF_FAR
   %endif
  %endif
 %endif
%endif

