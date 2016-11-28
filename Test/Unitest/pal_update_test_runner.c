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


// pal Update API tests
TEST_GROUP_RUNNER(pal_update)
{

#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_start)
  RUN_TEST_CASE(pal_update, pal_update_start);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_writeSmallChunk_5b)
//  RUN_TEST_CASE(pal_update, pal_update_writeSmallChunk_5b);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_writeUnaligned_1001b)
 // RUN_TEST_CASE(pal_update, pal_update_writeUnaligned_1001b);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_1k)
//  RUN_TEST_CASE(pal_update, pal_update_1k);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_2k)
//  RUN_TEST_CASE(pal_update, pal_update_2k);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_4k)
  RUN_TEST_CASE(pal_update, pal_update_4k);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_8k)
//  RUN_TEST_CASE(pal_update, pal_update_8k);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_16k)
//  RUN_TEST_CASE(pal_update, pal_update_16k);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_Read)
  RUN_TEST_CASE(pal_update, pal_update_Read);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_stressTest)
//  RUN_TEST_CASE(pal_update, pal_update_stressTest);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_4k_write_1k_4_times)
  RUN_TEST_CASE(pal_update, pal_update_4k_write_1k_4_times);
#endif
#if (PAL_INCLUDE || BASIC_UPDATE_UNITY_TESTS || pal_update_getActiveHash)
//  RUN_TEST_CASE(pal_update, pal_update_getActiveHash);
#endif
}

