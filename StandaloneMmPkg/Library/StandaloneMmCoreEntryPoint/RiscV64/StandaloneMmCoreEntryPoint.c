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
#include <Library/BaseRiscVTeeLib.h>
#include <Library/CpuLib.h>

#define BOOT_PAYLOAD_VERSION  1
#define EFI_PARAM_ATTR_APTEE  1

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
  EFI_RISCV_MM_CONTEXT MmContext;
  EFI_RISCV_MM_CONTEXT MmCompleteEvt;
  ASSERT (((EFI_MM_COMMUNICATE_HEADER *)MmNsCommBufBase)->MessageLength == 0);

  ZeroMem (&MmContext, sizeof (EFI_RISCV_MM_CONTEXT));
  // SMC MM Func ID
  MmContext.FuncId = SBI_SMC_MM_COMPLETE_EVT;
  MmContext.PayloadAddress = (UINT64)&MmCompleteEvt;

  while (TRUE) {
#ifdef MM_WITH_COVE_ENABLE
    CpuSleep ();
    Status = CpuDriverEntryPoint (0, CpuId, MmNsCommBufBase);
#else
    SbiCallSmcMm (&MmContext);
    //
    // Passes SMC FID of the MM_COMMUNICATE interface as the Event ID upon
    // receipt of a synchronous MM request. Use the Event ID to distinguish
    // between synchronous and asynchronous events.
    //
    if (SBI_SMC_MM_COMMUNICATE != (UINT32)MmCompleteEvt.FuncId)
    {
      DEBUG ((DEBUG_ERROR, "UnRecognized Event - 0x%x\n", (UINT32)MmCompleteEvt.FuncId));
      continue;
    }

    Status = CpuDriverEntryPoint (MmCompleteEvt.FuncId, CpuId, MmNsCommBufBase);
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
  if ((PayloadBootInfo->Header.Attr | EFI_PARAM_ATTR_APTEE) != 0) {
    //
    // Register shared memory
    //
    SbiTeeGuestShareMemoryRegion (PayloadBootInfo->MmNsCommBufBase, PayloadBootInfo->MmNsCommBufSize);
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

  DelegatedEventLoop (CpuId, PayloadBootInfo->MmNsCommBufBase + sizeof (EFI_MMRAM_DESCRIPTOR));
}
