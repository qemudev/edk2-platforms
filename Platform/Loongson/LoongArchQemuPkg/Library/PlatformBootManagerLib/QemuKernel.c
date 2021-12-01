/** @file
  Try to run Linux kernel.

  Copyright (c) 2021 Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/LoadLinuxLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BpiLib/Bpi.h>
#include <string.h>
#include <Library/BaseMemoryLib.h>

#define ALIGN_UP(addr, align) \
    ((addr + (typeof (addr)) align - 1) & ~((typeof (addr)) align - 1))

#define LOONGARCH_CSR_DMWIN1         0x181 /* 64 direct map win1: MEM & IF */
UINT8
calculatechecksum8 (const UINT8 *buffer, UINTN length)
{
    UINT8 checksum;
    UINTN count;

    for (checksum = 0, count = 0; count < length; count++)
    {
      checksum = (UINT8) (checksum + *(buffer + count));
    }
    return (UINT8) (0x100 - checksum);
}

UINT32 memmap_sort(struct MemMap array[], UINT32 length, MEM_MAP * bpmem, UINT32 index, UINT32 memtype)
{
    UINT64 tempmemsize = 0;
    UINT32 j = 0;
    UINT32 t = 0;

    for(j = 0; j < length;)
    {
      tempmemsize = array[j].MemSize;
      for(t = j + 1; t < length; t++)
      {
        if(array[j].MemStart + tempmemsize == array[t].MemStart)
        {
          tempmemsize += array[t].MemSize;
        }
        else
        {
          break;
        }
     }
     bpmem->Map[index].MemType = memtype;
     bpmem->Map[index].MemStart = array[j].MemStart;
     bpmem->Map[index].MemSize = tempmemsize;
     DEBUG ((EFI_D_INFO, "map[%d]:type %x, start 0x%llx, end 0x%llx\n",
             index,
             bpmem->Map[index].MemType,
             bpmem->Map[index].MemStart,
             bpmem->Map[index].MemStart+ bpmem->Map[index].MemSize
            ));
     j = t;
     index++;
    }
    return index;
}

MEM_MAP * findnewinterfacemem(IN struct BootParamsInterface *Bp)
{
    char *p =NULL;
    MEM_MAP *new_interface_mem = NULL;
    EXT_LIST *listpointer = NULL;

    if(NULL == Bp)
    {
        return new_interface_mem;
    }

    p = (char *)&(Bp->Signature);
    if(0 == AsciiStrnCmp(p, "BPI", 3)){
        listpointer = Bp->ExtList;
        for(;listpointer != NULL; listpointer = listpointer->next) {
            char *pl = (char *)&(listpointer->Signature);
            if(0 == AsciiStrnCmp(pl, "MEM", 3)) {
                new_interface_mem = (MEM_MAP *)listpointer;
            }
        }
    }

    return new_interface_mem;
}


EFI_MEMORY_DESCRIPTOR * get_system_memap(UINTN *MapKey, UINTN *MemoryMapSize, UINTN *DescriptorSize)
{
    EFI_STATUS                           Status = EFI_SUCCESS;
    EFI_MEMORY_DESCRIPTOR                *MemoryMap = NULL;
    UINTN                                MemoryMapSizeTemp = 0;
    UINTN                                DescriptorSizeTemp = 0;
    UINT8                                TmpMemoryMap[1];
    UINT32                               DescriptorVersion = 0;

    // Get System MemoryMapSize
    MemoryMapSizeTemp = sizeof (TmpMemoryMap);
    Status = gBS->GetMemoryMap (
                    &MemoryMapSizeTemp,
                    (EFI_MEMORY_DESCRIPTOR *)TmpMemoryMap,
                    MapKey,
                    &DescriptorSizeTemp,
                    &DescriptorVersion
                    );
    ASSERT (Status == EFI_BUFFER_TOO_SMALL);

    DEBUG ((EFI_D_INFO, "%a %a:%d Status 0x%x\n", __FILE__, __func__, __LINE__, Status));
    // Enlarge space here, because we will allocate pool now.
    MemoryMapSizeTemp += EFI_PAGE_SIZE;
    Status = gBS->AllocatePool (
                    EfiLoaderData,
                    MemoryMapSizeTemp,
                    (VOID **) &MemoryMap
                    );
    ASSERT_EFI_ERROR (Status);

    DEBUG ((EFI_D_INFO, "%a %a:%d Status 0x%x\n", __FILE__, __func__, __LINE__, Status));

    *MemoryMapSize = MemoryMapSizeTemp;
    *DescriptorSize = DescriptorSizeTemp;
    if(NULL == MemoryMap)
    {
        return NULL;
    }

    // Get System MemoryMap
    Status = gBS->GetMemoryMap (
                    &MemoryMapSizeTemp,
                    MemoryMap,
                    MapKey,
                    &DescriptorSizeTemp,
                    &DescriptorVersion
                    );
    ASSERT_EFI_ERROR (Status);

    DEBUG ((EFI_D_INFO, "%a %a:%d Status 0x%x\n", __FILE__, __func__, __LINE__, Status));
    *MemoryMapSize = MemoryMapSizeTemp;
    *DescriptorSize = DescriptorSizeTemp;

    return MemoryMap;
}

VOID MemMapSort(MEM_MAP *new_interface_mem, EFI_MEMORY_DESCRIPTOR *MemoryMapPtr, UINTN MemoryMapSize,
                UINTN DescriptorSize)
{
    struct MemMap reserve_mem[MAX_MEM_MAP];
    struct MemMap free_mem[MAX_MEM_MAP];
    struct MemMap acpi_table_mem[MAX_MEM_MAP];
    struct MemMap acpi_nvs_mem[MAX_MEM_MAP];
    UINT32 free_index = 0;
    UINT32 reserve_index = 0;
    UINT32 acpi_table_index = 0;
    UINT32 acpi_nvs_index = 0;
    UINT64 tempMemsize = 0;
    UINT32 tmp_index = 0;
    UINT32 Index, j, t;
    UINT8 checksum = 0;
    if((NULL == new_interface_mem) ||
       (NULL == MemoryMapPtr))
    {
        return ;
    }
    ZeroMem(reserve_mem, sizeof(struct MemMap) * MAX_MEM_MAP);
    ZeroMem(free_mem, sizeof(struct MemMap) * MAX_MEM_MAP);
    ZeroMem(acpi_table_mem, sizeof(struct MemMap) * MAX_MEM_MAP);
    ZeroMem(acpi_nvs_mem, sizeof(struct MemMap) * MAX_MEM_MAP);

    tmp_index = new_interface_mem->MapCount;

    for (Index = 0; Index < (MemoryMapSize / DescriptorSize); Index++) {

        if (MemoryMapPtr->NumberOfPages == 0) {
          continue;
        }

        switch(MemoryMapPtr->Type)
        {
            case EfiACPIReclaimMemory:
            {
                acpi_table_mem[acpi_table_index].MemType = ACPI_TABLE;
                acpi_table_mem[acpi_table_index].MemStart = (MemoryMapPtr->PhysicalStart) & 0xffffffffffff;
                acpi_table_mem[acpi_table_index].MemSize = MemoryMapPtr->NumberOfPages * 4096;
                acpi_table_index++;
                break;
            }
            case EfiACPIMemoryNVS:
            {
                acpi_nvs_mem[acpi_nvs_index].MemType = ACPI_NVS;
                acpi_nvs_mem[acpi_nvs_index].MemStart = (MemoryMapPtr->PhysicalStart) & 0xffffffffffff;
                acpi_nvs_mem[acpi_nvs_index].MemSize = MemoryMapPtr->NumberOfPages * 4096;
                acpi_nvs_index++;
                break;
            }
            case EfiRuntimeServicesData:
            case EfiRuntimeServicesCode:
            case EfiReservedMemoryType:
            case EfiPalCode:
            {
                reserve_mem[reserve_index].MemType = SYSTEM_RAM_RESERVED;
                reserve_mem[reserve_index].MemStart = (MemoryMapPtr->PhysicalStart) & 0xffffffffffff;
                reserve_mem[reserve_index].MemSize = MemoryMapPtr->NumberOfPages * 4096;
                reserve_index++;
                break;
            }
            default :
            {
                free_mem[free_index].MemType = SYSTEM_RAM;
                free_mem[free_index].MemStart = (MemoryMapPtr->PhysicalStart) & 0xffffffffffff;
                free_mem[free_index].MemSize = MemoryMapPtr->NumberOfPages * 4096;
                free_index++;
                break;
            }
        }
        // Get next item
        MemoryMapPtr = (EFI_MEMORY_DESCRIPTOR *)((UINTN)MemoryMapPtr + DescriptorSize);
    }

    /* Recovery sort */
    for(j = 0; j < free_index;)
    {
        tempMemsize = free_mem[j].MemSize;
        for(t = j + 1; t < free_index; t++)
        {
          if((free_mem[j].MemStart + tempMemsize == free_mem[t].MemStart) && (free_mem[j].MemType == free_mem[t].MemType))
          {
            tempMemsize += free_mem[t].MemSize;
          }else{
            break;
          }
        }

        new_interface_mem->Map[tmp_index].MemType = SYSTEM_RAM;
        new_interface_mem->Map[tmp_index].MemStart = free_mem[j].MemStart;
        new_interface_mem->Map[tmp_index].MemSize = tempMemsize;

        j = t;
        tmp_index++;
    }

        tmp_index = memmap_sort(reserve_mem, reserve_index, new_interface_mem, tmp_index, SYSTEM_RAM_RESERVED);

    new_interface_mem->MapCount = tmp_index;
    new_interface_mem->Header.CheckSum = 0;

    checksum = calculatechecksum8((const UINT8 *)new_interface_mem, new_interface_mem->Header.Length);
    new_interface_mem->Header.CheckSum = checksum;

    return ;
}

STATIC VOID SetupLinuxBootParams (IN OUT struct BootParamsInterface *Bp)
{

    EFI_MEMORY_DESCRIPTOR *MemoryMapPtr = NULL;
    MEM_MAP *new_interface_mem = NULL;
    UINTN MapKey = 0;
    UINTN MemoryMapSize = 0;
    UINTN DescriptorSize = 0;

    new_interface_mem = findnewinterfacemem(Bp);
    MemoryMapPtr = get_system_memap(&MapKey, &MemoryMapSize, &DescriptorSize);

    DEBUG ((EFI_D_INFO, "new_interface_mem %p MemoryMapPtr %p MapKey %x.\n",
           new_interface_mem, MemoryMapPtr, MapKey));
    MemMapSort(new_interface_mem, MemoryMapPtr, MemoryMapSize, DescriptorSize);

    gBS->ExitBootServices (gImageHandle, MapKey);

    return ;
}



/**
 * execute from kernel entry point.
 *
 * @param[in] Argc		The count of args.
 * @param[in] Argv		The pointer to args array.
 * @param[in] Bpi		The pointer to bootparaminterface struct.
 * @param[in] Vec		The fourth args for kernel.
 ***/
typedef
VOID
(EFIAPI *EFI_KERNEL_ENTRY_POINT)(
  IN UINTN  Argc,
  IN VOID   *Argv,
  IN VOID   *Bpi,
  IN VOID   *Vec
  );

EFI_STATUS
TryRunningQemuKernel (
  VOID
  )
{
    EFI_STATUS                Status;
    UINT32                    Size;
    UINT32                    Argc;
    VOID                      **Argv;
    UINTN                     CommandLineSize;
    CHAR8                     *CommandLine;
    VOID                      *KernelEntryPoint;
    VOID                      *Bpi;
    EFI_PHYSICAL_ADDRESS Address;

    CommandLine = NULL;
    CHAR8 *Args[] = {"a0", CommandLine};
    Argc = ARRAY_SIZE(Args);
    Size = Argc * sizeof (VOID **);
    Size += sizeof (VOID **);

    Size += ALIGN_UP (AsciiStrLen(Args[0]) + 1, 4);

    if (!QemuFwCfgIsAvailable ()) {
      return EFI_NOT_FOUND;
    }

    /* get command line size */
    QemuFwCfgSelectItem (QemuFwCfgItemCommandLineSize);
    CommandLineSize = (UINTN) QemuFwCfgRead32 ();
    DEBUG ((EFI_D_INFO, "command line size: %d.\n", CommandLineSize));

    Size += ALIGN_UP (CommandLineSize + 1, 4);
    DEBUG ((EFI_D_INFO, "kernel args size: %d.\n", Size));
    //Argv = LoadLinuxAllocateCommandLinePages (EFI_SIZE_TO_PAGES (Size));
    Status = gBS->AllocatePages(
                                AllocateAnyPages,
                                EfiRuntimeServicesData,
                                EFI_SIZE_TO_PAGES(Size),
                                &Address
                                );
    if (EFI_ERROR (Status)) {
        return Status;
    }
    Argv = (VOID *)(UINTN) Address;
    DEBUG ((EFI_D_INFO, "kernel argv address: 0x%0x.\n", Argv));

    VOID **P = Argv;
    CHAR8 *Pos = (CHAR8 *)(Argv + (Argc + 1));
    AsciiStrCpyS(Pos, 256, Args[0]);
    *P++ = Pos;

    Pos += ALIGN_UP (AsciiStrLen(Args[0]) + 1, 4);
    /* get command line content */
    QemuFwCfgSelectItem (QemuFwCfgItemCommandLineData);
    QemuFwCfgReadBytes (CommandLineSize, Pos);
    *P++ = Pos;

    *P = NULL;

    /* get kernel entry point */
    QemuFwCfgSelectItem (QemuFwCfgItemKernelEntry);
    KernelEntryPoint = (VOID *)((UINTN)((UINT32)QemuFwCfgRead64 ()));

    DEBUG ((EFI_D_INFO, "kernel entry point: %p.\n", KernelEntryPoint));
    if (KernelEntryPoint == NULL) {
        DEBUG ((EFI_D_INFO, "kernel entry point invalid.\n"));
        return EFI_NOT_FOUND;
    }

    Status = EfiGetSystemConfigurationTable (
               &gEfiLoongsonBootparamsTableGuid,
               (VOID **) &Bpi
               );
    if (EFI_ERROR (Status) || Bpi == NULL) {
        DEBUG ((EFI_D_ERROR, "Get Boot Params Table Failed!\n"));
        return EFI_NOT_FOUND;
    }

    SetupLinuxBootParams((struct BootParamsInterface *)Bpi);

    DEBUG ((EFI_D_INFO, "kernel argc: %d, argv: 0x%0x, bpi: 0x%0x.\n", Argc, Argv, Bpi));
    DEBUG ((EFI_D_INFO, "entry kernel ...\n"));
    ((EFI_KERNEL_ENTRY_POINT)KernelEntryPoint)(Argc, Argv, Bpi, NULL);

    return EFI_SUCCESS;
}
