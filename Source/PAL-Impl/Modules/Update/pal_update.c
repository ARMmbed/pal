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

#if (defined(TARGET_K64F))

#include "pal_plat_update.h"
#include "pal_update.h"
#include "pal_macros.h"



static uint8_t palUpdateInitFlag = 0;

palStatus_t pal_imageInitAPI(palImageSignalEvent_t CBfunction)
{
    PAL_MODULE_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageInitAPI(CBfunction);
    return status;
}

palStatus_t pal_imageDeInit(void)
{
    PAL_MODULE_DEINIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageDeInit();
    return status;
}



palStatus_t pal_imagePrepare(palImageId_t imageId, palImageHeaderDeails_t *headerDetails)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    pal_plat_imageSetHeader(imageId,headerDetails);
    status = pal_plat_imageReserveSpace(imageId, headerDetails->imageSize);

    return status;
}

palStatus_t pal_imageWrite (palImageId_t imageId, size_t offset, palConstBuffer_t *chunk)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageWrite(imageId, offset, chunk);
    return status;
}

palStatus_t  pal_imageFinalize(palImageId_t imageId)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageFlush(imageId);
    return status;
}

palStatus_t pal_imageGetDirectMemoryAccess(palImageId_t imageId, void** imagePtr, size_t* imageSizeInBytes)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageGetDirectMemAccess(imageId, imagePtr, imageSizeInBytes);
    return status;
}

palStatus_t pal_imageReadToBuffer(palImageId_t imageId, size_t offset, palBuffer_t *chunk)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;

    status = pal_plat_imageReadToBuffer(imageId,offset,chunk);
    return status;
}

palStatus_t pal_imageActivate(palImageId_t imageId)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageActivate(imageId);
    return status;
}

palStatus_t pal_imageGetActiveHash(palBuffer_t *hash)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageGetActiveHash(hash);
    return status;
}

palStatus_t pal_imageGetActiveVersion(palBuffer_t *version)
{
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    palStatus_t status = PAL_SUCCESS;
    status = pal_plat_imageGetActiveVersion(version);
    return status;
}

palStatus_t pal_imageWriteDataToMemory(palImagePlatformData_t dataId, const palConstBuffer_t * const dataBuffer)
{
    palStatus_t status = PAL_SUCCESS;
    PAL_MODULE_IS_INIT(palUpdateInitFlag);
    // this switch is for further use when there will be more options
    switch(dataId)
    {
    case PAL_IMAGE_DATA_HASH:
        status = pal_plat_imageWriteHashToMemory(dataBuffer);
        break;
    default:
        status = PAL_ERR_GENERIC_FAILURE;
    }
    return status;
}
#endif /* defined (defined(TARGET_K64F)) */

