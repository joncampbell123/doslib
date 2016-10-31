bits 16			; 16-bit real mode

section inittext class=CODE

extern _callmemaybe
extern _callmenever
extern _callmethis
extern _readtimer
extern _readvga
extern _readaport

global _header_sig
global _header_function_count
global _header_function_table
global _header_fence
global _header_end

; header. must come FIRST
..start:
_header_sig:
        db          "CLSG"          ; Code Loadable Segment Group
_header_function_count:
        dw          6               ; number of functions
_header_function_table:
        dw          _callmemaybe
        dw          _callmenever
        dw          _callmethis
        dw          _readtimer
        dw          _readvga
        dw          _readaport
_header_fence:
        dw          0xFFFF          ; fence
_header_end:

