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


#include "pal_plat_update.h"




#include <mbed.h>
#include <flash-journal-strategy-sequential/flash_journal_strategy_sequential.h>
#include <storage-volume-manager/storage_volume_manager.h>
#include "flash-journal-strategy-sequential/flash_journal_crc.h"



extern int32_t flashJournalStrategySequential_format(ARM_DRIVER_STORAGE      *mtd,
                                                    uint32_t                 numSlots,
                                                    FlashJournal_Callback_t  callback);

#if (!defined(PAL_UPDATE_JOURNAL_SIZE))
#define PAL_UPDATE_JOURNAL_SIZE 0x70000UL
#endif

#if (!defined(PAL_UPDATE_JOURNAL_START_OFFSET))
#define PAL_UPDATE_JOURNAL_START_OFFSET 0x90000UL
#endif

#if (!defined(PAL_UPDATE_JOURNAL_NUM_SLOTS))
#define PAL_UPDATE_JOURNAL_NUM_SLOTS 1
#endif

#if (!defined(PAL_UPDATE_ACTIVE_METADATA_HEADER_OFFSET))
#define PAL_UPDATE_ACTIVE_METADATA_HEADER_OFFSET 0x80000UL
#endif

#define SIZEOF_SHA256 256/8
#define FIRMWARE_HEADER_MAGIC   0x5a51b3d4UL
#define FIRMWARE_HEADER_VERSION 1




typedef enum {
    PAL_PI_MBED_FSM_NONE,
	PAL_PI_MBED_FSM_SETUP,
    PAL_PI_MBED_FSM_WRITE,
    PAL_PI_MBED_FSM_READ,
    PAL_PI_MBED_FSM_COMMIT,
    PAL_PI_MBED_FSM_GETACTIVEHASH
} pal_pi_mbed_fsm_t;

typedef enum {
    PAL_PI_GETATIVEHASH_UNINITIALIZED,
    PAL_PI_GETATIVEHASH_VOLUME_MANAGER_INITIALIZED,
    PAL_PI_GETATIVEHASH_STORAGE_DRIVER_INITIALIZED,
    PAL_PI_GETATIVEHASH_DONE,
    PAL_PI_GETATIVEHASH_ERROR
} pal_pi_mbed_getativehash_state_t;

typedef enum {
    PAL_PI_SETUP_UNINITIALIZED,
    PAL_PI_SETUP_VOLUME_MANAGER_INITIALIZED,
	PAL_PI_SETUP_STORAGE_DRIVER_FORMAT,
    PAL_PI_SETUP_STORAGE_DRIVER_INITIALIZED,
    PAL_PI_SETUP_DONE,
    PAL_PI_SETUP_ERROR = -1,
} pal_pi_mbed_setup_state_t;

typedef enum {
    PAL_PI_READ_SKIP_METADATA,
    PAL_PI_READ_UNINITIALIZED,
    PAL_PI_READ_DONE,
    PAL_PI_READ_ERROR,
} pal_pi_mbed_read_state_t;

typedef enum {
    PAL_PI_COMMIT_UNINITIALIZED,
    PAL_PI_COMMIT_RESIDUAL_LOGGED,
    PAL_PI_COMMIT_DONE,
    PAL_PI_COMMIT_ERROR,
} pal_pi_mbed_commit_state_t;



static int32_t palTranslateDriverErr(int32_t platErr)
{
	int32_t palErr = 0;
	if (platErr >= 0)
		return PAL_SUCCESS;
	switch (platErr)
	{
	case ARM_DRIVER_ERROR:
	case ARM_DRIVER_ERROR_SPECIFIC:
		palErr = PAL_ERR_UPDATE_ERROR;
		break;
	case ARM_DRIVER_ERROR_BUSY:
		palErr = PAL_ERR_UPDATE_BUSY;
		break;
	case ARM_DRIVER_ERROR_TIMEOUT:
		palErr = PAL_ERR_UPDATE_TIMEOUT;
		break;
	case ARM_DRIVER_ERROR_UNSUPPORTED:
		palErr = PAL_ERR_NOT_SUPPORTED;
		break;
	case ARM_DRIVER_ERROR_PARAMETER:
		palErr = PAL_ERR_INVALID_ARGUMENT;
		break;
	default:
		palErr = PAL_ERR_UPDATE_ERROR;
		break;
	}
	return palErr;
}



static int32_t palTranslateJournalErr(int32_t platErr)
{
	int32_t palErr = 0;
	if (platErr >= 0)
		return PAL_SUCCESS;
	switch (platErr)
	{
	case JOURNAL_STATUS_ERROR:
		palErr = PAL_ERR_UPDATE_ERROR;
		break;
	case JOURNAL_STATUS_BUSY:
		palErr = PAL_ERR_UPDATE_BUSY;
		break;
	case JOURNAL_STATUS_TIMEOUT:
		palErr = PAL_ERR_UPDATE_TIMEOUT;
		break;
	case JOURNAL_STATUS_UNSUPPORTED:
		palErr = PAL_ERR_NOT_SUPPORTED;
		break;
	case JOURNAL_STATUS_PARAMETER:
		palErr = PAL_ERR_INVALID_ARGUMENT;
		break;
	case JOURNAL_STATUS_BOUNDED_CAPACITY:
		palErr = PAL_ERR_UPDATE_OUT_OF_BOUNDS;
		break;
	case JOURNAL_STATUS_STORAGE_API_ERROR:
		palErr = PAL_ERR_UPDATE_PALFROM_API;
		break;
	case JOURNAL_STATUS_STORAGE_IO_ERROR:
		palErr = PAL_ERR_UPDATE_PALFROM_IO;
		break;
	case JOURNAL_STATUS_NOT_INITIALIZED:
		palErr = PAL_ERR_NOT_INITIALIZED;
		break;
	case JOURNAL_STATUS_EMPTY:
		palErr = PAL_ERR_UPDATE_END_OF_IMAGE;
		break;
	case JOURNAL_STATUS_SMALL_LOG_REQUEST:
		palErr = PAL_ERR_UPDATE_CHUNK_TO_SMALL;
		break;
	default:
		palErr = PAL_ERR_UPDATE_ERROR;
		break;
	}
	return  palErr;
}

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


typedef struct {
    uint32_t package_id;
    uint32_t fragment_offset;
    palBuffer_t* buffer;
    int32_t numberOfByesRemain;
} pal_pi_mbed_read_context_t;




void PAL_PI_MBED_journal_callbackHandler(int32_t status, FlashJournal_OpCode_t cmd_code);
void PAL_PI_MBED_journalMTD_callbackHandler(int32_t status, ARM_STORAGE_OPERATION operation);
void PAL_PI_MBED_volumeManager_initializeCallbackHandler(int32_t status);



FlashJournal_t pal_pi_mbed_journal;
FlashJournal_Info_t pal_pi_mbed_journal_info;
uint8_t *pal_pi_mbed_overflow_buffer;
uint32_t pal_pi_mbed_overflow_buffer_size = 0;
extern ARM_DRIVER_STORAGE ARM_Driver_Storage_MTD_K64F;
ARM_DRIVER_STORAGE *mtd = &ARM_Driver_Storage_MTD_K64F;
uint8_t pal_pi_mbed_metadata_logged;
static uint8_t flag_volumne_manager_intialized = 0;
static uint8_t flag_hash_volumne_intialized = 0;
static palBuffer_t* pal_pi_mbed_hash_buffer = NULL;

 extern StorageVolumeManager volumeManager;

static _ARM_DRIVER_STORAGE journalMTD;
static _ARM_DRIVER_STORAGE metadataHeaderMTD;
static pal_pi_mbed_getativehash_state_t pal_pi_mbed_getativehash_state;
static pal_pi_mbed_read_state_t pal_pi_mbed_read_state;
static pal_pi_mbed_read_context_t pal_pi_mbed_read_context;
static pal_pi_mbed_fsm_t pal_pi_mbed_active_fsm = PAL_PI_MBED_FSM_NONE;
static pal_pi_mbed_setup_state_t pal_pi_mbed_setup_state;
static FirmwareHeader_t pal_pi_mbed_firmware_header;
static pal_pi_mbed_commit_state_t pal_pi_mbed_commit_state;




//forward declaration
int PAL_PI_MBED_Setup_StateMachine_Enter();
void PAL_PI_MBED_Setup_StateMachine_Advance(int32_t status);

int PAL_PI_MBED_Write_StateMachine_Enter(int32_t status);
void PAL_PI_MBED_Write_StateMachine_Advance(int32_t status);

int PAL_PI_MBED_Read_StateMachine_Enter();
void PAL_PI_MBED_Read_StateMachine_Advance(int32_t status);

int PAL_PI_MBED_Commit_StateMachine_Enter();
void PAL_PI_MBED_Commit_StateMachine_Advance(int32_t status);
/*
 * call back functions
 *
 */

static palImageSignalEvent_t g_palUpdateServiceCBfunc;

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
	g_palUpdateServiceCBfunc = CBfunction;
	g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_INIT);
	return PAL_SUCCESS;
}

palStatus_t pal_plat_imageDeInit(void)
{
	palStatus_t status = PAL_SUCCESS;
    if (NULL != pal_pi_mbed_overflow_buffer) // if allocated free
    	free(pal_pi_mbed_overflow_buffer);
	return status;
}

#if 1
void PAL_PI_MBED_journal_callbackHandler(int32_t status, FlashJournal_OpCode_t cmd_code)
{
	int32_t rc = 0;
	//solve compilation warning to be removed
	DEBUG_PRINT("pal_pi_mbed_active_fsm %u cmd_code %u\n", pal_pi_mbed_active_fsm, cmd_code);
	if (FLASH_JOURNAL_OPCODE_RESET == cmd_code)
		return;
	switch (pal_pi_mbed_active_fsm)
	{
		case PAL_PI_MBED_FSM_NONE:
			PAL_PI_MBED_Setup_StateMachine_Advance(status);
			rc = PAL_PI_MBED_Setup_StateMachine_Enter();
			break;

		case PAL_PI_MBED_FSM_SETUP:
			PAL_PI_MBED_Setup_StateMachine_Advance(status);
			rc = PAL_PI_MBED_Setup_StateMachine_Enter();
			break;


		case PAL_PI_MBED_FSM_READ:
			PAL_PI_MBED_Read_StateMachine_Advance(status);
			rc = PAL_PI_MBED_Read_StateMachine_Enter();
			break;

		case PAL_PI_MBED_FSM_WRITE:
			PAL_PI_MBED_Write_StateMachine_Advance(status);
			rc = PAL_PI_MBED_Write_StateMachine_Enter(status);
			break;

		case PAL_PI_MBED_FSM_COMMIT:
			PAL_PI_MBED_Commit_StateMachine_Advance(status);
			rc = PAL_PI_MBED_Commit_StateMachine_Enter();
			break;

		default:
			break;
	}

	DEBUG_PRINT("rc is %d\r\n",rc);
}






#else
void PAL_PI_MBED_journal_callbackHandler(int32_t status, FlashJournal_OpCode_t cmd_code)
{
    int32_t rc = 0;
    DEBUG_PRINT("pal_pi_mbed_active_fsm %u cmd_code %u\n", pal_pi_mbed_active_fsm, cmd_code);
    switch (cmd_code)
    {
    	case FLASH_JOURNAL_OPCODE_FORMAT:
            DEBUG_PRINT("FLASH_JOURNAL_OPCODE_FORMAT\r\n");
            switch (pal_pi_mbed_active_fsm)
            {
                case PAL_PI_MBED_FSM_NONE:
                    PAL_PI_MBED_Setup_StateMachine_Advance(status);
                    rc = PAL_PI_MBED_Setup_StateMachine_Enter();
                    break;

                default:
                    break;
            }
            break;
        case FLASH_JOURNAL_OPCODE_INITIALIZE:
            DEBUG_PRINT("FLASH_JOURNAL_OPCODE_INITIALIZE\r\n");
            switch (pal_pi_mbed_active_fsm)
            {
                case PAL_PI_MBED_FSM_SETUP:
                    PAL_PI_MBED_Setup_StateMachine_Advance(status);
                    rc = PAL_PI_MBED_Setup_StateMachine_Enter();
                    break;

                default:
                    break;
            }
            break;

        case FLASH_JOURNAL_OPCODE_READ_BLOB:
            DEBUG_PRINT("FLASH_JOURNAL_OPCODE_READ_BLOB\r\n");
            switch (pal_pi_mbed_active_fsm)
            {
                case PAL_PI_MBED_FSM_READ:
                    PAL_PI_MBED_Read_StateMachine_Advance(status);
                    rc = PAL_PI_MBED_Read_StateMachine_Enter();
                    break;

                default:
                    break;
            }
            break;

        case FLASH_JOURNAL_OPCODE_LOG_BLOB:
            DEBUG_PRINT("FLASH_JOURNAL_OPCODE_LOG_BLOB\r\n");
            switch (pal_pi_mbed_active_fsm)
            {
                case PAL_PI_MBED_FSM_WRITE:
                    PAL_PI_MBED_Write_StateMachine_Advance(status);
                    rc = PAL_PI_MBED_Write_StateMachine_Enter(status);
                    break;

                case PAL_PI_MBED_FSM_COMMIT:
                    PAL_PI_MBED_Commit_StateMachine_Advance(status);
                    rc = PAL_PI_MBED_Commit_StateMachine_Enter();
                    break;

                default:
                    break;
            }
            break;

        case FLASH_JOURNAL_OPCODE_COMMIT:
            DEBUG_PRINT("FLASH_JOURNAL_OPCODE_COMMIT\r\n");
            switch (pal_pi_mbed_active_fsm)
            {
                case PAL_PI_MBED_FSM_COMMIT:
                    PAL_PI_MBED_Commit_StateMachine_Advance(status);
                    rc = PAL_PI_MBED_Commit_StateMachine_Enter();
                    break;

                default:
                    break;
            }
            break;

        case FLASH_JOURNAL_OPCODE_RESET:
            DEBUG_PRINT("FLASH_JOURNAL_OPCODE_RESET\r\n");
            break;

        default:
            break;
    }

    if (rc < 0)
    {
        switch (pal_pi_mbed_active_fsm)
        {
            case PAL_PI_MBED_FSM_SETUP:
                PAL_PI_MBED_Setup_StateMachine_Advance(rc);
                PAL_PI_MBED_Setup_StateMachine_Enter();
                break;

            case PAL_PI_MBED_FSM_READ:
                PAL_PI_MBED_Read_StateMachine_Advance(rc);
                PAL_PI_MBED_Read_StateMachine_Enter();
                break;

            case PAL_PI_MBED_FSM_WRITE:
                PAL_PI_MBED_Write_StateMachine_Advance(rc);
                PAL_PI_MBED_Write_StateMachine_Enter(0);
                break;

            case PAL_PI_MBED_FSM_COMMIT:
                PAL_PI_MBED_Commit_StateMachine_Advance(rc);
                PAL_PI_MBED_Commit_StateMachine_Enter();
                break;

            default:
                break;
        }
    }
}
#endif


void PAL_PI_MBED_journalMTD_callbackHandler(int32_t status, ARM_STORAGE_OPERATION operation)
{
    DEBUG_PRINT("in journalMTD_callbackHandler for operation %d with status %d\r\n", operation, status);
    /* TODO implement for possible asynch behaviour of MTD */
}

void PAL_PI_MBED_volumeManager_initializeCallbackHandler(int32_t status)
{
    int rc = 0;
    DEBUG_PRINT("in volumeManager_initializeCallbackHandler with status %d\r\n", status);
    switch (pal_pi_mbed_active_fsm)
    {
        case PAL_PI_MBED_FSM_SETUP:
            PAL_PI_MBED_Setup_StateMachine_Advance(status);
            rc = PAL_PI_MBED_Setup_StateMachine_Enter();
            if (rc < 0)
            {
                PAL_PI_MBED_Setup_StateMachine_Advance(rc);
                PAL_PI_MBED_Setup_StateMachine_Enter();
            }
            break;

        case PAL_PI_MBED_FSM_GETACTIVEHASH:
            if (status < 0)
            {
                pal_pi_mbed_getativehash_state = PAL_PI_GETATIVEHASH_ERROR;
            }
            rc = PAL_PI_MBED_Setup_StateMachine_Enter();
            if (rc < 0)
            {
                pal_pi_mbed_getativehash_state = PAL_PI_GETATIVEHASH_ERROR;
                PAL_PI_MBED_Setup_StateMachine_Enter();
            }
            break;
        default:
            break;
    }
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

int PAL_PI_MBED_GetAtiveHash_StateMachine()
{
    int rc = JOURNAL_STATUS_OK;
    DEBUG_PRINT("PAL_PI_MBED_GetAtiveHash_StateMachine %u\r\n", pal_pi_mbed_getativehash_state);

    switch(pal_pi_mbed_getativehash_state)
    {
        case PAL_PI_GETATIVEHASH_UNINITIALIZED:
            DEBUG_PRINT("PAL_PI_GETATIVEHASH_UNINITIALIZED\r\n");
            if (volumeManager.isInitialized() == 0)
            {
                rc = volumeManager.initialize(mtd, PAL_PI_MBED_volumeManager_initializeCallbackHandler);
            }
            else
            {
                rc = ARM_DRIVER_OK+1; // continue onto next state
            }

            if (rc < ARM_DRIVER_OK)
            {
            	rc = palTranslateDriverErr(rc);
                DEBUG_PRINT("Volume Manager Initialize Error %i\r\n", rc);
                break;
            }
            else if (rc > ARM_DRIVER_OK)
            {
                /* sychronous completion */
                flag_volumne_manager_intialized = 1;
                pal_pi_mbed_getativehash_state = PAL_PI_GETATIVEHASH_VOLUME_MANAGER_INITIALIZED;
                rc = PAL_PI_MBED_GetAtiveHash_StateMachine();
            }
            break;

        case PAL_PI_GETATIVEHASH_VOLUME_MANAGER_INITIALIZED:
            DEBUG_PRINT("PAL_PI_GETATIVEHASH_VOLUME_MANAGER_INITIALIZED\r\n");
            if (flag_hash_volumne_intialized == 0)
            {
                rc = volumeManager.addVolume_C(PAL_UPDATE_ACTIVE_METADATA_HEADER_OFFSET, sizeof(FirmwareHeader_t), &metadataHeaderMTD);

                if (rc < ARM_DRIVER_OK)
                {
                	rc = palTranslateDriverErr(rc);
                    DEBUG_PRINT("addVolume_C Error %d\r\n", rc);
                    break;
                }

                DEBUG_PRINT("metadataHeaderMTD initialize\r\n");
                rc = metadataHeaderMTD.Initialize(PAL_PI_MBED_journalMTD_callbackHandler);
            }
            else
            {
                rc = ARM_DRIVER_OK+1;
            }

            if (rc < ARM_DRIVER_OK)
            {
            	rc = palTranslateDriverErr(rc);
                break; // handle error
            }
            else if (rc > ARM_DRIVER_OK)
            {
                flag_hash_volumne_intialized = 1;
                pal_pi_mbed_getativehash_state = PAL_PI_GETATIVEHASH_STORAGE_DRIVER_INITIALIZED;
                rc = PAL_PI_MBED_GetAtiveHash_StateMachine();
            }

            break;

        case PAL_PI_GETATIVEHASH_STORAGE_DRIVER_INITIALIZED:
            DEBUG_PRINT("PAL_PI_GETATIVEHASH_STORAGE_DRIVER_INITIALIZED\r\n");
            /* TODO validate the header before returning the member data */
            rc = metadataHeaderMTD.ReadData(offsetof(FirmwareHeader_t, firmwareSHA256), pal_pi_mbed_hash_buffer->buffer, SIZEOF_SHA256);
            if (rc < ARM_DRIVER_OK)
            {
            	rc = palTranslateDriverErr(rc);
                break; // handle error
            }
            else if (rc > ARM_DRIVER_OK)
            {
                pal_pi_mbed_hash_buffer->bufferLength = SIZEOF_SHA256;
                pal_pi_mbed_getativehash_state = PAL_PI_GETATIVEHASH_DONE;
                rc = PAL_PI_MBED_GetAtiveHash_StateMachine();
            }
            break;

        case PAL_PI_GETATIVEHASH_DONE:
            DEBUG_PRINT("PAL_PI_GETATIVEHASH_DONE\r\n");
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_GETACTIVEHASH);
            break;

        case PAL_PI_GETATIVEHASH_ERROR:
            DEBUG_PRINT("PAL_PI_GETATIVEHASH_ERROR\r\n");
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
            break;
    }

    return rc;
}

palStatus_t pal_plat_imageGetActiveHash(palBuffer_t *hash)
{
    pal_pi_mbed_hash_buffer = hash;
    pal_pi_mbed_active_fsm = PAL_PI_MBED_FSM_GETACTIVEHASH;
    pal_pi_mbed_getativehash_state = PAL_PI_GETATIVEHASH_UNINITIALIZED;


    return  PAL_PI_MBED_GetAtiveHash_StateMachine();
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


/*
 * init apis
 */




void PAL_PI_MBED_Setup_StateMachine_Advance(int32_t status)
{
    if (status < 0)
    {
        pal_pi_mbed_setup_state = PAL_PI_SETUP_ERROR;
    }
    else
    {
        switch(pal_pi_mbed_setup_state)
        {
            case PAL_PI_SETUP_UNINITIALIZED:
                pal_pi_mbed_setup_state = PAL_PI_SETUP_VOLUME_MANAGER_INITIALIZED;
                break;

            case PAL_PI_SETUP_VOLUME_MANAGER_INITIALIZED:
                pal_pi_mbed_setup_state = PAL_PI_SETUP_STORAGE_DRIVER_FORMAT;
                break;
            case PAL_PI_SETUP_STORAGE_DRIVER_FORMAT:
                pal_pi_mbed_setup_state = PAL_PI_SETUP_STORAGE_DRIVER_INITIALIZED;
                break;
            case PAL_PI_SETUP_STORAGE_DRIVER_INITIALIZED:
                pal_pi_mbed_setup_state = PAL_PI_SETUP_DONE;
                break;

            default:
                break;
        }
    }
}

int PAL_PI_MBED_Setup_StateMachine_Enter()
{
    int rc = 0;

    switch(pal_pi_mbed_setup_state)
    {
        case PAL_PI_SETUP_UNINITIALIZED:
            DEBUG_PRINT("PAL_PI_SETUP_UNINITIALIZED\r\n");

            pal_pi_mbed_metadata_logged = 0;
            if (volumeManager.isInitialized() == 0)
            {
                rc = volumeManager.initialize(mtd, PAL_PI_MBED_volumeManager_initializeCallbackHandler);
            }
            else
            {
                DEBUG_PRINT("volumeManager already initialized\r\n");
                rc = ARM_DRIVER_OK+1;
            }

            if (rc < ARM_DRIVER_OK)
            {
            	rc = palTranslateDriverErr(rc);
                DEBUG_PRINT("Initialize Error %i\r\n", rc);
                break;
            }
            else if (rc > ARM_DRIVER_OK)
            {
                PAL_PI_MBED_Setup_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Setup_StateMachine_Enter();
                /* sychronous completion */
            }
            break;

        case PAL_PI_SETUP_VOLUME_MANAGER_INITIALIZED:
        	DEBUG_PRINT("PAL_PI_SETUP_VOLUME_MANAGER_INITIALIZED\r\n");

            rc = volumeManager.addVolume_C(PAL_UPDATE_JOURNAL_START_OFFSET, PAL_UPDATE_JOURNAL_SIZE, &journalMTD);
            if (rc < ARM_DRIVER_OK)
            {
            	DEBUG_PRINT("addVolume_C Error %d\r\n", rc);
            	rc = palTranslateDriverErr(rc);
                break;
            }

            DEBUG_PRINT("journalMTD initialize\r\n");
            rc = journalMTD.Initialize(PAL_PI_MBED_journalMTD_callbackHandler);
            if (rc < JOURNAL_STATUS_OK)
            {
            	DEBUG_PRINT("journalMTD.Initialize Error %i\r\n", rc);
            	rc = palTranslateJournalErr(rc);
                break; // handle error
            }
            else if (rc > ARM_DRIVER_OK)
            {
                PAL_PI_MBED_Setup_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Setup_StateMachine_Enter();
            }

            break;
        case PAL_PI_SETUP_STORAGE_DRIVER_FORMAT:
        	DEBUG_PRINT("PAL_PI_SETUP_STORAGE_DRIVER_FORMAT\r\n");
        	DEBUG_PRINT("pal_pi_mbed_active_fsm %d\n", pal_pi_mbed_active_fsm);
            rc = flashJournalStrategySequential_format((ARM_DRIVER_STORAGE *)&journalMTD, PAL_UPDATE_JOURNAL_NUM_SLOTS, PAL_PI_MBED_journal_callbackHandler);
            if (rc < JOURNAL_STATUS_OK)
            {
            	DEBUG_PRINT("FlashJournal_initialize Error %i\r\n", rc);
            	rc = palTranslateJournalErr(rc);
                break;// handle error
            }
            else if (rc > JOURNAL_STATUS_OK)
            {
                PAL_PI_MBED_Setup_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Setup_StateMachine_Enter();
            }
            break;
        case PAL_PI_SETUP_STORAGE_DRIVER_INITIALIZED:
            DEBUG_PRINT("PAL_PI_SETUP_STORAGE_DRIVER_INITIALIZED\r\n");
            rc = FlashJournal_initialize(&pal_pi_mbed_journal, (ARM_DRIVER_STORAGE *)&journalMTD, &FLASH_JOURNAL_STRATEGY_SEQUENTIAL, PAL_PI_MBED_journal_callbackHandler);
            if (rc < JOURNAL_STATUS_OK)
            {
            	DEBUG_PRINT("FlashJournal_initialize Error %i\r\n", rc);
            	rc = palTranslateJournalErr(rc);
                break;// handle error
            }
            else if (rc > JOURNAL_STATUS_OK)
            {
                PAL_PI_MBED_Setup_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Setup_StateMachine_Enter();
            }
            break;

        case PAL_PI_SETUP_DONE:
            DEBUG_PRINT("PAL_PI_SETUP_DONE\r\n");
            rc = FlashJournal_getInfo(&pal_pi_mbed_journal, &pal_pi_mbed_journal_info);
            if (rc < JOURNAL_STATUS_OK)
            {
            	DEBUG_PRINT("FlashJournal_getInfo Error %i\r\n", rc);
            	rc = palTranslateJournalErr(rc);
                break; // handle error
            }
            else
            {
                DEBUG_PRINT("journalInfo: capacity %lu, size %lu, program_unit %lu\r\n",
                    (uint32_t)(pal_pi_mbed_journal_info.capacity),
                    (uint32_t)(pal_pi_mbed_journal_info.sizeofJournaledBlob),
                    (uint32_t)(pal_pi_mbed_journal_info.program_unit));
            }

            if (!pal_pi_mbed_overflow_buffer) // if not allocated before allocate now
            	pal_pi_mbed_overflow_buffer = (uint8_t*) malloc(pal_pi_mbed_journal_info.program_unit);
            // returning in a callback
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_PREPARE);
            break;

        case PAL_PI_SETUP_ERROR:
            DEBUG_PRINT("PAL_PI_SETUP_ERROR\r\n");
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
            break;
    }

    return (rc);
}


//fill the image header data for writing, the writing will occur in the 1st image write
palStatus_t pal_plat_imageSetHeader(palImageId_t imageId,palImageHeaderDeails_t *details)
{
	palStatus_t status = PAL_SUCCESS;
	memset(&pal_pi_mbed_firmware_header,0,sizeof(pal_pi_mbed_firmware_header));
	pal_pi_mbed_firmware_header.totalSize = details->imageSize + sizeof(FirmwareHeader_t);
	pal_pi_mbed_firmware_header.magic = FIRMWARE_HEADER_MAGIC;
	pal_pi_mbed_firmware_header.version = FIRMWARE_HEADER_VERSION;
	pal_pi_mbed_firmware_header.firmwareVersion = details->version;

	memcpy(pal_pi_mbed_firmware_header.firmwareSHA256,details->hash.buffer,SIZEOF_SHA256);
	/*
	 * calculating and setting the checksum of the header.
	 * Have to call to flashJournalCrcReset before use.
	 */
	flashJournalCrcReset();
	pal_pi_mbed_firmware_header.checksum = flashJournalCrcCummulative((const unsigned char*) &pal_pi_mbed_firmware_header,
                                                                      (int) sizeof(pal_pi_mbed_firmware_header));

	return  status;
}


palStatus_t pal_plat_imageReserveSpace(palImageId_t imageId, size_t imageSize)
{
	palStatus_t status;
	DEBUG_PRINT("pal_pi_mbed_active_fsm %d\n", pal_pi_mbed_active_fsm);
	pal_pi_mbed_active_fsm = PAL_PI_MBED_FSM_SETUP;
	pal_pi_mbed_setup_state = PAL_PI_SETUP_UNINITIALIZED;
	status = PAL_PI_MBED_Setup_StateMachine_Enter();
	DEBUG_PRINT("pal_plat_imageReserveSpace size = %ld\r\n",imageSize);
	return status;
}





/*
 * write API
 */






typedef enum {
    PAL_PI_WRITE_UNINITIALIZED,
    PAL_PI_WRITE_METADATA_LOGGED,
    PAL_PI_WRITE_RESIDUAL_LOGGED,
    PAL_PI_WRITE_DONE,
    PAL_PI_WRITE_ERROR,
} pal_pi_mbed_write_state_t;

static pal_pi_mbed_write_state_t pal_pi_mbed_write_state;

typedef struct {
    uint32_t package_id;
    const uint8_t* package_fragment;
    int32_t fragment_size;
    uint32_t fragment_offset;
} pal_pi_mbed_write_context_t;

static pal_pi_mbed_write_context_t pal_pi_mbed_write_context;

int PAL_PI_MBED_Write_LogResidual()
{
    // Log residual from last write
    if (pal_pi_mbed_overflow_buffer_size > 0)
    {
        int32_t sz = pal_pi_mbed_journal_info.program_unit - pal_pi_mbed_overflow_buffer_size;
        uint8_t* dst = pal_pi_mbed_overflow_buffer + pal_pi_mbed_overflow_buffer_size;

        if (pal_pi_mbed_write_context.fragment_size < sz)
        {
            sz = pal_pi_mbed_write_context.fragment_size;
        }

        const uint8_t* src = pal_pi_mbed_write_context.package_fragment;
        memcpy(dst, src, sz);

        pal_pi_mbed_write_context.package_fragment += sz;
        pal_pi_mbed_write_context.fragment_size -= sz;
        pal_pi_mbed_overflow_buffer_size += sz;

        if (pal_pi_mbed_overflow_buffer_size < pal_pi_mbed_journal_info.program_unit)
        {
            sz = pal_pi_mbed_journal_info.program_unit - pal_pi_mbed_overflow_buffer_size;
            dst = pal_pi_mbed_overflow_buffer + pal_pi_mbed_overflow_buffer_size;
            memset(dst, 0x00, sz);
        }

        return FlashJournal_log(&pal_pi_mbed_journal, pal_pi_mbed_overflow_buffer, pal_pi_mbed_journal_info.program_unit);
    }

    return JOURNAL_STATUS_OK+1;
}

void PAL_PI_MBED_Write_StateMachine_Advance(int32_t status)
{
	DEBUG_PRINT("status = %d\r\n",status);
    if (status < 0)
    {
        pal_pi_mbed_write_state = PAL_PI_WRITE_ERROR;
    }
    else
    {
        switch(pal_pi_mbed_write_state)
        {
            case PAL_PI_WRITE_UNINITIALIZED:
                pal_pi_mbed_write_state = PAL_PI_WRITE_METADATA_LOGGED;
                break;

            case PAL_PI_WRITE_METADATA_LOGGED:
                pal_pi_mbed_write_state = PAL_PI_WRITE_RESIDUAL_LOGGED;
                break;

            case PAL_PI_WRITE_RESIDUAL_LOGGED:
                pal_pi_mbed_write_state = PAL_PI_WRITE_DONE;
                break;

            default:
                break;
        }
    }
}

int PAL_PI_MBED_Write_StateMachine_Enter(int32_t status)
{
    int rc = status;


    switch(pal_pi_mbed_write_state)
    {
        case PAL_PI_WRITE_UNINITIALIZED:
            DEBUG_PRINT("PAL_PI_WRITE_UNINITIALIZED\r\n");
            // If the journal is clean, write metadata header first
            if (pal_pi_mbed_metadata_logged == 0)
            {
                /* TODO Calculate header checksum */

                rc = FlashJournal_log(&pal_pi_mbed_journal, &pal_pi_mbed_firmware_header, sizeof(pal_pi_mbed_firmware_header));
                if (rc < JOURNAL_STATUS_OK)
                {
                	DEBUG_PRINT("FlashJournal_log Error %i\r\n", rc);
                	rc = palTranslateJournalErr(rc);
                    return rc;
                }
                else if (rc == JOURNAL_STATUS_OK)
                {
                    break;
                }
            }

            PAL_PI_MBED_Write_StateMachine_Advance(0);
            rc = PAL_PI_MBED_Write_StateMachine_Enter(rc);
            break;

        case PAL_PI_WRITE_METADATA_LOGGED:
            DEBUG_PRINT("PAL_PI_WRITE_METADATA_LOGGED\r\n");

            if (pal_pi_mbed_metadata_logged == 0 && rc < (int)sizeof(pal_pi_mbed_firmware_header))
            {
                uint32_t residual = sizeof(pal_pi_mbed_firmware_header) - rc;
                memcpy(pal_pi_mbed_overflow_buffer, &pal_pi_mbed_firmware_header+rc, residual);
                pal_pi_mbed_overflow_buffer_size = residual;
            }

            pal_pi_mbed_metadata_logged = 1;
            rc = PAL_PI_MBED_Write_LogResidual();
            if (rc < JOURNAL_STATUS_OK)
            {
            	DEBUG_PRINT("PAL_PI_MBED_Write_LogResidual Error %i\r\n", rc);
            	rc = palTranslateJournalErr(rc);
            	return rc;
            }
            else if (rc > JOURNAL_STATUS_OK)
            {
                PAL_PI_MBED_Write_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Write_StateMachine_Enter(rc);
            }

            break;

        case PAL_PI_WRITE_RESIDUAL_LOGGED:
            DEBUG_PRINT("PAL_PI_WRITE_RESIDUAL_LOGGED\r\n");

            pal_pi_mbed_overflow_buffer_size = 0;

            DEBUG_PRINT("fragment_size %d package_fragment=%p\r\n", pal_pi_mbed_write_context.fragment_size,pal_pi_mbed_write_context.package_fragment);
            if (pal_pi_mbed_write_context.fragment_size > 0)
            {
                rc = FlashJournal_log(&pal_pi_mbed_journal, pal_pi_mbed_write_context.package_fragment, pal_pi_mbed_write_context.fragment_size);
                if (rc == JOURNAL_STATUS_SMALL_LOG_REQUEST)
                {
                    rc = PAL_SUCCESS;
                }
                else if (rc < JOURNAL_STATUS_OK)
                {
                	DEBUG_PRINT("FlashJournal_log Error %i\r\n", rc);
                	rc = palTranslateJournalErr(rc);
                	return rc;
                }
                else if (rc == JOURNAL_STATUS_OK)
                {
                    break;
                }
            }

            PAL_PI_MBED_Write_StateMachine_Advance(0);
            rc = PAL_PI_MBED_Write_StateMachine_Enter(rc);
            break;

        case PAL_PI_WRITE_DONE:
            DEBUG_PRINT("PAL_PI_WRITE_DONE\r\n");
            if (status < pal_pi_mbed_write_context.fragment_size)
            {
                uint32_t residual = pal_pi_mbed_write_context.fragment_size - rc;
                memcpy(pal_pi_mbed_overflow_buffer, pal_pi_mbed_write_context.package_fragment+rc, residual);
                pal_pi_mbed_overflow_buffer_size = residual;
            }

            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_WRITE);
            break;

        case PAL_PI_WRITE_ERROR:
            DEBUG_PRINT("PAL_PI_WRITE_ERROR\r\n");
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
            break;

        default:
            break;
    }

    return rc;
}
palStatus_t pal_plat_imageWrite(palImageId_t imageId, size_t offset, palConstBuffer_t *chunk)
{
    pal_pi_mbed_write_context.package_id       = imageId;
    pal_pi_mbed_write_context.package_fragment = chunk->buffer;
    pal_pi_mbed_write_context.fragment_size    = chunk->bufferLength;
    pal_pi_mbed_write_context.fragment_offset  = offset;
    pal_pi_mbed_write_state                    = PAL_PI_WRITE_UNINITIALIZED;
    pal_pi_mbed_active_fsm = PAL_PI_MBED_FSM_WRITE;
    return PAL_PI_MBED_Write_StateMachine_Enter(0);
}


/*
 * read APIs
 */



void PAL_PI_MBED_Read_StateMachine_Advance(int32_t status)
{
    if (status < 0)
    {
        pal_pi_mbed_read_state = PAL_PI_READ_ERROR;
    }
    else
    {
        switch(pal_pi_mbed_read_state)
        {
            case PAL_PI_READ_SKIP_METADATA:
                pal_pi_mbed_read_state = PAL_PI_READ_UNINITIALIZED;
                break;

            case PAL_PI_READ_UNINITIALIZED:
            	//in a-sync mode the status value is the number of bytes that was read
            	// if not set by now
            	if (0 == pal_pi_mbed_read_context.buffer->bufferLength)
            	{
                	DEBUG_PRINT("Working in a-sync mode number of bytes read = %d\r\n",status);
                	pal_pi_mbed_read_context.buffer->bufferLength = status;
            	}
                pal_pi_mbed_read_state = PAL_PI_READ_DONE;
                break;

            default:
                break;
        }
    }
}

/* TODO Handle end of journal */
int PAL_PI_MBED_Read_StateMachine_Enter()
{
    int rc = 0;

    switch(pal_pi_mbed_read_state)
    {
        case PAL_PI_READ_SKIP_METADATA:
            DEBUG_PRINT("PAL_PI_READ_SKIP_METADATA\r\n");
            pal_pi_mbed_read_context.numberOfByesRemain = pal_pi_mbed_firmware_header.totalSize - sizeof(FirmwareHeader_t);
            rc = FlashJournal_read(&pal_pi_mbed_journal, &pal_pi_mbed_firmware_header, sizeof(pal_pi_mbed_firmware_header));
            DEBUG_PRINT("FlashJournal_read header have return %d size of header is %d\r\n",rc,sizeof(pal_pi_mbed_firmware_header));
            if (rc <= JOURNAL_STATUS_OK)
            {
            	rc = palTranslateJournalErr(rc);
                break;
            }
            PAL_PI_MBED_Read_StateMachine_Advance(0);
            rc = PAL_PI_MBED_Read_StateMachine_Enter();
            break;

        case PAL_PI_READ_UNINITIALIZED:
            DEBUG_PRINT("PAL_PI_READ_UNINITIALIZED\r\n");
            rc = FlashJournal_read(&pal_pi_mbed_journal, pal_pi_mbed_read_context.buffer->buffer, pal_pi_mbed_read_context.buffer->maxBufferLength);
            DEBUG_PRINT("FlashJournal_read have return %i\r\n",rc);
            if (rc > JOURNAL_STATUS_OK) // handle synchronous completion
            {
            	/*
            	 * the journal API pads the image to align to size of program_unit.
            	 * to ignore the padded byes if the journal read more then the size of the image,
            	 * the API keep tracking on how many byes are left to read from the image.
            	 * if the journal API return that it read more bytes just ignore them
            	 */
            	DEBUG_PRINT("numberOfByesRemain have  %i\r\n",pal_pi_mbed_read_context.numberOfByesRemain);
            	if (rc <= pal_pi_mbed_read_context.numberOfByesRemain)
            	{
            		pal_pi_mbed_read_context.buffer->bufferLength = rc;
            		pal_pi_mbed_read_context.numberOfByesRemain = pal_pi_mbed_read_context.numberOfByesRemain - rc;
            	}
            	else
            	{
            		pal_pi_mbed_read_context.buffer->bufferLength =  pal_pi_mbed_read_context.numberOfByesRemain;
            	}
            	DEBUG_PRINT("Working in sync mode number of bytes read = %d\r\n",pal_pi_mbed_read_context.buffer->bufferLength);
            	//in sync mode the return value is the number of bytes that was read
                PAL_PI_MBED_Read_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Read_StateMachine_Enter();
            }
            else if (rc < JOURNAL_STATUS_OK)
            {
            	rc = palTranslateJournalErr(rc);
            	break;
            }
            break;

        case PAL_PI_READ_DONE:
            DEBUG_PRINT("PAL_PI_READ_DONE\r\n");
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_READTOBUFFER);
            break;

        case PAL_PI_READ_ERROR:
            DEBUG_PRINT("PAL_PI_READ_ERROR\r\n");
            g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
            break;

        default:
            break;
    }

    return rc;
}


palStatus_t pal_plat_imageReadToBuffer(palImageId_t imageId, size_t offset, palBuffer_t *chunk)
{
	/*check if this is the 1st time reading if so you have to skip the header
	 * if not you can read normally
	 */
    DEBUG_PRINT("pal_plat_imageReadToBuffer\r\n");
    if (pal_pi_mbed_active_fsm != PAL_PI_MBED_FSM_READ)
    {
        pal_pi_mbed_read_state = PAL_PI_READ_SKIP_METADATA;
    }
    else
    {
        pal_pi_mbed_read_state = PAL_PI_READ_UNINITIALIZED;
    }

    pal_pi_mbed_active_fsm = PAL_PI_MBED_FSM_READ;

    pal_pi_mbed_read_context.package_id = imageId;
    pal_pi_mbed_read_context.fragment_offset = offset;
    /*update the size you want to read from the flash*/
    pal_pi_mbed_read_context.buffer = chunk;
    pal_pi_mbed_read_context.buffer->bufferLength = 0;


    return PAL_PI_MBED_Read_StateMachine_Enter();
}



/*
 * commit functions
 * */

void PAL_PI_MBED_Commit_StateMachine_Advance(int32_t status)
{
    if (status < 0)
    {
        pal_pi_mbed_commit_state = PAL_PI_COMMIT_ERROR;
    }
    else
    {
        switch(pal_pi_mbed_commit_state)
        {
            case PAL_PI_COMMIT_UNINITIALIZED:
                pal_pi_mbed_commit_state = PAL_PI_COMMIT_RESIDUAL_LOGGED;
                break;

            case PAL_PI_COMMIT_RESIDUAL_LOGGED:
                pal_pi_mbed_commit_state = PAL_PI_COMMIT_DONE;
                break;

            default:
                break;
        }
    }
}

int PAL_PI_MBED_Commit_StateMachine_Enter()
{
    int rc = 0;

    switch(pal_pi_mbed_commit_state)
    {
        case PAL_PI_COMMIT_UNINITIALIZED:
            DEBUG_PRINT("PAL_PI_COMMIT_UNINITIALIZED\r\n");
            rc = FlashJournal_getInfo(&pal_pi_mbed_journal, &pal_pi_mbed_journal_info);
            if (rc < JOURNAL_STATUS_OK)
            {
            	rc = palTranslateJournalErr(rc);
                break;// handle error
            }

            rc = PAL_PI_MBED_Write_LogResidual();
            if (rc < JOURNAL_STATUS_OK)
            {
            	rc = palTranslateJournalErr(rc);
                break;
            }
            else if (rc > JOURNAL_STATUS_OK)
            {
                PAL_PI_MBED_Commit_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Commit_StateMachine_Enter();
            }

            break;

        case PAL_PI_COMMIT_RESIDUAL_LOGGED:
            DEBUG_PRINT("PAL_PI_COMMIT_RESIDUAL_LOGGED\r\n");
            // Clear the overflow buffer
            pal_pi_mbed_overflow_buffer_size = 0;

            rc = FlashJournal_commit(&pal_pi_mbed_journal);
            if (rc < JOURNAL_STATUS_OK)
            {
            	rc = palTranslateJournalErr(rc);
                break;
            }
            else if (rc > JOURNAL_STATUS_OK)
            {
                PAL_PI_MBED_Commit_StateMachine_Advance(0);
                rc = PAL_PI_MBED_Commit_StateMachine_Enter();
            }

            break;

        case PAL_PI_COMMIT_DONE:
			DEBUG_PRINT("PAL_PI_COMMIT_DONE\r\n");
			g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_FINALIZE);
			break;

		case PAL_PI_COMMIT_ERROR:
			DEBUG_PRINT("PAL_PI_COMMIT_ERROR\r\n");
			g_palUpdateServiceCBfunc(PAL_IMAGE_EVENT_ERROR);
			break;
        default:
            break;
    }

    return rc;
}

palStatus_t pal_plat_imageFlush(palImageId_t package_id)
{
    pal_pi_mbed_active_fsm   = PAL_PI_MBED_FSM_COMMIT;
    pal_pi_mbed_commit_state = PAL_PI_COMMIT_UNINITIALIZED;

    return PAL_PI_MBED_Commit_StateMachine_Enter();
}

