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

#include "pal_update.h"
#include "pal.h"
#include "unity.h"
#include "unity_fixture.h"
#include "pal_test_utils.h"
#include "string.h"


#define KILOBYTE 1024

TEST_GROUP(pal_update);


palBuffer_t g_writeBuffer = {0};
palBuffer_t g_readBuffer = {0};
palImageHeaderDeails_t g_imageHeader = {0};
uint8_t g_isTestDone;


uint8_t numberofBlocks = 0;


TEST_SETUP(pal_update)
{
    TEST_PRINTF("running new test\r\n");

}

typedef enum _updateTestState
{
    test_init = 1,
    test_write,
    test_commit,
    test_read
}updateTestState;

static void stateAdvance(palImageEvents_t state)
{
    TEST_PRINTF("finished event %d\r\n",state);
    state++;
    TEST_PRINTF("starting event %d\r\n",state);
    int rc = PAL_SUCCESS;
    TEST_PRINTF("write ptr = (%p - %p) read ptr = (%p - %p)\r\n",
            g_writeBuffer.buffer,g_writeBuffer.buffer + g_writeBuffer.maxBufferLength,
            g_readBuffer.buffer, g_readBuffer.buffer + g_readBuffer.maxBufferLength);
    switch (state)
    {
    case PAL_IMAGE_EVENT_PREPARE:
          rc = pal_imagePrepare(1,&g_imageHeader);
          TEST_PRINTF("pal_imagePrepare returned %d \r\n",rc);
          break;
    case PAL_IMAGE_EVENT_WRITE:
          rc = pal_imageWrite(1,0,(palConstBuffer_t*)&g_writeBuffer);
          TEST_PRINTF("pal_imageWrite returned %d \r\n",rc);
          TEST_ASSERT_TRUE(rc >= 0);
          break;
    case PAL_IMAGE_EVENT_FINALIZE:
          rc = pal_imageFinalize(1);
          TEST_PRINTF("pal_imageFinalize returned %d \r\n",rc);
          TEST_ASSERT_TRUE(rc >= 0);
          break;
    case PAL_IMAGE_EVENT_READTOBUFFER:
          rc = pal_imageReadToBuffer(1,0,&g_readBuffer);
          TEST_ASSERT_TRUE(rc >= 0);
          TEST_PRINTF("pal_imageReadToBuffer  with offset %d return %d \r\n",0,rc);
          break;
    case PAL_IMAGE_EVENT_ACTIVATE:
          TEST_PRINTF("Checking the output\r\n");
          TEST_PRINTF("\r\ng_readBuffer bufferLength=%d\r\n",g_readBuffer.maxBufferLength);

          TEST_ASSERT_TRUE(!memcmp(g_readBuffer.buffer,g_writeBuffer.buffer,g_readBuffer.maxBufferLength));
          TEST_PRINTF("write ptr = %p read ptr = %p\r\n",g_writeBuffer.buffer,g_readBuffer.buffer);
          free(g_readBuffer.buffer);
          free(g_writeBuffer.buffer);
          g_isTestDone = 1;
          break;
    default:
        TEST_PRINTF("error this should not happen\r\n");
        TEST_PRINTF("write ptr = %p read ptr = %p\r\n",g_writeBuffer.buffer,g_readBuffer.buffer);
        free(g_readBuffer.buffer);
        free(g_writeBuffer.buffer);
        g_isTestDone = 1;
    }
}






void printBuffer(uint8_t* buffer, size_t bufSize)
{
    size_t i = 0;
    TEST_PRINTF("0x");
    for (i=0;i < bufSize;i++)
    {
        TEST_PRINTF("%x",buffer[i]);
    }
    TEST_PRINTF("\r\n");
}


static void fillBuffer(uint8_t* buffer, size_t bufSize)
{
    size_t  i = 0;
    uint8_t value = 0;
    uint8_t step = -1;
    TEST_PRINTF("filling buffer size %d\r\n",bufSize);
    for(i=0; i < bufSize ; i++)
    {
        buffer[i] = value;
        if ((0 == value) || (255 == value))
        {
            step*=-1;
        }
        value+=step;
    }
    TEST_PRINTF("buffer is full\r\n");

}


TEST_TEAR_DOWN(pal_update)
{
}

TEST(pal_update, pal_update_start)
{
  uint32_t test = 0;

  TEST_ASSERT_TRUE(test == 0);
}

TEST(pal_update, pal_update_init)
{
  palStatus_t rc = PAL_SUCCESS;

  uint8_t writeData[] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x20,0x21,0x22,0x23,0x24,0x25,0x26, 0x27, 0x28, 0x29, 0x30, 0x30, 0x31, 0x32};

  uint8_t readData[8] = {0};

  uint64_t version = 11111111;
  uint32_t hash    = 0x22222222;



  g_writeBuffer.buffer = writeData;
  g_writeBuffer.bufferLength = sizeof(writeData);
  g_writeBuffer.maxBufferLength = sizeof(writeData);

  TEST_PRINTF("write buffer length %ld max length %ld\r\n",g_writeBuffer.bufferLength,g_writeBuffer.maxBufferLength);
  printBuffer(g_writeBuffer.buffer,g_writeBuffer.bufferLength);
  g_readBuffer.buffer = readData;
  g_readBuffer.maxBufferLength = 8;

  g_imageHeader.version = version;

  g_imageHeader.hash.buffer =(uint8_t*)&hash;
  g_imageHeader.hash.bufferLength = sizeof(hash);
  g_imageHeader.hash.maxBufferLength = sizeof(hash);

  g_imageHeader.imageSize = 32;
  TEST_PRINTF("size = %d\r\n",g_imageHeader.imageSize );


  rc = pal_imageInitAPI(stateAdvance);
  TEST_PRINTF("pal_imageInitAPI returned %ld \r\n",rc);
  TEST_ASSERT_TRUE(rc >= 0);

  TEST_ASSERT_TRUE(rc >= 0);
  TEST_PRINTF("writing to image number %d  %d bytes values\r\n",1, g_writeBuffer.bufferLength);
  printBuffer(g_writeBuffer.buffer, g_writeBuffer.bufferLength);
  rc = pal_imageWrite(1,10,(palConstBuffer_t*)&g_writeBuffer);
  TEST_PRINTF("pal_imageWrite returned %ld \r\n",rc);
  TEST_ASSERT_TRUE(rc >= 0);
  rc = pal_imageFinalize(1);
  TEST_PRINTF("pal_imageFinalize returned %ld \r\n",rc);
  TEST_ASSERT_TRUE(rc >= 0);
  rc = pal_imageReadToBuffer(1,0,&g_readBuffer);
  TEST_PRINTF("pal_imageReadToBuffer  with offset %ld return %ld \r\n",0,rc);
  printBuffer(g_readBuffer.buffer, 8);
  TEST_ASSERT_TRUE(rc >= 0);
  TEST_ASSERT_TRUE(!memcmp(g_readBuffer.buffer,writeData,8));
  rc = pal_imageReadToBuffer(1,16,&g_readBuffer);
  TEST_PRINTF("pal_imageReadToBuffer  with offset %ld return %ld \r\n",16,rc);
  printBuffer(g_readBuffer.buffer, 8);
  TEST_ASSERT_TRUE(rc >= 0);
  TEST_ASSERT_TRUE(!memcmp(g_readBuffer.buffer,&writeData[16],8));
}


void pal_update_xK(int sizeInK)
{

  palStatus_t rc = PAL_SUCCESS;
  if (!(sizeInK % KILOBYTE))
  {
      TEST_PRINTF("\n-====== PAL_UPDATE_%dKb ======- \n",sizeInK / KILOBYTE);
  }
  else
  {
      TEST_PRINTF("\n-====== PAL_UPDATE_%db ======- \n",sizeInK);
  }
  uint8_t *writeData = (uint8_t*)malloc(sizeInK);
  uint8_t *readData  = (uint8_t*)malloc(sizeInK);

  TEST_ASSERT_TRUE(writeData != NULL);
  TEST_ASSERT_TRUE(readData != NULL);

  uint64_t version = 11111111;
  uint32_t hash    = 0x22222222;

  g_isTestDone = 0;

  g_imageHeader.version = version;

  g_imageHeader.hash.buffer =(uint8_t*)&hash;
  g_imageHeader.hash.bufferLength = sizeof(hash);
  g_imageHeader.hash.maxBufferLength = sizeof(hash);

  g_imageHeader.imageSize = sizeInK;

  g_writeBuffer.buffer = writeData;
  g_writeBuffer.bufferLength = sizeInK;
  g_writeBuffer.maxBufferLength = sizeInK;

  TEST_PRINTF("write buffer length %d max length %ld\r\n",g_writeBuffer.bufferLength,g_writeBuffer.maxBufferLength);
  fillBuffer(g_writeBuffer.buffer,g_writeBuffer.bufferLength);

  g_readBuffer.buffer = readData;
  g_readBuffer.maxBufferLength = sizeInK;


  rc =pal_imageInitAPI(stateAdvance);
  TEST_PRINTF("pal_imageInitAPI returned %ld \r\n",rc);
  TEST_ASSERT_TRUE(rc >= 0);

  /*wait until the a-sync test will finish*/
  while (!g_isTestDone)
      pal_osDelay(5); //this to make the OS to switch context

}

TEST(pal_update, pal_update_1k)
{
    pal_update_xK(1*KILOBYTE);
}

TEST(pal_update, pal_update_2k)
{
    pal_update_xK(2*KILOBYTE);
}

TEST(pal_update, pal_update_4k)
{
    pal_update_xK(4*KILOBYTE);
}

TEST(pal_update, pal_update_8k)
{
    pal_update_xK(8*KILOBYTE);
}

TEST(pal_update, pal_update_16k)
{
    pal_update_xK(16*KILOBYTE);
}

TEST(pal_update,pal_update_writeSmallChunk_5b)
{
    pal_update_xK(5);
}

TEST(pal_update,pal_update_writeUnaligned_1001b)
{
    //1039 is a prime number so probebly never aligned
    pal_update_xK(1039);
}




static void multiWriteMultiRead(palImageEvents_t state)
{
    int rc = PAL_SUCCESS;
    static uint8_t counter = 0;
    static uint8_t *writeBase = NULL;
    static uint8_t *readBase = NULL;
    if (NULL == writeBase)
    {
        writeBase = g_writeBuffer.buffer;
        g_writeBuffer.maxBufferLength = g_writeBuffer.maxBufferLength/numberofBlocks;
        g_writeBuffer.bufferLength = g_writeBuffer.bufferLength/numberofBlocks;

        readBase = g_readBuffer.buffer;
        g_readBuffer.maxBufferLength = g_readBuffer.maxBufferLength/numberofBlocks;
    }
    TEST_PRINTF("finished event %d\r\n",state);
    if (PAL_IMAGE_EVENT_WRITE == state) // just wrote data
    {
        counter++;
        if (numberofBlocks == counter) // wrote all needed blocks
        {
            state++; // advance to next state
            counter = 0; //init counter
        }
    }
    else if (PAL_IMAGE_EVENT_READTOBUFFER == state) // just read data
    {
        counter++ ;
        if (numberofBlocks == counter) // read all needed blocks
        {
            state++; // advance to next state
            counter = 0;
        }
    }
    else
    {
        state++; // advance to next state
    }

    TEST_PRINTF("starting event %d\r\n",state);

    switch (state)
    {
    case PAL_IMAGE_EVENT_PREPARE:
          rc = pal_imagePrepare(1,&g_imageHeader);
          TEST_PRINTF("pal_imagePrepare returned %d \r\n",rc);
          break;
    case PAL_IMAGE_EVENT_WRITE:
          TEST_PRINTF("write KILOBYTE * %d = %d\r\n",counter,KILOBYTE*(counter));
          g_writeBuffer.buffer = &writeBase[KILOBYTE*(counter)];// writing 1k every time
          rc = pal_imageWrite(1,KILOBYTE*(counter),(palConstBuffer_t*)&g_writeBuffer);
          TEST_PRINTF("pal_imageWrite returned %d \r\n",rc);
          TEST_ASSERT_TRUE(rc >= 0);
          break;
    case PAL_IMAGE_EVENT_FINALIZE:
          rc = pal_imageFinalize(1);
          TEST_PRINTF("pal_imageFinalize returned %d \r\n",rc);
          TEST_ASSERT_TRUE(rc >= 0);
          break;
    case PAL_IMAGE_EVENT_READTOBUFFER:
          TEST_PRINTF("read KILOBYTE * %d = %d\r\n",counter, KILOBYTE*(counter));
          g_readBuffer.buffer = &readBase[KILOBYTE*(counter)];// writing 1k every time
          rc = pal_imageReadToBuffer(1,KILOBYTE*(counter),&g_readBuffer);
          TEST_PRINTF("pal_imageReadToBuffer  with offset %d return %d \r\n",0,rc);
          break;
    case PAL_IMAGE_EVENT_ACTIVATE:

          TEST_PRINTF("Checking the output\r\n");

          TEST_ASSERT_TRUE(rc >= 0);
          TEST_ASSERT_TRUE(!memcmp(readBase,writeBase,numberofBlocks*KILOBYTE));
          free(writeBase);
          free(readBase);
          g_isTestDone = 1;
          break;
    default:
        TEST_PRINTF("error\r\n");
        free(writeBase);
        free(readBase);
        g_isTestDone = 1;
        break;
    }
}



TEST(pal_update, pal_update_4k_write_1k_4_times)
{
    printf("\r\nit=%d\r\n",5);
  palStatus_t rc = PAL_SUCCESS;

  uint8_t *writeData = (uint8_t*)malloc(4*KILOBYTE);
  uint8_t *readData  = (uint8_t*)malloc(4*KILOBYTE);


  TEST_ASSERT_TRUE(writeData != NULL);
  TEST_ASSERT_TRUE(readData != NULL);

  uint64_t version = 11111111;
  uint32_t hash    = 0x22222222;
  g_isTestDone = 0;

  g_imageHeader.version = version;

  g_imageHeader.hash.buffer =(uint8_t*)&hash;
  g_imageHeader.hash.bufferLength = sizeof(hash);
  g_imageHeader.hash.maxBufferLength = sizeof(hash);

  g_imageHeader.imageSize = 4*KILOBYTE;

  fillBuffer(writeData,4*KILOBYTE);

  g_writeBuffer.buffer = writeData;
  g_writeBuffer.bufferLength = 4*KILOBYTE;
  g_writeBuffer.maxBufferLength = 4*KILOBYTE;
  TEST_PRINTF("pal_update_4k");
  TEST_PRINTF("write buffer length %d max length %d\r\n",g_writeBuffer.bufferLength,g_writeBuffer.maxBufferLength);
  fillBuffer(g_writeBuffer.buffer,g_writeBuffer.bufferLength);

  g_readBuffer.buffer = readData;
  g_readBuffer.maxBufferLength =  4*KILOBYTE;

  numberofBlocks = 4;

  rc = pal_imageInitAPI(multiWriteMultiRead);
  TEST_PRINTF("pal_imageInitAPI returned %d \r\n",rc);
  TEST_ASSERT_TRUE(rc >= 0);
  /*wait until the a-sync test will finish*/
  while (!g_isTestDone)
      pal_osDelay(5); //this to make the OS to switch context

}


TEST(pal_update, pal_update_stressTest)
{
    uint8_t it,j;
    TEST_PRINTF("****************************************************\r\n");
    TEST_PRINTF("******* Testing multiple writes sequentially *******\r\n");
    TEST_PRINTF("****************************************************\r\n");
    for (j=0; j < 5; j++)
    {
        TEST_PRINTF("1\r\n");
        for (it = 1; it < 32; it*=2)
        {
            pal_update_xK(it*KILOBYTE);

        }
    }
}






static void readStateMachine(palImageEvents_t state)
{
    static uint8_t *readData = NULL ;
    static uint32_t bytesWasRead = 0;
    int rc = PAL_SUCCESS;
    TEST_PRINTF("finished event %d\r\n",state);
    /*if just finish reading*/
    if (PAL_IMAGE_EVENT_READTOBUFFER == state)
    {
        TEST_PRINTF("g_readBuffer.bufferLength %d\r\n",g_readBuffer.bufferLength);
        if (0 < g_readBuffer.bufferLength)
        {
            TEST_PRINTF("writing %d bytes to readData[%d]\r\n",g_readBuffer.bufferLength,bytesWasRead);
            memcpy(&readData[bytesWasRead],g_readBuffer.buffer,g_readBuffer.bufferLength);
            bytesWasRead+=g_readBuffer.bufferLength;
        }
        else
        {
            state++;
        }
    }
    else
    {
        state++;
    }
    TEST_PRINTF("starting event %d\r\n",state);
    switch (state)
    {
    case PAL_IMAGE_EVENT_PREPARE:
          bytesWasRead = 0;
          TEST_PRINTF("Allocating %d byes for test \r\n",g_writeBuffer.maxBufferLength);
          readData = (uint8_t*)malloc(g_writeBuffer.maxBufferLength);
          TEST_ASSERT_TRUE(readData != NULL);
          rc = pal_imagePrepare(1,&g_imageHeader);
          TEST_PRINTF("pal_imagePrepare returned %d \r\n",rc);
          break;
    case PAL_IMAGE_EVENT_WRITE:
          rc = pal_imageWrite(1,0,(palConstBuffer_t*)&g_writeBuffer);
          TEST_PRINTF("pal_imageWrite returned %d \r\n",rc);
          TEST_ASSERT_TRUE(rc >= 0);
          break;
    case PAL_IMAGE_EVENT_FINALIZE:
          rc = pal_imageFinalize(1);
          TEST_PRINTF("pal_imageFinalize returned %d \r\n",rc);
          TEST_ASSERT_TRUE(rc >= 0);
          break;
    case PAL_IMAGE_EVENT_READTOBUFFER:
          g_readBuffer.bufferLength = 0;
          memset(g_readBuffer.buffer,0,g_readBuffer.maxBufferLength);
          rc = pal_imageReadToBuffer(1,0,&g_readBuffer);
          TEST_PRINTF("pal_imageReadToBuffer  with offset %d return %d \r\n",0,rc);
          //TEST_ASSERT_TRUE((rc >= 0) || (rc != -10));
          break;
    case PAL_IMAGE_EVENT_ACTIVATE:
          TEST_PRINTF("Checking the output\r\n");
          TEST_PRINTF("\r\ng_readBuffer bufferLength=%d\r\n",g_readBuffer.maxBufferLength);
          TEST_ASSERT_TRUE(!memcmp(readData,g_writeBuffer.buffer,g_writeBuffer.bufferLength));
          TEST_PRINTF("write ptr = %p read ptr = %p\r\n",g_writeBuffer.buffer,g_readBuffer.buffer);
          free(g_readBuffer.buffer);
          free(g_writeBuffer.buffer);
          free(readData);
          g_isTestDone = 1;
          break;
    default:
        TEST_PRINTF("error this should not happen\r\n");
        TEST_PRINTF("write ptr = %p read ptr = %p\r\n",g_writeBuffer.buffer,g_readBuffer.buffer);
        free(g_readBuffer.buffer);
        free(g_writeBuffer.buffer);
        free(readData);
        g_isTestDone = 1;
        TEST_ASSERT_TRUE(rc >= 0);
    }
}




TEST(pal_update, pal_update_Read)
{
      uint32_t sizeIn=1500;
      palStatus_t rc = PAL_SUCCESS;
      TEST_PRINTF("\n-====== PAL_UPDATE_READ TEST %d b ======- \n",sizeIn);
      uint8_t *writeData = (uint8_t*)malloc(sizeIn);
      uint8_t *readData  = (uint8_t*)malloc(sizeIn/5);

      TEST_ASSERT_TRUE(writeData != NULL);
      TEST_ASSERT_TRUE(readData != NULL);

      uint64_t version = 11111111;
      uint32_t hash    = 0x22222222;

      g_isTestDone = 0;

      g_imageHeader.version = version;

      g_imageHeader.hash.buffer = (uint8_t*)&hash;
      g_imageHeader.hash.bufferLength = sizeof(hash);
      g_imageHeader.hash.maxBufferLength = sizeof(hash);

      g_imageHeader.imageSize = sizeIn;

      g_writeBuffer.buffer = writeData;
      g_writeBuffer.bufferLength = sizeIn;
      g_writeBuffer.maxBufferLength = sizeIn;

      TEST_PRINTF("write buffer length %d max length %ld\r\n",g_writeBuffer.bufferLength,g_writeBuffer.maxBufferLength);
      fillBuffer(g_writeBuffer.buffer,g_writeBuffer.bufferLength);

      g_readBuffer.buffer = readData;
      g_readBuffer.maxBufferLength = sizeIn/5;
      g_readBuffer.bufferLength = 0;

      rc =pal_imageInitAPI(readStateMachine);
      TEST_PRINTF("pal_imageInitAPI returned %ld \r\n",rc);
      TEST_ASSERT_TRUE(rc >= 0);

      /*wait until the a-sync test will finish*/
      while (!g_isTestDone)
          pal_osDelay(5); //this to make the OS to switch context

}





static void getActiveHashStateMachine(palImageEvents_t state)
{
    int status = PAL_SUCCESS;
    TEST_PRINTF("finished event %d\r\n",state);
    if (state == PAL_IMAGE_EVENT_INIT)
    {
        status = pal_imageGetActiveHash(&g_readBuffer);
        TEST_ASSERT_TRUE(status >= 0);
    }
    if (state == PAL_IMAGE_EVENT_GETACTIVEHASH)
    {
        printBuffer(g_readBuffer.buffer,32);
        free(g_readBuffer.buffer);
        g_isTestDone = 1;
    }
    if (state == PAL_IMAGE_EVENT_ERROR)
    {
        TEST_PRINTF("Error have occur\r\n");
        free(g_readBuffer.buffer);
        g_isTestDone = 1;
        TEST_ASSERT_TRUE(0);
    }
}






TEST(pal_update, pal_update_getActiveHash)
{
      uint8_t *hashData = malloc(32);
      int32_t status = PAL_SUCCESS;
      TEST_ASSERT_TRUE(hashData != NULL);

      g_readBuffer.buffer = hashData;
      g_readBuffer.maxBufferLength = 32;
      g_readBuffer.bufferLength = 0;

      g_isTestDone = 1;

      status = pal_imageInitAPI(getActiveHashStateMachine);
      TEST_PRINTF("pal_imageInitAPI returned %d \r\n",status);
      TEST_ASSERT_TRUE(status >= 0);

      /*wait until the a-sync test will finish*/
      while (!g_isTestDone)
          pal_osDelay(5); //this to make the OS to switch context

}


