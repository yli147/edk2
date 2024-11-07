/** @file
 * Parser functionality specific to Risc-V
  Copyright (c) 2024, Rivos Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Pi/PiHob.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include "../UefiPayloadEntry/UefiPayloadEntry.h"

/**
  Build RV CPU HOB using infomation from FDT.

  @param  FdtBase
**/
STATIC VOID
EFIAPI
BuildRVCpuHob (
  IN VOID  *FdtBase
  )
{
  INT32               Node;
  INT32               Property;
  CONST FDT_PROPERTY  *PropertyPtr;
  INT32               TempLen;
  UINT32              *Data32;
  INT32               Len;
  EFI_HOB_RISCV_CPU   *Hob;

  Node = FdtSubnodeOffsetNameLen (FdtBase, 0, "cpus", (INT32)AsciiStrLen ("cpus"));
  ASSERT (Node > 0);
  if (FdtGetProperty (FdtBase, Node, "boot-hart", &Len) != 0) {
    Property    = FdtFirstPropertyOffset (FdtBase, Node);
    PropertyPtr = FdtGetPropertyByOffset (FdtBase, Property, &TempLen);
    Data32      = (UINT32 *)(PropertyPtr->Data);

    Hob = CreateHob (EFI_HOB_TYPE_RISCV_CPU, sizeof (EFI_HOB_RISCV_CPU));
    ASSERT (Hob != NULL);
    Hob->CpuId = (UINT8)Fdt32ToCpu (*Data32);

    //
    // Zero the reserved space to match HOB spec
    //
    ZeroMem (Hob->Reserved, sizeof (Hob->Reserved));
  }
}

/**
  Build FDT HOB using infomation from FDT.

  @param  FdtBase
**/
STATIC VOID
EFIAPI
BuildFdtHob (
  IN VOID  *FdtBase
  )
{
  VOID    *NewBase;
  UINTN   FdtSize;
  UINTN   FdtPages;
  UINT64  *FdtHobData;

  ASSERT (FdtCheckHeader (FdtBase) == 0);
  FdtSize  = FdtTotalSize (FdtBase);
  FdtPages = EFI_SIZE_TO_PAGES (FdtSize);
  NewBase  = AllocatePages (FdtPages);
  ASSERT (NewBase != NULL);
  FdtOpenInto (FdtBase, NewBase, EFI_PAGES_TO_SIZE (FdtPages));

  FdtHobData = BuildGuidHob (&gFdtHobGuid, sizeof *FdtHobData);
  ASSERT (FdtHobData != NULL);
  *FdtHobData = (UINTN)NewBase;
}

/**
  It will Parse FDT -custom node based on information from bootloaders.
  @param[in]  FdtBase The starting memory address of FdtBase
  @param[in]  HobList The starting memory address of New Hob list.

**/
UINTN
EFIAPI
CustomFdtNodeParser (
  IN VOID  *FdtBase,
  IN VOID  *HobList
  )
{
  BuildRVCpuHob (FdtBase);
  BuildFdtHob (FdtBase);
  return EFI_SUCCESS;
}
