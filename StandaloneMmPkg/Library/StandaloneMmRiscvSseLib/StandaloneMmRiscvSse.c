/** @file
  SSE support functions

Copyright (c) 2024, Rivos Inc. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <StandaloneMmCpu.h>

#include <PiPei.h>
#include <Guid/MmramMemoryReserve.h>
#include <Guid/MpInformation.h>

#include <Library/RiscV64/StandaloneMmRiscvSse.h>

#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SerialPortLib.h>
#include <Library/PcdLib.h>

#include <Library/BaseRiscVSbiLib.h>

EFI_STATUS
EFIAPI
SbiSseRegisterEvent (IN UINT32 EventId, IN VOID *EventArgs, IN SSE_EVENT_CALLBACK EventCallback)
{
  EFI_STATUS        Status;
  SBI_RET           Ret;
  SSE_EVENT_CONTEXT *Context;

  Status = SbiProbeExtension (SBI_EXT_SSE);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  Context = AllocatePool (sizeof(*Context));
  ASSERT (Context != NULL);

  Context->Args = EventArgs;
  Context->Callback = EventCallback;
  Context->EventId = EventId;
  Ret = SbiCall (SBI_EXT_SSE, SBI_SSE_EVENT_REGISTER, 3, EventId, _SseEntryPoint, Context);

  return TranslateError (Ret.Error);
}

EFI_STATUS
EFIAPI
SbiSseEnableEvent (IN UINT32 EventId)
{
  SBI_RET Ret;

  Ret = SbiCall (SBI_EXT_SSE, SBI_SSE_EVENT_ENABLE, 1, EventId);

  return TranslateError (Ret.Error);
}

VOID
EFIAPI
SbiSseEntryPoint (IN SSE_EVENT_CONTEXT *Context)
{
  Context->Callback (Context->EventId, Context->Args);
}
