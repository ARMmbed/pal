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

/** @file  configure-store.h
 *
 * mbed Microcontroller Library
 * Copyright (c) 2006-2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __PAL_CFSTORE_H
#define __PAL_CFSTORE_H
#include "pal.h"
#include "pal_plat_cfstore.h"


/*
 * This is the interface file to configuration store service.
 * The actual interface is a subset of mbedOS configuration store.
 * Look at the pal_plat_internal_cfstore.h file for the exact platform specific interface.
 */

#ifdef __cplusplus
extern "C" {
#endif


/*! Application specific item system structure
* All sizes are in 4 byte words. The value does not include the item header which is two words(8 bytes).
* Different flash drivers have different alignment requirements:
* Platform         Alignment		Write buffer be rounded to Alignment value Supports unrestricted file name system
* K64/flash			8				Yes											No
* K64/fatfs_sd		1				No											Yes
* Realtek-AMEBA 	4				No											No
* Linux				1				No											Yes
* mbedOS			1				No											Yes
* So for the code to be as generic as possible we assume 8
* BUFFERS SUPPLIED TO READ AND WRITE FUNCTIONS MAY NEED TO HAVE A SIZE ROUNDED UP TO THIS VALUE (8)!!!!!
* No item should overlap a sector (4096 bytes)
*/
struct pal_cstore_item_context_t{
		const char    *itemName;
		const int16_t maxLengthInWords;
		// The flash sector where the item is located 0,1...
		const uint8_t sector;
};

struct pal_cstore_context_t{
	const struct pal_cstore_item_context_t *itemContext;
	// Number of items
	const uint8_t nItems;
	// Total number of flash sectors
	const uint8_t nSectors;
	// List of sector numbers that will be erased on factory reset
	const uint8_t *sectorsToEraseOnFactoryReset;
	// Number of elements in the above array.
	const uint8_t nSectorsToEraseOnFactoryReset;
};

/*! Erase any storage that is required to be erased when the device is factory reset.
* @param[in] CStoreContext - A fixed file system context or 0 for an unrestricted file name system.
* \return palStatus_t which will be PAL_SUCCESS(0) in case of success
*/
int pal_cfstore_factory_reset(struct pal_cstore_context_t *CStoreContext);


#ifdef __cplusplus
}
#endif

#endif /* __PAL_CFSTORE_H */
