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
# Make files that include this must define PROJECT and TARGET_PLATFORM
# Requirements
# - mbed-cli must be installed
# - MORPHEUS_ROOT must be defined ( e.g. $(PAL_ROOT)/mbed-os)
#
# To install mbed-os the following may help:
#pip uninstall mbed-cli
#cd mbed-client-pal
#mbed new
#mbed deploy
#mbed config root mbed-client-pal/mbed-os
#cd mbed-os
#sudo pip install -r core/requirements.txt
#sudo pip install -U colorama
#sudo pip install -U jinja2
#mbed remove frameworks/unity
#
# The following targets may also be defined:
# $(PROJECT)_ADDITIONAL_SOURCES
#
# Clive Bluston
###########################################################################

# Add targets for platform/project combination
.PHONY: $(TARGET_PLATFORM)_all $(TARGET_PLATFORM)_clean $(TARGET_PLATFORM)_check
$(TARGET_PLATFORM)_all: $(TARGET_PLATFORM)_$(PROJECT) 
$(TARGET_PLATFORM)_clean: $(TARGET_PLATFORM)_clean_$(PROJECT)
$(TARGET_PLATFORM)_check: $(TARGET_PLATFORM)_check_$(PROJECT)

# Process command line argument of the form INCLUDE=, in order to pass it to the compilation.
# One or more tests can be selected in this way. If the argument is not present then all tests are selected.
$(info PAL_TEST=$(PAL_TEST))
ifeq ($(strip $(PAL_TEST)),)
  CC_TESTS = PAL_INCLUDE=1
else
  CC_TESTS = PAL_INCLUDE=0 $(patsubst %,%=1, $(PAL_TEST))
endif

# Define variables to be used in the recipes of the targets in the context of the main target.
# We do this because the original variables will be overridden by other make files by the time the
# recipe is executed. Note that these variables are recursively inherited by all prerequisites.
$(TARGET_PLATFORM)_$(PROJECT) : MORPHEUS_CC_TESTS_D := $(patsubst %,-D%, $(CC_TESTS))
#################################################################################################################
# Target platform dependant definitions.
# All os references relative to the root.
# Can define a MBEDOS_ROOT somewhere else

ifeq ($(strip $(MBEDOS_ROOT)),)
  MORPHEUS_ROOT := $(PAL_ROOT)/Test
else
  MORPHEUS_ROOT := $(strip $(MBEDOS_ROOT))
endif


ifeq ($(DEBUG), 1)
  $(info "DEBUG")
  $(TARGET_PLATFORM)_$(PROJECT) : DEBUG_FLAGS += -DDEBUG
endif

ifeq ($(PAL_IGNORE_UNIQUE_THREAD_PRIORITY), 1)
  $(info "PAL_IGNORE_UNIQUE_THREAD_PRIORITY")
  $(TARGET_PLATFORM)_$(PROJECT) : DEBUG_FLAGS += -DPAL_IGNORE_UNIQUE_THREAD_PRIORITY
endif

ifeq ($(VERBOSE), 1)
  $(info "VERBOSE")
  $(TARGET_PLATFORM)_$(PROJECT) : DEBUG_FLAGS += -DVERBOSE
endif

#################################################################################################################

### UNITY FILES ###
UNITY_ROOT=$(PAL_ROOT)/Test/Unity
UNITY_INCLUDE_PATHS=$(UNITY_ROOT)/src $(UNITY_ROOT)/extras/fixture/src
INCLUDE_PATHS = $(UNITY_INCLUDE_PATHS) $(PAL_ROOT)/Source/PAL-Impl/Services-API $(PAL_ROOT)/Source/Port/Platform-API  $(PAL_ROOT)/Test/Common
# mbed compile command searches include folders for c files, so it will find unity c files
# If we were to specify them here, they would be muliply defined.
UNITY_OBJECTS =
#UNITY_OBJECTS = $(UNITY_ROOT)/src/unity.c $(UNITY_ROOT)/extras/fixture/src/unity_fixture.c  
###################

# Add this optional file to the dependencies only if it exists.
ifneq ("$(wildcard $(PAL_ROOT)/Test/Common/$(PROJECT)_test_utils.c)","")
# mbed compile command searches include folders for c files, so it will find /Test/Common c files
# If we were to specify them here, they would be muliply defined.
#  $(PROJECT)_ADDITIONAL_SOURCES += $(PAL_ROOT)/Test/Common/$(PROJECT)_test_utils.c
endif

# Fixed list of test files for each test executable.
TST_SOURCES:=	$(INCLUDE_PATHS) \
				$(UNITY_OBJECTS) \
				$(MORPHEUS_ROOT)/mbed-os \
				$(PAL_ROOT)/Test/$(TYPE)/$(PROJECT)_test.c \
				$(PAL_ROOT)/Test/$(TYPE)/$(PROJECT)_test_runner.c \
				$(PAL_ROOT)/Test/$(TYPE)/$(PROJECT)_test_main_$(TARGET_PLATFORM).cpp \
				$($(PROJECT)_ADDITIONAL_SOURCES)
ifeq ($(findstring HAS_UPDATE,$(TARGET_CONFIGURATION_DEFINES)),HAS_UPDATE)
TST_SOURCES:= 	$(TST_SOURCES) \
				$(MORPHEUS_ROOT)/storage-volume-manager \
				$(MORPHEUS_ROOT)/storage-abstraction/ \
				$(MORPHEUS_ROOT)/mbed-client-libservice \
				$(MORPHEUS_ROOT)/mbed-trace/

endif

# Build executables and listings.
.PHONY: $(TARGET_PLATFORM)_$(PROJECT) 
$(TARGET_PLATFORM)_$(PROJECT):  $(MORPHEUS_ROOT) $(OUTOBJ)  $(OUT)/$(PROJECT).bin 

$(TARGET_PLATFORM)_$(PROJECT):  MORPHEUS_ROOT:=$(MORPHEUS_ROOT) 


$(OUT)/$(PROJECT).bin:  $(TST_SOURCES) 
	# Always remove the test runner since PAL_TEST argument change requires that it is recompiled
	$(RM) $(dir $@)obj/$(PROJECT)_test_runner.o
	# Ignore some mbed libraries in the subsequent build. (The minus ignores the error if the library does not exist)
	-$(ECHO) "*" > $(strip $(MORPHEUS_ROOT))/mbed-os/features/frameworks/unity/.mbedignore
	# Morpheus build.
	mbed compile -v -N ../$(notdir $(basename $@)) --build $(dir $@)obj -t GCC_ARM -m K64F $(addprefix  --source=, $^) $(MORPHEUS_CC_TESTS_D) $(DEBUG_FLAGS)

# Create a list of files to delete for each target on the first pass of the make
$(TARGET_PLATFORM)_clean_$(PROJECT) : OUTPUTS:= $(OUT)


# Remove files in the list $(PROJECT)_OUTPUTS. 
# We dynamically create the list variable from the target
PHONY: $(TARGET_PLATFORM)_clean_$(PROJECT) 
$(TARGET_PLATFORM)_clean_$(PROJECT): 
	$(RM) $(OUTPUTS)

# Check that the script exists
$(PAL_ROOT)/Test/Scripts/perform_test_mbedos.py:

# This makes sure anyone who is dependant on it always executes its recipe
.FORCE:

# Always run the test.
$(OUT)/$(PROJECT)_result.txt: $(PAL_ROOT)/Test/Scripts/perform_test_mbedOS.py $(TARGET_PLATFORM)_$(PROJECT) $(OUT)/$(PROJECT).bin .FORCE
	# Install and run the test. Pass script and binary file. 
	python   $(word 1, $^) $(word 3, $^)

# check. Install and run tests
.PHONY: $(TARGET_PLATFORM)check_$(PROJECT)
$(TARGET_PLATFORM)_check_$(PROJECT):  $(OUT)/$(PROJECT)_result.txt
