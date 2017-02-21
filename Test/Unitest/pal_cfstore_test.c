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

#include "pal.h"
#include "pal_cfstore.h"
#include "unity.h"
#include "unity_fixture.h"
// For memset
#include <string.h>

// This should not be in use when code is committed to master!!!
#undef TEST_PRINTF
#define TEST_PRINTF printf


// Application specific item system structure
// All sizes are in 4 byte words. The value does not include the item header which is two words(8 bytes).
// Different flash drivers have different alignment requirements:
// Platform         Alignment		Write buffer be rounded to Alignment value Supports unrestricted file name system
// K64/flash		8				Yes											No
// K64/fatfs_sd		1				No											Yes
// Realtek-AMEBA 	4				No											No
// Linux			1				No											Yes
// mbedOS			1				No											Yes
// So for the code to be as generic as possible we assume 8
// BUFFERS SUPPLIED TO READ AND WRITE FUNCTIONS MAY NEED TO HAVE A SIZE ROUNDED UP TO THIS VALUE (8)!!!!!
// No item should overlap a sector (4096 bytes)
static const struct  pal_cstore_item_context_t cItemContext[]=
{
	// Standard items for provisioning
	{"testKey0.txt",254,0},
	{"testKey1.txt",254,0},
	{"testKey2.txt",254,0},
	{"testKey3.txt",254,0},
	// User items for provisioning(Assets)
	{"testKey4.txt",254,1},
	// Standard items for Connector
	{"testKey5.txt",254,1},
	{"testKey6.txt",254,1},
	{"testKey7.txt",254,1},
	{"state.txt",16,2},
	// Others
	{"testKey9.txt",254,2},
	{"testKey10.txt",254,2},
	{"teststate.txt",16,2}
};
// List of sector numbers that will be erased on factory reset
static const uint8_t cSectorsToEraseOnFactoryReset[] = {0,1,2};

static struct pal_cstore_context_t s_CStoreContext = {
		cItemContext,
		// Number of items
		(sizeof(cItemContext)/sizeof(cItemContext[0])),
		// Number of sectors
		3,
		cSectorsToEraseOnFactoryReset,
		// Number of sectors to erase on factory reset
		(sizeof(cSectorsToEraseOnFactoryReset)/sizeof(cSectorsToEraseOnFactoryReset[0]))
};

struct State_t{
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
};

// In order to test persistence after reset we use a persistent state machine.
// The state is maintained in the file teststate.txt. If it does not exist we assume the Unknown
// state and perform a factory reset.
enum {Unknown, FactoryResetDone, AfterReset};
static int getTestState(void)
{
	int32_t cfsStatus;
	PAL_CFSTORE *cfstore = &pal_cfstore;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	PAL_CFSTORE_FMODE flags;
	struct State_t testState = {0 ,0 ,0 ,0 };
	size_t readCount = sizeof(struct State_t);


	memset(&flags, 0, sizeof(flags));
	flags.read = true;
	cfsStatus = cfstore->Open("teststate.txt", flags, hkey);
	if(cfsStatus == PAL_CFSTORE_OK)
	{
		// Get state
		cfsStatus = cfstore->Read(hkey, &testState, &readCount);
		if(cfsStatus != sizeof(struct State_t) || sizeof(struct State_t) !=  readCount)
			return -1;
	}
	else
	{	
		return Unknown;
	}
	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Flush();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	return testState.a;
}

static void setTestState(int state)
{
	int32_t cfsStatus;

	PAL_CFSTORE *cfstore = &pal_cfstore;
	size_t writeCount = sizeof(struct State_t);
	PAL_CFSTORE_KEYDESC keyDesc;
	PAL_CFSTORE_FMODE flags;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	struct State_t testState = {9,10, 11 ,12 };

	// Create the state file
	memset(&keyDesc, 0, sizeof(keyDesc));
	keyDesc.drl = PAL_RETENTION_NVM;
	keyDesc.flags.read = true;
	keyDesc.flags.write = true;

	if(cfstore->Create("teststate.txt", sizeof(struct State_t), &keyDesc, hkey) != PAL_CFSTORE_OK)
	{
		memset(&flags, 0, sizeof(flags));
		flags.read = true;
		flags.write = true;
		cfsStatus = cfstore->Open("teststate.txt", flags, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
	}
	testState.a = state;
	cfsStatus = cfstore->Write(hkey, (char*)&testState, &writeCount);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), writeCount);


	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Flush();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
}

TEST_GROUP(pal_cfstore);


TEST_SETUP(pal_cfstore)
{
	int32_t cfsStatus;
	PAL_CFSTORE *cfstore = &pal_cfstore;
	// _FATFS  comes from fatfs header files. It is possible also to use FATFS with the fixed file system.
	// __linux__ seems to be a good indication that compilation is for linux
#if defined(_FATFS) ||  defined(__linux__)
	// Do not use the fixed file system
	struct pal_cstore_context_t *pCStoreContext = 0;
#else
	// Use the fixed file system
	struct pal_cstore_context_t *pCStoreContext = &s_CStoreContext;
#endif
	cfsStatus = cfstore->Initialize(0, pCStoreContext);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	if(getTestState() == Unknown)
	{
		cfsStatus = cfstore->Uninitialize();
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK,cfsStatus);

		cfsStatus = pal_cfstore_factory_reset(pCStoreContext);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK,cfsStatus);

		cfsStatus = cfstore->Initialize(0, pCStoreContext);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK,cfsStatus);

		setTestState(FactoryResetDone);
	}
}

TEST_TEAR_DOWN(pal_cfstore)
{
	int32_t cfsStatus;

	PAL_CFSTORE *cfstore = &pal_cfstore;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	PAL_CFSTORE_FMODE flags;
	memset(&flags, 0, sizeof(flags));

	flags.write = true;
	cfsStatus = cfstore->Open("testKey1.txt", flags, hkey);
	if(cfsStatus == PAL_CFSTORE_OK)
	{
		cfstore->Delete(hkey);
		cfstore->Close(hkey);
	}
	cfsStatus = cfstore->Uninitialize();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK,cfsStatus);
}

// That C standard promises that the compiler will initialize to 0.
static uint8_t s_buff1[1016];
static uint8_t s_buff2[1016];

TEST(pal_cfstore, cfstoreTest)
{
	int32_t cfsStatus;

	PAL_CFSTORE *cfstore = &pal_cfstore;
	size_t writeCount = sizeof(s_buff1);
	size_t readCount = sizeof(s_buff1);
	PAL_CFSTORE_KEYDESC keyDesc;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	PAL_CFSTORE_FMODE flags;

	memset(&s_buff1, '1', sizeof(s_buff1));

	PAL_CFSTORE_CAPABILITIES capabilities = cfstore->GetCapabilities();
	TEST_ASSERT_EQUAL(0, capabilities.asynchronous_ops);
	TEST_ASSERT_EQUAL(0, capabilities.uvisor_support_enabled);

	memset(&flags, 0, sizeof(flags));
	flags.write = true;

	//cfsStatus = cfstore->Open("testKey1.txt", flags, hkey);
	//TEST_ASSERT_EQUAL(PAL_CFSTORE_DRIVER_ERROR_KEY_NOT_FOUND, cfsStatus);

	memset(&keyDesc, 0, sizeof(keyDesc));
	keyDesc.drl = PAL_RETENTION_NVM;
	keyDesc.flags.read = true;
	keyDesc.flags.write = true;

	cfsStatus = cfstore->Create("testKey1.txt", sizeof(s_buff1), &keyDesc, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	// Test a non rounded data size
	writeCount = sizeof(s_buff1)/2 -1;
	cfsStatus = cfstore->Write(hkey, (const char *)s_buff1, &writeCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1)/2 -1, cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(s_buff1)/2 -1, writeCount);

	readCount = writeCount;
	cfsStatus = cfstore->Read(hkey, s_buff2, &readCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1)/2 -1, cfsStatus);
	TEST_ASSERT_EQUAL(readCount, writeCount);
	TEST_ASSERT_EQUAL_INT8_ARRAY(s_buff1, s_buff2, readCount);

	// Test a rounded data size
	memset(&s_buff1, '1', sizeof(s_buff1));
	writeCount = sizeof(s_buff1);
	cfsStatus = cfstore->Write(hkey, (const char *)s_buff1, &writeCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), writeCount);


	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Flush();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);


	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	cfsStatus = cfstore->Open("testKey1.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
 
	cfsStatus = cfstore->GetValueLen(hkey, &readCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
	TEST_ASSERT_EQUAL(readCount, writeCount);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	cfsStatus = cfstore->Open("testKey1.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	memset(&s_buff2, '0', sizeof(s_buff2));

	cfsStatus = cfstore->Read(hkey, s_buff2, &readCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
	TEST_ASSERT_EQUAL(readCount, writeCount);
	TEST_ASSERT_EQUAL_INT8_ARRAY(s_buff1, s_buff2, readCount);

	cfsStatus = cfstore->Delete(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
}

const char* fileNames[] = {
	"testKey0.txt",
	"testKey1.txt",
	"testKey2.txt",
	"testKey3.txt",
	"testKey4.txt",
	"testKey5.txt",
	"testKey6.txt",
	"testKey7.txt",
	"state.txt",
	"testKey9.txt"
};

TEST(pal_cfstore, cfstoreTestMultiple)
{
	int32_t cfsStatus;

	PAL_CFSTORE *cfstore = &pal_cfstore;
	size_t writeCount = sizeof(s_buff1);
	size_t readCount = sizeof(s_buff1);
	PAL_CFSTORE_KEYDESC keyDesc;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	PAL_CFSTORE_FMODE flags;

	memset(&s_buff1, '2', sizeof(s_buff1));

	PAL_CFSTORE_CAPABILITIES capabilities = cfstore->GetCapabilities();
	TEST_ASSERT_EQUAL(0, capabilities.asynchronous_ops);
	TEST_ASSERT_EQUAL(0, capabilities.uvisor_support_enabled);

	// Create, write and read the files
	for (int i=0; i< 8 ; i++)
	{
		memset(&keyDesc, 0, sizeof(keyDesc));
		keyDesc.drl = PAL_RETENTION_NVM;
		keyDesc.flags.read = true;
		keyDesc.flags.write = true;

		cfsStatus = cfstore->Create(fileNames[i], sizeof(s_buff1), &keyDesc, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Write(hkey, (const char *)s_buff1, &writeCount);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), writeCount);


		cfsStatus = cfstore->Close(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Flush();
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);


		memset(&flags, 0, sizeof(flags));
		flags.read = true;

		cfsStatus = cfstore->Open(fileNames[i], flags, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->GetValueLen(hkey, &readCount);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
		TEST_ASSERT_EQUAL(readCount, writeCount);

		cfsStatus = cfstore->Close(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		memset(&flags, 0, sizeof(flags));
		flags.read = true;

		cfsStatus = cfstore->Open(fileNames[i], flags, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Read(hkey, s_buff2, &readCount);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
		TEST_ASSERT_EQUAL(readCount, writeCount);
		TEST_ASSERT_EQUAL_INT8_ARRAY(s_buff2, s_buff1, readCount);

		cfsStatus = cfstore->Close(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
	}
        // Delete the files
	for (int i=0; i< 8 ; i++)
	{
		flags.write = true;
		memset(&flags, 0, sizeof(flags));
		flags.read = true;

		cfsStatus = cfstore->Open(fileNames[i], flags, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Delete(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Close(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	}
}



TEST(pal_cfstore, cfstoreTestRewrite)
{
	int32_t cfsStatus;

	PAL_CFSTORE *cfstore = &pal_cfstore;
	size_t writeCount = sizeof(s_buff1);
	size_t readCount = sizeof(s_buff1);
	PAL_CFSTORE_KEYDESC keyDesc;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	PAL_CFSTORE_FMODE flags;

	const struct State_t state1 = {1 ,2 ,3 ,4 };
	const struct State_t state2 = {5 ,6 ,7 ,8 };
	struct State_t stateRead = {0 ,0 ,0 ,0 };

	PAL_CFSTORE_CAPABILITIES capabilities = cfstore->GetCapabilities();
	TEST_ASSERT_EQUAL(0, capabilities.asynchronous_ops);
	TEST_ASSERT_EQUAL(0, capabilities.uvisor_support_enabled);

	// Write a file after the state file
	memset(&keyDesc, 0, sizeof(keyDesc));
	keyDesc.drl = PAL_RETENTION_NVM;
	keyDesc.flags.read = true;
	keyDesc.flags.write = true;

	cfsStatus = cfstore->Create("testKey9.txt", sizeof(s_buff1), &keyDesc, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	memset(&s_buff1, '2', sizeof(s_buff1));
	cfsStatus = cfstore->Write(hkey, (const char *)s_buff1, &writeCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), writeCount);


	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Flush();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	// Create the state file
	memset(&keyDesc, 0, sizeof(keyDesc));
	keyDesc.drl = PAL_RETENTION_NVM;
	keyDesc.flags.read = true;
	keyDesc.flags.write = true;

	cfsStatus = cfstore->Create("state.txt", sizeof(struct State_t), &keyDesc, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	writeCount = sizeof(struct State_t);
	cfsStatus = cfstore->Write(hkey, (const char*)&state1, &writeCount);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), writeCount);


	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Flush();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	// Check length of state file
	cfsStatus = cfstore->Open("state.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->GetValueLen(hkey, &readCount);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), cfsStatus);
	TEST_ASSERT_EQUAL(readCount, writeCount);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	cfsStatus = cfstore->Open("state.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Read(hkey, &stateRead, &readCount);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), cfsStatus);
	TEST_ASSERT_EQUAL(readCount, writeCount);
	TEST_ASSERT_EQUAL_INT8_ARRAY(&state1, &stateRead, readCount);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	// Rewrite the state file
	memset(&flags, 0, sizeof(flags));
	flags.read = true;
	flags.write = true;

	cfsStatus = cfstore->Open("state.txt",  flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	writeCount = sizeof(struct State_t);
	cfsStatus = cfstore->Write(hkey, (const char *)&state2, &writeCount);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), writeCount);


	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Flush();
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	// Read back the state file
	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	cfsStatus = cfstore->Open("state.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Read(hkey, &stateRead, &readCount);
	TEST_ASSERT_EQUAL(sizeof(struct State_t), cfsStatus);
	TEST_ASSERT_EQUAL(readCount, writeCount);
	TEST_ASSERT_EQUAL_INT8_ARRAY(&state2, &stateRead, readCount);

	cfsStatus = cfstore->Delete(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);   
        
	// Check that the file after it is still ok
	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	cfsStatus = cfstore->Open("testKey9.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->GetValueLen(hkey, &readCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), readCount);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	memset(&flags, 0, sizeof(flags));
	flags.read = true;

	cfsStatus = cfstore->Open("testKey9.txt", flags, hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Read(hkey, s_buff2, &readCount);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
	TEST_ASSERT_EQUAL(sizeof(s_buff1), readCount);
	TEST_ASSERT_EQUAL_INT8_ARRAY(s_buff2, s_buff1, readCount);

	cfsStatus = cfstore->Delete(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

	cfsStatus = cfstore->Close(hkey);
	TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
}



TEST(pal_cfstore, cfstorePersistantAfterReset)
{
	int32_t cfsStatus;

	PAL_CFSTORE *cfstore = &pal_cfstore;
	size_t writeCount = sizeof(s_buff1);
	size_t readCount = sizeof(s_buff1);
	PAL_CFSTORE_KEYDESC keyDesc;
	PAL_CFSTORE_HANDLE_INIT(hkey);
	PAL_CFSTORE_FMODE flags;

	memset(&s_buff1, '5', sizeof(s_buff1));

	if(getTestState() == FactoryResetDone)
	{
		// Write a file after the state file
		memset(&keyDesc, 0, sizeof(keyDesc));
		keyDesc.drl = PAL_RETENTION_NVM;
		keyDesc.flags.read = true;
		keyDesc.flags.write = true;

		cfsStatus = cfstore->Create("testKey10.txt", sizeof(s_buff1), &keyDesc, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Write(hkey, (const char *)s_buff1, &writeCount);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), writeCount);

		cfsStatus = cfstore->Close(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Flush();
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		setTestState(AfterReset);

		TEST_pal_cfstore_TEAR_DOWN();

		// Resets the device. The test will not be marked as passed until it is rerun
		// after the reset.
		pal_osReboot();
	}
	else if(getTestState() == AfterReset)
	{
		// Read a file after the state file
		cfsStatus = cfstore->Open("testKey10.txt", flags, hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Read(hkey, s_buff2, &readCount);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), cfsStatus);
		TEST_ASSERT_EQUAL(sizeof(s_buff1), readCount);
		TEST_ASSERT_EQUAL_INT8_ARRAY(s_buff1, s_buff2, readCount);

		cfsStatus = cfstore->Delete(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);

		cfsStatus = cfstore->Close(hkey);
		TEST_ASSERT_EQUAL(PAL_CFSTORE_OK, cfsStatus);
		setTestState(Unknown);
	}

}
