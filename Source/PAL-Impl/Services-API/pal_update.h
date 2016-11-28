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


#ifndef SOURCE_PAL_IMPL_SERVICES_API_PAL_UPDATE_H_
#define SOURCE_PAL_IMPL_SERVICES_API_PAL_UPDATE_H_

#include "pal_macros.h"
#include "pal_types.h"



#ifdef __cplusplus
extern "C" {
#endif


typedef uint32_t palImageId_t;

typedef struct _palImageHeaderDeails_t
{
                size_t imageSize;
                palBuffer_t hash;
                uint64_t version;
}palImageHeaderDeails_t;


typedef enum _palImagePlatformData_t
{
    PAL_IMAGE_DATA_FIRST = 0,
    PAL_IMAGE_DATA_HASH = PAL_IMAGE_DATA_FIRST,
    PAL_IMAGE_DATA_LAST
}palImagePlatformData_t;

typedef enum _palImageEvents_t
{
        PAL_IMAGE_EVENT_FIRST = -1,
        PAL_IMAGE_EVENT_ERROR = PAL_IMAGE_EVENT_FIRST,
        PAL_IMAGE_EVENT_INIT ,
        PAL_IMAGE_EVENT_PREPARE,
        PAL_IMAGE_EVENT_WRITE,
        PAL_IMAGE_EVENT_FINALIZE,
        PAL_IMAGE_EVENT_READTOBUFFER,
        PAL_IMAGE_EVENT_ACTIVATE,
        PAL_IMAGE_EVENT_GETACTIVEHASH,
        PAL_IMAGE_EVENT_GETACTIVEVERSION,
        PAL_IMAGE_EVENT_WRITEDATATOMEMORY,
        PAL_IMAGE_EVENT_LAST
} palImageEvents_t;

typedef void (*palImageSignalEvent_t)( palImageEvents_t event);



/*! Sets the callback function that will be called before the end of each API (except imageGetDirectMemAccess).
 *
 * WARNING: Please do not change this function!
 * This function loads a callback function received from the upper layer (service).
 * The callback should be called at the end of each function (except pal_plat_imageGetDirectMemAccess).
 * The callback receives the event type that just occurred defined by the ENUM palImageEvents_t.
 *
 * If you do not call the callback at the end, the service behaviour will be undefined.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 *
 * @param[in] CBfunction A pointer to the callback function.
 * \return The status in the form of palStatus_t. It is PAL_SUCCESS(0) in case of success and a negative value indicating a specific error code in case of failure.
 */
palStatus_t pal_imageInitAPI(palImageSignalEvent_t CBfunction);


/*! Clears all the resources used by the pal_update APIs.
 * \return The status in the form of palStatus_t. It is PAL_SUCCESS(0) in case of success and a negative value indicating a specific error code in case of failure.
 */
palStatus_t pal_imageDeInit(void);



/*! Prepares to write image with ID (imageId) and size (imageSize) in a suitable memory region. The space available is verified and reserved.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 * @param[in] imageId The image ID.
 * @param[in] headerDetails The size of the image.
 * \return The status in the form of palStatus_t. It is PAL_SUCCESS(0) in case of success and a negative value indicating a specific error code in case of failure.
 */
palStatus_t pal_imagePrepare(palImageId_t imageId, palImageHeaderDeails_t* headerDetails);

/*! Writes the data in chunk buffer with size chunk bufferLength in the location of imageId adding the relative offset.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 * @param[in] imageId The image ID.
 * @param[in] offset The offset to write the data into.
 * @param[in] chunk A pointer to struct containing the data and the data length to write.
 * \return The status in the form of palStatus_t. It is PAL_SUCCESS(0) in case of success and a negative value indicating a specific error code in case of failure.
 */
palStatus_t pal_imageWrite (palImageId_t imageId, size_t offset, palConstBuffer_t *chunk);



/*! Flushes the image data and sets the version of imageId to imageVersion.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 * @param[in] imageId The image ID.
 * \return The status in the form of palStatus_t. It is PAL_SUCCESS(0) in case of success and a negative value indicating a specific error code in case of failure.
 */
palStatus_t  pal_imageFinalize(palImageId_t imageId);


/*! Verifies whether the image (imageId) is readable and sets imagePtr to point to the beginning of the image in the memory and imageSizeInBytes to the image size.
 * In case of failure, sets imagePtr to NULL and returns relevant palStatus_t error.
 * @param[in] imageId The image ID.
 * @param[out] imagePtr A pointer to the beginning of the image.
 * @param[out] imageSizeInBytes The size of the image.
 * \return The status in the form of palStatus_t. It is PAL_SUCCESS(0) in case of success and a negative value indicating a specific error code in case of failure.
 */
palStatus_t pal_imageGetDirectMemoryAccess(palImageId_t imageId, void** imagePtr, size_t *imageSizeInBytes);

/*! Reads the max of chunk (maxBufferLength) bytes from image (imageId) with relative offset offset and stores it in the chunk buffer.
 * Also sets the chunk bufferLength value to the actual number of bytes read.
 * Note:
 * Please use this API in the case image is not directly accessible via the imageGetDirectMemAccess function.
 *
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 *
 * @param[in] imageId The image ID.
 * @param[in] offset The offset to start reading from.
 * @param[out] chunk A struct containing the data and actual bytes read.
 */
palStatus_t pal_imageReadToBuffer(palImageId_t imageId, size_t offset, palBuffer_t* chunk);

/*! Sets an image (imageId) as the active image (after device reset).
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 *
 * @param[in] imageId The image ID.
 */
palStatus_t pal_imageActivate(palImageId_t imageId);

/*! Retrieves the hash value of the active image to the hash buffer with the max size hash (maxBufferLength) and sets the hash size to hash bufferLength.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 * @param[out] hash A struct containing the hash and actual size of hash read.
 */
palStatus_t pal_imageGetActiveHash(palBuffer_t* hash);

/*! Retrieves the version of the active image to the version buffer with the size set to version bufferLength.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 * @param[out] version A struct containing the version and actual size of version read.
 */
palStatus_t pal_imageGetActiveVersion(palBuffer_t* version);

/*! Writes the data of dataId stored in dataBuffer to a memory accessible to the bootloader. Currently, only hash is available.
 * The function must call g_palUpdateServiceCBfunc before completing an event.
 * @param[in] dataId One of the members of palImagePlatformData_t enum.
 * @param[in] dataBuffer A struct containing the data and actual bytes to be written.
 */
palStatus_t pal_imageWriteDataToMemory(palImagePlatformData_t dataId, const palConstBuffer_t* const dataBuffer);

#ifdef __cplusplus
}
#endif
#endif /* SOURCE_PAL_IMPL_SERVICES_API_PAL_UPDATE_H_ */
