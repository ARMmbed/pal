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
#include "pal_network.h"
#include "unity.h"
#include "unity_fixture.h"
#include "pal_test_utils.h"
#include "pal_socket_test_utils.h"
#include "string.h"


#define PAL_INCLUDE 1
#define PAL_TEST_THREAD_STACK_SIZE 512*sizeof(uint32_t)


TEST_GROUP(pal_socket);

//sometimes you may want to get at local data in a module.
//for example: If you plan to pass by reference, this could be useful
//however, it should often be avoided
//extern int Counter;

#define PAL_NET_SUPPORT_LWIP 1
//#define PAL_NET_TEST_SERVER_NAME "e109180-lin.kfn.arm.com"
#define PAL_NET_TEST_SERVER_NAME   "www.arm.com"
#define PAL_NET_TEST_SERVER_NAME_UDP   "8.8.8.8"


#define PAL_NET_TEST_SERVER_HTTP_PORT 80
#define PAL_NET_TEST_SERVER_UDP_PORT 53
#define PAL_NET_TEST_INCOMING_PORT 8002
#define PAL_NET_TEST_INCOMING_PORT2 8989

#define PAL_NET_TEST_LOCAL_LOOPBACK_IF_INDEX 0
void * g_networkInterface = NULL;


static uint32_t s_callbackcounter = 0;



TEST_SETUP(pal_socket)
{

    uint32_t index = 0;
    palStatus_t status = PAL_SUCCESS;
    static void * interfaceCTX = NULL;
    //This is run before EACH TEST
    if (!interfaceCTX)
    {
        status = pal_init();
        if (PAL_SUCCESS == status)
        {
            interfaceCTX = palTestGetNetWorkInterfaceContext();
            pal_registerNetworkInterface(interfaceCTX , &index);
            g_networkInterface = interfaceCTX;
        }
    }
}

TEST_TEAR_DOWN(pal_socket)
{
}

#define PAL_TEST_BUFFER_SIZE 50
static void socketCallback1()
{
    s_callbackcounter++;
}

TEST(pal_socket, socketUDPCreationOptionsTest)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocket_t sock2 = 0;
    palSocket_t sock3 = 0;
    palSocket_t sock5 = 0;
    uint32_t numInterface = 0;
    palNetInterfaceInfo_t interfaceInfo;
    uint32_t interfaceIndex = 0;
    uint32_t interfaceIndex2 = 0;
    uint32_t sockOptVal = 5000;
    uint32_t sockOptLen = sizeof(sockOptVal);

    memset(&interfaceInfo,0,sizeof(interfaceInfo));
    // check that re-adding he network interface returns the same index
    result = pal_registerNetworkInterface(g_networkInterface, &interfaceIndex);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_registerNetworkInterface(g_networkInterface, &interfaceIndex2);
    TEST_ASSERT_EQUAL(interfaceIndex, interfaceIndex2);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_getNetInterfaceInfo(interfaceIndex, &interfaceInfo);
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);
    TEST_PRINTF("default interface address: %u %u %u %u \r\n",
        (unsigned char)interfaceInfo.address.addressData[2],
        (unsigned char)interfaceInfo.address.addressData[3],
        (unsigned char)interfaceInfo.address.addressData[4],
        (unsigned char)interfaceInfo.address.addressData[5]);;


    //blocking
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, interfaceIndex, &sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, interfaceIndex, &sock2);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    //non-blocking
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, true, interfaceIndex, &sock5);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
#if    PAL_NET_ASYNCHRONOUS_SOCKET_API
    result = pal_asynchronousSocket(PAL_AF_INET, PAL_SOCK_STREAM, false, interfaceIndex, socketCallback1, &sock3);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
#endif // PAL_NET_ASYNCHRONOUS_SOCKET_API

    result = pal_getNumberOfNetInterfaces(&numInterface);
    TEST_ASSERT_NOT_EQUAL(numInterface, 0);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_setSocketOptions(sock, PAL_SO_RCVTIMEO, &sockOptVal, sockOptLen);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);


#if    PAL_NET_ASYNCHRONOUS_SOCKET_API
    result = pal_close(&sock3);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

#endif // PAL_NET_ASYNCHRONOUS_SOCKET_API

    result = pal_close(&sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_close(&sock2);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_close(&sock5);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

}


TEST(pal_socket, basicTCPclientSendRecieve)
{


    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    const char message[] = "GET / HTTP/1.0\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    palSocketLength_t addrlen = 0;

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME, &address, &addrlen);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    TEST_PRINTF("addr: %u %u %u %u \r\n",
        (unsigned char)address.addressData[2],
        (unsigned char)address.addressData[3],
        (unsigned char)address.addressData[4],
        (unsigned char)address.addressData[5]);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_HTTP_PORT);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_connect(sock, &address, 16);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_send(sock, message, sizeof(message) - 1, &sent);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_recv(sock, buffer, 99, &read);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');
    pal_close(&sock);

}

TEST(pal_socket, basicUDPclientSendRecieve)
{

    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    palSocketAddress_t address2 = { 0 };
    uint8_t buffer[33] = { 0x8e, 0xde, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x61, 0x72, 0x73, 0x74, 0x65, 0x63, 0x68, 0x6e, 0x69, 0x63, 0x61, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01 };
    uint8_t buffer_in[10];
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 0;

    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, 0, &sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME_UDP, &address, &addrlen);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_UDP_PORT);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_sendTo(sock, buffer, sizeof(buffer), &address, 16, &sent);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT_EQUAL(sent, sizeof(buffer));
    result = pal_receiveFrom(sock, buffer_in, 10, &address2, &addrlen, &read);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT_EQUAL(read, 10);
    pal_close(&sock);
}



static palSocket_t basicSocketScenario3Sock = 0;
// This is an example showing how to check for a socket that has been closed remotely.
// It causes a crash on mbed, so is not used.
#if 0
static void basicSocketScenario3Callback()
{
    char buffer[400];
    size_t read = 0;
    palStatus_t result;


    s_callbackcounter++;
    result = pal_recv(basicSocketScenario3Sock, buffer, 999, &read);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    // If 0 bytes are read it means that the peer has performed an orderly shutdown so we must close the socket
    // in order to avoid ppoll from checking it. Checking a socket whose other end has been shutdown causes ppoll to immediately return
    // with revents == 0x1.
    if(read == 0)
    {
        pal_close(&basicSocketScenario3Sock);
    }
    else
    {
        buffer[read] = '\0';
        if(s_callbackcounter == 0)
        {
            TEST_ASSERT(read >= 4);
            TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');
        }
    }

}
#endif
palSemaphoreID_t s_semaphoreID = NULLPTR;

static void socketCallback2()
{
    palStatus_t result;
    if(s_callbackcounter == 0)
    {
        result = pal_osSemaphoreRelease(s_semaphoreID);
        TEST_ASSERT_EQUAL( PAL_SUCCESS, result);
    }
    s_callbackcounter++;

}

TEST(pal_socket, basicSocketScenario3)
{
    palStatus_t result = PAL_SUCCESS;
    //palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    const char* message = "GET / HTTP/1.0\r\nHost:10.45.48.68:8000\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    s_callbackcounter = 0;
    palSocketLength_t addrlen = 0;
    int32_t countersAvailable;
    result = pal_osSemaphoreCreate(1, &s_semaphoreID);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_osSemaphoreWait(s_semaphoreID, 40000, &countersAvailable);
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);
    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME, &address, &addrlen);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);


#if PAL_NET_ASYNCHRONOUS_SOCKET_API
    result = pal_asynchronousSocket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, socketCallback2, &basicSocketScenario3Sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_HTTP_PORT);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_connect(basicSocketScenario3Sock, &address, 16);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_send(basicSocketScenario3Sock, message, 45, &sent);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    // Give a chance for the callback to be called.
    result=pal_osSemaphoreWait(s_semaphoreID, 40000,  &countersAvailable);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result=pal_osSemaphoreDelete(&s_semaphoreID);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_recv(basicSocketScenario3Sock, buffer, 99, &read);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');
    TEST_ASSERT(s_callbackcounter > 0);
    pal_close(&basicSocketScenario3Sock);
#endif // PAL_NET_ASYNCHRONOUS_SOCKET_API
}

TEST(pal_socket, basicSocketScenario4)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocket_t sock2 = 0;
    palSocket_t sock3 = 0;
    palSocketAddress_t address = { 0 };
    const char* message = "GET / HTTP/1.0\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    palSocketLength_t addlen = 0;
    uint32_t numSockets = 0;
    palSocket_t socketsToCheck[2] = { 0 };
    pal_timeVal_t tv = {0};
    uint8_t palSocketStatus[2] = { 0 };
    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, 0, &sock2);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);


    result = pal_getAddressInfo("www.arm.com", &address, &addlen);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_setSockAddrPort(&address, 80);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_connect(sock, &address, 16);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_send(sock, message, strlen(message), &sent);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    socketsToCheck[0] = sock;
    socketsToCheck[1] = sock2;
    tv.pal_tv_sec = 5;
    result = pal_socketMiniSelect(socketsToCheck, 2, &tv, palSocketStatus, &numSockets);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    if (numSockets == 0) // clean up to prevent resource leak.
    {
	pal_close(&sock);
	pal_close(&sock2);
    }
    TEST_ASSERT( 0 <  numSockets); // will currently fail on mbedOS
    TEST_ASSERT(0< palSocketStatus[0] );
    TEST_ASSERT((palSocketStatus[1] & (PAL_NET_SOCKET_SELECT_RX_BIT | PAL_NET_SOCKET_SELECT_ERR_BIT)) ==    0);



    result = pal_recv(sock, buffer, 99, &read);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');

    pal_close(&sock);

    numSockets = 0;
    palSocketStatus[0] =0;
    palSocketStatus[1] =0;
    socketsToCheck[0] = sock2;
    socketsToCheck[1] = 0;
    tv.pal_tv_sec = 1;

    result = pal_socketMiniSelect(socketsToCheck, 1, &tv, palSocketStatus, &numSockets);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT((palSocketStatus[0] & (PAL_NET_SOCKET_SELECT_RX_BIT | PAL_NET_SOCKET_SELECT_ERR_BIT)) ==    0);


    pal_close(&sock2);

    // non responsive socket connection
    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, true, 0, &sock3);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    numSockets = 0;
    palSocketStatus[0] =0;
    palSocketStatus[1] =0;
    socketsToCheck[0] = sock3;
    socketsToCheck[1] = 0;
    tv.pal_tv_sec = 1;

     result = pal_getAddressInfo("192.0.2.0", &address, &addlen); // address intended for testing not a real address - we don't expect a connection.
     TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
     result = pal_setSockAddrPort(&address, 80);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

     result = pal_connect(sock3, &address, 16);
     //TEST_ASSERT_EQUAL_HEX( PAL_ERR_SOCKET_IN_PROGRES, result); // comment back in when non-blocking connect enabled on mbedOS

    result = pal_socketMiniSelect(socketsToCheck, 1, &tv, palSocketStatus, &numSockets);

    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT( 0 ==  numSockets);
    TEST_ASSERT(0 == palSocketStatus[0] );

    pal_close(&sock3);

     result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, true, 0, &sock3);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    numSockets = 0;
    palSocketStatus[0] =0;
    palSocketStatus[1] =0;
    socketsToCheck[0] = sock3;
    socketsToCheck[1] = 0;
    tv.pal_tv_sec = 2;

     result = pal_getAddressInfo("www.arm.com", &address, &addlen); // address intended for testing not a real address - we don't expect a connection.
     TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
     result = pal_setSockAddrPort(&address, 80);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

     result = pal_connect(sock3, &address, 16);
     //TEST_ASSERT_EQUAL_HEX( PAL_ERR_SOCKET_IN_PROGRES, result);   // comment back in when non-blocking connect enabled on mbedOS

    result = pal_socketMiniSelect(socketsToCheck, 1, &tv, palSocketStatus, &numSockets);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT( 1 ==  numSockets);
    TEST_ASSERT( PAL_NET_SOCKET_SELECT_TX_BIT ==  (palSocketStatus[0]& ( PAL_NET_SOCKET_SELECT_TX_BIT) ));

   pal_close(&sock3);

}


typedef struct palNetTestThreadData{
    palSemaphoreID_t sem1;
    palSemaphoreID_t sem2;
} palNetTestThreadData_t;

char s_rcv_buffer[20] = {0};
char s_rcv_buffer2[50]  = {0};

void palNetClinetFunc(void const *argument)
{
    palStatus_t result = PAL_SUCCESS;
    int32_t tmp = 0;
    size_t sent = 0;
    size_t read = 0;
    palNetTestThreadData_t* dualSem = (palNetTestThreadData_t*)argument;
    palSocket_t sock3 = 0;
    palSocketLength_t addrlen = 16;
    //palSocketAddress_t address = { 0 };
    palNetInterfaceInfo_t interfaceInfo;
    const char* message = "GET / HTTP/1.0\r\n\r\n";

    result = pal_osSemaphoreWait(dualSem->sem1, 500, &tmp);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_getNetInterfaceInfo(PAL_NET_TEST_LOCAL_LOOPBACK_IF_INDEX, &interfaceInfo);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_setSockAddrPort(&(interfaceInfo.address), PAL_NET_TEST_INCOMING_PORT);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock3);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_connect(sock3, &(interfaceInfo.address), addrlen);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_send(sock3, message, 18, &sent);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_recv(sock3, s_rcv_buffer, 15, &read);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    TEST_PRINTF(s_rcv_buffer);

    pal_close(&sock3);

    result = pal_osSemaphoreRelease(dualSem->sem2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
}


TEST(pal_socket, ServerSocketScenario)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocket_t sock2 = 0;
    palSocketAddress_t address2 = { 0 };
    const char* messageOut = "HTTP/1.0 200 OK";
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 16;

    palSemaphoreID_t semaphoreID = NULLPTR;
    palSemaphoreID_t semaphoreID2 = NULLPTR;
    palNetTestThreadData_t dualSem = {0};
    palThreadID_t threadID1 = NULLPTR;
    int32_t tmp = 0;
    palNetInterfaceInfo_t interfaceInfo;
    memset(&interfaceInfo,0,sizeof(interfaceInfo));



    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM_SERVER, false, 0, &sock);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    //int reuseSocket = 1;
    //result = pal_setSocketOptions(sock, PAL_SO_REUSEADDR, &reuseSocket, sizeof(reuseSocket));
    //TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

        result = pal_getNetInterfaceInfo(PAL_NET_TEST_LOCAL_LOOPBACK_IF_INDEX, &interfaceInfo);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

        TEST_PRINTF("interface addr: %u %u %u %u \r\n",
        (unsigned char)interfaceInfo.address.addressData[2],
        (unsigned char)interfaceInfo.address.addressData[3],
        (unsigned char)interfaceInfo.address.addressData[4],
        (unsigned char)interfaceInfo.address.addressData[5]);;
    result = pal_setSockAddrPort(&(interfaceInfo.address), PAL_NET_TEST_INCOMING_PORT);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

     result = pal_bind(sock, &(interfaceInfo.address), interfaceInfo.addressSize);
     if (PAL_SUCCESS != result )
     {
	    // cleanup in case binding failed:
	        pal_close(&sock2);
		pal_close(&sock);
		//result = pal_osSemaphoreDelete(&semaphoreID);
		//result = pal_osSemaphoreDelete(&semaphoreID2);
     }
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);


         // start client thread to connect to the server.
    result = pal_osSemaphoreCreate(1 ,&semaphoreID);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    result = pal_osSemaphoreWait(semaphoreID, 1000, &tmp);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);


    result = pal_osSemaphoreCreate(1 ,&semaphoreID2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    result = pal_osSemaphoreWait(semaphoreID2, 1000, &tmp);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    dualSem.sem1 = semaphoreID;
    dualSem.sem2 = semaphoreID2;

    result = pal_osThreadCreate(palNetClinetFunc, &dualSem , PAL_osPriorityBelowNormal, PAL_TEST_THREAD_STACK_SIZE, NULL, NULL, &threadID1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_listen(sock, 10);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_osSemaphoreRelease(dualSem.sem1);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);


    TEST_PRINTF("waiting for connection:\r\n");
    result = pal_accept(sock, &address2, &addrlen, &sock2);
    TEST_PRINTF("after accept: %d\r\n", result);
    if (PAL_SUCCESS != result )
    {
         result = pal_accept(sock, &address2, &addrlen, &sock2);
         TEST_PRINTF("after accept: %d\r\n",result);
    }
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);

    result = pal_recv(sock2, s_rcv_buffer2, 49, &read);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    TEST_PRINTF(s_rcv_buffer2);

    result = pal_send(sock2, messageOut, 15, &sent);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);


//cleanup

    pal_close(&sock2);
    pal_close(&sock);

    result = pal_osSemaphoreWait(semaphoreID2, 5000, &tmp);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    pal_osDelay(2000);
   	pal_osThreadTerminate(&threadID1);
    result = pal_osSemaphoreDelete(&semaphoreID);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    TEST_ASSERT_EQUAL(NULL, semaphoreID);

    result = pal_osSemaphoreDelete(&semaphoreID2);
    TEST_ASSERT_EQUAL(PAL_SUCCESS, result);
    TEST_ASSERT_EQUAL(NULL, semaphoreID2);
}

TEST(pal_socket, basicSocketScenario5)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocket_t sock2 = 0;
    palSocket_t sock3 = 0;
    //palSocketAddress_t address = { 0 };
    palSocketAddress_t address2 = { 0 };
    //palIpV4Addr_t ipv4 = { 10,45,48,192 };
    char buffer[100] = { 0 };
    const char* messageOut = "HTTP/1.0 200 OK";
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 16;
    palNetInterfaceInfo_t interfaceInfo;
    uint32_t interfaceIndex = 0;

    memset(&interfaceInfo,0,sizeof(interfaceInfo));

    result = pal_registerNetworkInterface(g_networkInterface, &interfaceIndex);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM_SERVER, false, 0, &sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock2);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_getNetInterfaceInfo(interfaceIndex, &interfaceInfo);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_setSockAddrPort(&(interfaceInfo.address), PAL_NET_TEST_INCOMING_PORT);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_bind(sock, &(interfaceInfo.address), interfaceInfo.addressSize);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_listen(sock, 10);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    // must open a terminal and give: curl 10.45.48.192:8000 (replace with the correct ip)
    TEST_PRINTF("waiting for connection:\r\n");
    result = pal_accept(sock, &address2, &addrlen, &sock2);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);


    result = pal_recv(sock2, buffer, 20, &read);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_send(sock2, messageOut, 15, &sent);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);


    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock3);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL( 0, sock3);

    pal_close(&sock2);
    pal_close(&sock3);
    pal_close(&sock);

}

static volatile uint32_t s_callbackCounterNonBlock = 0;

static void nonBlockCallback()
{
    s_callbackCounterNonBlock++;
}

TEST(pal_socket, nonBlockingAsyncTest)
{
    palStatus_t result = PAL_SUCCESS;
    //palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    const char* message = "GET / HTTP/1.0\r\nHost:10.45.48.68:8000\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    s_callbackcounter = 0;
    palSocketLength_t addrlen = 0;
    int32_t waitIterations = 0;
    palSocket_t sock = 0;

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME, &address, &addrlen);
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);



#if PAL_NET_ASYNCHRONOUS_SOCKET_API
    result = pal_asynchronousSocket(PAL_AF_INET, PAL_SOCK_STREAM, true, 0, nonBlockCallback, &sock);
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_HTTP_PORT);
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);

    result = pal_connect(sock, &address, 16);
    if (PAL_ERR_SOCKET_IN_PROGRES == result)
    {
        pal_osDelay(400);
    }
    else
    {
	TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);
    }
    s_callbackCounterNonBlock = 0;
    result = pal_send(sock, message, 45, &sent);
    while (PAL_ERR_SOCKET_IN_PROGRES == result)
    {
        pal_osDelay(100);
	result = pal_send(sock, message, strlen(message), &sent);
    }
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);

    result = pal_recv(sock, buffer, 5, &read); // may block
    if (PAL_ERR_SOCKET_WOULD_BLOCK == result)
    {
        s_callbackCounterNonBlock = 0;
        while (s_callbackCounterNonBlock == 0)
        {
            waitIterations++;
            pal_osDelay(100);
        }
        result = pal_recv(sock, buffer, 5, &read); // shouldnt block
    }
    pal_close(&sock);
    TEST_ASSERT_EQUAL_HEX(PAL_SUCCESS, result);
    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');
    TEST_ASSERT(s_callbackCounterNonBlock > 0);

#endif // PAL_NET_ASYNCHRONOUS_SOCKET_API
}


TEST(pal_socket, tProvUDPTest)
{

    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0,{0} };
    //palSocketAddress_t address2 = { 0, {0} };
    uint8_t buffer[100] = { 0 };
    //const char* messageOut = "HTTP/1.0 200 OK";
    uint8_t buffer_dns[33] = { 0x8e, 0xde, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x61, 0x72, 0x73, 0x74, 0x65, 0x63, 0x68, 0x6e, 0x69, 0x63, 0x61, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01 };
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 16;
    //palSocketLength_t addrlen2 = 16;
    int timeout = 1000;
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, 0, &sock);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME_UDP, &address, &addrlen);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_UDP_PORT);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_setSocketOptions(sock, PAL_SO_SNDTIMEO, &timeout, sizeof(timeout));
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    timeout = 1000;
    result = pal_setSocketOptions(sock, PAL_SO_RCVTIMEO, &timeout, sizeof(timeout));
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);

    result = pal_sendTo(sock, buffer_dns, sizeof(buffer_dns), &address, addrlen, &sent);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT_EQUAL(sent, sizeof(buffer_dns));

    result = pal_receiveFrom(sock, buffer, 16, NULL, NULL, &read);
    TEST_ASSERT_EQUAL_HEX( PAL_SUCCESS, result);
    TEST_ASSERT_EQUAL(read, 16);

    result = pal_receiveFrom(sock, buffer, 100, NULL, NULL, &read); //  should get timeout
    TEST_ASSERT_EQUAL(result, PAL_ERR_SOCKET_WOULD_BLOCK);

    pal_close(&sock);
}



