/** @file
  Entry point to the Standalone MM Foundation when initialized during the SEC
  phase on RISCV platforms

Copyright (c) 2017 - 2021, Arm Ltd. All rights reserved.<BR>
Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <StandaloneMmCpu.h>
#include <Library/RiscV64/StandaloneMmCoreEntryPoint.h>

#include <PiPei.h>
#include <Guid/MmramMemoryReserve.h>
#include <Guid/MpInformation.h>

#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SerialPortLib.h>
#include <Library/PcdLib.h>

#include <Library/BaseRiscVSbiLib.h>
#include <Library/DxeRiscvMpxy.h>

PI_MM_CPU_DRIVER_ENTRYPOINT  CpuDriverEntryPoint = NULL;

/**
  Retrieve a pointer to and print the boot information passed by privileged
  secure firmware.

  @param  [in] SharedBufAddress   The pointer memory shared with privileged
                                  firmware.

**/
EFI_RISCV_SMM_PAYLOAD_INFO *
GetAndPrintBootinformation (
  IN VOID  *PayloadInfoAddress
  )
{
  EFI_RISCV_SMM_PAYLOAD_INFO *PayloadBootInfo;
  EFI_RISCV_SMM_CPU_INFO     *PayloadCpuInfo;
  UINTN                         Index;

  PayloadBootInfo = (EFI_RISCV_SMM_PAYLOAD_INFO *)PayloadInfoAddress;

  if (PayloadBootInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "PayloadBootInfo NULL\n"));
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

  PayloadCpuInfo = (EFI_RISCV_SMM_CPU_INFO *)&(PayloadBootInfo->CpuInfo);

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
DelegatedEventLoop (IN UINTN CpuId, IN UINTN ChannelId, IN RISCV_SMM_MSG_COMM_ARGS  *EventCompleteSvcArgs)
{
  EFI_STATUS  Status;
  UINTN       SmmStatus;
  UINTN       SmmMsgLen, SmmRespLen;
  SmmMsgLen = sizeof(RISCV_SMM_MSG_COMM_ARGS);

  while (TRUE) {
    Status = SbiMpxySendMessage (
                   ChannelId,
				   RISCV_MSG_ID_SMM_EVENT_COMPLETE,
				   EventCompleteSvcArgs,
				   SmmMsgLen,
				   EventCompleteSvcArgs,
				   &SmmRespLen);
    if (EFI_ERROR (Status) || (sizeof(RISCV_SMM_MSG_COMM_ARGS) != SmmRespLen)) {
      DEBUG ((
        DEBUG_ERROR,
        "DelegatedEventLoop: "
        "Failed to commuicate\n"
        ));
      Status = EFI_ACCESS_DENIED;
      ASSERT (0);
    }

    Status = CpuDriverEntryPoint (
                   0,
                   CpuId,
                   EventCompleteSvcArgs->Arg0
                   );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Failed delegated Status 0x%x\n",
        Status
        ));
    }
    switch (Status) {
      case EFI_SUCCESS:
        SmmStatus = RISCV_SMM_RET_SUCCESS;
        break;
      case EFI_INVALID_PARAMETER:
        SmmStatus = RISCV_SMM_RET_INVALID_PARAMS;
        break;
      case EFI_ACCESS_DENIED:
        SmmStatus = RISCV_SMM_RET_DENIED;
        break;
      case EFI_OUT_OF_RESOURCES:
        SmmStatus = RISCV_SMM_RET_NO_MEMORY;
        break;
      case EFI_UNSUPPORTED:
        SmmStatus = RISCV_SMM_RET_NOT_SUPPORTED;
        break;
      default:
        SmmStatus = RISCV_SMM_RET_NOT_SUPPORTED;
        break;
    }
    EventCompleteSvcArgs->Arg0 = SmmStatus;
  }
}

/**
  Initialize parameters to be sent via SMM call.

  @param[out]     InitMmFoundationSmmArgs  Args structure

**/
STATIC
VOID
InitRiscVSmmArgs (
  OUT RISCV_SMM_MSG_COMM_ARGS  *InitMmFoundationSmmArgs
  )
{
  EFI_STATUS  Status;
  VOID *SbiShmem;
  UINT64 ShmemP;
  UINT32 PhysHiAddress;
  UINT32 PhysLoAddress;

  //
	// Allocate memory to be shared with OpenSBI for MPXY
	//
	SbiShmem = AllocateAlignedPages (EFI_SIZE_TO_PAGES(RISCV_SMM_MSG_SHMEM_SIZE),
				RISCV_SMM_MSG_SHMEM_SIZE // Align
				);
	if (SbiShmem == NULL) {
		 ASSERT (0);
	}
  ZeroMem (SbiShmem, RISCV_SMM_MSG_SHMEM_SIZE);
  ShmemP = (UINT64)(SbiShmem);
  PhysHiAddress = (UINT32)(ShmemP >> 32);
  PhysLoAddress = (UINT32)(ShmemP & 0xFFFFFFFF);
  Status = SbiMpxySetShmem(PhysHiAddress, PhysLoAddress, RISCV_SMM_MSG_SHMEM_SIZE);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "DelegatedEventLoop: "
      "Failed to set shared memory\n"
      ));
    Status = EFI_ACCESS_DENIED;
    ASSERT (0);
  }

  InitMmFoundationSmmArgs->Arg0 = RISCV_SMM_RET_SUCCESS;
  InitMmFoundationSmmArgs->Arg1 = 0;
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
  IN VOID    *PayloadInfoAddress
  )
{
  EFI_RISCV_SMM_PAYLOAD_INFO    *PayloadBootInfo;
  RISCV_SMM_MSG_COMM_ARGS          InitMmFoundationSmmArgs;
  VOID                            *HobStart;

  PayloadBootInfo = GetAndPrintBootinformation (PayloadInfoAddress);
  if (PayloadBootInfo == NULL) {
    return;
  }

  //
  // Create Hoblist based upon boot information passed by privileged software
  //
  HobStart = CreateHobListFromBootInfo (&CpuDriverEntryPoint, PayloadBootInfo);

  //
  // Call the MM Core entry point
  //
  ProcessModuleEntryPointList (HobStart);

  DEBUG ((DEBUG_INFO, "Cpu Driver EP %p\n", (VOID *)CpuDriverEntryPoint));

  ZeroMem (&InitMmFoundationSmmArgs, sizeof (InitMmFoundationSmmArgs));
  InitRiscVSmmArgs (&InitMmFoundationSmmArgs);
  DelegatedEventLoop (CpuId, PayloadBootInfo->MpxyChannelId, &InitMmFoundationSmmArgs);
}
