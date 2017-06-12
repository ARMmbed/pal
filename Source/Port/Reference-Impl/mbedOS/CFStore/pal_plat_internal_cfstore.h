/*
* Copyright (c) 2016 ARM Limited. All rights reserved.
* SPDX-License-Identifier: Apache-2.0
* Licensed under the Apache License, Version 2.0 (the License); you may
* not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an AS IS BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef __PAL_PLAT_CFSTORE_INTERNAL_H
#define __PAL_PLAT_CFSTORE_INTERNAL_H

#if (defined(TARGET_K64F))

// From mbedOS
#include "configuration_store.h"

/* CFSTORE Specific error codes*/
#define PAL_CFSTORE_DRIVER_ERROR_UNINITIALISED     ARM_CFSTORE_DRIVER_ERROR_UNINITIALISED  /* Start of driver specific errors */
#define PAL_CFSTORE_DRIVER_ERROR_PREEXISTING_KEY   ARM_CFSTORE_DRIVER_ERROR_PREEXISTING_KEY
#define PAL_CFSTORE_DRIVER_ERROR_KEY_NOT_FOUND     ARM_CFSTORE_DRIVER_ERROR_KEY_NOT_FOUND
#define PAL_CFSTORE_DRIVER_ERROR_INVALID_HANDLE    ARM_CFSTORE_DRIVER_ERROR_INVALID_HANDLE


// Can be zero for file system implementations.
#define PAL_CFSTORE_FLASH_SECTOR_SIZE_IN_BYTES  (0)

#define PAL_CFSTORE_FMODE ARM_CFSTORE_FMODE


#define PAL_RETENTION_NVM  ARM_RETENTION_NVM
#define PAL_CFSTORE_KEYDESC ARM_CFSTORE_KEYDESC

/* General return codes */
#define PAL_CFSTORE_OK      ARM_DRIVER_OK
#define PAL_CFSTORE_ERROR   ARM_DRIVER_ERROR
#define PAL_CFSTORE_CAPABILITIES   ARM_CFSTORE_CAPABILITIES

#define PAL_CFSTORE_HANDLE ARM_CFSTORE_HANDLE

#define PAL_CFSTORE_HANDLE_INIT(name) ARM_CFSTORE_HANDLE_INIT(name)

#define PAL_CFSTORE ARM_CFSTORE_DRIVER
#define pal_cfstore cfstore_driver

#endif /* (defined(TARGET_K64F)) */
#endif /* __PAL_PLAT_CFSTORE_INTERNAL_H */
