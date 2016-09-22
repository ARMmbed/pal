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


###########################################################################
# Define test targets based on PROJECT
# Make files that include this must define TARGET_PLATFORM and TARGET_CONFIGURATION_DEFINES
#
# Clive Bluston
###########################################################################

# The root of PAL. All sources should be relative to this.
#PAL_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PAL_ROOT=..
$(info PAL_ROOT=$(PAL_ROOT))

# output folder:
OUT:=$(PAL_ROOT)/out/$(TARGET_PLATFORM)
OUTOBJ:=$(OUT)/obj
$(OUTOBJ):
	$(MKDIR_QUIET) $@


INIT_SRC   = $(PAL_ROOT)/Source/PAL-Impl/pal_init.c 

RTOS_SRC   = $(PAL_ROOT)/Source/PAL-Impl/Modules/RTOS/pal_rtos.c \
			 $(PAL_ROOT)/Source/Port/Reference-Impl/$(TARGET_PLATFORM)/RTOS/pal_plat_rtos.c \
			 
SOCKET_SRC = $(PAL_ROOT)/Source/PAL-Impl/Modules/Networking/pal_network.c \
			 $(PAL_ROOT)/Source/Port/Reference-Impl/$(TARGET_PLATFORM)/Networking/pal_plat_network.cpp
			 
UPDATE_SRC = 



ALL_SRC = $(INIT_SRC) $(RTOS_SRC) $(SOCKET_SRC) $(UPDATE_SRC)			 



#========================================================================
ifeq ($(findstring HAS_ALL,$(TARGET_CONFIGURATION_DEFINES)),HAS_ALL)
PROJECT=pal_all
TYPE=Unitest

$(PROJECT)_ADDITIONAL_SOURCES:= $(ALL_SRC) \
								$(PAL_ROOT)/Test/$(TYPE)/pal_socket_test.c \
								$(PAL_ROOT)/Test/$(TYPE)/pal_socket_test_runner.c \
								$(PAL_ROOT)/Test/$(TYPE)/pal_rtos_test.c \
								$(PAL_ROOT)/Test/$(TYPE)/pal_rtos_test_runner.c \
																						

include BUILD_TEST_$(TARGET_PLATFORM).mk
else
#========================================================================
#=======================================================================
ifeq ($(findstring HAS_SOCKET,$(TARGET_CONFIGURATION_DEFINES)),HAS_SOCKET)
PROJECT=pal_socket
TYPE=Unitest

$(PROJECT)_ADDITIONAL_SOURCES:=  $(ALL_SRC) 



include BUILD_TEST_$(TARGET_PLATFORM).mk
endif
#========================================================================
ifeq ($(findstring HAS_RTOS,$(TARGET_CONFIGURATION_DEFINES)),HAS_RTOS)
PROJECT=pal_rtos
TYPE=Unitest

$(PROJECT)_ADDITIONAL_SOURCES:= $(ALL_SRC) 

include BUILD_TEST_$(TARGET_PLATFORM).mk
endif

#========================================================================

#========================================================================
ifeq ($(findstring HAS_UPDATE,$(TARGET_CONFIGURATION_DEFINES)),HAS_UPDATE)
PROJECT=pal_update
TYPE=Unitest

$(PROJECT)_ADDITIONAL_SOURCES:= $(ALL_SRC) 

include BUILD_TEST_$(TARGET_PLATFORM).mk
endif
#========================================================================

endif