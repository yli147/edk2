/** @file
  Entry point to the Standalone MM Foundation when initialized during the SEC
  phase on RISCV platforms

Copyright (c) 2017 - 2021, Arm Ltd. All rights reserved.<BR>
Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <Library/StandaloneMmCpu.h>
#include <Library/RiscV64/StandaloneMmCoreEntryPoint.h>

#include <PiPei.h>
#include <Guid/MmramMemoryReserve.h>
#include <Guid/MpInformation.h>

#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SerialPortLib.h>
#include <Library/PcdLib.h>

#include "Library/BaseRiscVSbiLib.h"
#include <Library/BaseRiscVCoVELib.h>
#include <Library/CpuLib.h>

#define BOOT_PAYLOAD_VERSION  1
#define EFI_PARAM_ATTR_COVE   1

PI_MM_CPU_DRIVER_ENTRYPOINT  CpuDriverEntryPoint = NULL;

/**
  Retrieve a pointer to and print the boot information passed by privileged
  secure firmware.

  @param  [in] SharedBufAddress   The pointer memory shared with privileged
                                  firmware.

**/
EFI_RISCV_MM_BOOT_INFO *
GetAndPrintBootinformation (
  IN VOID  *BootInfoAddress
  )
{
  EFI_RISCV_MM_BOOT_INFO        *PayloadBootInfo;
  EFI_RISCV_MM_CPU_INFO         *PayloadCpuInfo;
  UINTN                         Index;

  PayloadBootInfo = (EFI_RISCV_MM_BOOT_INFO *)BootInfoAddress;

  if (PayloadBootInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "PayloadBootInfo NULL\n"));
    return NULL;
  }

  if (PayloadBootInfo->Header.Version != BOOT_PAYLOAD_VERSION) {
    DEBUG ((
      DEBUG_ERROR,
      "Boot Information Version Mismatch. Current=0x%x, Expected=0x%x.\n",
      PayloadBootInfo->Header.Version,
      BOOT_PAYLOAD_VERSION
      ));
    return NULL;
  }

  DEBUG ((DEBUG_INFO, "NumMmMemRegions - 0x%x\n", PayloadBootInfo->NumMmMemRegions));
  DEBUG ((DEBUG_INFO, "MmMemBase       - 0x%lx\n", PayloadBootInfo->MmMemBase));
  DEBUG ((DEBUG_INFO, "MmMemLimit      - 0x%lx\n", PayloadBootInfo->MmMemLimit));
  DEBUG ((DEBUG_INFO, "MmImageBase     - 0x%lx\n", PayloadBootInfo->MmImageBase));
  DEBUG ((DEBUG_INFO, "MmStackBase     - 0x%lx\n", PayloadBootInfo->MmStackBase));
  DEBUG ((DEBUG_INFO, "MmHeapBase      - 0x%lx\n", PayloadBootInfo->MmHeapBase));
  DEBUG ((DEBUG_INFO, "MmNsCommBufBase - 0x%lx\n", PayloadBootInfo->MmNsCommBufBase));
  DEBUG ((DEBUG_INFO, "MmSharedBufBase - 0x%lx\n", PayloadBootInfo->MmSharedBufBase));

  DEBUG ((DEBUG_INFO, "MmImageSize     - 0x%x\n", PayloadBootInfo->MmImageSize));
  DEBUG ((DEBUG_INFO, "MmPcpuStackSize - 0x%x\n", PayloadBootInfo->MmPcpuStackSize));
  DEBUG ((DEBUG_INFO, "MmHeapSize      - 0x%x\n", PayloadBootInfo->MmHeapSize));
  DEBUG ((DEBUG_INFO, "MmNsCommBufSize - 0x%x\n", PayloadBootInfo->MmNsCommBufSize));
  DEBUG ((DEBUG_INFO, "MmSharedBufSize - 0x%x\n", PayloadBootInfo->MmSharedBufSize));

  DEBUG ((DEBUG_INFO, "NumCpus         - 0x%x\n", PayloadBootInfo->NumCpus));

  PayloadCpuInfo = (EFI_RISCV_MM_CPU_INFO *)&(PayloadBootInfo->CpuInfo);

  for (Index = 0; Index < PayloadBootInfo->NumCpus; Index++) {
    DEBUG ((DEBUG_INFO, "ProcessorId        - 0x%lx\n", PayloadCpuInfo[Index].ProcessorId));
    DEBUG ((DEBUG_INFO, "Package            - 0x%x\n", PayloadCpuInfo[Index].Package));
    DEBUG ((DEBUG_INFO, "Core               - 0x%x\n", PayloadCpuInfo[Index].Core));
  }
  return PayloadBootInfo;
}

/**
  A loop to delegated events.

  @param  [in] EventCompleteSvcArgs   Pointer to the event completion arguments.

**/
VOID
EFIAPI
DelegatedEventLoop (IN UINTN CpuId, IN UINT64 MmNsCommBufBase)
{
  EFI_STATUS  Status;
  ASSERT (((EFI_MM_COMMUNICATE_HEADER *)MmNsCommBufBase)->MessageLength == 0);

#ifndef MM_WITH_COVE_ENABLE
  RPMI_RESULT RpmiResult;
  Status = SbiRpxySetShmem(EFI_PAGE_SIZE, MmNsCommBufBase & ~(EFI_PAGE_SIZE - 1));
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "DelegatedEventLoop: "
      "Failed to set shared memory\n"
      ));
    Status = EFI_ACCESS_DENIED;
    ASSERT (0);
  }
#endif

  while (TRUE) {
#ifndef MM_WITH_COVE_ENABLE
    Status = SbiRpxySendNormalMessage(SBI_RPMI_MM_TRANSPORT_ID, SBI_RPMI_MM_SRV_GROUP, SBI_RPMI_MM_SRV_COMPLETE);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "DelegatedEventLoop: "
        "Failed to commuicate\n"
        ));
      Status = EFI_ACCESS_DENIED;
      ASSERT (0);
    }
    CopyMem (&RpmiResult, (VOID *)MmNsCommBufBase, sizeof(RPMI_RESULT));
    if (RPMI_ERROR (RpmiResult)) {
      DEBUG ((
        DEBUG_ERROR,
        "RPMI Error 0x%x\n",
        RpmiResult
        ));
    }
    Status = CpuDriverEntryPoint (0, CpuId, MmNsCommBufBase + sizeof(RPMI_RESULT));
#else
    CpuSleep ();
    Status = CpuDriverEntryPoint (0, CpuId, MmNsCommBufBase + sizeof (EFI_MMRAM_DESCRIPTOR));
#endif
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed delegated Status 0x%x\n",
        Status
        ));
    }
  }
}

/**
  The entry point of Standalone MM Foundation.

  @param  [in]  CpuId             The Id assigned to this running CPU
  @param  [in]  BootInfoAddress   The address of boot info

**/
VOID
EFIAPI
CModuleEntryPoint (
  IN UINT64  CpuId,
  IN VOID    *BootInfoAddress
  )
{
  EFI_RISCV_MM_BOOT_INFO          *PayloadBootInfo;
  VOID                            *HobStart;

  PayloadBootInfo = GetAndPrintBootinformation (BootInfoAddress);
  if (PayloadBootInfo == NULL) {
    return;
  }

#ifdef MM_WITH_COVE_ENABLE
  if ((PayloadBootInfo->Header.Attr | EFI_PARAM_ATTR_COVE) != 0) {
    //
    // Register shared memory
    //
    SbiCoVGShareMemoryRegion (PayloadBootInfo->MmNsCommBufBase, PayloadBootInfo->MmNsCommBufSize);
  }
#endif

  //
  // Create Hoblist based upon boot information passed by privileged software
  //
  HobStart = CreateHobListFromBootInfo (&CpuDriverEntryPoint, PayloadBootInfo);

  //
  // Call the MM Core entry point
  //
  ProcessModuleEntryPointList (HobStart);

  DEBUG ((DEBUG_INFO, "Cpu Driver EP %p\n", (VOID *)CpuDriverEntryPoint));

  DelegatedEventLoop (CpuId, PayloadBootInfo->MmNsCommBufBase);
}
