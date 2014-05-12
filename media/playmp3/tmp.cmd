option quiet system nt_win
file win32/playcom.obj file win32/qt.obj file win32/qtreader.obj file win32/playmp3.obj
library ../../hw/dos/win32/dos.lib library ../../hw/cpu/win32/cpu.lib library ../../windows/ntvdm/win32/ntvdmlib.lib library ../../hw/dos/win32/dos.lib library ../../hw/cpu/win32/cpu.lib  library ../../hw/dos/win32/dos.lib library ../../hw/cpu/win32/cpu.lib library ../../windows/ntvdm/win32/ntvdmlib.lib library ../../hw/dos/win32/dos.lib library ../../hw/cpu/win32/cpu.lib  library ../../ext/libmad/win32/libmad.lib library ../../ext/faad/win32/faad.lib library ../../ext/libogg/win32/libogg.lib library ../../ext/vorbis/win32/vorbis.lib library ../../ext/speex/win32/speex.lib library ../../ext/flac/win32/flac.lib library ../../ext/zlib/win32/zlib.lib
op resource=win32/playmp3.res library winmm.lib
name win32/playmp3.exe
