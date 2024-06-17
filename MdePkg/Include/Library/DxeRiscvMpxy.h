/** @file
  This module implements functions to be used by MPXY client

  Copyright (c) 2024, Ventana Micro Systems, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef SBI_MPXY_H_
#define SBI_MPXY_H_

/**
  Set a shared memory between EDK2 and OpenSBI

  @param[in]  ShmemPhysHi   Upper XLEN bits of physical memory
  @param[in]  ShmemPhysLo   Lower XLEN bits of physical memory
  @param[in]  ShmemSize     Size of the shared memory

  @retval EFI_SUCCESS    The shared memory was registered with OpenSBI
  @retval Other          Some error occured during the operation
**/
EFI_STATUS
EFIAPI
SbiMpxySetShmem(
  IN UINT64 ShmemPhysHi,
  IN UINT64 ShemPhysLo,
  IN UINT64 ShemSize
  );

/**
  Disable the shared memory between OpenSBI and EDK2

  @param[in]  None

  @retval EFI_SUCCESS    The shared memory was disabled
  @retval Other          Some error occured during the operation
**/
EFI_STATUS
EFIAPI
SbiMpxyDisableShmem(
  VOID
  );

/**
  Check if Mpxy shared memory is initialized and setup with SBI.

  @retval TRUE  If the shared memory is registered with SBI, FALSE otherwise
**/
BOOLEAN
SbiMpxyShmemInitialized(
  VOID
  );

/**
  Send a message with response over Mpxy.

  @param[in] ChannelId       The Channel on which message would be sent
  @param[in] MessageId       Message protocol specific message identification
  @param[in] MessageDataLen  Length of the message to be sent
  @param[in] Message         Pointer to buffer containing message
  @param[in] Response        Pointer to buffer to which response should be written
  @param[in] ResponseLen     Pointer where the size of response should be written

  @retval EFI_SUCCESS    The shared memory was disabled
  @retval Other          Some error occured during the operation
**/
EFI_STATUS
EFIAPI
SbiMpxySendMessage(
  IN UINTN ChannelId,
  IN UINTN MessageId,
  IN VOID *Message,
  IN UINTN MessageDataLen,
  OUT VOID *Response,
  OUT UINTN *ResponseLen
  );

#endif

