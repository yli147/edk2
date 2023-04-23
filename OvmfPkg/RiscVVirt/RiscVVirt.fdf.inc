## @file
#  Definitions of Flash definition file on RiscVVirt RISC-V platform
#
#  Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
#  Copyright (c) 2022, Ventana Micro Systems Inc. All rights reserved.<BR>
#  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##
[Defines]
DEFINE BLOCK_SIZE                  = 0x1000

DEFINE PFLASH1_BASE                = 0x22000000

DEFINE FW_BASE_ADDRESS             = $(PFLASH1_BASE)
DEFINE FW_SIZE                     = 0x00800000
DEFINE FW_BLOCKS                   = 0x800

DEFINE CODE_BASE_ADDRESS           = $(FW_BASE_ADDRESS)
DEFINE CODE_SIZE                   = 0x00740000
DEFINE CODE_BLOCKS                 = 0x740

DEFINE VARS_SIZE                    = 0x000C0000
DEFINE VARS_BLOCK_SIZE              = 0x40000
DEFINE VARS_BLOCKS                  = 0x3

#
# EFI Variable memory region.
# The total size of EFI Variable FD must include
# all of sub regions of EFI Variable
#
DEFINE VARS_OFFSET                   = $(CODE_SIZE)
DEFINE VARS_LIVE_SIZE                = 0x00040000
DEFINE VARS_FTW_WORKING_OFFSET       = $(VARS_OFFSET) + $(VARS_LIVE_SIZE)
DEFINE VARS_FTW_WORKING_SIZE         = 0x00040000
DEFINE VARS_FTW_SPARE_OFFSET         = $(VARS_FTW_WORKING_OFFSET) + $(VARS_FTW_WORKING_SIZE)
DEFINE VARS_FTW_SPARE_SIZE           = 0x00040000

!if $(SECURE_BOOT_ENABLE)
DEFINE PFLASH0_BASE                 = 0x20000000
DEFINE SECURE_FW_BASE_ADDRESS       = $(PFLASH0_BASE)
DEFINE SECURE_FW_SIZE               = 0x01000000
DEFINE SECURE_FW_BLOCKS             = 0x1000

DEFINE SECURE_CODE_BASE_ADDRESS     = $(SECURE_FW_BASE_ADDRESS)
DEFINE SECURE_CODE_SIZE             = 0x0 # 0x00F40000
DEFINE SECURE_CODE_BLOCKS           = 0x0 # 0xF40

DEFINE SECURE_VARS_SIZE             = 0x000C0000
DEFINE SECURE_VARS_BLOCK_SIZE       = 0x40000
DEFINE SECURE_VARS_BLOCKS           = 0x3

DEFINE SECURE_VARS_OFFSET           = $(SECURE_CODE_SIZE)
DEFINE SECURE_VARS_LIVE_SIZE        = 0x00040000
DEFINE SECURE_VARS_FTW_WORKING_OFFSET = $(SECURE_VARS_OFFSET) + $(SECURE_VARS_LIVE_SIZE)
DEFINE SECURE_VARS_FTW_WORKING_SIZE = 0x00040000
DEFINE SECURE_VARS_FTW_SPARE_OFFSET = $(SECURE_VARS_FTW_WORKING_OFFSET) + $(SECURE_VARS_FTW_WORKING_SIZE)
DEFINE SECURE_VARS_FTW_SPARE_SIZE   = 0x00040000
!endif

SET gUefiCpuPkgTokenSpaceGuid.PcdCpuCoreCrystalClockFrequency = 10000000
SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfSecPeiTempRamBase = 0x83FF0000
SET gUefiOvmfPkgTokenSpaceGuid.PcdOvmfSecPeiTempRamSize = 0x00010000
