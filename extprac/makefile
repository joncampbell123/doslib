# do not use GNU make.
# Only use Open Watcom WMAKE

all: cpu16.exe cpu16h.exe cpu32.exe

clean: .symbolic
        rm -v *.exe *.obj *.map

DOSLIBDIR=..
!include $(DOSLIBDIR)/extdep.mak

dos86l_libs = $(DOSLIBDIR)/hw/cpu/dos86l/cpu.lib $(DOSLIBDIR)/hw/dos/dos86l/dos.lib
dos86h_libs = $(DOSLIBDIR)/hw/cpu/dos86h/cpu.lib $(DOSLIBDIR)/hw/dos/dos86h/dos.lib
dos386f_libs = $(DOSLIBDIR)/hw/cpu/dos386f/cpu.lib $(DOSLIBDIR)/hw/dos/dos386f/dos.lib

# library deps 16-bit large
$(DOSLIBDIR)/hw/cpu/dos86l/cpu.lib:
        cd $(DOSLIBDIR)/hw/cpu && ./make.sh
$(DOSLIBDIR)/hw/dos/dos86l/dos.lib:
        cd $(DOSLIBDIR)/hw/dos && ./make.sh

# library deps 16-bit huge
$(DOSLIBDIR)/hw/cpu/dos86h/cpu.lib:
        cd $(DOSLIBDIR)/hw/cpu && ./make.sh
$(DOSLIBDIR)/hw/dos/dos86h/dos.lib:
        cd $(DOSLIBDIR)/hw/dos && ./make.sh

# library deps 32-bit flat
$(DOSLIBDIR)/hw/cpu/dos386f/cpu.lib:
        cd $(DOSLIBDIR)/hw/cpu && ./make.sh
$(DOSLIBDIR)/hw/dos/dos386f/dos.lib:
        cd $(DOSLIBDIR)/hw/dos && ./make.sh

# NOTE: dos86l = 16-bit large memory model. memory model must match!
cpu16.exe: cpu16.obj $(dos86l_libs)
        *wlink option quiet option map=cpu16.map $(DOSLIB_LDFLAGS_DOS16L) file cpu16.obj name cpu16.exe library {$(dos86l_libs)}
cpu16.obj: cpu.c
        *wcl $(FLAGS) -fo=cpu16.obj -ml $(DOSLIB_CFLAGS_DOS16L) -c cpu.c

# NOTE: dos86h = 16-bit huge memory model. memory model must match!
cpu16h.exe: cpu16h.obj $(dos86h_libs)
        *wlink option quiet option map=cpu16h.map $(DOSLIB_LDFLAGS_DOS16H) file cpu16h.obj name cpu16h.exe library {$(dos86h_libs)}
cpu16h.obj: cpu.c
        *wcl $(FLAGS) -fo=cpu16h.obj -mh $(DOSLIB_CFLAGS_DOS16H) -c cpu.c

# NOTE: dos386f = 32-bit flat memory model. memory model must match!
cpu32.exe: cpu32.obj $(dos386f_libs)
        *wlink option quiet option map=cpu32.map $(DOSLIB_LDFLAGS_DOS32) file cpu32.obj name cpu32.exe library {$(dos386f_libs)}
cpu32.obj: cpu.c
        *wcl386 $(FLAGS) -fo=cpu32.obj -mf $(DOSLIB_CFLAGS_DOS32) -c cpu.c

