/**@file

Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
Portions copyright (c) 2011 - 2018, ARM Ltd. All rights reserved.<BR>
Portions Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PeCoffLib.h>
#include <Library/PeCoffExtraActionLib.h>

typedef RETURN_STATUS (*REGION_PERMISSION_UPDATE_FUNC) (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  );

STATIC
RETURN_STATUS
UpdatePeCoffPermissions (
  IN  CONST PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext,
  IN  REGION_PERMISSION_UPDATE_FUNC       NoExecUpdater,
  IN  REGION_PERMISSION_UPDATE_FUNC       ReadOnlyUpdater
  )
{
  //
  // Need to RiscV Mmu lib to update the permission
  //
  return RETURN_UNSUPPORTED;
}

/**
  Performs additional actions after a PE/COFF image has been loaded and relocated.

  If ImageContext is NULL, then ASSERT().

  @param  ImageContext  Pointer to the image context structure that describes the
                        PE/COFF image that has already been loaded and relocated.

**/
VOID
EFIAPI
PeCoffLoaderRelocateImageExtraAction (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  UpdatePeCoffPermissions (
    ImageContext,
    NULL, // Not Implemented
    NULL  // Not Implemented
    );
}

/**
  Performs additional actions just before a PE/COFF image is unloaded.  Any resources
  that were allocated by PeCoffLoaderRelocateImageExtraAction() must be freed.

  If ImageContext is NULL, then ASSERT().

  @param  ImageContext  Pointer to the image context structure that describes the
                        PE/COFF image that is being unloaded.

**/
VOID
EFIAPI
PeCoffLoaderUnloadImageExtraAction (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  UpdatePeCoffPermissions (
    ImageContext,
    NULL, // Not Implemented
    NULL  // Not Implemented
    );
}
