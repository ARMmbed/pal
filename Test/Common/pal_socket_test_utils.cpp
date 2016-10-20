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
#include "pal_socket_test_utils.h"
#include "NetworkInterface.h"
#include "EthernetInterface.h"

#define TEST_PRINTF printf

void* palTestGetNetWorkInterfaceContext()
{
    palStatus_t result = PAL_SUCCESS;
    EthernetInterface* netInterface = new EthernetInterface();
    if (NULL != netInterface)
    {
        TEST_PRINTF("new interface created\r\n");
        result = netInterface->connect();
        if (PAL_SUCCESS == result)
        {
            TEST_PRINTF("interface registered : OK \r\n");
        }
    }
    return netInterface;
}
