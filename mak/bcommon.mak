
!ifeq HPS /
MAKECMD=env "parent_build_list=$(TO_BUILD)" ./make.sh
COPY=cp
RM=rm
!else
MAKECMD=make.bat
COPY=copy
RM=del
!endif

# we need to know where "HERE" is
HERE = $+$(%cwd)$-

# HW\VGA-----------------------------------------------------------------------------------
HW_VGA_LIB_DIR=$(REL)$(HPS)hw$(HPS)vga
HW_VGA_LIB=$(HW_VGA_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)vga.lib
!ifdef TARGET_WINDOWS
HW_VGA_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(WINDOWS_DISPDIB_LIB) $(WINDOWS_DISPDIB_LIB_DEPENDENCIES)
HW_VGA_LIB_WLINK_LIBRARIES=library $(HW_VGA_LIB) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_DISPDIB_LIB_WLINK_LIBRARIES)
!else
HW_VGA_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES)
HW_VGA_LIB_WLINK_LIBRARIES=library $(HW_VGA_LIB) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES)
!endif

# HW\VGA-----------------------------------------------------------------------------------
HW_VGATTY_LIB_DIR=$(REL)$(HPS)hw$(HPS)vga
HW_VGATTY_LIB=$(HW_VGATTY_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)vgatty.lib
!ifdef TARGET_WINDOWS
HW_VGATTY_LIB_DEPENDENCIES=$(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(WINDOWS_DISPDIB_LIB) $(WINDOWS_DISPDIB_LIB_DEPENDENCIES)
HW_VGATTY_LIB_WLINK_LIBRARIES=library $(HW_VGATTY_LIB) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_DISPDIB_LIB_WLINK_LIBRARIES)
!else
HW_VGATTY_LIB_DEPENDENCIES=$(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES)
HW_VGATTY_LIB_WLINK_LIBRARIES=library $(HW_VGATTY_LIB) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES)
!endif

# HW\VGA-----------------------------------------------------------------------------------
HW_VGAGUI_LIB_DIR=$(REL)$(HPS)hw$(HPS)vga
HW_VGAGUI_LIB=$(HW_VGAGUI_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)vgagui.lib
!ifdef TARGET_WINDOWS
HW_VGAGUI_LIB_DEPENDENCIES=$(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(WINDOWS_DISPDIB_LIB) $(WINDOWS_DISPDIB_LIB_DEPENDENCIES)
HW_VGAGUI_LIB_WLINK_LIBRARIES=library $(HW_VGAGUI_LIB) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_DISPDIB_LIB_WLINK_LIBRARIES)
!else
HW_VGAGUI_LIB_DEPENDENCIES=$(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES)
HW_VGAGUI_LIB_WLINK_LIBRARIES=library $(HW_VGAGUI_LIB) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES)
!endif

# HW\CPU------------------------------------------------------------------------------------
HW_CPU_LIB=$(HW_CPU_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)cpu.lib
HW_CPU_LIB_DIR=$(REL)$(HPS)hw$(HPS)cpu
HW_CPU_LIB_DEPENDENCIES=$(WINDOWS_NTVDMLIB_LIB) $(HW_DOS_LIB)
HW_CPU_LIB_WLINK_LIBRARIES=$(WINDOWS_NTVDMLIB_LIB_WLINK_LIBRARIES) library $(HW_DOS_LIB) library $(HW_CPU_LIB)

# HW\DOS------------------------------------------------------------------------------------
HW_DOS_LIB=$(HW_DOS_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)dos.lib
HW_DOS_LIB_DIR=$(REL)$(HPS)hw$(HPS)dos
HW_DOS_LIB_DEPENDENCIES=$(WINDOWS_NTVDMLIB_LIB) $(HW_CPU_LIB)
HW_DOS_LIB_WLINK_LIBRARIES=$(WINDOWS_NTVDMLIB_LIB_WLINK_LIBRARIES) library $(HW_DOS_LIB) library $(HW_CPU_LIB)

# HW\LLMEM----------------------------------------------------------------------------------
HW_LLMEM_LIB_DIR=$(REL)$(HPS)hw$(HPS)llmem
HW_LLMEM_LIB=$(HW_LLMEM_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)llmem.lib
HW_LLMEM_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_CPU_LIB) $(WINDOWS_NTVDMLIB_LIB)
HW_LLMEM_LIB_WLINK_LIBRARIES=library $(HW_DOS_LIB) library $(HW_CPU_LIB) $(WINDOWS_NTVDMLIB_LIB_WLINK_LIBRARIES) library $(HW_LLMEM_LIB)

# HW\8042-----------------------------------------------------------------------------------
HW_8042_LIB_DIR=$(REL)$(HPS)hw$(HPS)8042
HW_8042_LIB=$(HW_8042_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)8042.lib
HW_8042_LIB_DEPENDENCIES=
HW_8042_LIB_WLINK_LIBRARIES=library $(HW_8042_LIB)

# HW\ACPI-----------------------------------------------------------------------------------
HW_ACPI_LIB_DIR=$(REL)$(HPS)hw$(HPS)acpi
HW_ACPI_LIB=$(HW_ACPI_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)acpi.lib
HW_ACPI_LIB_DEPENDENCIES=
HW_ACPI_LIB_WLINK_LIBRARIES=library $(HW_ACPI_LIB)

# HW\8237-----------------------------------------------------------------------------------
HW_8237_LIB_DIR=$(REL)$(HPS)hw$(HPS)8237
HW_8237_LIB=$(HW_8237_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)8237.lib
HW_8237_LIB_DEPENDENCIES=
HW_8237_LIB_WLINK_LIBRARIES=library $(HW_8237_LIB)

# HW\8250-----------------------------------------------------------------------------------
HW_8250_LIB_DIR=$(REL)$(HPS)hw$(HPS)8250
HW_8250_LIB=$(HW_8250_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)8250.lib
HW_8250_LIB_DEPENDENCIES=
HW_8250_LIB_WLINK_LIBRARIES=library $(HW_8250_LIB)

HW_8250PNP_LIB_DIR=$(REL)$(HPS)hw$(HPS)8250
HW_8250PNP_LIB=$(HW_8250PNP_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)8250pnp.lib
HW_8250PNP_LIB_DEPENDENCIES=$(HW_8250_LIB)
HW_8250PNP_LIB_WLINK_LIBRARIES=library $(HW_8250PNP_LIB) library $(HW_8250_LIB)

# HW\8254-----------------------------------------------------------------------------------
HW_8254_LIB_DIR=$(REL)$(HPS)hw$(HPS)8254
HW_8254_LIB=$(HW_8254_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)8254.lib
HW_8254_LIB_DEPENDENCIES=
HW_8254_LIB_WLINK_LIBRARIES=library $(HW_8254_LIB)

# HW\8259-----------------------------------------------------------------------------------
HW_8259_LIB_DIR=$(REL)$(HPS)hw$(HPS)8259
HW_8259_LIB=$(HW_8259_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)8259.lib
HW_8259_LIB_DEPENDENCIES=
HW_8259_LIB_WLINK_LIBRARIES=library $(HW_8259_LIB)

# HW\ACPI-----------------------------------------------------------------------------------
HW_ACPI_LIB_DIR=$(REL)$(HPS)hw$(HPS)acpi
HW_ACPI_LIB=$(HW_ACPI_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)acpi.lib
HW_ACPI_LIB_DEPENDENCIES=
HW_ACPI_LIB_WLINK_LIBRARIES=library $(HW_ACPI_LIB)

# HW\APM------------------------------------------------------------------------------------
HW_APM_LIB_DIR=$(REL)$(HPS)hw$(HPS)apm
HW_APM_LIB=$(HW_APM_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)apm.lib
HW_APM_LIB_DEPENDENCIES=
HW_APM_LIB_WLINK_LIBRARIES=library $(HW_APM_LIB)

# HW\IDE------------------------------------------------------------------------------------
HW_IDE_LIB_DIR=$(REL)$(HPS)hw$(HPS)ide
HW_IDE_LIB=$(HW_IDE_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)idelib.lib
HW_IDE_LIB_DEPENDENCIES=
HW_IDE_LIB_WLINK_LIBRARIES=library $(HW_IDE_LIB)

# HW\FLOPPY---------------------------------------------------------------------------------
HW_FLOPPY_LIB_DIR=$(REL)$(HPS)hw$(HPS)floppy
HW_FLOPPY_LIB=$(HW_FLOPPY_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)floppy.lib
HW_FLOPPY_LIB_DEPENDENCIES=
HW_FLOPPY_LIB_WLINK_LIBRARIES=library $(HW_FLOPPY_LIB)

# HW\ADLIB----------------------------------------------------------------------------------
HW_ADLIB_LIB_DIR=$(REL)$(HPS)hw$(HPS)adlib
HW_ADLIB_LIB=$(HW_ADLIB_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)adlib.lib
HW_ADLIB_LIB_DEPENDENCIES=
HW_ADLIB_LIB_WLINK_LIBRARIES=library $(HW_ADLIB_LIB)

# HW\ISAPNP---------------------------------------------------------------------------------
HW_ISAPNP_LIB_DIR=$(REL)$(HPS)hw$(HPS)isapnp
HW_ISAPNP_LIB=$(HW_ISAPNP_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)isapnp.lib
HW_ISAPNP_LIB_DEPENDENCIES=
HW_ISAPNP_LIB_WLINK_LIBRARIES=library $(HW_ISAPNP_LIB)

# HW\SMBIOS---------------------------------------------------------------------------------
HW_SMBIOS_LIB_DIR=$(REL)$(HPS)hw$(HPS)smbios
HW_SMBIOS_LIB=$(HW_SMBIOS_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)smbios.lib
HW_SMBIOS_LIB_DEPENDENCIES=
HW_SMBIOS_LIB_WLINK_LIBRARIES=library $(HW_SMBIOS_LIB)

# HW\PARPORT--------------------------------------------------------------------------------
HW_PARPORT_LIB_DIR=$(REL)$(HPS)hw$(HPS)parport
HW_PARPORT_LIB=$(HW_PARPORT_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)parport.lib
HW_PARPORT_LIB_DEPENDENCIES=
HW_PARPORT_LIB_WLINK_LIBRARIES=library $(HW_PARPORT_LIB)

HW_PARPNP_LIB_DIR=$(HW_PARPORT_LIB_DIR)
HW_PARPNP_LIB=$(HW_PARPNP_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)parpnp.lib
HW_PARPNP_LIB_DEPENDENCIES=
HW_PARPNP_LIB_WLINK_LIBRARIES=library $(HW_PARPNP_LIB)

# HW\FLATREAL-------------------------------------------------------------------------------
HW_FLATREAL_LIB_DIR=$(REL)$(HPS)hw$(HPS)flatreal
HW_FLATREAL_LIB=$(HW_FLATREAL_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)flatreal.lib
HW_FLATREAL_LIB_DEPENDENCIES=
HW_FLATREAL_LIB_WLINK_LIBRARIES=library $(HW_FLATREAL_LIB)

# HW\BIOSDISK-------------------------------------------------------------------------------
HW_BIOSDISK_LIB_DIR=$(REL)$(HPS)hw$(HPS)biosdisk
HW_BIOSDISK_LIB=$(HW_BIOSDISK_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)biosdisk.lib
HW_BIOSDISK_LIB_DEPENDENCIES=
HW_BIOSDISK_LIB_WLINK_LIBRARIES=library $(HW_BIOSDISK_LIB)

# HW\PCI------------------------------------------------------------------------------------
HW_PCI_LIB_DIR=$(REL)$(HPS)hw$(HPS)pci
HW_PCI_LIB=$(HW_PCI_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)pci.lib
HW_PCI_LIB_DEPENDENCIES=
HW_PCI_LIB_WLINK_LIBRARIES=library $(HW_PCI_LIB)

# HW\PCIE-----------------------------------------------------------------------------------
HW_PCIE_LIB_DIR=$(REL)$(HPS)hw$(HPS)pcie
HW_PCIE_LIB=$(HW_PCIE_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)pcie.lib
HW_PCIE_LIB_DEPENDENCIES=$(HW_ACPI_LIB) $(HW_FLATREAL_LIB) $(HW_LLMEM_LIB)
HW_PCIE_LIB_WLINK_LIBRARIES=library $(HW_PCIE_LIB) $(HW_ACPI_LIB_WLINK_LIBRARIES) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) $(HW_LLMEM_LIB_WLINK_LIBRARIES)

# HW\RTC------------------------------------------------------------------------------------
HW_RTC_LIB_DIR=$(REL)$(HPS)hw$(HPS)rtc
HW_RTC_LIB=$(HW_RTC_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)rtc.lib
HW_RTC_LIB_DEPENDENCIES=
HW_RTC_LIB_WLINK_LIBRARIES=library $(HW_RTC_LIB)

# HW\VESA-----------------------------------------------------------------------------------
HW_VESA_LIB_DIR=$(REL)$(HPS)hw$(HPS)vesa
HW_VESA_LIB=$(HW_VESA_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)vesa.lib
HW_VESA_LIB_DEPENDENCIES=
HW_VESA_LIB_WLINK_LIBRARIES=library $(HW_VESA_LIB)

# HW\SNDSB----------------------------------------------------------------------------------
HW_SNDSB_LIB_DIR=$(REL)$(HPS)hw$(HPS)sndsb
HW_SNDSB_LIB=$(HW_SNDSB_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)sndsb.lib
HW_SNDSB_LIB_DEPENDENCIES=
HW_SNDSB_LIB_WLINK_LIBRARIES=library $(HW_SNDSB_LIB)

HW_SNDSBPNP_LIB_DIR=$(HW_SNDSB_LIB_DIR)
HW_SNDSBPNP_LIB=$(HW_SNDSBPNP_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)sndsbpnp.lib
HW_SNDSBPNP_LIB_DEPENDENCIES=
HW_SNDSBPNP_LIB_WLINK_LIBRARIES=library $(HW_SNDSBPNP_LIB)

# HW\ULTRASND-------------------------------------------------------------------------------
HW_ULTRASND_LIB_DIR=$(REL)$(HPS)hw$(HPS)ultrasnd
HW_ULTRASND_LIB=$(HW_ULTRASND_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)ultrasnd.lib
HW_ULTRASND_LIB_DEPENDENCIES=
HW_ULTRASND_LIB_WLINK_LIBRARIES=library $(HW_ULTRASND_LIB)

# HW\MB\INTEL\PIIX3-------------------------------------------------------------------------
HW_MB_INTEL_PIIX3_LIB=$(REL)$(HPS)hw$(HPS)mb$(HPS)intel$(HPS)piix3
HW_MB_INTEL_PIIX3=$(HW_MB_INTEL_PIIX3_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)piix3.lib
HW_MB_INTEL_PIIX3_DEPENDENCIES=
HW_MB_INTEL_PIIX3_WLINK_LIBRARIES=library $(HW_MB_INTEL_PIIX3_LIB)

# WINDOWS\NTVDM-----------------------------------------------------------------------------
!ifndef TARGET_OS2
WINDOWS_NTVDMLIB_LIB_DIR=$(REL)$(HPS)windows$(HPS)ntvdm
WINDOWS_NTVDMLIB_LIB=$(WINDOWS_NTVDMLIB_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)ntvdmlib.lib
WINDOWS_NTVDMLIB_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_CPU_LIB)
WINDOWS_NTVDMLIB_LIB_WLINK_LIBRARIES=library $(HW_DOS_LIB) library $(HW_CPU_LIB) library $(WINDOWS_NTVDMLIB_LIB)
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 32
!   ifeq TARGET_WINDOWS 40
WINDOWS_NTVDMVDD_LIB_DIR=$(REL)$(HPS)windows$(HPS)ntvdm
WINDOWS_NTVDMVDD_LIB=$(WINDOWS_NTVDMVDD_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)ntvdmvdd.lib
WINDOWS_NTVDMVDD_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_CPU_LIB) $(WINDOWS_NTVDMLIB_LIB)
WINDOWS_NTVDMVDD_LIB_WLINK_LIBRARIES=library $(HW_DOS_LIB) library $(HW_CPU_LIB) library $(WINDOWS_NTVDMLIB_LIB) library $(WINDOWS_NTVDMVDD_LIB)
!   endif
!  endif
! endif
!endif

# WINDOWS\W9XVMM----------------------------------------------------------------------------
WINDOWS_W9XVMM_LIB_DIR=$(REL)$(HPS)windows$(HPS)w9xvmm
WINDOWS_W9XVMM_LIB=$(WINDOWS_W9XVMM_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)w9xvmm.lib
WINDOWS_W9XVMM_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_CPU_LIB) $(WINDOWS_NTVDMLIB_LIB)
WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES=library $(HW_DOS_LIB) library $(HW_CPU_LIB) library $(WINDOWS_NTVDMLIB_LIB) library $(WINDOWS_W9XVMM_LIB)

# EXT\FAAD----------------------------------------------------------------------------------
# libfaad does not compile properly in 16-bit real mode
!ifeq TARGET_MSDOS 16
EXT_FAAD_LIB_NO_LIB = 1
EXT_FAAD_LIB_NO_EXE = 1
!endif
!ifndef EXT_FAAD_LIB_NO_LIB
EXT_FAAD_LIB_DIR=$(REL)$(HPS)ext$(HPS)faad
EXT_FAAD_LIB=$(EXT_FAAD_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)faad.lib
EXT_FAAD_LIB_DEPENDENCIES=
EXT_FAAD_LIB_WLINK_LIBRARIES=library $(EXT_FAAD_LIB)
!endif

# EXT\LIBOGG--------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_LIBOGG_LIB_NO_LIB = 1
EXT_LIBOGG_LIB_NO_EXE = 1
!endif
!ifndef EXT_LIBOGG_LIB_NO_LIB
EXT_LIBOGG_LIB_DIR=$(REL)$(HPS)ext$(HPS)libogg
EXT_LIBOGG_LIB=$(EXT_LIBOGG_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)libogg.lib
EXT_LIBOGG_LIB_DEPENDENCIES=
EXT_LIBOGG_LIB_WLINK_LIBRARIES=library $(EXT_LIBOGG_LIB)
!endif

# EXT\FLAC----------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_FLAC_LIB_NO_LIB = 1
EXT_FLAC_LIB_NO_EXE = 1
!endif
!ifndef EXT_FLAC_LIB_NO_LIB
EXT_FLAC_LIB_DIR=$(REL)$(HPS)ext$(HPS)flac
EXT_FLAC_LIB=$(EXT_FLAC_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)flac.lib
EXT_FLAC_LIB_DEPENDENCIES=
EXT_FLAC_LIB_WLINK_LIBRARIES=library $(EXT_FLAC_LIB)
!endif
!ifndef EXT_FLAC_LIB_NO_EXE
EXT_FLAC_EXE = $(SUBDIR)$(HPS)flac.exe
!endif

# EXT\JPEG----------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_JPEG_LIB_NO_LIB = 1
EXT_JPEG_LIB_NO_EXE = 1
!endif
!ifndef EXT_JPEG_LIB_NO_LIB
EXT_JPEG_LIB_DIR=$(REL)$(HPS)ext$(HPS)jpeg
EXT_JPEG_LIB=$(EXT_JPEG_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)jpeg.lib
EXT_JPEG_LIB_DEPENDENCIES=
EXT_JPEG_LIB_WLINK_LIBRARIES=library $(EXT_JPEG_LIB)
!endif
!ifndef EXT_JPEG_LIB_NO_EXE
EXT_JPEG_CJPEG_EXE = $(SUBDIR)$(HPS)cjpeg.exe
EXT_JPEG_DJPEG_EXE = $(SUBDIR)$(HPS)djpeg.exe
!endif

# EXT\LAME----------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_LAME_LIB_NO_LIB = 1
EXT_LAME_LIB_NO_EXE = 1
!endif
!ifndef EXT_LAME_LIB_NO_LIB
EXT_LAME_LIB_DIR=$(REL)$(HPS)ext$(HPS)lame
EXT_LAME_LIB=$(EXT_LAME_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)lame.lib
EXT_LAME_LIB_DEPENDENCIES=
EXT_LAME_LIB_WLINK_LIBRARIES=library $(EXT_LAME_LIB)
!endif
!ifndef EXT_LAME_LIB_NO_EXE
EXT_LAME_LAME_EXE = $(SUBDIR)$(HPS)lame.exe
!endif

# EXT\LIBMAD--------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_LIBMAD_LIB_NO_LIB = 1
EXT_LIBMAD_LIB_NO_EXE = 1
!endif
!ifndef EXT_LIBMAD_LIB_NO_LIB
EXT_LIBMAD_LIB_DIR=$(REL)$(HPS)ext$(HPS)libmad
EXT_LIBMAD_LIB=$(EXT_LIBMAD_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)libmad.lib
EXT_LIBMAD_LIB_DEPENDENCIES=
EXT_LIBMAD_LIB_WLINK_LIBRARIES=library $(EXT_LIBMAD_LIB)
!endif

# EXT\SPEEX---------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_SPEEX_LIB_NO_LIB = 1
EXT_SPEEX_LIB_NO_EXE = 1
!endif
!ifndef EXT_SPEEX_LIB_NO_LIB
EXT_SPEEX_LIB_DIR=$(REL)$(HPS)ext$(HPS)speex
EXT_SPEEX_LIB=$(EXT_SPEEX_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)speex.lib
EXT_SPEEX_LIB_DEPENDENCIES=
EXT_SPEEX_LIB_WLINK_LIBRARIES=library $(EXT_SPEEX_LIB)
!endif
!ifndef EXT_SPEEX_LIB_NO_EXE
EXT_SPEEX_SPEEXDEC_EXE = $(SUBDIR)$(HPS)speexdec.exe
EXT_SPEEX_SPEEXENC_EXE = $(SUBDIR)$(HPS)speexenc.exe
!endif

# EXT\VORBIS--------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_VORBIS_LIB_NO_LIB = 1
EXT_VORBIS_LIB_NO_EXE = 1
!endif
!ifndef EXT_VORBIS_LIB_NO_LIB
EXT_VORBIS_LIB_DIR=$(REL)$(HPS)ext$(HPS)vorbis
EXT_VORBIS_LIB=$(EXT_VORBIS_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)vorbis.lib
EXT_VORBIS_LIB_DEPENDENCIES=
EXT_VORBIS_LIB_WLINK_LIBRARIES=library $(EXT_VORBIS_LIB)
!endif

# EXT\ZLIB----------------------------------------------------------------------------------
!ifndef EXT_ZLIB_LIB_NO_LIB
EXT_ZLIB_LIB_DIR=$(REL)$(HPS)ext$(HPS)zlib
EXT_ZLIB_LIB=$(EXT_ZLIB_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)zlib.lib
EXT_ZLIB_LIB_DEPENDENCIES=
EXT_ZLIB_LIB_WLINK_LIBRARIES=library $(EXT_ZLIB_LIB)
!endif
!ifndef EXT_ZLIB_LIB_NO_EXE
EXT_ZLIB_MINIGZIP_EXE = $(SUBDIR)$(HPS)minigzip.exe
EXT_ZLIB_EXAMPLE_EXE = $(SUBDIR)$(HPS)example.exe
!endif

# EXT\BZIP2---------------------------------------------------------------------------------
!ifeq TARGET_MSDOS 16
EXT_BZIP2_LIB_NO_LIB = 1
EXT_BZIP2_LIB_NO_EXE = 1
!endif
!ifndef EXT_BZIP2_LIB_NO_LIB
EXT_BZIP2_LIB_DIR=$(REL)$(HPS)ext$(HPS)bzip2
EXT_BZIP2_LIB=$(EXT_BZIP2_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)bzip2.lib
EXT_BZIP2_LIB_DEPENDENCIES=
EXT_BZIP2_LIB_WLINK_LIBRARIES=library $(EXT_BZIP2_LIB)
!endif
!ifndef EXT_BZIP2_LIB_NO_EXE
EXT_BZIP2_LIB_BZIP2_EXE = $(SUBDIR)$(HPS)bzip2.exe
EXT_BZIP2_LIB_BZIP2REC_EXE = $(SUBDIR)$(HPS)bzip2rec.exe
!endif

# WINDOWS\WIN16EB--------------------------------------------------------------------------
WINDOWS_WIN16EB_LIB_DIR=$(REL)$(HPS)windows$(HPS)win16eb
WINDOWS_WIN16EB_LIB=$(WINDOWS_WIN16EB_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)win16eb.lib
WINDOWS_WIN16EB_LIB_DEPENDENCIES=$(HW_DOS_LIB) $(HW_CPU_LIB)
WINDOWS_WIN16EB_LIB_WLINK_LIBRARIES=library $(WINDOWS_WIN16EB_LIB) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES)

# WINDOWS\DISPDIB--------------------------------------------------------------------------
WINDOWS_DISPDIB_LIB_DIR=$(REL)$(HPS)windows$(HPS)dispdib
WINDOWS_DISPDIB_LIB=$(WINDOWS_DISPDIB_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)dispdib.lib
WINDOWS_DISPDIB_LIB_DEPENDENCIES=$(WINDOWS_WIN16EB_LIB) $(WINDOWS_WIN16EB_LIB_DEPENDENCIES)
WINDOWS_DISPDIB_LIB_WLINK_LIBRARIES=library $(WINDOWS_DISPDIB_LIB) $(WINDOWS_WIN16EB_LIB_WLINK_LIBRARIES)

# HW\USB\OHCI------------------------------------------------------------------------------
HW_USB_OHCI_LIB_DIR=$(REL)$(HPS)hw$(HPS)usb$(HPS)ohci
HW_USB_OHCI_LIB=$(HW_USB_OHCI_LIB_DIR)$(HPS)$(SUBDIR)$(HPS)ohci.lib
HW_USB_OHCI_LIB_DEPENDENCIES=
HW_USB_OHCI_LIB_WLINK_LIBRARIES=library $(HW_USB_OHCI_LIB)

