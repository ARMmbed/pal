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

import re
import sys
from tabulate import tabulate

def extract_tags(logLines):

    # These are the Unity tag names we are going to look for.
    tagNames = [
        "UnityTest",
        "UnityIgnoredTest",
        "UnityResult",
    ]

    # All tags will end up here.
    tags = []

    for tagName in tagNames:

        # Fetch start and end locations for the start and end markers.

        startMatches = re.finditer(re.escape("<***" + tagName + "***>"), logLines)
        endMatches = re.finditer(re.escape("</***" + tagName + "***>"), logLines)

        startOffsets = [match.end() for match in startMatches]
        endOffsets = [match.start() - 1 for match in endMatches]

        # If the amount of start and end markers isn't identical, this is an error.
        if len(startOffsets) != len(endOffsets):
            raise Exception("For tag '" + tagName + "', start and end tags do not match!")

        # Append all found tags to the tags list.
        for tagOffsets in zip(startOffsets, endOffsets):
            tagContent = logLines[tagOffsets[0]: tagOffsets[1] + 1]
            tags.append((tagOffsets[0], tagName, tagContent))

    # Sort the tags list by the offset.
    tags.sort(key=lambda tag_: tag_[0])

    # Throw away the offset (once sorted, it is no longer needed).
    tags = [tag[1:] for tag in tags]

    # At this point we are left with list of tags, each one a pair, consisting of
    # (group name, test name, status).
    return tags

def get_test_group_and_name(printableName):
    match = re.match(r"TEST\((.*), (.*)\)", printableName)
    if match is not None:
        return match.group(1), match.group(2)
    else:
        raise Exception("Erorr parsing test group and name")

def get_ignored_test_group_and_name(printableName):
    match = re.match(r"IGNORE_TEST\((.*), (.*)\)", printableName)
    if match is not None:
        return match.group(1), match.group(2)
    else:
        raise Exception("Erorr parsing test group and name")

def collect_tests(tags):

    tests = []

    # Build the list of tests, with status for each.
    curTest = ""
    resultHandled = False
    for tag in tags:

        tagName = tag[0]
        tagValue = tag[1]

        if tagName == "UnityTest":
            curTest = get_test_group_and_name(tagValue)
            resultHandled = False
        elif tagName == "UnityResult":
            if not resultHandled:
                tests.append((curTest, tagValue))
                resultHandled = True
        elif tagName == "UnityIgnoredTest":
            curTest = get_ignored_test_group_and_name(tagValue)
            tests.append((curTest, "IGNORE"))
        else:
            raise Exception("Unknown tag '" + tagName + "' encountered")

    return tests

def generate_junit_xml_output(packageName, tests, xmlOutFile):

    testsCount = len(tests)

    with open(xmlOutFile, "wt") as f:
        print >> f, '<testsuite tests="' + str(testsCount) + '">'
        for test in tests:
            print >> f, '    <testcase classname="' + packageName + "." + test[0][0] + '" name="' + test[0][1] + '">'
            if test[1] == "FAIL":
                print >> f, '        <failure/>'
            if test[1] == "IGNORE":
                print >> f, '        <skipped/>'
            print >> f, '    </testcase>'
        print >> f, '</testsuite>'

def generate_text_output(packageName, tests, textOutFile):

    testsCount = len(tests)

    failingTests = 0
    passedTests = 0
    ignoredTests = 0

    testsTable = []
    for test in tests:
        if test[1] == "FAIL":
            failingTests += 1
        if test[1] == "PASS":
            passedTests += 1
        if test[1] == "IGNORE":
            ignoredTests += 1

        testsTable.append([test[0][0], test[0][1], test[1]])

    resultsTableHeader = ["TOTAL", "PASS", "FAIL", "IGNORE"]
    resultsTable = [[str(testsCount), str(passedTests), str(failingTests), str(ignoredTests)]]

    with open(textOutFile, "wt") as f:

        print >> f, "==== " + packageName + " ===="
        print >> f, tabulate(testsTable)
        print >> f, tabulate(resultsTable, headers=resultsTableHeader)

        if testsCount == 0 or failingTests > 0:
            finalStatus = "FAIL"
        else:
            finalStatus = "PASS"
        print >> f
        print >> f, "Final status: " + finalStatus + "."

def unity_to_junit(packageName, inputFile, xmlOutFile, textOutFile):

    with open(inputFile, "rt") as f:
        logLines = f.read()

    tags = extract_tags(logLines)
    tests = collect_tests(tags)

    generate_junit_xml_output(packageName, tests, xmlOutFile)
    generate_text_output(packageName, tests, textOutFile)


if __name__ == "__main__":

    if len(sys.argv) != 5:
        print "Usage: <package-name> <input-file> <xml-out-file> <text-out-file>"
        sys.exit(1)

    unity_to_junit(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])

