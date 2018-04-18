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

#include "unity.h"
#include "unity_fixture.h"


// pal Socket API tests
TEST_GROUP_RUNNER(pal_cfstore)
{
#if (PAL_RUN_ALL_TESTS || cfstorePersistantAfterReset)
	RUN_TEST_CASE(pal_cfstore, cfstorePersistantAfterReset);
#endif
#if (PAL_RUN_ALL_TESTS || cfstoreTest)
	RUN_TEST_CASE(pal_cfstore, cfstoreTest);
#endif
#if (PAL_RUN_ALL_TESTS || cfstoreTestMultiple)
	RUN_TEST_CASE(pal_cfstore, cfstoreTestMultiple);
#endif
#if (PAL_RUN_ALL_TESTS || cfstoreTestRewrite)
	RUN_TEST_CASE(pal_cfstore, cfstoreTestRewrite);
#endif
}

// Each of these should be in a separate file.


// pal OS API tests
//TEST_GROUP_RUNNER(pal_OS)
//{
//}

/* Run all the tests
void RunAllTests(void)
{
	  RUN_TEST_GROUP(pal_socket);
	  //RUN_TEST_GROUP(pal_OS);
}*/

