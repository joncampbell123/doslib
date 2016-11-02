; auto-generated code, do not edit
bits 16			; 16-bit real mode

section _TEXT class=CODE

global _exe_normal_entry
global _header_sig
global _header_function_count
global _header_function_table
global _header_fence
global _header_end

extern _get_message
extern _callmemaybe
extern _callmenever
extern _callmethis
extern _readtimer
extern _readvga
extern _readaport

; header. must come FIRST
_header_sig:
        db          "CLSG"          ; Code Loadable Segment Group
_header_function_count:
        dw          8               ; number of functions
_header_function_table:
        dw          _get_message
        dw          _callmemaybe
        dw          _callmenever
        dw          _callmethis
        dw          _readtimer
        dw          _readvga
        dw          __empty_slot
        dw          _readaport
_header_fence:
        dw          0xFFFF          ; fence
_header_end:

__empty_slot:
        retf

; EXE entry point points here, in case the user foolishly tries to run this EXE
..start:
_exe_normal_entry:
        mov         ax,4C00h        ; if the user tries to run this EXE, terminate
        int         21h

