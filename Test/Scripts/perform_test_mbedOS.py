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

#####################################################################
# Install and run PAL tests for mbed targets.
#
# Arguments: One or more binary files containing tests to execute.
#
# Output: <binary file>_result.txt - Textual result summary
#         <binary file>.xml - Junit result summary
#  ARM
#  Clive Bluston
#####################################################################
from mbed import MBED
import os
import sys
import unity_to_junit
from time import sleep

# Check that at least one test is specified on the command line
if len(sys.argv) < 2:
	sys.exit()
else:
	tests = sys.argv[1:]

# List of textual result files.
resultFiles = []

# Supported mbed devices. Can add to this list
deviceList = ["K64F"]

# Loop over attached mbed devices.
for device in deviceList:
	mbed = MBED(device)
	deviceDetected = mbed.detect()
	if deviceDetected:
		# Loop over tests
		for test in tests:
			mbed.install_bin(test)
			noSuffix = os.path.splitext(test)[0]
			intermediateResultsFile = noSuffix+".int"
			# Remove intermediate file just in case it was left over from a failed run.
			if os.path.isfile(intermediateResultsFile):
				os.remove(intermediateResultsFile)
			resultFile = noSuffix+"_result.txt"
			# This delay is to allow output that was generated before the reset to be discarded.
			sleep(30)
			# Capture test results from the serial port
			if mbed.run_and_capture_till_timeout(intermediateResultsFile,baud=9600,read_timeout=10,endOfData="***END OF TESTS**"):
				# Success. Convert results to Junit format and write to xml file.
				unity_to_junit.unity_to_junit("mbedos_" + os.path.basename(noSuffix), intermediateResultsFile, noSuffix+".xml", resultFile)
				# Output intermediate results to the console.
				with open(intermediateResultsFile, 'r') as fin:
					print fin.read()
				os.remove(intermediateResultsFile)
				# Add result file name to list
				resultFiles.append(resultFile)
			else:
				response = raw_input("Connect Serial port. Enter when ready")
		# Clean up. True parameter closes the device opened by mbed.detect()
		mbed.end_run(False,True)


# Copy result files to standard output
for file in resultFiles:
	with open(file, 'r') as fin:
		print fin.read()



