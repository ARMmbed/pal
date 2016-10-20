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
#include "rtos.h"

DigitalOut led1(LED1);
InterruptIn sw2(SW2);
uint32_t button_pressed = 0;
Thread *thread2;

void sw2_press(void)
{
    thread2->signal_set(0x1);
}

void led_thread(void const *argument)
{
    while (true) {
        led1 = !led1;
        Thread::wait(1000);
    }
}

void button_thread(void const *argument)
{
    while (true) {
        Thread::signal_wait(0x1);
        button_pressed++;
    }
}


// Run all the unity tests
//extern "C" void RunAllTests(void);
// Include explicitly and not using the h file because we are compiling this file with C++ and the h file does not
// declare it extern "C"
extern "C" int UnityMain(int argc, const char* argv[], void (*runAllTests)(void));

extern "C" void TEST_pal_socket_GROUP_RUNNER(void);

int main(int argc, const char * argv[])
{
    const char * myargv[] = {"app","-v"};

    Thread::wait(2000);
    printf("Start tests\n");
    fflush(stdout);

    UnityMain(sizeof(myargv)/sizeof(myargv[0]), myargv, TEST_pal_socket_GROUP_RUNNER);

    // This is detected by test runner app, so that it can know when to terminate without waiting for timeout.
    printf("***END OF TESTS**\n");for(int i=0;i<1000;i++)putchar('x');putchar('\n');
    fflush(stdout);
}
