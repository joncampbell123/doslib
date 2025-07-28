
# HW\VGA------------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_VGA_LIB
$(HW_VGA_LIB):
	@cd $(HW_VGA_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)

$(HW_VGATTY_LIB):
	@cd $(HW_VGATTY_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)

$(HW_VGAGUI_LIB):
	@cd $(HW_VGAGUI_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)

$(HW_VGAGFX_LIB):
	@cd $(HW_VGAGFX_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\VGA2------------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_VGA2_LIB
$(HW_VGA2_LIB):
	@cd $(HW_VGA2_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\UTTY------------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_UTTY_LIB
$(HW_UTTY_LIB):
	@cd $(HW_UTTY_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\CPU------------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_CPU_LIB
$(HW_CPU_LIB):
	@cd $(HW_CPU_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\DOS------------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_DOS_LIB
$(HW_DOS_LIB):
	@cd $(HW_DOS_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\SNDSB----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_SNDSB_LIB
$(HW_SNDSB_LIB):
	@cd $(HW_SNDSB_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\LLMEM----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_LLMEM_LIB
$(HW_LLMEM_LIB):
	@cd $(HW_LLMEM_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\8042-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_8042_LIB
$(HW_8042_LIB):
	@cd $(HW_8042_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\8237-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_8237_LIB
$(HW_8237_LIB):
	@cd $(HW_8237_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\8250-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_8250_LIB
$(HW_8250_LIB):
	@cd $(HW_8250_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\8251-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_8251_LIB
$(HW_8251_LIB):
	@cd $(HW_8251_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\8254-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_8254_LIB
$(HW_8254_LIB):
	@cd $(HW_8254_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\8259-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_8259_LIB
$(HW_8259_LIB):
	@cd $(HW_8259_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\ACPI-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_ACPI_LIB
$(HW_ACPI_LIB):
	@cd $(HW_ACPI_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\PCI------------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_PCI_LIB
$(HW_PCI_LIB):
	@cd $(HW_PCI_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\PCIE-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_PCIE_LIB
$(HW_PCIE_LIB):
	@cd $(HW_PCIE_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\MB\INTEL\PIIX3-------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_MB_INTEL_PIIX3_LIB
$(HW_MB_INTEL_PIIX3_LIB):
	@cd $(HW_MB_INTEL_PIIX3_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\ADLIB----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_ADLIB_LIB
$(HW_ADLIB_LIB):
	@cd $(HW_ADLIB_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\NECPC98----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_NECPC98_LIB
$(HW_NECPC98_LIB):
	@cd $(HW_NECPC98_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\IDE==----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_IDE_LIB
$(HW_IDE_LIB):
	@cd $(HW_IDE_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\FLOPPY---------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_FLOPPY_LIB
$(HW_FLOPPY_LIB):
	@cd $(HW_FLOPPY_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\DOSBOXID---------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_DOSBOXID_LIB
$(HW_DOSBOXID_LIB):
	@cd $(HW_DOSBOXID_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\ISAPNP---------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_ISAPNP_LIB
$(HW_ISAPNP_LIB):
	@cd $(HW_ISAPNP_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\ULTRASND-------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_ULTRASND_LIB
$(HW_ULTRASND_LIB):
	@cd $(HW_ULTRASND_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\BIOSDISK-------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_BIOSDISK_LIB
$(HW_BIOSDISK_LIB):
	@cd $(HW_BIOSDISK_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\FLATREAL-------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_FLATREAL_LIB
$(HW_FLATREAL_LIB):
	@cd $(HW_FLATREAL_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# WINDOWS\NTVDM-----------------------------------------------------------------------------
!ifneq NOW_BUILDING WINDOWS_NTVDM
! ifdef WINDOWS_NTVDMLIB_LIB
$(WINDOWS_NTVDMLIB_LIB):
	@cd $(WINDOWS_NTVDMLIB_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# HW\VESA-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_VESA_LIB
$(HW_VESA_LIB):
	@cd $(HW_VESA_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# WINDOWS\W9XVMM----------------------------------------------------------------------------
!ifneq NOW_BUILDING WINDOWS_W9XVMM
$(WINDOWS_W9XVMM_LIB):
	@cd $(WINDOWS_W9XVMM_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# EXT\FAAD----------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_FAAD_LIB
! ifdef EXT_FAAD_LIB
$(EXT_FAAD_LIB):
	@cd $(EXT_FAAD_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\LIBOGG--------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_LIBOGG_LIB
! ifdef EXT_LIBOGG_LIB
$(EXT_LIBOGG_LIB):
	@cd $(EXT_LIBOGG_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\FLAC----------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_FLAC_LIB
! ifdef EXT_FLAC_LIB
$(EXT_FLAC_LIB):
	@cd $(EXT_FLAC_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
! ifdef EXT_FLAC_EXE
$(EXT_FLAC_EXE):
	@cd $(EXT_FLAC_EXE_DIR)
	@$(MAKECMD) build exe $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\FLAC----------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_JPEG_LIB
! ifdef EXT_JPEG_LIB
$(EXT_JPEG_LIB):
	@cd $(EXT_JPEG_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\LAME----------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_LAME_LIB
! ifdef EXT_LAME_LIB
$(EXT_LAME_LIB):
	@cd $(EXT_LAME_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\LIBMAD--------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_LIBMAD_LIB
! ifdef EXT_LIBMAD_LIB
$(EXT_LIBMAD_LIB):
	@cd $(EXT_LIBMAD_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\SPEEX---------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_SPEEX_LIB
! ifdef EXT_SPEEX_LIB
$(EXT_SPEEX_LIB):
	@cd $(EXT_SPEEX_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\VORBIS--------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_VORBIS_LIB
! ifdef EXT_VORBIS_LIB
$(EXT_VORBIS_LIB):
	@cd $(EXT_VORBIS_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\ZLIB----------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_ZLIB_LIB
! ifdef EXT_ZLIB_LIB
$(EXT_ZLIB_LIB):
	@cd $(EXT_ZLIB_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\ZLIBIMIN------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_ZLIBIMIN_LIB
! ifdef EXT_ZLIBIMIN_LIB
$(EXT_ZLIBIMIN_LIB):
	@cd $(EXT_ZLIBIMIN_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# EXT\BZIP2---------------------------------------------------------------------------------
!ifneq NOW_BUILDING EXT_BZIP2_LIB
! ifdef EXT_BZIP2_LIB
$(EXT_BZIP2_LIB):
	@cd $(EXT_BZIP2_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
! endif
!endif

# WINDOWS\WIN16EB---------------------------------------------------------------------------
!ifneq NOW_BUILDING WINDOWS_WIN16EB_LIB
$(WINDOWS_WIN16EB_LIB):
	@cd $(WINDOWS_WIN16EB_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# WINDOWS\DISPDIB---------------------------------------------------------------------------
!ifneq NOW_BUILDING WINDOWS_DISPDIB_LIB
$(WINDOWS_DISPDIB_LIB):
	@cd $(WINDOWS_DISPDIB_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# HW\USB\OHCI-------------------------------------------------------------------------------
!ifneq NOW_BUILDING HW_USB_OHCI_LIB
$(HW_USB_OHCI_LIB):
	@cd $(HW_USB_OHCI_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# FMT\OMF-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING FMT_OMF_LIB
$(FMT_OMF_LIB):
	@cd $(FMT_OMF_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# FMT\BMP-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING FMT_BMP_LIB
$(FMT_BMP_LIB):
	@cd $(FMT_BMP_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# FMT\OMF\OMFSEGDG utility
!ifeq HPS /
$(OMFSEGDG):
	@cd $(FMT_OMF_LIB_DIR)
	make
	@cd $(HERE)
!else
$(OMFSEGDG):
	@cd $(FMT_OMF_LIB_DIR)
	@$(MAKECMD)
	@cd $(HERE)
!endif

# FMT\MINIPNG-----------------------------------------------------------------------------------
!ifneq NOW_BUILDING FMT_MINIPNG_LIB
$(FMT_MINIPNG_LIB):
	@cd $(FMT_MINIPNG_LIB_DIR)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)
!endif

# DEBUG
!ifndef NOW_BUILDING
! error no NOW_BUILDING
!endif

