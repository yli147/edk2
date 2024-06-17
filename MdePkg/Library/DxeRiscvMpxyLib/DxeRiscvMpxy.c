/** @file
  This module implements functions to be used by MPXY client

  Copyright (c) 2024, Ventana Micro Systems, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Base.h>
#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/SafeIntLib.h>
#include <Library/BaseRiscVSbiLib.h>

#define INVAL_PHYS_ADDR      0xFFFFFFFFFFFFFFFFUL

STATIC UINT64 gShmemPhysHi = INVAL_PHYS_ADDR;
STATIC UINT64 gShmemPhysLo = INVAL_PHYS_ADDR;
STATIC UINT64 gShmemSize = 0;
STATIC UINT64 gShmemSet = 0;

EFI_STATUS
EFIAPI
SbiMpxySetShmem(
  IN UINT64 ShmemPhysHi,
  IN UINT64 ShmemPhysLo,
  IN UINT64 ShmemSize
  )
{
  SBI_RET  Ret;

  Ret = SbiCall (
          SBI_EXT_MPXY,
          SBI_EXT_MPXY_SET_SHMEM,
          4,
          ShmemSize,
          ShmemPhysLo,
          ShmemPhysHi,
          0 /* Ignore shared memory state and force setup */
          );

  if (Ret.Error == SBI_SUCCESS) {
    gShmemPhysLo = ShmemPhysLo;
    gShmemPhysHi = ShmemPhysHi;
    gShmemSize = ShmemSize;
    gShmemSet = 1;
  }

  return TranslateError (Ret.Error);
}

EFI_STATUS
EFIAPI
SbiMpxyDisableShmem(
  VOID
  )
{
  SBI_RET  Ret;

  if (!gShmemSet)
    return EFI_SUCCESS;

  Ret = SbiCall (
          SBI_EXT_MPXY,
          SBI_EXT_MPXY_SET_SHMEM,
          4,
          0,
          INVAL_PHYS_ADDR,
          INVAL_PHYS_ADDR,
          0 /* Ignore shared memory state and force setup */
          );

  if (Ret.Error == SBI_SUCCESS) {
    gShmemSize = 0;
    gShmemPhysHi = INVAL_PHYS_ADDR;
    gShmemPhysLo = INVAL_PHYS_ADDR;
    gShmemSet = 0;
  }

  return TranslateError (Ret.Error);
}

BOOLEAN
SbiMpxyShmemInitialized(
  VOID
  )
{
  if (gShmemSet)
    return TRUE;

  return FALSE;
}

EFI_STATUS
EFIAPI
SbiMpxySendMessage(
  IN UINTN ChannelId,
  IN UINTN MessageId,
  IN VOID *Message,
  IN UINTN MessageDataLen,
  OUT VOID *Response,
  OUT UINTN *ResponseLen
  )
{
  SBI_RET  Ret;
  UINT64 Phys = gShmemPhysLo;

  if (!gShmemSet) {
    return EFI_DEVICE_ERROR;
  }

  if (MessageDataLen >= gShmemSize) {
    return EFI_INVALID_PARAMETER;
  }

  /* Copy message to Hart's shared memory */
  CopyMem (
    (VOID *)Phys,
    Message,
    MessageDataLen
    );

  Ret = SbiCall (
          SBI_EXT_MPXY,
          SBI_EXT_MPXY_SEND_MSG_WITH_RESP,
          3,
          ChannelId,
          MessageId,
          MessageDataLen
          );

  if (Ret.Error == SBI_SUCCESS && Response) {
    /* Copy the response to out buffer */
    CopyMem (
      Response,
      (const VOID *)Phys,
      Ret.Value
      );
  }

  return TranslateError (Ret.Error);
}
