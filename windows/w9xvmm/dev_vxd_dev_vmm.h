/* auto-generated from dev_vxd_dev_vmm.vxddef, do not edit. */

/* Windows Virtual Machine Manager                                                 */
/*                                                                                 */
/* This is "built in" to the kernel and is a core device of the Windows system.    */
/*                                                                                 */
/* File location (Windows 3.0/3.1): C:\WINDOWS\SYSTEM\WIN386.EXE                   */
/* File location (Windows 95/98/ME): C:\WINDOWS\SYSTEM\VMM32.VXD                   */
/*                                                                                 */
/* Note that in both cases, the VMM does not exist as a standalone VxD device      */
/* but as code "built in" to the Windows kernel. VMM32.VXD and WIN386.EXE are      */
/* executables containing the kernel and a packed structure containing the         */
/* VxD device drivers in a "pre-digested" form. The VxD drivers contained within   */
/* do not contain the MS-DOS header, but start with the LE header. The LE header   */
/* is modified slightly to change one of the fields to instead indicate resident   */
/* length. This packed structure is known as 'W3' and 'W4', referring to the       */
/* extended header pointed to by the MS-DOS header. The 'W3' header is merely      */
/* a list of the VxDs and where they exist elsewhere in the same EXE               */
/* (Windows 3.0/3.1). The 'W4' header is the same format, but with compression     */
/* applied to any part of the EXE past the W4 header to compress both the list     */
/* and the VxDs contained elsewhere (Windows 95/98/ME). These built-in VxDs        */
/* provide both core devices as well as the "base drivers" that allow Windows      */
/* to function in the absence of external VxD drivers. Such drivers can be         */
/* referenced in SYSTEM.INI as "device=*vxdname" rather than "device=VXDFILE.VXD". */
/*                                                                                 */
/* For more information on the W3 and W4 formats, see DOSLIB tools:                */
/* - tool/w3list.pl (show contents of W3 header)                                   */
/* - tool/w3extract.pl (extract devices from W3 pack)                              */
/* - tool/w4tow3/w4tow3.c (decompress/convert W4 to W3 format)                     */
/*                                                                                 */
/* References:                                                                     */
/*                                                                                 */
/* * Windows 3.1 DDK (CD-ROM drive D) D:\386\INCLUDE\VMM.INC                       */
/* - "VMM.INC - Include file for Virtual Machine Manager"                          */
/* - "Version 1.00 - 05-May-1988 - By RAL"                                         */
/*                                                                                 */
/* * Windows 95 DDK (installed in C:\DDK95) C:\DDK95\INC32\VMM.INC                 */
/* - (C) 1993-1995 Microsoft                                                       */
/*                                                                                 */
/* * Windows Undocumented File Formats - Working Inside 16- and 32-bit Windows     */
/* - Pete Davis and Mike Wallace                                                   */
/* - R&D Books (C) 1997                                                            */
/*                                                                                 */
/* This DOSLIB header file (C) 2016-2017 Jonathan Campbell                         */

/* VXD device ID. Combine with service call ID when using VMMCall/VMMJmp */
#define VMM_Device_ID                                    0x0001

/* VXD services (total: 435, 0x01B2) */
#define VMM_snr_Get_VMM_Version                              0x0000    /* ver 3.0+ */
#define VMM_snr_Get_Cur_VM_Handle                            0x0001    /* ver 3.0+ */
#define VMM_snr_Test_Cur_VM_Handle                           0x0002    /* ver 3.0+ */
#define VMM_snr_Get_Sys_VM_Handle                            0x0003    /* ver 3.0+ */
#define VMM_snr_Test_Sys_VM_Handle                           0x0004    /* ver 3.0+ */
#define VMM_snr_Validate_VM_Handle                           0x0005    /* ver 3.0+ */
#define VMM_snr_Get_VMM_Reenter_Count                        0x0006    /* ver 3.0+ */
#define VMM_snr_Begin_Reentrant_Execution                    0x0007    /* ver 3.0+ */
#define VMM_snr_End_Reentrant_Execution                      0x0008    /* ver 3.0+ */
#define VMM_snr_Install_V86_Break_Point                      0x0009    /* ver 3.0+ */
#define VMM_snr_Remove_V86_Break_Point                       0x000A    /* ver 3.0+ */
#define VMM_snr_Allocate_V86_Call_Back                       0x000B    /* ver 3.0+ */
#define VMM_snr_Allocate_PM_Call_Back                        0x000C    /* ver 3.0+ */
#define VMM_snr_Call_When_VM_Returns                         0x000D    /* ver 3.0+ */
#define VMM_snr_Schedule_Global_Event                        0x000E    /* ver 3.0+ */
#define VMM_snr_Schedule_VM_Event                            0x000F    /* ver 3.0+ */
#define VMM_snr_Call_Global_Event                            0x0010    /* ver 3.0+ */
#define VMM_snr_Call_VM_Event                                0x0011    /* ver 3.0+ */
#define VMM_snr_Cancel_Global_Event                          0x0012    /* ver 3.0+ */
#define VMM_snr_Cancel_VM_Event                              0x0013    /* ver 3.0+ */
#define VMM_snr_Call_Priority_VM_Event                       0x0014    /* ver 3.0+ */
#define VMM_snr_Cancel_Priority_VM_Event                     0x0015    /* ver 3.0+ */
#define VMM_snr_Get_NMI_Handler_Addr                         0x0016    /* ver 3.0+ */
#define VMM_snr_Set_NMI_Handler_Addr                         0x0017    /* ver 3.0+ */
#define VMM_snr_Hook_NMI_Event                               0x0018    /* ver 3.0+ */
#define VMM_snr_Call_When_VM_Ints_Enabled                    0x0019    /* ver 3.0+ */
#define VMM_snr_Enable_VM_Ints                               0x001A    /* ver 3.0+ */
#define VMM_snr_Disable_VM_Ints                              0x001B    /* ver 3.0+ */
#define VMM_snr_Map_Flat                                     0x001C    /* ver 3.0+ */
#define VMM_snr_Map_Lin_To_VM_Addr                           0x001D    /* ver 3.0+ */
#define VMM_snr_Adjust_Exec_Priority                         0x001E    /* ver 3.0+ */
#define VMM_snr_Begin_Critical_Section                       0x001F    /* ver 3.0+ */
#define VMM_snr_End_Critical_Section                         0x0020    /* ver 3.0+ */
#define VMM_snr_End_Crit_And_Suspend                         0x0021    /* ver 3.0+ */
#define VMM_snr_Claim_Critical_Section                       0x0022    /* ver 3.0+ */
#define VMM_snr_Release_Critical_Section                     0x0023    /* ver 3.0+ */
#define VMM_snr_Call_When_Not_Critical                       0x0024    /* ver 3.0+ */
#define VMM_snr_Create_Semaphore                             0x0025    /* ver 3.0+ */
#define VMM_snr_Destroy_Semaphore                            0x0026    /* ver 3.0+ */
#define VMM_snr_Wait_Semaphore                               0x0027    /* ver 3.0+ */
#define VMM_snr_Signal_Semaphore                             0x0028    /* ver 3.0+ */
#define VMM_snr_Get_Crit_Section_Status                      0x0029    /* ver 3.0+ */
#define VMM_snr_Call_When_Task_Switched                      0x002A    /* ver 3.0+ */
#define VMM_snr_Suspend_VM                                   0x002B    /* ver 3.0+ */
#define VMM_snr_Resume_VM                                    0x002C    /* ver 3.0+ */
#define VMM_snr_No_Fail_Resume_VM                            0x002D    /* ver 3.0+ */
#define VMM_snr_Nuke_VM                                      0x002E    /* ver 3.0+ */
#define VMM_snr_Crash_Cur_VM                                 0x002F    /* ver 3.0+ */
#define VMM_snr_Get_Execution_Focus                          0x0030    /* ver 3.0+ */
#define VMM_snr_Set_Execution_Focus                          0x0031    /* ver 3.0+ */
#define VMM_snr_Get_Time_Slice_Priority                      0x0032    /* ver 3.0+ */
#define VMM_snr_Set_Time_Slice_Priority                      0x0033    /* ver 3.0+ */
#define VMM_snr_Get_Time_Slice_Granularity                   0x0034    /* ver 3.0+ */
#define VMM_snr_Set_Time_Slice_Granularity                   0x0035    /* ver 3.0+ */
#define VMM_snr_Get_Time_Slice_Info                          0x0036    /* ver 3.0+ */
#define VMM_snr_Adjust_Execution_Time                        0x0037    /* ver 3.0+ */
#define VMM_snr_Release_Time_Slice                           0x0038    /* ver 3.0+ */
#define VMM_snr_Wake_Up_VM                                   0x0039    /* ver 3.0+ */
#define VMM_snr_Call_When_Idle                               0x003A    /* ver 3.0+ */
#define VMM_snr_Get_Next_VM_Handle                           0x003B    /* ver 3.0+ */
#define VMM_snr_Set_Global_Time_Out                          0x003C    /* ver 3.0+ */
#define VMM_snr_Set_VM_Time_Out                              0x003D    /* ver 3.0+ */
#define VMM_snr_Cancel_Time_Out                              0x003E    /* ver 3.0+ */
#define VMM_snr_Get_System_Time                              0x003F    /* ver 3.0+ */
#define VMM_snr_Get_VM_Exec_Time                             0x0040    /* ver 3.0+ */
#define VMM_snr_Hook_V86_Int_Chain                           0x0041    /* ver 3.0+ */
#define VMM_snr_Get_V86_Int_Vector                           0x0042    /* ver 3.0+ */
#define VMM_snr_Set_V86_Int_Vector                           0x0043    /* ver 3.0+ */
#define VMM_snr_Get_PM_Int_Vector                            0x0044    /* ver 3.0+ */
#define VMM_snr_Set_PM_Int_Vector                            0x0045    /* ver 3.0+ */
#define VMM_snr_Simulate_Int                                 0x0046    /* ver 3.0+ */
#define VMM_snr_Simulate_Iret                                0x0047    /* ver 3.0+ */
#define VMM_snr_Simulate_Far_Call                            0x0048    /* ver 3.0+ */
#define VMM_snr_Simulate_Far_Jmp                             0x0049    /* ver 3.0+ */
#define VMM_snr_Simulate_Far_Ret                             0x004A    /* ver 3.0+ */
#define VMM_snr_Simulate_Far_Ret_N                           0x004B    /* ver 3.0+ */
#define VMM_snr_Build_Int_Stack_Frame                        0x004C    /* ver 3.0+ */
#define VMM_snr_Simulate_Push                                0x004D    /* ver 3.0+ */
#define VMM_snr_Simulate_Pop                                 0x004E    /* ver 3.0+ */
#define VMM_snr__HeapAllocate                                0x004F    /* ver 3.0+ */
#define VMM_snr__HeapReAllocate                              0x0050    /* ver 3.0+ */
#define VMM_snr__HeapFree                                    0x0051    /* ver 3.0+ */
#define VMM_snr__HeapGetSize                                 0x0052    /* ver 3.0+ */
#define VMM_snr__PageAllocate                                0x0053    /* ver 3.0+ */
#define VMM_snr__PageReAllocate                              0x0054    /* ver 3.0+ */
#define VMM_snr__PageFree                                    0x0055    /* ver 3.0+ */
#define VMM_snr__PageLock                                    0x0056    /* ver 3.0+ */
#define VMM_snr__PageUnLock                                  0x0057    /* ver 3.0+ */
#define VMM_snr__PageGetSizeAddr                             0x0058    /* ver 3.0+ */
#define VMM_snr__PageGetAllocInfo                            0x0059    /* ver 3.0+ */
#define VMM_snr__GetFreePageCount                            0x005A    /* ver 3.0+ */
#define VMM_snr__GetSysPageCount                             0x005B    /* ver 3.0+ */
#define VMM_snr__GetVMPgCount                                0x005C    /* ver 3.0+ */
#define VMM_snr__MapIntoV86                                  0x005D    /* ver 3.0+ */
#define VMM_snr__PhysIntoV86                                 0x005E    /* ver 3.0+ */
#define VMM_snr__TestGlobalV86Mem                            0x005F    /* ver 3.0+ */
#define VMM_snr__ModifyPageBits                              0x0060    /* ver 3.0+ */
#define VMM_snr__CopyPageTable                               0x0061    /* ver 3.0+ */
#define VMM_snr__LinMapIntoV86                               0x0062    /* ver 3.0+ */
#define VMM_snr__LinPageLock                                 0x0063    /* ver 3.0+ */
#define VMM_snr__LinPageUnLock                               0x0064    /* ver 3.0+ */
#define VMM_snr__SetResetV86Pageable                         0x0065    /* ver 3.0+ */
#define VMM_snr__GetV86PageableArray                         0x0066    /* ver 3.0+ */
#define VMM_snr__PageCheckLinRange                           0x0067    /* ver 3.0+ */
#define VMM_snr__PageOutDirtyPages                           0x0068    /* ver 3.0+ */
#define VMM_snr__PageDiscardPages                            0x0069    /* ver 3.0+ */
#define VMM_snr__GetNulPageHandle                            0x006A    /* ver 3.0+ */
#define VMM_snr__GetFirstV86Page                             0x006B    /* ver 3.0+ */
#define VMM_snr__MapPhysToLinear                             0x006C    /* ver 3.0+ */
#define VMM_snr__GetAppFlatDSAlias                           0x006D    /* ver 3.0+ */
#define VMM_snr__SelectorMapFlat                             0x006E    /* ver 3.0+ */
#define VMM_snr__GetDemandPageInfo                           0x006F    /* ver 3.0+ */
#define VMM_snr__GetSetPageOutCount                          0x0070    /* ver 3.0+ */
#define VMM_snr_Hook_V86_Page                                0x0071    /* ver 3.0+ */
#define VMM_snr__Assign_Device_V86_Pages                     0x0072    /* ver 3.0+ */
#define VMM_snr__DeAssign_Device_V86_Pages                   0x0073    /* ver 3.0+ */
#define VMM_snr__Get_Device_V86_Pages_Array                  0x0074    /* ver 3.0+ */
#define VMM_snr_MMGR_SetNULPageAddr                          0x0075    /* ver 3.0+ */
#define VMM_snr__Allocate_GDT_Selector                       0x0076    /* ver 3.0+ */
#define VMM_snr__Free_GDT_Selector                           0x0077    /* ver 3.0+ */
#define VMM_snr__Allocate_LDT_Selector                       0x0078    /* ver 3.0+ */
#define VMM_snr__Free_LDT_Selector                           0x0079    /* ver 3.0+ */
#define VMM_snr__BuildDescriptorDWORDs                       0x007A    /* ver 3.0+ */
#define VMM_snr__GetDescriptor                               0x007B    /* ver 3.0+ */
#define VMM_snr__SetDescriptor                               0x007C    /* ver 3.0+ */
#define VMM_snr__MMGR_Toggle_HMA                             0x007D    /* ver 3.0+ */
#define VMM_snr_Get_Fault_Hook_Addrs                         0x007E    /* ver 3.0+ */
#define VMM_snr_Hook_V86_Fault                               0x007F    /* ver 3.0+ */
#define VMM_snr_Hook_PM_Fault                                0x0080    /* ver 3.0+ */
#define VMM_snr_Hook_VMM_Fault                               0x0081    /* ver 3.0+ */
#define VMM_snr_Begin_Nest_V86_Exec                          0x0082    /* ver 3.0+ */
#define VMM_snr_Begin_Nest_Exec                              0x0083    /* ver 3.0+ */
#define VMM_snr_Exec_Int                                     0x0084    /* ver 3.0+ */
#define VMM_snr_Resume_Exec                                  0x0085    /* ver 3.0+ */
#define VMM_snr_End_Nest_Exec                                0x0086    /* ver 3.0+ */
#define VMM_snr_Allocate_PM_App_CB_Area                      0x0087    /* ver 3.0+ */
#define VMM_snr_Get_Cur_PM_App_CB                            0x0088    /* ver 3.0+ */
#define VMM_snr_Set_V86_Exec_Mode                            0x0089    /* ver 3.0+ */
#define VMM_snr_Set_PM_Exec_Mode                             0x008A    /* ver 3.0+ */
#define VMM_snr_Begin_Use_Locked_PM_Stack                    0x008B    /* ver 3.0+ */
#define VMM_snr_End_Use_Locked_PM_Stack                      0x008C    /* ver 3.0+ */
#define VMM_snr_Save_Client_State                            0x008D    /* ver 3.0+ */
#define VMM_snr_Restore_Client_State                         0x008E    /* ver 3.0+ */
#define VMM_snr_Exec_VxD_Int                                 0x008F    /* ver 3.0+ */
#define VMM_snr_Hook_Device_Service                          0x0090    /* ver 3.0+ */
#define VMM_snr_Hook_Device_V86_API                          0x0091    /* ver 3.0+ */
#define VMM_snr_Hook_Device_PM_API                           0x0092    /* ver 3.0+ */
#define VMM_snr_System_Control                               0x0093    /* ver 3.0+ */
#define VMM_snr_Simulate_IO                                  0x0094    /* ver 3.0+ */
#define VMM_snr_Install_Mult_IO_Handlers                     0x0095    /* ver 3.0+ */
#define VMM_snr_Install_IO_Handler                           0x0096    /* ver 3.0+ */
#define VMM_snr_Enable_Global_Trapping                       0x0097    /* ver 3.0+ */
#define VMM_snr_Enable_Local_Trapping                        0x0098    /* ver 3.0+ */
#define VMM_snr_Disable_Global_Trapping                      0x0099    /* ver 3.0+ */
#define VMM_snr_Disable_Local_Trapping                       0x009A    /* ver 3.0+ */
#define VMM_snr_List_Create                                  0x009B    /* ver 3.0+ */
#define VMM_snr_List_Destroy                                 0x009C    /* ver 3.0+ */
#define VMM_snr_List_Allocate                                0x009D    /* ver 3.0+ */
#define VMM_snr_List_Attach                                  0x009E    /* ver 3.0+ */
#define VMM_snr_List_Attach_Tail                             0x009F    /* ver 3.0+ */
#define VMM_snr_List_Insert                                  0x00A0    /* ver 3.0+ */
#define VMM_snr_List_Remove                                  0x00A1    /* ver 3.0+ */
#define VMM_snr_List_Deallocate                              0x00A2    /* ver 3.0+ */
#define VMM_snr_List_Get_First                               0x00A3    /* ver 3.0+ */
#define VMM_snr_List_Get_Next                                0x00A4    /* ver 3.0+ */
#define VMM_snr_List_Remove_First                            0x00A5    /* ver 3.0+ */
#define VMM_snr__AddInstanceItem                             0x00A6    /* ver 3.0+ */
#define VMM_snr__Allocate_Device_CB_Area                     0x00A7    /* ver 3.0+ */
#define VMM_snr__Allocate_Global_V86_Data_Area               0x00A8    /* ver 3.0+ */
#define VMM_snr__Allocate_Temp_V86_Data_Area                 0x00A9    /* ver 3.0+ */
#define VMM_snr__Free_Temp_V86_Data_Area                     0x00AA    /* ver 3.0+ */
#define VMM_snr_Get_Profile_Decimal_Int                      0x00AB    /* ver 3.0+ */
#define VMM_snr_Convert_Decimal_String                       0x00AC    /* ver 3.0+ */
#define VMM_snr_Get_Profile_Fixed_Point                      0x00AD    /* ver 3.0+ */
#define VMM_snr_Convert_Fixed_Point_String                   0x00AE    /* ver 3.0+ */
#define VMM_snr_Get_Profile_Hex_Int                          0x00AF    /* ver 3.0+ */
#define VMM_snr_Convert_Hex_String                           0x00B0    /* ver 3.0+ */
#define VMM_snr_Get_Profile_Boolean                          0x00B1    /* ver 3.0+ */
#define VMM_snr_Convert_Boolean_String                       0x00B2    /* ver 3.0+ */
#define VMM_snr_Get_Profile_String                           0x00B3    /* ver 3.0+ */
#define VMM_snr_Get_Next_Profile_String                      0x00B4    /* ver 3.0+ */
#define VMM_snr_Get_Environment_String                       0x00B5    /* ver 3.0+ */
#define VMM_snr_Get_Exec_Path                                0x00B6    /* ver 3.0+ */
#define VMM_snr_Get_Config_Directory                         0x00B7    /* ver 3.0+ */
#define VMM_snr_OpenFile                                     0x00B8    /* ver 3.0+ */
#define VMM_snr_Get_PSP_Segment                              0x00B9    /* ver 3.0+ */
#define VMM_snr_GetDOSVectors                                0x00BA    /* ver 3.0+ */
#define VMM_snr_Get_Machine_Info                             0x00BB    /* ver 3.0+ */
#define VMM_snr_GetSet_HMA_Info                              0x00BC    /* ver 3.0+ */
#define VMM_snr_Set_System_Exit_Code                         0x00BD    /* ver 3.0+ */
#define VMM_snr_Fatal_Error_Handler                          0x00BE    /* ver 3.0+ */
#define VMM_snr_Fatal_Memory_Error                           0x00BF    /* ver 3.0+ */
#define VMM_snr_Update_System_Clock                          0x00C0    /* ver 3.0+ */
#define VMM_snr_Test_Debug_Installed                         0x00C1    /* ver 3.0+ */
#define VMM_snr_Out_Debug_String                             0x00C2    /* ver 3.0+ */
#define VMM_snr_Out_Debug_Chr                                0x00C3    /* ver 3.0+ */
#define VMM_snr_In_Debug_Chr                                 0x00C4    /* ver 3.0+ */
#define VMM_snr_Debug_Convert_Hex_Binary                     0x00C5    /* ver 3.0+ */
#define VMM_snr_Debug_Convert_Hex_Decimal                    0x00C6    /* ver 3.0+ */
#define VMM_snr_Debug_Test_Valid_Handle                      0x00C7    /* ver 3.0+ */
#define VMM_snr_Validate_Client_Ptr                          0x00C8    /* ver 3.0+ */
#define VMM_snr_Test_Reenter                                 0x00C9    /* ver 3.0+ */
#define VMM_snr_Queue_Debug_String                           0x00CA    /* ver 3.0+ */
#define VMM_snr_Log_Proc_Call                                0x00CB    /* ver 3.0+ */
#define VMM_snr_Debug_Test_Cur_VM                            0x00CC    /* ver 3.0+ */
#define VMM_snr_Get_PM_Int_Type                              0x00CD    /* ver 3.0+ */
#define VMM_snr_Set_PM_Int_Type                              0x00CE    /* ver 3.0+ */
#define VMM_snr_Get_Last_Updated_System_Time                 0x00CF    /* ver 3.0+ */
#define VMM_snr_Get_Last_Updated_VM_Exec_Time                0x00D0    /* ver 3.0+ */
#define VMM_snr_Test_DBCS_Lead_Byte                          0x00D1    /* ver 3.1+ */
#define VMM_snr__AddFreePhysPage                             0x00D2    /* ver 3.1+ */
#define VMM_snr__PageResetHandlePAddr                        0x00D3    /* ver 3.1+ */
#define VMM_snr__SetLastV86Page                              0x00D4    /* ver 3.1+ */
#define VMM_snr__GetLastV86Page                              0x00D5    /* ver 3.1+ */
#define VMM_snr__MapFreePhysReg                              0x00D6    /* ver 3.1+ */
#define VMM_snr__UnmapFreePhysReg                            0x00D7    /* ver 3.1+ */
#define VMM_snr__XchgFreePhysReg                             0x00D8    /* ver 3.1+ */
#define VMM_snr__SetFreePhysRegCalBk                         0x00D9    /* ver 3.1+ */
#define VMM_snr_Get_Next_Arena                               0x00DA    /* ver 3.1+ */
#define VMM_snr_Get_Name_Of_Ugly_TSR                         0x00DB    /* ver 3.1+ */
#define VMM_snr_Get_Debug_Options                            0x00DC    /* ver 3.1+ */
#define VMM_snr_Set_Physical_HMA_Alias                       0x00DD    /* ver 3.1+ */
#define VMM_snr__GetGlblRng0V86IntBase                       0x00DE    /* ver 3.1+ */
#define VMM_snr__Add_Global_V86_Data_Area                    0x00DF    /* ver 3.1+ */
#define VMM_snr_GetSetDetailedVMError                        0x00E0    /* ver 3.1+ */
#define VMM_snr_Is_Debug_Chr                                 0x00E1    /* ver 3.1+ */
#define VMM_snr_Clear_Mono_Screen                            0x00E2    /* ver 3.1+ */
#define VMM_snr_Out_Mono_Chr                                 0x00E3    /* ver 3.1+ */
#define VMM_snr_Out_Mono_String                              0x00E4    /* ver 3.1+ */
#define VMM_snr_Set_Mono_Cur_Pos                             0x00E5    /* ver 3.1+ */
#define VMM_snr_Get_Mono_Cur_Pos                             0x00E6    /* ver 3.1+ */
#define VMM_snr_Get_Mono_Chr                                 0x00E7    /* ver 3.1+ */
#define VMM_snr_Locate_Byte_In_ROM                           0x00E8    /* ver 3.1+ */
#define VMM_snr_Hook_Invalid_Page_Fault                      0x00E9    /* ver 3.1+ */
#define VMM_snr_Unhook_Invalid_Page_Fault                    0x00EA    /* ver 3.1+ */
#define VMM_snr_Set_Delete_On_Exit_File                      0x00EB    /* ver 3.1+ */
#define VMM_snr_Close_VM                                     0x00EC    /* ver 3.1+ */
#define VMM_snr_Enable_Touch_1st_Meg                         0x00ED    /* ver 3.1+ */
#define VMM_snr_Disable_Touch_1st_Meg                        0x00EE    /* ver 3.1+ */
#define VMM_snr_Install_Exception_Handler                    0x00EF    /* ver 3.1+ */
#define VMM_snr_Remove_Exception_Handler                     0x00F0    /* ver 3.1+ */
#define VMM_snr_Get_Crit_Status_No_Block                     0x00F1    /* ver 3.1+ */
#define VMM_snr__GetLastUpdatedThreadExecTime                0x00F2    /* ver 4.0+ */
#define VMM_snr__Trace_Out_Service                           0x00F3    /* ver 4.0+ */
#define VMM_snr__Debug_Out_Service                           0x00F4    /* ver 4.0+ */
#define VMM_snr__Debug_Flags_Service                         0x00F5    /* ver 4.0+ */
#define VMM_snr_VMMAddImportModuleName                       0x00F6    /* ver 4.0+ */
#define VMM_snr_VMM_Add_DDB                                  0x00F7    /* ver 4.0+ */
#define VMM_snr_VMM_Remove_DDB                               0x00F8    /* ver 4.0+ */
#define VMM_snr_Test_VM_Ints_Enabled                         0x00F9    /* ver 4.0+ */
#define VMM_snr__BlockOnID                                   0x00FA    /* ver 4.0+ */
#define VMM_snr_Schedule_Thread_Event                        0x00FB    /* ver 4.0+ */
#define VMM_snr_Cancel_Thread_Event                          0x00FC    /* ver 4.0+ */
#define VMM_snr_Set_Thread_Time_Out                          0x00FD    /* ver 4.0+ */
#define VMM_snr_Set_Async_Time_Out                           0x00FE    /* ver 4.0+ */
#define VMM_snr__AllocateThreadDataSlot                      0x00FF    /* ver 4.0+ */
#define VMM_snr__FreeThreadDataSlot                          0x0100    /* ver 4.0+ */
#define VMM_snr__CreateMutex                                 0x0101    /* ver 4.0+ */
#define VMM_snr__DestroyMutex                                0x0102    /* ver 4.0+ */
#define VMM_snr__GetMutexOwner                               0x0103    /* ver 4.0+ */
#define VMM_snr_Call_When_Thread_Switched                    0x0104    /* ver 4.0+ */
#define VMM_snr_VMMCreateThread                              0x0105    /* ver 4.0+ */
#define VMM_snr__GetThreadExecTime                           0x0106    /* ver 4.0+ */
#define VMM_snr_VMMTerminateThread                           0x0107    /* ver 4.0+ */
#define VMM_snr_Get_Cur_Thread_Handle                        0x0108    /* ver 4.0+ */
#define VMM_snr_Test_Cur_Thread_Handle                       0x0109    /* ver 4.0+ */
#define VMM_snr_Get_Sys_Thread_Handle                        0x010A    /* ver 4.0+ */
#define VMM_snr_Test_Sys_Thread_Handle                       0x010B    /* ver 4.0+ */
#define VMM_snr_Validate_Thread_Handle                       0x010C    /* ver 4.0+ */
#define VMM_snr_Get_Initial_Thread_Handle                    0x010D    /* ver 4.0+ */
#define VMM_snr_Test_Initial_Thread_Handle                   0x010E    /* ver 4.0+ */
#define VMM_snr_Debug_Test_Valid_Thread_Handle               0x010F    /* ver 4.0+ */
#define VMM_snr_Debug_Test_Cur_Thread                        0x0110    /* ver 4.0+ */
#define VMM_snr_VMM_GetSystemInitState                       0x0111    /* ver 4.0+ */
#define VMM_snr_Cancel_Call_When_Thread_Switched             0x0112    /* ver 4.0+ */
#define VMM_snr_Get_Next_Thread_Handle                       0x0113    /* ver 4.0+ */
#define VMM_snr_Adjust_Thread_Exec_Priority                  0x0114    /* ver 4.0+ */
#define VMM_snr__Deallocate_Device_CB_Area                   0x0115    /* ver 4.0+ */
#define VMM_snr_Remove_IO_Handler                            0x0116    /* ver 4.0+ */
#define VMM_snr_Remove_Mult_IO_Handlers                      0x0117    /* ver 4.0+ */
#define VMM_snr_Unhook_V86_Int_Chain                         0x0118    /* ver 4.0+ */
#define VMM_snr_Unhook_V86_Fault                             0x0119    /* ver 4.0+ */
#define VMM_snr_Unhook_PM_Fault                              0x011A    /* ver 4.0+ */
#define VMM_snr_Unhook_VMM_Fault                             0x011B    /* ver 4.0+ */
#define VMM_snr_Unhook_Device_Service                        0x011C    /* ver 4.0+ */
#define VMM_snr__PageReserve                                 0x011D    /* ver 4.0+ */
#define VMM_snr__PageCommit                                  0x011E    /* ver 4.0+ */
#define VMM_snr__PageDecommit                                0x011F    /* ver 4.0+ */
#define VMM_snr__PagerRegister                               0x0120    /* ver 4.0+ */
#define VMM_snr__PagerQuery                                  0x0121    /* ver 4.0+ */
#define VMM_snr__PagerDeregister                             0x0122    /* ver 4.0+ */
#define VMM_snr__ContextCreate                               0x0123    /* ver 4.0+ */
#define VMM_snr__ContextDestroy                              0x0124    /* ver 4.0+ */
#define VMM_snr__PageAttach                                  0x0125    /* ver 4.0+ */
#define VMM_snr__PageFlush                                   0x0126    /* ver 4.0+ */
#define VMM_snr__SignalID                                    0x0127    /* ver 4.0+ */
#define VMM_snr__PageCommitPhys                              0x0128    /* ver 4.0+ */
#define VMM_snr__Register_Win32_Services                     0x0129    /* ver 4.0+ */
#define VMM_snr_Cancel_Call_When_Not_Critical                0x012A    /* ver 4.0+ */
#define VMM_snr_Cancel_Call_When_Idle                        0x012B    /* ver 4.0+ */
#define VMM_snr_Cancel_Call_When_Task_Switched               0x012C    /* ver 4.0+ */
#define VMM_snr__Debug_Printf_Service                        0x012D    /* ver 4.0+ */
#define VMM_snr__EnterMutex                                  0x012E    /* ver 4.0+ */
#define VMM_snr__LeaveMutex                                  0x012F    /* ver 4.0+ */
#define VMM_snr_Simulate_VM_IO                               0x0130    /* ver 4.0+ */
#define VMM_snr_Signal_Semaphore_No_Switch                   0x0131    /* ver 4.0+ */
#define VMM_snr__ContextSwitch                               0x0132    /* ver 4.0+ */
#define VMM_snr__PageModifyPermissions                       0x0133    /* ver 4.0+ */
#define VMM_snr__PageQuery                                   0x0134    /* ver 4.0+ */
#define VMM_snr__EnterMustComplete                           0x0135    /* ver 4.0+ */
#define VMM_snr__LeaveMustComplete                           0x0136    /* ver 4.0+ */
#define VMM_snr__ResumeExecMustComplete                      0x0137    /* ver 4.0+ */
#define VMM_snr__GetThreadTerminationStatus                  0x0138    /* ver 4.0+ */
#define VMM_snr__GetInstanceInfo                             0x0139    /* ver 4.0+ */
#define VMM_snr__ExecIntMustComplete                         0x013A    /* ver 4.0+ */
#define VMM_snr__ExecVxDIntMustComplete                      0x013B    /* ver 4.0+ */
#define VMM_snr_Begin_V86_Serialization                      0x013C    /* ver 4.0+ */
#define VMM_snr_Unhook_V86_Page                              0x013D    /* ver 4.0+ */
#define VMM_snr_VMM_GetVxDLocationList                       0x013E    /* ver 4.0+ */
#define VMM_snr_VMM_GetDDBList                               0x013F    /* ver 4.0+ */
#define VMM_snr_Unhook_NMI_Event                             0x0140    /* ver 4.0+ */
#define VMM_snr_Get_Instanced_V86_Int_Vector                 0x0141    /* ver 4.0+ */
#define VMM_snr_Get_Set_Real_DOS_PSP                         0x0142    /* ver 4.0+ */
#define VMM_snr_Call_Priority_Thread_Event                   0x0143    /* ver 4.0+ */
#define VMM_snr_Get_System_Time_Address                      0x0144    /* ver 4.0+ */
#define VMM_snr_Get_Crit_Status_Thread                       0x0145    /* ver 4.0+ */
#define VMM_snr_Get_DDB                                      0x0146    /* ver 4.0+ */
#define VMM_snr_Directed_Sys_Control                         0x0147    /* ver 4.0+ */
#define VMM_snr__RegOpenKey                                  0x0148    /* ver 4.0+ */
#define VMM_snr__RegCloseKey                                 0x0149    /* ver 4.0+ */
#define VMM_snr__RegCreateKey                                0x014A    /* ver 4.0+ */
#define VMM_snr__RegDeleteKey                                0x014B    /* ver 4.0+ */
#define VMM_snr__RegEnumKey                                  0x014C    /* ver 4.0+ */
#define VMM_snr__RegQueryValue                               0x014D    /* ver 4.0+ */
#define VMM_snr__RegSetValue                                 0x014E    /* ver 4.0+ */
#define VMM_snr__RegDeleteValue                              0x014F    /* ver 4.0+ */
#define VMM_snr__RegEnumValue                                0x0150    /* ver 4.0+ */
#define VMM_snr__RegQueryValueEx                             0x0151    /* ver 4.0+ */
#define VMM_snr__RegSetValueEx                               0x0152    /* ver 4.0+ */
#define VMM_snr__CallRing3                                   0x0153    /* ver 4.0+ */
#define VMM_snr_Exec_PM_Int                                  0x0154    /* ver 4.0+ */
#define VMM_snr__RegFlushKey                                 0x0155    /* ver 4.0+ */
#define VMM_snr__PageCommitContig                            0x0156    /* ver 4.0+ */
#define VMM_snr__GetCurrentContext                           0x0157    /* ver 4.0+ */
#define VMM_snr__LocalizeSprintf                             0x0158    /* ver 4.0+ */
#define VMM_snr__LocalizeStackSprintf                        0x0159    /* ver 4.0+ */
#define VMM_snr_Call_Restricted_Event                        0x015A    /* ver 4.0+ */
#define VMM_snr_Cancel_Restricted_Event                      0x015B    /* ver 4.0+ */
#define VMM_snr_Register_PEF_Provider                        0x015C    /* ver 4.0+ */
#define VMM_snr__GetPhysPageInfo                             0x015D    /* ver 4.0+ */
#define VMM_snr__RegQueryInfoKey                             0x015E    /* ver 4.0+ */
#define VMM_snr_MemArb_Reserve_Pages                         0x015F    /* ver 4.0+ */
#define VMM_snr_Time_Slice_Sys_VM_Idle                       0x0160    /* ver 4.0+ */
#define VMM_snr_Time_Slice_Sleep                             0x0161    /* ver 4.0+ */
#define VMM_snr_Boost_With_Decay                             0x0162    /* ver 4.0+ */
#define VMM_snr_Set_Inversion_Pri                            0x0163    /* ver 4.0+ */
#define VMM_snr_Reset_Inversion_Pri                          0x0164    /* ver 4.0+ */
#define VMM_snr_Release_Inversion_Pri                        0x0165    /* ver 4.0+ */
#define VMM_snr_Get_Thread_Win32_Pri                         0x0166    /* ver 4.0+ */
#define VMM_snr_Set_Thread_Win32_Pri                         0x0167    /* ver 4.0+ */
#define VMM_snr_Set_Thread_Static_Boost                      0x0168    /* ver 4.0+ */
#define VMM_snr_Set_VM_Static_Boost                          0x0169    /* ver 4.0+ */
#define VMM_snr_Release_Inversion_Pri_ID                     0x016A    /* ver 4.0+ */
#define VMM_snr_Attach_Thread_To_Group                       0x016B    /* ver 4.0+ */
#define VMM_snr_Detach_Thread_From_Group                     0x016C    /* ver 4.0+ */
#define VMM_snr_Set_Group_Static_Boost                       0x016D    /* ver 4.0+ */
#define VMM_snr__GetRegistryPath                             0x016E    /* ver 4.0+ */
#define VMM_snr__GetRegistryKey                              0x016F    /* ver 4.0+ */
#define VMM_snr_Cleanup_Thread_State                         0x0170    /* ver 4.0+ */
#define VMM_snr__RegRemapPreDefKey                           0x0171    /* ver 4.0+ */
#define VMM_snr_End_V86_Serialization                        0x0172    /* ver 4.0+ */
#define VMM_snr__Assert_Range                                0x0173    /* ver 4.0+ */
#define VMM_snr__Sprintf                                     0x0174    /* ver 4.0+ */
#define VMM_snr__PageChangePager                             0x0175    /* ver 4.0+ */
#define VMM_snr__RegCreateDynKey                             0x0176    /* ver 4.0+ */
#define VMM_snr__RegQueryMultipleValues                      0x0177    /* ver 4.0+ */
#define VMM_snr_Boost_Thread_With_VM                         0x0178    /* ver 4.0+ */
#define VMM_snr_Get_Boot_Flags                               0x0179    /* ver 4.0+ */
#define VMM_snr_Set_Boot_Flags                               0x017A    /* ver 4.0+ */
#define VMM_snr__lstrcpyn                                    0x017B    /* ver 4.0+ */
#define VMM_snr__lstrlen                                     0x017C    /* ver 4.0+ */
#define VMM_snr__lmemcpy                                     0x017D    /* ver 4.0+ */
#define VMM_snr__GetVxDName                                  0x017E    /* ver 4.0+ */
#define VMM_snr_Force_Mutexes_Free                           0x017F    /* ver 4.0+ */
#define VMM_snr_Restore_Forced_Mutexes                       0x0180    /* ver 4.0+ */
#define VMM_snr__AddReclaimableItem                          0x0181    /* ver 4.0+ */
#define VMM_snr__SetReclaimableItem                          0x0182    /* ver 4.0+ */
#define VMM_snr__EnumReclaimableItem                         0x0183    /* ver 4.0+ */
#define VMM_snr_Time_Slice_Wake_Sys_VM                       0x0184    /* ver 4.0+ */
#define VMM_snr_VMM_Replace_Global_Environment               0x0185    /* ver 4.0+ */
#define VMM_snr_Begin_Non_Serial_Nest_V86_Exec               0x0186    /* ver 4.0+ */
#define VMM_snr_Get_Nest_Exec_Status                         0x0187    /* ver 4.0+ */
#define VMM_snr_Open_Boot_Log                                0x0188    /* ver 4.0+ */
#define VMM_snr_Write_Boot_Log                               0x0189    /* ver 4.0+ */
#define VMM_snr_Close_Boot_Log                               0x018A    /* ver 4.0+ */
#define VMM_snr_EnableDisable_Boot_Log                       0x018B    /* ver 4.0+ */
#define VMM_snr__Call_On_My_Stack                            0x018C    /* ver 4.0+ */
#define VMM_snr_Get_Inst_V86_Int_Vec_Base                    0x018D    /* ver 4.0+ */
#define VMM_snr__lstrcmpi                                    0x018E    /* ver 4.0+ */
#define VMM_snr__strupr                                      0x018F    /* ver 4.0+ */
#define VMM_snr_Log_Fault_Call_Out                           0x0190    /* ver 4.0+ */
#define VMM_snr__AtEventTime                                 0x0191    /* ver 4.0+ */
#define VMM_snr__PageOutPages                                0x0192    /* ver 4.10+ */
#define VMM_snr__Call_On_My_Not_Flat_Stack                   0x0193    /* ver 4.10+ */
#define VMM_snr__LinRegionLock                               0x0194    /* ver 4.10+ */
#define VMM_snr__LinRegionUnLock                             0x0195    /* ver 4.10+ */
#define VMM_snr__AttemptingSomethingDangerous                0x0196    /* ver 4.10+ */
#define VMM_snr__Vsprintf                                    0x0197    /* ver 4.10+ */
#define VMM_snr__Vsprintfw                                   0x0198    /* ver 4.10+ */
#define VMM_snr_Load_FS_Service                              0x0199    /* ver 4.10+ */
#define VMM_snr_Assert_FS_Service                            0x019A    /* ver 4.10+ */
#define VMM_snr__Begin_Preemptable_Code                      0x019B    /* ver 4.10+ */
#define VMM_snr__End_Preemptable_Code                        0x019C    /* ver 4.10+ */
#define VMM_snr__Get_CPUID_Flags                             0x019D    /* ver 4.10+ */
#define VMM_snr__RegisterGARTHandler                         0x019E    /* ver 4.10+ */
#define VMM_snr__GARTReserve                                 0x019F    /* ver 4.10+ */
#define VMM_snr__GARTCommit                                  0x01A0    /* ver 4.10+ */
#define VMM_snr__GARTUnCommit                                0x01A1    /* ver 4.10+ */
#define VMM_snr__GARTFree                                    0x01A2    /* ver 4.10+ */
#define VMM_snr__GARTMemAttributes                           0x01A3    /* ver 4.10+ */
#define VMM_snr_VMMCreateThreadEx                            0x01A4    /* ver 4.10+ */
#define VMM_snr__FlushCaches                                 0x01A5    /* ver 4.10+ */
#define VMM_snr_Set_Thread_Win32_Pri_NoYield                 0x01A6    /* ver 4.10+ */
#define VMM_snr__FlushMappedCacheBlock                       0x01A7    /* ver 4.10+ */
#define VMM_snr__ReleaseMappedCacheBlock                     0x01A8    /* ver 4.10+ */
#define VMM_snr_Run_Preemptable_Events                       0x01A9    /* ver 4.10+ */
#define VMM_snr__MMPreSystemExit                             0x01AA    /* ver 4.10+ */
#define VMM_snr__MMPageFileShutDown                          0x01AB    /* ver 4.10+ */
#define VMM_snr__Set_Global_Time_Out_Ex                      0x01AC    /* ver 4.10+ */
#define VMM_snr_Query_Thread_Priority                        0x01AD    /* ver 4.10+ */
#define VMM_snr__UnmapPhysToLinear                           0x01AE    /* ver 4.90+ */
#define VMM_snr__VmmRtInfo                                   0x01AF    /* ver 4.90+ */
#define VMM_snr__MPGetProcessorCount                         0x01B0    /* ver 4.90+ */
#define VMM_snr__MPEnterSingleProcessor                      0x01B1    /* ver 4.90+ */
#define VMM_snr__MPLeaveSingleProcessor                      0x01B2    /* ver 4.90+ */

/* NOTE: Some VxD calls are defined static inline to return a struct. As long as you simply read the */
/*       structure members, GCC's optimizer will boil it down to direct register access of the values */
/*       returned by the call and direct testing of the CPU flags. */

#if defined(__GNUC__) /* GCC only, for now */
# if defined(GCC_INLINE_ASM_SUPPORTS_cc_OUTPUT) /* we require GCC 6.1 or higher with support for CPU flags as output */
/*-------------------------------------------------------------*/
/* VMM Get_VMM_Version (VMMCall dev=0x0001 serv=0x0000) */

/* description: */
/*   Return VMM version */

/* inputs: */
/*   None */

/* outputs: */
/*   AX = version (AH=Major AL=Minor (example: 0x030A = 3.10)) */
/*   ECX = debug (debug development revision number) */

typedef struct Get_VMM_Version__response {
    uint16_t version; /* AX */
    uint32_t debug; /* ECX */
} Get_VMM_Version__response;

static inline Get_VMM_Version__response Get_VMM_Version(void) {
    register Get_VMM_Version__response r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_VMM_Version)
        : /* outputs */ "=a" (r.version), "=c" (r.debug)
        : /* inputs */
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Get_Cur_VM_Handle (VMMCall dev=0x0001 serv=0x0001) */

/* description: */
/*   Return current VM handle */

/* inputs: */
/*   None */

/* outputs: */
/*   EBX = Handle of the current VM */

/* returns: */
/*   Current VM handle */

static inline vxd_vm_handle_t Get_Cur_VM_Handle(void) {
    register vxd_vm_handle_t r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_Cur_VM_Handle)
        : /* outputs */ "=b" (r)
        : /* inputs */
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Test_Cur_VM_Handle (VMMCall dev=0x0001 serv=0x0002) */

/* description: */
/*   Test whether VM handle is current VM */

/* inputs: */
/*   EBX = vm (VM handle to test) */

/* outputs: */
/*   ZF = ZF set if vm handle matches */

/* returns: */
/*   Boolean value. True if VM handle matches, false if not. */

static inline _Bool Test_Cur_VM_Handle(const vxd_vm_handle_t vm/*ebx*/) {
    register _Bool r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Test_Cur_VM_Handle)
        : /* outputs */ "=@ccz" (r)
        : /* inputs */ "b" (vm)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Get_Sys_VM_Handle (VMMCall dev=0x0001 serv=0x0003) */

/* description: */
/*   Return system VM handle */

/* inputs: */
/*   None */

/* outputs: */
/*   EBX = Handle of the system VM */

/* returns: */
/*   System VM handle */

static inline vxd_vm_handle_t Get_Sys_VM_Handle(void) {
    register vxd_vm_handle_t r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_Sys_VM_Handle)
        : /* outputs */ "=b" (r)
        : /* inputs */
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Test_Sys_VM_Handle (VMMCall dev=0x0001 serv=0x0004) */

/* description: */
/*   Test whether VM handle is system VM */

/* inputs: */
/*   EBX = vm (VM handle to test) */

/* outputs: */
/*   ZF = ZF set if vm handle matches */

/* returns: */
/*   Boolean value. True if VM handle matches the one given, false if not. */

static inline _Bool Test_Sys_VM_Handle(const vxd_vm_handle_t vm/*ebx*/) {
    register _Bool r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Test_Sys_VM_Handle)
        : /* outputs */ "=@ccz" (r)
        : /* inputs */ "b" (vm)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Validate_VM_Handle (VMMCall dev=0x0001 serv=0x0005) */

/* description: */
/*   Verify that the VM handle is valid */

/* inputs: */
/*   EBX = vm (VM handle to test) */

/* outputs: */
/*   !CF = CF is set if NOT valid, clear if valid. Return value should invert sense. */

/* returns: */
/*   Boolean value. True if VM handle is valid, false if invalid. */

static inline _Bool Validate_VM_Handle(const vxd_vm_handle_t vm/*ebx*/) {
    register _Bool r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Validate_VM_Handle)
        : /* outputs */ "=@ccnc" (r)
        : /* inputs */ "b" (vm)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Get_VMM_Reenter_Count (VMMCall dev=0x0001 serv=0x0006) */

/* description: */
/*   Return the number of times the VMM has been reentered. If nonzero, use only asynchronous calls. */

/* inputs: */
/*   None */

/* outputs: */
/*   ECX = number of times VMM has been reentered */

static inline uint32_t Get_VMM_Reenter_Count(void) {
    register uint32_t r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_VMM_Reenter_Count)
        : /* outputs */ "=c" (r)
        : /* inputs */
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Begin_Reentrant_Execution (VMMCall dev=0x0001 serv=0x0007) */

/* description: */
/*   Start reentrant execution. You can use this when hooking VMM faults (reentrant processor faults) */
/*   in order to call non-asynchronous VMM or virtual device services or execute a virtual machine.   */
/*   Do not use this service to avoid scheduling events on hardware interrupts.                       */

/* inputs: */
/*   None */

/* outputs: */
/*   ECX = old reentrancy count */

/* returns: */
/*   unsigned int containing old reentrancy count, which must be saved and given to End_Reentrancy_Execution later on. */

static inline uint32_t Begin_Reentrant_Execution(void) {
    register uint32_t r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Begin_Reentrant_Execution)
        : /* outputs */ "=c" (r)
        : /* inputs */
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM End_Reentrant_Execution (VMMCall dev=0x0001 serv=0x0008) */

/* description: */
/*   Ends reentrant execution, after Begin_Reentrant_Execution. */

/* inputs: */
/*   ECX = count (reentrancy count returned by Begin_Reentrant_Execution) */

/* outputs: */
/*   None */

static inline void End_Reentrant_Execution(const uint32_t count/*ecx*/) {
    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_End_Reentrant_Execution)
        : /* outputs */
        : /* inputs */ "c" (count)
        : /* clobbered */
    );
}

/*-------------------------------------------------------------*/
/* VMM Install_V86_Break_Point (VMMCall dev=0x0001 serv=0x0009) */

/* description: */
/*   Insert a break point in virtual 8086 memory of the current virtual machine, and         */
/*   insert a breakpoint callback procedure to receive control when the break point happens. */

/* inputs: */
/*   EAX = breakaddr (V86 address to place the break point) */
/*   EDX = refdata (pointer to reference data to be passed to callback procedure) */
/*   ESI = callback (pointer to callback procedure to install (32-bit offset)) */

/* outputs: */
/*   !CF = success (CF clear) or failure (CF set) */

/* returns: */
/*   Bool, true if success, false if failure (not installed) */

static inline _Bool Install_V86_Break_Point(const void*const breakaddr/*eax*/,const void*const refdata/*edx*/,const void*const callback/*esi*/) {
    register _Bool r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Install_V86_Break_Point)
        : /* outputs */ "=@ccnc" (r)
        : /* inputs */ "a" (breakaddr), "d" (refdata), "S" (callback)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Remove_V86_Break_Point (VMMCall dev=0x0001 serv=0x000A) */

/* description: */
/*   Remove a virtual 8086 break point installed with Install_V86_Break_Point in the current VM */

/* inputs: */
/*   EAX = breakaddr (V86 address to remove break point from) */

/* outputs: */
/*   None */

static inline void Remove_V86_Break_Point(const void*const breakaddr/*eax*/) {
    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Remove_V86_Break_Point)
        : /* outputs */
        : /* inputs */ "a" (breakaddr)
        : /* clobbered */
    );
}

/*-------------------------------------------------------------*/
/* VMM Allocate_V86_Call_Back (VMMCall dev=0x0001 serv=0x000B) */

/* description: */
/*   Install a callback procedure for virtual 8086 mode applications can call to execute code in */
/*   a virtual device.                                                                           */

/* inputs: */
/*   EDX = refdata (points to reference data to pass to callback procedure) */
/*   ESI = callback (points to callback procedure to call) */

/* outputs: */
/*   CF = error (if success, CF=0 and EAX=realmode ptr. if failure, CF=1) */
/*   EAX = callbackaddr (if CF=0, segment:offset of real-mode callback address) */

typedef struct Allocate_V86_Call_Back__response {
    _Bool error; /* CF */
    uint32_t callbackaddr; /* EAX */
} Allocate_V86_Call_Back__response;

static inline Allocate_V86_Call_Back__response Allocate_V86_Call_Back(const void*const refdata/*edx*/,const void*const callback/*esi*/) {
    register Allocate_V86_Call_Back__response r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Allocate_V86_Call_Back)
        : /* outputs */ "=@ccc" (r.error), "=a" (r.callbackaddr)
        : /* inputs */ "d" (refdata), "S" (callback)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Allocate_PM_Call_Back (VMMCall dev=0x0001 serv=0x000C) */

/* description: */
/*   Install a callback procedure for protected mode applications to call to execute code in a virtual device. */

/* inputs: */
/*   EDX = refdata (points to reference data to pass to callback procedure) */
/*   ESI = callback (points to callback procedure to call) */

/* outputs: */
/*   CF = error (if success, CF=0 and EAX=realmode ptr. if failure, CF=1) */
/*   EAX = callbackaddr (if CF=0, address of callback procedure) */

typedef struct Allocate_PM_Call_Back__response {
    _Bool error; /* CF */
    uint32_t callbackaddr; /* EAX */
} Allocate_PM_Call_Back__response;

static inline Allocate_PM_Call_Back__response Allocate_PM_Call_Back(const void*const refdata/*edx*/,const void*const callback/*esi*/) {
    register Allocate_PM_Call_Back__response r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Allocate_PM_Call_Back)
        : /* outputs */ "=@ccc" (r.error), "=a" (r.callbackaddr)
        : /* inputs */ "d" (refdata), "S" (callback)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Call_When_VM_Returns (VMMCall dev=0x0001 serv=0x000D) */

/* description: */
/*   Install a callback procedure to receive control when a virtual machine executes the IRET instruction for  */
/*   the current interrupt.                                                                                    */
/*                                                                                                             */
/*   if TimeOut is positive, callback is called if VM does not execute IRET within the timeout period.         */
/*   if TimeOut is negative, callabck is called when timeout occurs and again when IRET is executed by the VM. */
/*   if TimeOut is zero, timeout is ignored.                                                                   */

/* inputs: */
/*   EAX = timeout (number of milliseconds for timeout. see description for details.) */
/*   EDX = refdata (pointer to reference data to pass to callback) */
/*   ESI = callback (callback procedure (32-bit flat)) */

/* outputs: */
/*   None */

static inline void Call_When_VM_Returns(const int32_t timeout/*eax*/,const void*const refdata/*edx*/,const void*const callback/*esi*/) {
    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Call_When_VM_Returns)
        : /* outputs */
        : /* inputs */ "a" (timeout), "d" (refdata), "S" (callback)
        : /* clobbered */
    );
}

/*-------------------------------------------------------------*/
/* VMM Schedule_Global_Event (VMMCall dev=0x0001 serv=0x000E) */

/* description: */
/*   Schedule a global event, which does not require a specific virtual machine to process it. */
/*   The system does not switch tasks before calling the procedure.                            */
/*                                                                                             */
/*   The callback can carry out any actions and use any VMM services. It is called like this:  */
/*                                                                                             */
/*   mov ebx,VM ; current VM handle                                                            */
/*   mov edx,RefData ; reference data pointer                                                  */
/*   mov ebp,crs ; pointer to a Client_Reg_Struc                                               */
/*   call [EventCallback]                                                                      */

/* inputs: */
/*   ESI = eventcallback (pointer to callback procedure (32-bit flat)) */
/*   EDX = refdata (pointer to reference data to pass to callback) */

/* outputs: */
/*   ESI = event handle */

static inline uint32_t Schedule_Global_Event(const void*const eventcallback/*esi*/,const void*const refdata/*edx*/) {
    register uint32_t r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Schedule_Global_Event)
        : /* outputs */ "=S" (r)
        : /* inputs */ "S" (eventcallback), "d" (refdata)
        : /* clobbered */
    );

    return r;
}

/*-------------------------------------------------------------*/
/* VMM Schedule_VM_Event (VMMCall dev=0x0001 serv=0x000F) */

/* description: */
/*   Schedule an event for the specified virtual machine. The system will carry out a task switch */
/*   to the virtual machine before calling the event callback procedure.                          */

/* inputs: */
/*   EBX = vm (VM handle to schedule event) */
/*   ESI = eventcallback (pointer to callback procedure (32-bit flat)) */
/*   EDX = refdata (pointer to reference data to pass to callback) */

/* outputs: */
/*   ESI = event handle */

static inline uint32_t Schedule_VM_Event(const vxd_vm_handle_t vm/*ebx*/,const void*const eventcallback/*esi*/,const void*const refdata/*edx*/) {
    register uint32_t r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Schedule_VM_Event)
        : /* outputs */ "=S" (r)
        : /* inputs */ "b" (vm), "S" (eventcallback), "d" (refdata)
        : /* clobbered */
    );

    return r;
}

# endif /*GCC_INLINE_ASM_SUPPORTS_cc_OUTPUT*/
#endif /*defined(__GNUC__)*/
