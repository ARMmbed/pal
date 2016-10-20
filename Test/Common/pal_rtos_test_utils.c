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

#include "pal_rtos_test_utils.h"
#include "pal_rtos.h"
#include "unity_fixture.h"

#include "pal.h"

threadsArgument_t threadsArg;
timerArgument_t timerArgs;

void palThreadFunc1(void const *argument)
{
    palThreadID_t threadID = 10;
    uint32_t* threadStorage = NULL;
    threadsArgument_t *tmp = (threadsArgument_t*)argument;
#ifdef MUTEX_UNITY_TEST
    palStatus_t status = PAL_SUCCESS;
    TEST_PRINTF("palThreadFunc1::before mutex\n");
    status = pal_osMutexWait(mutex1, 100);
    TEST_PRINTF("palThreadFunc1::after mutex: 0x%08x\n", status);
    TEST_PRINTF("palThreadFunc1::after mutex (expected): 0x%08x\n", PAL_ERR_RTOS_TIMEOUT);
    TEST_ASSERT_EQUAL(PAL_ERR_RTOS_TIMEOUT, status);
    return; // for Mutex scenario, this should end here
#endif //MUTEX_UNITY_TEST

    tmp->arg1 = 10;

    threadID = pal_osThreadGetId();
    TEST_PRINTF("palThreadFunc1::Thread ID is %d\n", threadID);

    threadStorage = pal_osThreadGetLocalStore();
    if (threadStorage == g_threadStorage)
    {
        TEST_PRINTF("Thread storage updated as expected\n");    
    }
    TEST_ASSERT_EQUAL(threadStorage, g_threadStorage);
#ifdef MUTEX_UNITY_TEST
    status = pal_osMutexRelease(mutex1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //MUTEX_UNITY_TEST
    TEST_PRINTF("palThreadFunc1::STAAAAM\n");

}

void palThreadFunc2(void const *argument)
{

    palThreadID_t threadID = 10;
    threadsArgument_t *tmp = (threadsArgument_t*)argument;
#ifdef MUTEX_UNITY_TEST
    palStatus_t status = PAL_SUCCESS;
    TEST_PRINTF("palThreadFunc2::before mutex\n");
    status = pal_osMutexWait(mutex2, 300);
    TEST_PRINTF("palThreadFunc2::after mutex: 0x%08x\n", status);
    TEST_PRINTF("palThreadFunc2::after mutex (expected): 0x%08x\n", PAL_SUCCESS);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //MUTEX_UNITY_TEST

    tmp->arg2 = 20;

    threadID = pal_osThreadGetId();
    TEST_PRINTF("palThreadFunc2::Thread ID is %d\n", threadID);
#ifdef MUTEX_UNITY_TEST
    status = pal_osMutexRelease(mutex2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //MUTEX_UNITY_TEST
    TEST_PRINTF("palThreadFunc2::STAAAAM\n");
}

void palThreadFunc3(void const *argument)
{

    palThreadID_t threadID = 10;
    threadsArgument_t *tmp = (threadsArgument_t*)argument;

#ifdef SEMAPHORE_UNITY_TEST
    palStatus_t status = PAL_SUCCESS;
    uint32_t semaphoresAvailable = 10;
    status = pal_osSemaphoreWait(semaphore1, 200, &semaphoresAvailable);
    
    if (PAL_SUCCESS == status)
    {
        TEST_PRINTF("palThreadFunc3::semaphoresAvailable: %d\n", semaphoresAvailable);
        TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
    }
    else if(PAL_ERR_RTOS_TIMEOUT == status)
    {
        TEST_PRINTF("palThreadFunc3::semaphoresAvailable: %d\n", semaphoresAvailable);
        TEST_PRINTF("palThreadFunc3::status: 0x%08x\n", status);
        TEST_PRINTF("palThreadFunc3::failed to get Semaphore as expected\n", status);
        TEST_ASSERT_EQUAL(PAL_ERR_RTOS_TIMEOUT, status);
        return;
    }
    pal_osDelay(6000);
#endif //SEMAPHORE_UNITY_TEST
    tmp->arg3 = 30;
    threadID = pal_osThreadGetId();
    TEST_PRINTF("palThreadFunc3::Thread ID is %d\n", threadID);

#ifdef SEMAPHORE_UNITY_TEST
    status = pal_osSemaphoreRelease(semaphore1);
    TEST_PRINTF("palThreadFunc3::pal_osSemaphoreRelease res: 0x%08x\n", status);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //SEMAPHORE_UNITY_TEST
    TEST_PRINTF("palThreadFunc3::STAAAAM\n");
}

void palThreadFunc4(void const *argument)
{
    palThreadID_t threadID = 10;
    threadsArgument_t *tmp = (threadsArgument_t*)argument;
#ifdef MUTEX_UNITY_TEST
    palStatus_t status = PAL_SUCCESS;
    TEST_PRINTF("palThreadFunc4::before mutex\n");
    status = pal_osMutexWait(mutex1, 200);
    TEST_PRINTF("palThreadFunc4::after mutex: 0x%08x\n", status);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
    pal_osDelay(3500);  //wait 3.5 seconds to make sure that the next thread arrive to this point
#endif //MUTEX_UNITY_TEST


    tmp->arg4 = 40;

    threadID = pal_osThreadGetId();
    TEST_PRINTF("Thread ID is %d\n", threadID);



#ifdef MUTEX_UNITY_TEST
    status = pal_osMutexRelease(mutex1);
    TEST_PRINTF("palThreadFunc4::after release mutex: 0x%08x\n", status);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //MUTEX_UNITY_TEST
    TEST_PRINTF("palThreadFunc4::STAAAAM\n");
}

void palThreadFunc5(void const *argument)
{
    palThreadID_t threadID = 10;
    threadsArgument_t *tmp = (threadsArgument_t*)argument;
#ifdef MUTEX_UNITY_TEST
    palStatus_t status = PAL_SUCCESS;
    TEST_PRINTF("palThreadFunc5::before mutex\n");
    status = pal_osMutexWait(mutex1, 4500);
    TEST_PRINTF("palThreadFunc5::after mutex: 0x%08x\n", status);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //MUTEX_UNITY_TEST
    tmp->arg5 = 50;

    threadID = pal_osThreadGetId();
    TEST_PRINTF("Thread ID is %d\n", threadID);
#ifdef MUTEX_UNITY_TEST
    status = pal_osMutexRelease(mutex1);
    TEST_PRINTF("palThreadFunc5::after release mutex: 0x%08x\n", status);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
#endif //MUTEX_UNITY_TEST
    TEST_PRINTF("palThreadFunc5::STAAAAM\n");
}

void palThreadFunc6(void const *argument)
{
    palThreadID_t threadID = 10;
    threadsArgument_t *tmp = (threadsArgument_t*)argument;
#ifdef SEMAPHORE_UNITY_TEST
    palStatus_t status = PAL_SUCCESS;
    uint32_t semaphoresAvailable = 10;
    status = pal_osSemaphoreWait(123456, 200, &semaphoresAvailable);  //MUST fail, since there is no semaphore with ID=3
    TEST_PRINTF("palThreadFunc6::semaphoresAvailable: %d\n", semaphoresAvailable);
    TEST_ASSERT_EQUAL(PAL_ERR_RTOS_PARAMETER, status);
    return;
#endif //SEMAPHORE_UNITY_TEST
    tmp->arg6 = 60;

    threadID = pal_osThreadGetId();
    TEST_PRINTF("Thread ID is %d\n", threadID);
#ifdef SEMAPHORE_UNITY_TEST
    status = pal_osSemaphoreRelease(123456);
    TEST_PRINTF("palThreadFunc6::pal_osSemaphoreRelease res: 0x%08x\n", status);
    TEST_ASSERT_EQUAL(PAL_ERR_RTOS_PARAMETER, status);
#endif //SEMAPHORE_UNITY_TEST
    TEST_PRINTF("palThreadFunc6::STAAAAM\n");
}


void palTimerFunc1(void const *argument)
{
    g_timerArgs.ticksInFunc1 = pal_osKernelSysTick();
    TEST_PRINTF("ticks in palTimerFunc1: 0 - %d\n", g_timerArgs.ticksInFunc1);
    TEST_PRINTF("Once Timer function was called\n");
}

void palTimerFunc2(void const *argument)
{
    g_timerArgs.ticksInFunc2 = pal_osKernelSysTick();
    TEST_PRINTF("ticks in palTimerFunc2: 0 - %d\n", g_timerArgs.ticksInFunc2);
    TEST_PRINTF("Periodic Timer function was called\n");    
}

void palThreadFuncCustom1(void const *argument)
{
    TEST_PRINTF("palThreadFuncCustom1 was called\n");
}

void palThreadFuncCustom2(void const *argument)
{
    TEST_PRINTF("palThreadFuncCustom2 was called\n");
}

void palThreadFuncCustom3(void const *argument)
{
    TEST_PRINTF("palThreadFuncCustom3 was called\n");
}

void palThreadFuncCustom4(void const *argument)
{
    TEST_PRINTF("palThreadFuncCustom4 was called\n");
}

void palRunThreads()
{
  palStatus_t status = PAL_SUCCESS;
  palThreadID_t threadID1 = NULLPTR;
  palThreadID_t threadID2 = NULLPTR;
  palThreadID_t threadID3 = NULLPTR;
  palThreadID_t threadID4 = NULLPTR;
  palThreadID_t threadID5 = NULLPTR;
  palThreadID_t threadID6 = NULLPTR;

  uint32_t *stack1 = malloc(THREAD_STACK_SIZE);
  uint32_t *stack2 = malloc(THREAD_STACK_SIZE);
  uint32_t *stack3 = malloc(THREAD_STACK_SIZE);
  uint32_t *stack4 = malloc(THREAD_STACK_SIZE);
  uint32_t *stack5 = malloc(THREAD_STACK_SIZE);
  uint32_t *stack6 = malloc(THREAD_STACK_SIZE);

  status = pal_init(NULL);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osThreadCreate(palThreadFunc1, &g_threadsArg, PAL_osPriorityIdle, THREAD_STACK_SIZE, stack1, (palThreadLocalStore_t *)g_threadStorage, &threadID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status); 

  status = pal_osThreadCreate(palThreadFunc2, &g_threadsArg, PAL_osPriorityLow, THREAD_STACK_SIZE, stack2, NULL, &threadID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status); 

  status = pal_osThreadCreate(palThreadFunc3, &g_threadsArg, PAL_osPriorityNormal, THREAD_STACK_SIZE, stack3, NULL, &threadID3);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status); 

  status = pal_osThreadCreate(palThreadFunc4, &g_threadsArg, PAL_osPriorityBelowNormal, THREAD_STACK_SIZE, stack4, NULL, &threadID4);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status); 

  status = pal_osThreadCreate(palThreadFunc5, &g_threadsArg, PAL_osPriorityAboveNormal, THREAD_STACK_SIZE, stack5, NULL, &threadID5);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status); 

  status = pal_osThreadCreate(palThreadFunc6, &g_threadsArg, PAL_osPriorityHigh, THREAD_STACK_SIZE, stack6, NULL, &threadID6);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status); 

  
  free(stack1);
  free(stack2);
  free(stack3);
  free(stack4);
  free(stack5);
  free(stack6);
}
