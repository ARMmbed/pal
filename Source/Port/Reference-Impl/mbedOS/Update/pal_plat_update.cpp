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

#include <mbed.h>
#include "pal_plat_update.h"
#include "device_storage.h"

#define SIZEOF_SHA256               256/8
#define FIRMWARE_HEADER_MAGIC       0x5a51b3d4UL
#define FIRMWARE_HEADER_VERSION     1
#define FIRMWARE_DIR                "firmware"

#define IMAGE_COUNT_MAX             1
#define ACTIVE_IMAGE_INDEX          0xFFFFFFFF

#define SEEK_POS_INVALID            0xFFFFFFFF

typedef struct FirmwareHeader {
    uint32_t magic;                         /** Metadata-header specific magic code */
    uint32_t version;                       /** Revision number for this generic metadata header. */
    uint32_t checksum;                      /** A checksum of this header. This field should be considered to be zeroed out for
                                             *  the sake of computing the checksum. */
    uint32_t totalSize;                     /** Total space (in bytes) occupied by the firmware BLOB, including headers and any padding. */
    uint64_t firmwareVersion;               /** Version number for the accompanying firmware. Larger numbers imply more preferred (recent)
                                             *  versions. This defines the selection order when multiple versions are available. */
    uint8_t  firmwareSHA256[SIZEOF_SHA256]; /** A SHA-2 using a block-size of 256-bits of the firmware, including any firmware-padding. */
} FirmwareHeader_t;

static FirmwareHeader_t pal_pi_mbed_firmware_header;
static palImageSignalEvent_t g_palUpdateServiceCBfunc;
static FILE *image_file[IMAGE_COUNT_MAX];
static bool last_read_nwrite[IMAGE_COUNT_MAX];
static uint32_t last_seek_pos[IMAGE_COUNT_MAX];

static bool valid_index(uint32_t index);
static int safe_read(uint32_t index, size_t offset, uint8_t *buffer, uint32_t size);
static int safe_write(uint32_t index, size_t offset, const uint8_t *buffer, uint32_t size);
static bool open_if_necessary(uint32_t index, bool read_nwrite);
static bool seek_if_necessary(uint32_t index, size_t offset, bool read_nwrite);
static bool close_if_necessary(uint32_t index);
static const char *image_path_alloc_from_index(uint32_t index);
static const char *header_path_alloc_from_index(uint32_t index);
static const char *path_join_and_alloc(const char * const * path_list);

palStatus_t pal_get_fw_header(palImageId_t index, FirmwareHeader_t *headerP);
palStatus_t pal_set_fw_header(palImageId_t index, FirmwareHeader_t *headerP);

/*
 * WARNING: please do not change this function!
 * this function loads a call back function received from the upper layer (service).
 * the call back should be called at the end of each function (except pal_plat_imageGetDirectMemAccess)
 * the call back receives the event type that just happened defined by the ENUM  palImageEvents_t.
 *
 * if you will not call the call back at the end the service behaver will be undefined
 */
palStatus_t pal_plat_imageInitAPI(palImageSignalEvent_t CBfunction)
{
    DEBUG_PRINT("pal_plat_imageInitAPI\r\n");
    mbed_fs_inc_ref_count();

    const char * const path_list[] = {
        FIRMWARE_DIR,
        NULL
    };
    const char *path = path_join_and_alloc(path_list);
    mbed_fs_mkdir(path);
    free((void*)path);

	g_palUpdateServiceCBfunc = CBfunction;
	g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_INIT);
	return PAL_SUCCESS;
}

palStatus_t pal_plat_imageDeInit(void)
{
    DEBUG_PRINT("pal_plat_imageDeInit\r\n");
    for (int i = 0; i < IMAGE_COUNT_MAX; i++) {
        close_if_necessary(i);
    }
    mbed_fs_dec_ref_count();
	return PAL_SUCCESS;
}

palStatus_t pal_plat_imageGetMaxNumberOfImages(uint8_t *imageNumber)
{
	return PAL_ERR_NOT_IMPLEMENTED;
}

palStatus_t pal_plat_imageSetVersion(palImageId_t imageId, const palConstBuffer_t* version)
{
	return PAL_ERR_NOT_IMPLEMENTED;
}

palStatus_t pal_plat_imageGetDirectMemAccess(palImageId_t imageId, void** imagePtr, size_t *imageSizeInBytes)
{
	return PAL_ERR_NOT_IMPLEMENTED;
}

palStatus_t pal_plat_imageActivate(palImageId_t imageId)
{
	return PAL_ERR_NOT_IMPLEMENTED;
}

palStatus_t pal_plat_imageGetActiveHash(palBuffer_t *hash)
{
    palStatus_t ret = PAL_ERR_UPDATE_ERROR;
    palImageEvents_t event = PAL_IMAGE_EVENT_ERROR;
    DEBUG_PRINT("pal_plat_imageGetActiveHash\r\n");

    hash->bufferLength = 0;
    memset(hash->buffer, 0, hash->maxBufferLength);
    if (hash->maxBufferLength < SIZEOF_SHA256) {
        ret = PAL_ERR_BUFFER_TOO_SMALL;
        goto exit;
    }

    if (PAL_SUCCESS == pal_get_fw_header(ACTIVE_IMAGE_INDEX, &pal_pi_mbed_firmware_header)) {
        memcpy(hash->buffer, pal_pi_mbed_firmware_header.firmwareSHA256, SIZEOF_SHA256);
        hash->bufferLength = SIZEOF_SHA256;
    } else {
        // A header does not exist so return a sha filled with zeroes
        hash->bufferLength = SIZEOF_SHA256;
    }

    event = PAL_IMAGE_EVENT_GETACTIVEHASH;
    ret = PAL_SUCCESS;

exit:
    g_palUpdateServiceCBfunc(event);
    return ret;
}

//Retrieve the version of the active image to version buffer with size set to version bufferLength.
palStatus_t pal_plat_imageGetActiveVersion (palBuffer_t* version)
{
	return PAL_ERR_NOT_IMPLEMENTED;
}

//Writing the value of active image hash stored in hashValue to memory
palStatus_t pal_plat_imageWriteHashToMemory(const palConstBuffer_t* const hashValue)
{
	return PAL_ERR_NOT_IMPLEMENTED;
}

//fill the image header data for writing, the writing will occur in the 1st image write
palStatus_t pal_plat_imageSetHeader(palImageId_t imageId, palImageHeaderDeails_t *details)
{
    palStatus_t ret = PAL_ERR_UPDATE_ERROR;

    DEBUG_PRINT("pal_plat_imageSetHeader\r\n");
	memset(&pal_pi_mbed_firmware_header,0,sizeof(pal_pi_mbed_firmware_header));
	pal_pi_mbed_firmware_header.totalSize = details->imageSize;
	pal_pi_mbed_firmware_header.magic = FIRMWARE_HEADER_MAGIC;
	pal_pi_mbed_firmware_header.version = FIRMWARE_HEADER_VERSION;
	pal_pi_mbed_firmware_header.firmwareVersion = details->version;
	memcpy(pal_pi_mbed_firmware_header.firmwareSHA256,details->hash.buffer,SIZEOF_SHA256);
	pal_pi_mbed_firmware_header.checksum = 0;//TODO

	if (PAL_SUCCESS == pal_set_fw_header(imageId, &pal_pi_mbed_firmware_header)) {
	    ret = PAL_SUCCESS;
	}

	return ret;
}

palStatus_t pal_plat_imageReserveSpace(palImageId_t imageId, size_t imageSize)
{
	DEBUG_PRINT("pal_plat_imageReserveSpace(imageId=%lu, size=%lu)\r\n", imageId, imageSize);

	g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_PREPARE);
	return PAL_SUCCESS;
}

palStatus_t pal_plat_imageWrite(palImageId_t imageId, size_t offset, palConstBuffer_t *chunk)
{
    DEBUG_PRINT("pal_plat_imageWrite(imageId=%lu, offset=%lu)\r\n", imageId, offset);

    int xfer_size_or_error = safe_write(imageId, offset, chunk->buffer, chunk->bufferLength);
    if ((xfer_size_or_error < 0) || ((uint32_t)xfer_size_or_error != chunk->bufferLength)) {
        DEBUG_PRINT("  Error writing to file\r\n");
        g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
        return PAL_ERR_UPDATE_ERROR;
    }

    g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_WRITE);
    return PAL_SUCCESS;
}

palStatus_t pal_plat_imageReadToBuffer(palImageId_t imageId, size_t offset, palBuffer_t *chunk)
{
    static int count = 0;
    count++;
    if (count % 100 == 0) {
        DEBUG_PRINT("pal_plat_imageReadToBuffer(imageId=%lu, offset=%lu)\r\n", imageId, offset);
    }

    int xfer_size_or_error = safe_read(imageId, offset, chunk->buffer, chunk->maxBufferLength);
    if (xfer_size_or_error < 0) {
        DEBUG_PRINT("  Error reading from file\r\n");
        g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
        return PAL_ERR_UPDATE_ERROR;
    }

    chunk->bufferLength = xfer_size_or_error;
    g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_READTOBUFFER);
    return PAL_SUCCESS;
}

palStatus_t pal_plat_imageFlush(palImageId_t package_id)
{
    palImageEvents_t event = PAL_IMAGE_EVENT_ERROR;
    palStatus_t ret = PAL_ERR_UPDATE_ERROR;

    DEBUG_PRINT("pal_plat_imageFlush(id=%i)\r\n", package_id);
    if (close_if_necessary(package_id)) {
        event = PAL_IMAGE_EVENT_FINALIZE;
        ret = PAL_SUCCESS;
    }

    g_palUpdateServiceCBfunc(event);
    return ret;
}

palStatus_t pal_get_fw_header(palImageId_t index, FirmwareHeader_t *headerP)
{
    palStatus_t ret = PAL_ERR_UPDATE_ERROR;
    FILE *file = NULL;
    size_t xfer_size;
    const char *file_path = NULL;

    DEBUG_PRINT("pal_get_fw_header\r\n");

    memset(headerP, 0, sizeof(FirmwareHeader_t));

    file_path = header_path_alloc_from_index(index);
    file = fopen(file_path, "rb");
    if (NULL == file) {
        DEBUG_PRINT("  File does not exist %s\r\n", file);
        goto exit;
    }

    xfer_size = fread(headerP, 1, sizeof(FirmwareHeader_t), file);
    if (xfer_size != sizeof(FirmwareHeader_t)) {
        DEBUG_PRINT("  Size read %lu expected %lu\r\n", xfer_size, sizeof(FirmwareHeader_t));
        goto exit;
    }

    ret = PAL_SUCCESS;

exit:
    if (file != NULL) {
        fclose(file);
    }
    free((void*)file_path);

    return ret;
}

palStatus_t pal_set_fw_header(palImageId_t index, FirmwareHeader_t *headerP)
{
    palStatus_t ret = PAL_ERR_UPDATE_ERROR;
    FILE *file = NULL;
    size_t xfer_size;

    const char *file_path = header_path_alloc_from_index(index);
    file = fopen(file_path, "wb");
    if (NULL == file) {
        DEBUG_PRINT("  Error opening file %s\r\n", file_path);
        goto exit;
    }

    xfer_size = fwrite(headerP, 1, sizeof(FirmwareHeader_t), file);
    if (xfer_size != sizeof(FirmwareHeader_t)) {
        DEBUG_PRINT("  Size written %lu expected %lu\r\n", xfer_size, sizeof(FirmwareHeader_t));
        goto exit;
    }

    ret = PAL_SUCCESS;

exit:
    if (NULL != file) {
        if (fclose(file) != 0) {
            DEBUG_PRINT("  Error closing file %s\r\n", file_path);
            ret = PAL_SUCCESS == ret ? (palStatus_t)PAL_ERR_UPDATE_ERROR : ret;
        }
    }
    free((void*)file_path);

    return ret;
}

static bool valid_index(uint32_t index)
{
    return index < IMAGE_COUNT_MAX;
}

static int safe_read(uint32_t index, size_t offset, uint8_t *buffer, uint32_t size)
{
    const bool read_nwrite = true;
    if (!valid_index(index)) {
        return -1;
    }
    if (!open_if_necessary(index, read_nwrite)) {
        return -1;
    }
    if (!seek_if_necessary(index, offset, read_nwrite)) {
        return -1;
    }
    size_t xfer_size = fread(buffer, 1, size, image_file[index]);
    last_read_nwrite[index] = read_nwrite;
    last_seek_pos[index] += xfer_size;
    if ((size != xfer_size) && ferror(image_file[index])) {
        return -1;
    }
    return xfer_size;
}

static int safe_write(uint32_t index, size_t offset, const uint8_t *buffer, uint32_t size)
{
    const bool read_nwrite = false;
    if (!valid_index(index)) {
        return -1;
    }
    if (!open_if_necessary(index, read_nwrite)) {
        return -1;
    }
    if (!seek_if_necessary(index, offset, read_nwrite)) {
        return -1;
    }
    size_t xfer_size = fwrite(buffer, 1, size, image_file[index]);
    last_read_nwrite[index] = read_nwrite;
    last_seek_pos[index] += xfer_size;
    if (size != xfer_size) {
        return -1;
    }
    return xfer_size;
}

static bool open_if_necessary(uint32_t index, bool read_nwrite)
{
    if (!valid_index(index)) {
        return false;
    }
    if (NULL == image_file[index]) {
        const char *file_path = image_path_alloc_from_index(index);
        const char *mode = read_nwrite ? "rb+" : "wb+";
        image_file[index] = fopen(file_path, mode);
        free((void*)file_path);

        last_seek_pos[index] = 0;
        if (NULL == image_file[index]) {
            return false;
        }
    }
    return true;
}

static bool seek_if_necessary(uint32_t index, size_t offset, bool read_nwrite)
{
    if (!valid_index(index)) {
        return false;
    }
    if ((read_nwrite != last_read_nwrite[index]) ||
            (offset != last_seek_pos[index])) {
        if (fseek(image_file[index], offset, SEEK_SET) != 0) {
            last_seek_pos[index] = SEEK_POS_INVALID;
            return false;
        }
    }
    last_read_nwrite[index] = read_nwrite;
    last_seek_pos[index] = offset;
    return true;
}

static bool close_if_necessary(uint32_t index)
{
    if (!valid_index(index)) {
        return false;
    }
    FILE *file = image_file[index];

    image_file[index] = NULL;
    last_seek_pos[index] = SEEK_POS_INVALID;

    if (file != NULL) {
        if (fclose(file) != 0) {
            return false;
        }
    }
    return true;
}

static const char *image_path_alloc_from_index(uint32_t index)
{
    char file_name[32];
    snprintf(file_name, sizeof(file_name), "image_%lu.bin", index);
    file_name[sizeof(file_name) - 1] = 0;
    const char * const path_list[] = {
        mbed_fs_mountpoint(),
        FIRMWARE_DIR,
        file_name,
        NULL
    };
    return path_join_and_alloc(path_list);
}

static const char *header_path_alloc_from_index(uint32_t index)
{
    char file_name[32];
    if (ACTIVE_IMAGE_INDEX == index) {
        snprintf(file_name, sizeof(file_name), "header_active.bin");
    } else {
        snprintf(file_name, sizeof(file_name), "header_%lu.bin", index);
    }
    file_name[sizeof(file_name) - 1] = 0;
    const char * const path_list[] = {
        mbed_fs_mountpoint(),
        FIRMWARE_DIR,
        file_name,
        NULL
    };
    return path_join_and_alloc(path_list);
}

static const char *path_join_and_alloc(const char * const * path_list)
{
    uint32_t string_size = 1;
    uint32_t pos = 0;

    // Determine size of string to return
    while (path_list[pos] != NULL) {
        // Size of string and space for separator
        string_size += strlen(path_list[pos]) + 1;
        pos++;
    }

    // Allocate and initialize memory
    char *path = (char*)malloc(string_size);
    if (NULL == path) {
        error("memory allocation failed");
    }
    memset(path, 0, string_size);

    // Write joined path
    pos = 0;
    while (path_list[pos] != NULL) {
        bool has_slash = '/' == path_list[pos][strlen(path_list[pos]) - 1];
        bool is_last = NULL == path_list[pos + 1];
        strcat(path, path_list[pos]);
        if (!has_slash && !is_last) {
            strcat(path, "/");
        }
        pos++;
    }
    return path;
}

