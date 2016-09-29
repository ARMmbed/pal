# -----------------------------------------------------------------------
# Copyright (c) 2016 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
# Licensed under the Apache License, Version 2.0 (the License); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an AS IS BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# -----------------------------------------------------------------------


###############################################################
# make all target platforms
#
# Clive Bluston
###############################################################
# Make platform detection and definitions
include make_platform.mk 


.PHONY: all clean check

#====================================================
# Platform mbed-os
TARGET_PLATFORM:=mbedosClassic
TARGET_CONFIGURATION_DEFINES:=  HAS_RTOS HAS_UPDATE
all: mbedosClassic_all
check: mbedosClassic_check
clean: mbedosClassic_clean 
include all_tests.mk
#====================================================
# Platform morpheus
TARGET_PLATFORM:=mbedOS
TARGET_CONFIGURATION_DEFINES:=  HAS_RTOS HAS_SOCKET HAS_UPDATE HAS_ALL
all: mbedOS_all
check: mbedOS_check
clean: mbedOS_clean 
include all_tests.mk
#====================================================
# Platform FreeRTOS
TARGET_PLATFORM:=FreeRTOS
TARGET_CONFIGURATION_DEFINES:=  HAS_SOCKET
# Make all tests
all: FreeRTOS_all
check: FreeRTOS_check
clean: FreeRTOS_clean
include all_tests.mk
#====================================================

