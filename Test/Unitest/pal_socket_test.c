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
#include "pal_plat_network.h"
#include "unity.h"
#include "unity_fixture.h"
#include "pal_test_utils.h"
#include "pal_socket_test_utils.h"
#include "string.h"



TEST_GROUP(pal_socket);

//sometimes you may want to get at local data in a module.
//for example: If you plan to pass by reference, this could be useful
//however, it should often be avoided
//extern int Counter;

#define PAL_NET_SUPPORT_LWIP 1
#define PAL_NET_TEST_SERVER_NAME "e109180-lin.kfn.arm.com"
#define PAL_NET_TEST_SERVER_IP   {10,45,48,190}
#define PAL_NET_TEST_SERVER_IP_STRING   "10.45.48.190"
#define PAL_NET_TEST_SERVER_HTTP_PORT 8686
#define PAL_NET_TEST_SERVER_UDP_PORT 8383
#define PAL_NET_TEST_INCOMING_PORT 8000

void * g_networkInterface = NULL;


static uint32_t s_callbackcounter = 0;

void socketCallback()
{
    s_callbackcounter++;
}

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
    uint32_t sockOptVal = 5000;
    uint32_t sockOptLen = sizeof(sockOptVal);

    TEST_PRINTF("start socket test\r\n");

    memset(&interfaceInfo,0,sizeof(interfaceInfo));
    // check that re-addignt he network interface returns the same index
    pal_registerNetworkInterface(g_networkInterface, &interfaceIndex);
    TEST_ASSERT_EQUAL(interfaceIndex, 0);
    pal_registerNetworkInterface(g_networkInterface, &interfaceIndex);
    TEST_ASSERT_EQUAL(interfaceIndex, 0);

    TEST_PRINTF("create sockets\r\n");


    //blocking
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, interfaceIndex, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, interfaceIndex, &sock2);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    //non-blocking
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, true, interfaceIndex, &sock5);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_asynchronousSocket(PAL_AF_INET, PAL_SOCK_STREAM, false, interfaceIndex, socketCallback, &sock3);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);


    result = pal_getNumberOfNetInterfaces(&numInterface);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_ASSERT_EQUAL(numInterface, 1);

    result = pal_getNetInterfaceInfo(0, &interfaceInfo);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_PRINTF("interface addr: %d %d %d %d \r\n", interfaceInfo.address.addressData[2], interfaceInfo.address.addressData[3], interfaceInfo.address.addressData[4], interfaceInfo.address.addressData[5]);

    
    result = pal_setSocketOptions(sock, PAL_SO_RCVTIMEO, &sockOptVal, sockOptLen);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS); 

    TEST_PRINTF("close sockets\r\n");
    
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    result = pal_close(&sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    result = pal_close(&sock2);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    result = pal_close(&sock5);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    result = pal_close(&sock3);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    TEST_PRINTF("end\r\n");
 
}


TEST(pal_socket, basicTCPclinetSendRecieve)
{


    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    const char* message = "GET / HTTP/1.0\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    palSocketLength_t addrlen = 0;

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME, &address, &addrlen);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_HTTP_PORT);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_connect(sock, &address, 16);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    
    result = pal_send(sock, message, 45, &sent);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_recv(sock, buffer, 99, &read);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_PRINTF(buffer);

    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');
    pal_close(&sock);

    TEST_PRINTF("test Done");

}

TEST(pal_socket, basicUDPclinetSendRecieve)
{

    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    palSocketAddress_t address2 = { 0 };
    uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 };
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 0;

    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, 0, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME, &address, &addrlen);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_UDP_PORT);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    TEST_PRINTF("udp send \r\n");
    result = pal_sendTo(sock, buffer, 10, &address, 16, &sent);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_ASSERT_EQUAL(sent, 10);
    result = pal_plat_receiveFrom(sock, buffer, 10, &address2, &addrlen, &read);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_ASSERT_EQUAL(read, 10);
    TEST_PRINTF("udp done \r\n");
    pal_close(&sock);
}


TEST(pal_socket, basicSocketScenario3)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    const char* message = "GET / HTTP/1.0\r\nHost:10.45.48.68:8000\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    s_callbackcounter = 0;
    palSocketLength_t addrlen = 0;

    result = pal_asynchronousSocket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, socketCallback, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_NAME, &address, &addrlen);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_HTTP_PORT);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_connect(sock, &address, 16);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_send(sock, message, 45, &sent);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_recv(sock, buffer, 99, &read);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_PRINTF(buffer);

    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');
    TEST_ASSERT(s_callbackcounter > 0);
    TEST_PRINTF("callback counter %d \r\n", s_callbackcounter);
    pal_close(&sock);

    TEST_PRINTF("test Done");
}

TEST(pal_socket, basicSocketScenario4)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocket_t sock2 = 0;
    palSocketAddress_t address = { 0 };
    const char* message = "GET / HTTP/1.0\r\n\r\n";
    size_t sent = 0;
    char buffer[100] = { 0 };
    size_t read = 0;
    palSocketLength_t addlen = 0;
    uint32_t numSockets = 0;
    palSocket_t socketsToCheck[PAL_NET_SOCKET_SELECT_MAX_SOCKETS] = { 0 };
    pal_timeVal_t tv = {0};
    uint8_t palSocketStatus[PAL_NET_SOCKET_SELECT_MAX_SOCKETS] = { 0 };

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock2);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_getAddressInfo("www.w3.org", &address, &addlen);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_PRINTF("addr lookup: %d %d %d %d \r\n", address.addressData[2], address.addressData[3], address.addressData[4], address.addressData[5]);

    result = pal_setSockAddrPort(&address, 80);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_connect(sock, &address, 16);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_send(sock, message, 45, &sent);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    socketsToCheck[0] = sock;
    socketsToCheck[1] = sock2;
    tv.pal_tv_sec = 5;
    tv.pal_tv_usec = 1000;
    
    result = pal_plat_socketMiniSelect(socketsToCheck, 2, &tv, palSocketStatus, &numSockets);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    //TEST_ASSERT_EQUAL(numSockets, 1);
    //TEST_ASSERT(palSocketStatus[0] >= 0);
    
    result = pal_recv(sock, buffer, 99, &read);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_PRINTF(buffer);

    TEST_ASSERT(read >= 4);
    TEST_ASSERT(buffer[0] == 'H' && buffer[1] == 'T'&& buffer[2] == 'T' && buffer[3] == 'P');

    socketsToCheck[0] = sock2;
    socketsToCheck[1] = 0;
    tv.pal_tv_sec = 0;
    tv.pal_tv_usec = 20000;
    result = pal_plat_socketMiniSelect(socketsToCheck, 1, &tv, palSocketStatus, &numSockets);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_ASSERT_EQUAL(numSockets, 0);
    TEST_ASSERT(palSocketStatus[0] == 0);
    
    pal_close(&sock);
    pal_close(&sock2);

    TEST_PRINTF("test Done");

}

TEST(pal_socket, basicSocketScenario5)
{
    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocket_t sock2 = 0;
    palSocket_t sock3 = 0;

    palSocketAddress_t address2 = { 0 };

    char buffer[100] = { 0 };
    const char* messageOut = "HTTP/1.0 200 OK";
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 16;
    palNetInterfaceInfo_t interfaceInfo;

    memset(&interfaceInfo,0,sizeof(interfaceInfo));


    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM_SERVER, false, 0, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock2);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    
    result = pal_getNetInterfaceInfo(0, &interfaceInfo);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_setSockAddrPort(&(interfaceInfo.address), PAL_NET_TEST_INCOMING_PORT);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    
    result = pal_bind(sock, &(interfaceInfo.address), interfaceInfo.addressSize);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_listen(sock, 10);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    TEST_PRINTF("waiting for connection:\r\n");
    result = pal_accept(sock, &address2, &addrlen, &sock2);
    TEST_PRINTF("after accept:\r\n");
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);


    result = pal_recv(sock2, buffer, 99, &read);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_PRINTF(buffer);

    result = pal_send(sock2, messageOut, 15, &sent);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);


    result = pal_socket(PAL_AF_INET, PAL_SOCK_STREAM, false, 0, &sock3);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);



    pal_close(&sock2);
    pal_close(&sock3);
    pal_close(&sock);
    
    TEST_PRINTF("test Done");
}


TEST(pal_socket, tProvUDPTest)
{

    palStatus_t result = PAL_SUCCESS;
    palSocket_t sock = 0;
    palSocketAddress_t address = { 0 };
    palSocketAddress_t address2 = { 0 };
    char buffer[100] = { 0 };
    const char* messageOut = "HTTP/1.0 200 OK";
    size_t sent = 0;
    size_t read = 0;
    palSocketLength_t addrlen = 16;
    palSocketLength_t addrlen2 = 16;
    int timeout = 10000;
    result = pal_socket(PAL_AF_INET, PAL_SOCK_DGRAM, false, 0, &sock);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_getAddressInfo(PAL_NET_TEST_SERVER_IP_STRING, &address, &addrlen);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_setSockAddrPort(&address, PAL_NET_TEST_SERVER_UDP_PORT);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_setSocketOptions(sock, PAL_SO_SNDTIMEO, &timeout, sizeof(timeout));
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_sendTo(sock, messageOut, 16, &address, addrlen, &sent);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_ASSERT_EQUAL(sent, 16);

    result = pal_receiveFrom(sock, buffer, 100, NULL, NULL, &read);
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);
    TEST_ASSERT_EQUAL(read, 16);

    timeout = 1;
    result = pal_setSocketOptions(sock, PAL_SO_RCVTIMEO, &timeout, sizeof(timeout));
    TEST_ASSERT_EQUAL(result, PAL_SUCCESS);

    result = pal_receiveFrom(sock, buffer, 100, &address2, &addrlen2, &read); //  should get timeout
    TEST_ASSERT_EQUAL(result, PAL_ERR_SOCKET_WOULD_BLOCK);
}
