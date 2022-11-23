# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_USB_OHCI_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."
AFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."
NASMFLAGS_THIS = 

# NTS: CPU functions here are to be moved at some point to the cpu library!
C_SOURCE =    ohci.c
OBJS =        $(SUBDIR)$(HPS)ohci.obj
HW_USB_OHCI_LIB =  $(SUBDIR)$(HPS)ohci.lib
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

$(HW_USB_OHCI_LIB): $(OBJS)
	wlib -q -b -c $(HW_USB_OHCI_LIB) -+$(SUBDIR)$(HPS)ohci.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_USB_OHCI_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

$(TEST_EXE): $(HW_USB_OHCI_LIB) $(HW_USB_OHCI_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_PCI_LIB) $(HW_PCI_LIB_DEPENDENCIES) $(HW_PCIE_LIB) $(HW_PCIE_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_USB_OHCI_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_PCI_LIB_WLINK_LIBRARIES) $(HW_PCIE_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_USB_OHCI_LIB)
          del tmp.cmd
          @echo Cleaning done

