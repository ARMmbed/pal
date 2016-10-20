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

#include "pal_rtos.h"
#include "unity.h"
#include "unity_fixture.h"
#include "pal_rtos_test_utils.h"
#include "pal.h"
#include "pal_rtos_test_utils.h"

TEST_GROUP(pal_rtos);

//sometimes you may want to get at local data in a module.
//for example: If you plan to pass by reference, this could be useful
//however, it should often be avoided
//extern int Counter;
uint32_t g_threadStorage[20] = { 0 };
threadsArgument_t g_threadsArg = {0};
timerArgument_t g_timerArgs = {0};
palMutexID_t mutex1 = NULLPTR;
palMutexID_t mutex2 = NULLPTR;
palSemaphoreID_t semaphore1 = NULLPTR;

//forward declarations
void palRunThreads();


TEST_SETUP(pal_rtos)
{
  //This is run before EACH TEST
  //Counter = 0x5a5a;
}

TEST_TEAR_DOWN(pal_rtos)
{
}

TEST(pal_rtos, pal_osKernelSysTick_Unity)
{
  uint32_t tick1 = 0, tick2 = 0;
  tick1 = pal_osKernelSysTick();
  tick2 = pal_osKernelSysTick();
  
  TEST_ASSERT_TRUE(tick2 != tick1);
}

TEST(pal_rtos, pal_osKernelSysTick64_Unity)
{
  uint64_t tick1 = 0, tick2 = 0;
  
  tick1 = pal_osKernelSysTick64();
  tick2 = pal_osKernelSysTick64();
  
  TEST_ASSERT_TRUE(tick2 > tick1);
}

TEST(pal_rtos, pal_osKernelSysTickMicroSec_Unity)
{
  uint64_t tick = 0;
  uint64_t microSec = 2000 * 1000;

  tick = pal_osKernelSysTickMicroSec(microSec);
  TEST_ASSERT_TRUE(0 != tick);
}

TEST(pal_rtos, pal_osKernelSysMilliSecTick_Unity)
{
  uint64_t tick = 0;
  uint64_t microSec = 2000 * 1000;
  uint64_t milliseconds = 0;
  
  tick = pal_osKernelSysTickMicroSec(microSec);
  TEST_ASSERT_TRUE(0 != tick);
  
  milliseconds = pal_osKernelSysMilliSecTick(tick);
  TEST_ASSERT_EQUAL(milliseconds, microSec/1000);
}


TEST(pal_rtos, pal_osKernelSysTickFrequency_Unity)
{
  uint64_t frequency = 0;

  frequency = pal_osKernelSysTickFrequency();
  
  TEST_ASSERT_TRUE(frequency > 0);
}

TEST(pal_rtos, pal_osDelay_Unity)
{
  palStatus_t status = PAL_SUCCESS;
  uint32_t tick1 , tick2;

  tick1 = pal_osKernelSysTick();
  status = pal_osDelay(200);
  tick2 = pal_osKernelSysTick();

  TEST_ASSERT_TRUE(tick2 > tick1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
}

TEST(pal_rtos, BasicTimeScenario)
{
  palStatus_t status = PAL_SUCCESS;
  uint32_t tick, tick1 , tick2 , index, tickDiff, tickDelta;
  uint32_t tmp = 0;

  tick1 = pal_osKernelSysTick();
  for(index = 0 ; index < 2000 ; ++index)
      ++tmp;
  tick2 = pal_osKernelSysTick();
  
  TEST_ASSERT_TRUE(tick1 != tick2);
  TEST_ASSERT_TRUE(tick2 > tick1);  // to check that the tick counts are incremantal - be aware of wrap-arounds

  /****************************************/
  tick1 = pal_osKernelSysTick();
  status = pal_osDelay(2000);
  tick2 = pal_osKernelSysTick();
  
  TEST_ASSERT_TRUE(tick1 != tick2);
  TEST_ASSERT_TRUE(tick2 > tick1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  
  tickDiff = tick2 - tick1;
  tick = pal_osKernelSysTickMicroSec(2000 * 1000);
  // 10 milliseconds delta
  tickDelta = pal_osKernelSysTickMicroSec(10 * 1000);
  TEST_ASSERT_TRUE((tick - tickDelta < tickDiff) && (tickDiff < tick + tickDelta));
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
}

TEST(pal_rtos, TimerUnityTest)
{ 
  palStatus_t status = PAL_SUCCESS;
  palTimerID_t timerID1 = NULLPTR;
  palTimerID_t timerID2 = NULLPTR;
  status = pal_init();
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osTimerCreate(palTimerFunc1, NULL, palOsTimerOnce, &timerID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);  

  status = pal_osTimerCreate(palTimerFunc2, NULL, palOsTimerPeriodic, &timerID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);  

  g_timerArgs.ticksBeforeTimer = pal_osKernelSysTick();
  status = pal_osTimerStart(timerID1, 1000);
  TEST_PRINTF("ticks before Timer: 0 - %d\n", g_timerArgs.ticksBeforeTimer);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  g_timerArgs.ticksBeforeTimer = pal_osKernelSysTick();
  status = pal_osTimerStart(timerID2, 1000);
  TEST_PRINTF("ticks before Timer: 1 - %d\n", g_timerArgs.ticksBeforeTimer);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osDelay(1500);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osTimerStop(timerID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osTimerDelete(&timerID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  TEST_ASSERT_EQUAL(NULL, timerID1);

  status = pal_osTimerDelete(&timerID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  TEST_ASSERT_EQUAL(NULL, timerID2);
}

TEST(pal_rtos, PrimitivesUnityTest1)
{
    palStatus_t status = PAL_SUCCESS;
    status = pal_init(NULL);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

    status = pal_osMutexCreate(&mutex1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

    status = pal_osMutexCreate(&mutex2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

    status = pal_osSemaphoreCreate(1 ,&semaphore1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

    palRunThreads();

    //! sleep for 10 seconds to let the threads finish their functions
    status = pal_osDelay(10000);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

    status = pal_osSemaphoreDelete(&semaphore1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
    TEST_ASSERT_EQUAL(NULL, semaphore1);

    status = pal_osMutexDelete(&mutex1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
    TEST_ASSERT_EQUAL(NULL, mutex1);

    status = pal_osMutexDelete(&mutex2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
    TEST_ASSERT_EQUAL(NULL, mutex2);

}

TEST(pal_rtos, PrimitivesUnityTest2)
{
  palStatus_t status = PAL_SUCCESS;
  int32_t tmp = 0;
  palThreadID_t threadID = NULLPTR;
  uint32_t stack1; //we have small stack just to pass NON-NULL paramter

//Check Thread parameter validation
  status = pal_osThreadCreate(NULL, NULL, PAL_osPriorityIdle, 1024, &stack1, NULL, &threadID);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osThreadCreate(palThreadFunc1, NULL, PAL_osPriorityError, 1024, &stack1, NULL, &threadID);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osThreadCreate(palThreadFunc1, NULL, PAL_osPriorityIdle, 0, &stack1, NULL, &threadID);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osThreadCreate(palThreadFunc1, NULL, PAL_osPriorityIdle, 1024, NULL, NULL, &threadID);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osThreadCreate(palThreadFunc1, NULL, PAL_osPriorityIdle, 1024, &stack1, NULL, NULL);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

//Check Semaphore parameter validation
  status = pal_osSemaphoreCreate(1 ,NULL);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osSemaphoreDelete(NULL);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osSemaphoreWait(NULLPTR, 1000, &tmp);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);

  status = pal_osSemaphoreWait(tmp, 1000, NULL);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);
  
  status = pal_osSemaphoreRelease(NULLPTR);
  TEST_ASSERT_EQUAL(PAL_ERR_INVALID_ARGUMENT, status);
}

TEST(pal_rtos, MemoryPoolUnityTest)
{
  palStatus_t status = PAL_SUCCESS;
  palMemoryPoolID_t poolID1 = NULLPTR;
  palMemoryPoolID_t poolID2 = NULLPTR;
  uint8_t* ptr1[MEMORY_POOL1_BLOCK_COUNT] = {0};
  uint8_t* ptr2[MEMORY_POOL2_BLOCK_COUNT] = {0};

  status = pal_init(NULL);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osPoolCreate(MEMORY_POOL1_BLOCK_SIZE, MEMORY_POOL1_BLOCK_COUNT, &poolID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osPoolCreate(MEMORY_POOL2_BLOCK_SIZE, MEMORY_POOL2_BLOCK_COUNT, &poolID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  for(uint8_t block1 = 0 ; block1 < MEMORY_POOL1_BLOCK_COUNT; ++block1)
  {
    ptr1[block1] = pal_osPoolAlloc(poolID1);
    TEST_ASSERT_NOT_EQUAL(ptr1[block1], NULL);
  }
  for(uint8_t block2 = 0 ; block2 < MEMORY_POOL2_BLOCK_COUNT; ++block2)
  {
    ptr2[block2] = pal_osPoolCAlloc(poolID2);
    TEST_ASSERT_NOT_EQUAL(ptr2[block2], NULL);
  }

  for(uint8_t freeblock1 = 0; freeblock1 < MEMORY_POOL1_BLOCK_COUNT; ++freeblock1)
  {
    status = pal_osPoolFree(poolID1, ptr1[freeblock1]);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  }

  for(uint8_t freeblock2 = 0; freeblock2 < MEMORY_POOL2_BLOCK_COUNT; ++freeblock2)
  {
    status = pal_osPoolFree(poolID2, ptr2[freeblock2]);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  }

  status = pal_osPoolDestroy(&poolID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  TEST_ASSERT_EQUAL(poolID1, NULL);
  status = pal_osPoolDestroy(&poolID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  TEST_ASSERT_EQUAL(poolID2, NULL);
}


TEST(pal_rtos, MessageUnityTest)
{
  palStatus_t status = PAL_SUCCESS;
  palMessageQID_t messageQID = NULLPTR;
  uint32_t infoToSend = 3215;
  uint32_t infoToGet = 0;

  status = pal_init(NULL);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osMessageQueueCreate(10, &messageQID);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osMessagePut(messageQID, infoToSend, 1500);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osMessageGet(messageQID, 1500, &infoToGet);

  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  TEST_ASSERT_EQUAL_UINT32(infoToSend, infoToGet);

  status = pal_osMessageQueueDestroy(&messageQID);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  TEST_ASSERT_EQUAL(messageQID, NULL);
}

TEST(pal_rtos, AtomicIncrementUnityTest)
{
  int32_t num1 = 0;
  int32_t increment = 10;
  int32_t tmp = 0;
  int32_t original = num1;

  tmp = pal_osAtomicIncrement(&num1, increment);

  
  TEST_ASSERT_EQUAL(original + increment, tmp);
  
}

TEST(pal_rtos, pal_init_test)
{
  palStatus_t status = PAL_SUCCESS;
  
  status = pal_init();
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_init();
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_init();
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  pal_destroy();
    
  pal_destroy();
    
  pal_destroy();
    
  pal_destroy();
}

TEST(pal_rtos, CustomizedTest)
{
  palStatus_t status = PAL_SUCCESS;
  palThreadID_t threadID1 = NULLPTR;
  palThreadID_t threadID2 = NULLPTR;
  uint32_t *stack1 = (uint32_t*)malloc(sizeof(uint32_t) * 512);
  uint32_t *stack2 = (uint32_t*)malloc(sizeof(uint32_t) * 512);

  
  status = pal_osThreadCreate(palThreadFuncCustom1, NULL, PAL_osPriorityAboveNormal, 1024, stack1, NULL, &threadID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osThreadCreate(palThreadFuncCustom2, NULL, PAL_osPriorityHigh, 1024, stack2, NULL, &threadID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  pal_osDelay(3000);

  status = pal_osThreadTerminate(&threadID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osThreadTerminate(&threadID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);


  status = pal_osThreadCreate(palThreadFuncCustom1, NULL, PAL_osPriorityAboveNormal, 1024, stack1, NULL, &threadID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osThreadCreate(palThreadFuncCustom2, NULL, PAL_osPriorityHigh, 1024, stack2, NULL, &threadID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  pal_osDelay(3000);
  status = pal_osThreadTerminate(&threadID1);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);

  status = pal_osThreadTerminate(&threadID2);
  TEST_ASSERT_EQUAL(PAL_SUCCESS, status);
  pal_osDelay(500);

  free(stack1);
  free(stack2);
  
}

