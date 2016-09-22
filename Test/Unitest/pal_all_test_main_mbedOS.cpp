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

#include "mbed.h"


extern "C" int UnityMain(int argc, const char* argv[], void (*runAllTests)(void));

extern "C" void TEST_pal_update_GROUP_RUNNER(void);
extern "C" void TEST_pal_rtos_GROUP_RUNNER(void);
extern "C" void TEST_pal_socket_GROUP_RUNNER(void);

void TEST_pal_all_GROUPS_RUNNER(void)
{
	TEST_pal_rtos_GROUP_RUNNER();
	TEST_pal_socket_GROUP_RUNNER();
}


int main(int argc, const char * argv[])
{
    const char * myargv[] = {"app","-v"};

    printf("Start tests\r\n");
    fflush(stdout);

    UnityMain(sizeof(myargv)/sizeof(myargv[0]), myargv, TEST_pal_all_GROUPS_RUNNER);

    // This is detected by test runner app, so that it can know when to terminate without waiting for timeout.
    printf("***END OF TESTS**\n");for(int i=0;i<1000;i++)putchar('x');
    fflush(stdout);
}

