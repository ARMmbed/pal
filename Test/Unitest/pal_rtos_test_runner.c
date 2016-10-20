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


TEST_GROUP_RUNNER(pal_rtos)
{
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || pal_osKernelSysTick_Unity)
  RUN_TEST_CASE(pal_rtos, pal_osKernelSysTick_Unity);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || pal_osKernelSysTick64_Unity)
  RUN_TEST_CASE(pal_rtos, pal_osKernelSysTick64_Unity);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || pal_osKernelSysTickMicroSec_Unity)
  RUN_TEST_CASE(pal_rtos, pal_osKernelSysTickMicroSec_Unity);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || pal_osKernelSysMilliSecTick_Unity)
  RUN_TEST_CASE(pal_rtos, pal_osKernelSysMilliSecTick_Unity);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || pal_osKernelSysTickFrequency_Unity)
  RUN_TEST_CASE(pal_rtos, pal_osKernelSysTickFrequency_Unity);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || pal_osDelay_Unity)
  RUN_TEST_CASE(pal_rtos, pal_osDelay_Unity);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || BasicTimeScenario)
  RUN_TEST_CASE(pal_rtos, BasicTimeScenario);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || TimerUnityTest)
  RUN_TEST_CASE(pal_rtos, TimerUnityTest);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || MemoryPoolUnityTest)
  RUN_TEST_CASE(pal_rtos, MemoryPoolUnityTest);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || MessageUnityTest)
  RUN_TEST_CASE(pal_rtos, MessageUnityTest);
#endif
#if (PAL_INCLUDE || BASIC_RTOS_UNITY_TESTS || AtomicIncrementUnityTest)
  RUN_TEST_CASE(pal_rtos, AtomicIncrementUnityTest);
#endif

#if (PAL_INCLUDE || PRIMITIVES_UNITY_TEST || PrimitivesUnityTest1)
  RUN_TEST_CASE(pal_rtos, PrimitivesUnityTest1);
#endif
#if (PAL_INCLUDE || PRIMITIVES_UNITY_TEST || PrimitivesUnityTest2)
  RUN_TEST_CASE(pal_rtos, PrimitivesUnityTest2);
#endif
#if (PAL_INCLUDE || PAL_INIT_REFERENCE || pal_init_test)
RUN_TEST_CASE(pal_rtos, pal_init_test);
#endif
#if (CustomizedTest)
    RUN_TEST_CASE(pal_rtos, CustomizedTest);
#endif
}

