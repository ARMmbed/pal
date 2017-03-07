#!/usr/bin/env python
import argparse
import logging
import os
import re
import xml.etree.cElementTree as eTree

import sys

logger = logging.getLogger('unity-to-xml')


class TestCase(object):
    """
    represent single testcase report element
    """
    test_status_pattern = re.compile(
        r'<\*\*\*UnityResult\*\*\*>(\w+)</\*\*\*UnityResult\*\*\*>'
    )

    ignored_test_pattern = re.compile(
        r'TEST\(\w+, \w+\):IGNORE: (.*)'
    )

    failure_test_pattern = re.compile(
        r'===!!!===> <\*\*\*UnityResult\*\*\*>FAIL</\*\*\*UnityResult\*\*\*> <===!!!===: (.*)'
    )

    unity_test_pattern = re.compile(
        r'<\*\*\*UnityTest\*\*\*>TEST\((\w+?), (\w+?)\)</\*\*\*UnityTest\*\*\*>(.*)',
    )
    DISABLED = 'DISABLED'
    PASSED = 'PASSED'
    FAILED = 'FAILED'

    def __init__(self, log_iter, unity_test_match):
        """
        CTOR
        Args:
            log_iter: log file file iterator - in some cases we would need to read extra lines
            unity_test_match: unity_test_pattern match object
        """
        assert len(unity_test_match) == 3
        self.suite_name = unity_test_match[0]
        self.name = unity_test_match[1]
        self.message = None

        match = TestCase.ignored_test_pattern.search(unity_test_match[2])
        if match:
            self.status = TestCase.DISABLED
            self.message = match.group(1)
        else:
            match = TestCase.test_status_pattern.search(unity_test_match[2])
            self.message = unity_test_match[2]
            if match:
                self.status = TestCase.PASSED if match.group(1) == 'PASS' else TestCase.FAILED
            else:
                try:
                    line = log_iter.next()
                    self.message += line
                    while TestCase.test_status_pattern.search(line) is None:
                        line = log_iter.next()
                        self.message += line
                    match = TestCase.test_status_pattern.search(line)
                    self.status = TestCase.PASSED if match.group(1) == 'PASS' else TestCase.FAILED
                except StopIteration:
                    self.status = TestCase.FAILED

    def __str__(self):
        return 'Group:{suite_name} Test:{name} - {status}'.format(**self.__dict__)

    @staticmethod
    def consume_test_case_line(test_suites, log_iter, line):
        """
        try to consume log line
        Args:
            log_iter: log file file iterator - in some cases we would need to read extra lines
            test_suites: instance of TestSuites container
            line: line being processed

        Returns: True if consumed, False otherwise

        """
        assert isinstance(test_suites, TestSuites)
        match = TestCase.unity_test_pattern.search(line)
        if match:
            test_case = TestCase(log_iter, match.groups())
            logger.info(test_case)
            test_suite = test_suites.get_test_suite(test_case.suite_name)
            test_suite.register_test(test_case)
        return match


class TestSuite(object):
    """
    represent single JUNIT testsuite element.
    manage test counters and update XML element accordingly
    """
    def __init__(self, xml_element, test_suites):
        assert xml_element is not None
        assert isinstance(test_suites, TestSuites)
        self.xml_element = xml_element
        self.test_suites = test_suites
        self.num_failures = 0
        self.num_disabled = 0
        self.num_tests = 0
        self.xml_element.set('failures', str(0))
        self.xml_element.set('disabled', str(0))
        self.xml_element.set('tests', str(0))

    def register_test(self, test):
        assert isinstance(test, TestCase)
        self.num_tests += 1
        self.xml_element.set('tests', str(self.num_tests))
        test_case_xml_element = eTree.SubElement(
            self.xml_element, 'testcase', name=test.name)
        if test.status == TestCase.DISABLED:
            self.num_disabled += 1
            self.xml_element.set('disabled', str(self.num_disabled))
            self.test_suites.register_disabled_test()
            eTree.SubElement(test_case_xml_element, 'skipped', message=test.message)
        elif test.status == TestCase.FAILED:
            self.num_failures += 1
            self.xml_element.set('failures', str(self.num_failures))
            self.test_suites.register_failed_test()
            eTree.SubElement(test_case_xml_element, 'failure', message=test.message)
        else:
            assert test.status == TestCase.PASSED
            self.test_suites.register_passed_test()


class TestSuites(object):
    """
    represent JUNIT XML root element - testsuites
    manage test counters and update XML element accordingly
    cache TestSuite elements since they are reused while adding test - alternative would be using xPath to traverse DOM
    model for achieving reference to an element - which is much slower
    """

    unity_summary_pattern = re.compile(
        r'(\d+) Tests (\d+) Failures (\d+) Ignored',
    )

    def __init__(self, root_suite_name):
        """
        CTOR
        Args:
            root_suite_name: str: testsuites XML element name
        """
        self.test_suite_lookup_table = {}
        self.root_element = eTree.Element('testsuites', name=root_suite_name)
        self.num_failures = 0
        self.num_disabled = 0
        self.num_tests = 0
        self.root_element.set('failures', str(0))
        self.root_element.set('disabled', str(0))
        self.root_element.set('tests', str(0))
        self.root_suite_name = root_suite_name
        self.consumedEnd = False

    def register_passed_test(self):
        self.num_tests += 1
        self.root_element.set('tests', str(self.num_tests))

    def register_failed_test(self):
        self.num_tests += 1
        self.num_failures += 1
        self.root_element.set('tests', str(self.num_tests))
        self.root_element.set('failures', str(self.num_failures))

    def register_disabled_test(self):
        self.num_tests += 1
        self.num_disabled += 1
        self.root_element.set('tests', str(self.num_tests))
        self.root_element.set('disabled', str(self.num_disabled))

    def get_test_suite(self, test_suite_name):
        """
        get TestSuite instance. in case not yet exists - create it
        Args:
            test_suite_name: test suite name

        Returns: initialized TestSuite instance

        """
        new_name = '{r}.{s}'.format(r=self.root_suite_name, s=test_suite_name)
        test_suite = self.test_suite_lookup_table.get(
            new_name, None)
        if not test_suite:
            test_suite_element = eTree.SubElement(self.root_element, 'testsuite', name=new_name)
            test_suite = TestSuite(test_suite_element, self)
            self.test_suite_lookup_table[new_name] = test_suite
        return test_suite

    def get_xml(self):
        """
        Returns: JUNIT XML eTree.ElementTree
        """
        if self.consumedEnd is False:
            suite = self.get_test_suite('Incomplete_Test')
            case = TestCase(['Unfinished Test'].__iter__(), ('Incomplete_Test', 'Crash', ''))
            suite.register_test(case)
        return eTree.ElementTree(self.root_element)

    def consume_summary_line(self, line):
        """
        try to consume unity log summary line and assert test counters
        Args:
            line: log line being processed

        Returns: True in case line was consumed

        """
        match = TestSuites.unity_summary_pattern.search(line)
        if match:
            assert self.consumedEnd is False
            assert self.num_tests == int(match.group(1))
            assert self.num_failures == int(match.group(2))
            assert self.num_disabled == int(match.group(3))
            self.consumedEnd = True
        return match


def get_parser():
    """
    Returns: initialized argparse.ArgumentParser instance
    """
    parser = argparse.ArgumentParser(description='PAL Unity log to JUNIT report converter')

    parser.add_argument(
        'log_file',
        type=argparse.FileType('rt'),  # argparse will validate that file can be read by opening it and returning the
                                       # descriptor
        metavar='LOG_FILE',
        help='input Unity log file to be converted to JUNIT report',
    )
    parser.add_argument(
        '-o',
        '--output-file',
        type=argparse.FileType('wb'),  # argparse will validate that file can be written by opening it and returning the
                                       # descriptor. opening file in binary mode - as requested
                                       # in xml.etree.cElementTree documentation
        metavar='FILE',
        help='output file name. if omitted - input file name will derived from input file name'
             ' but with _junit.xml suffix instead'
    )

    parser.add_argument(
        '-s',
        '--root-suite',
        default='Unity',
        help='root test suite name. [default Unity]'
    )

    parser.add_argument(
        '-v',
        '--validate',
        action='store_true',
        help='validate generated XML against Jenkins XSD. Requires "requests" and "lxml" libraries '
    )

    parser.add_argument(
        '-q',
        '--quiet',
        action='store_true',
        help='run in quiet mode - do not print parsed test names'
    )

    return parser


def generate_junit_report(log_fh, root_suite_name):
    """
    generate JUNIT report XML string
    Args:
        log_fh: opened file handler for reading log file
        root_suite_name: root test suite name

    Returns: string representing JUNIT report
    """
    assert not log_fh.closed
    assert root_suite_name
    test_suites = TestSuites(root_suite_name)
    with log_fh:
        for line in log_fh:
            if TestCase.consume_test_case_line(test_suites, log_fh, line):
                continue
            test_suites.consume_summary_line(line)

    return test_suites.get_xml()


def do_validate(xml_file_name):
    """
    useful when developing/maintaining the script
    perform XSD validation against Jenkins Junit plugin schema file

    this function requires two modules (lxml.etree and requests) that are not part of typical Python installation,
    thus can be absent from developer machines. These modules can be installed via pip install command

    Args:
        xml_file_name: generated xml file name to be validated

    """
    from lxml import etree as lxml_etree
    import requests

    assert os.path.exists(xml_file_name)
    logging.getLogger('requests').setLevel(logging.ERROR)

    jenkins_junit_xsd = 'https://raw.githubusercontent.com/jenkinsci/xunit-plugin/master/src/main/resources/org/jenkinsci/plugins/xunit/types/model/xsd/junit-10.xsd'

    response = requests.get(jenkins_junit_xsd)  # send GET request
    response.raise_for_status()  # assert successful response

    xmlschema_doc = lxml_etree.fromstring(response.content)  # load XML document from HTTP response body
    xmlschema = lxml_etree.XMLSchema(xmlschema_doc)  # initialize XML schema object

    xml_doc = lxml_etree.parse(xml_file_name)  # load generated JUNIT report
    xmlschema.assertValid(xml_doc)  # validate schema
    logger.info('JUNIT report file is valid')


def main():
    args = get_parser().parse_args()

    level = logging.INFO if not args.quiet else logging.WARNING

    logging.basicConfig(
        level=level,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        stream=sys.stdout
    )

    if not args.output_file:  # output file is set to be optional argument - handle the case when it was not provided
        # derive output file name from input - by replacing the suffix
        args.output_file = open(os.path.splitext(args.log_file.name)[0] + '_junit.xml', 'wb')

    junit_xml = generate_junit_report(log_fh=args.log_file, root_suite_name=args.root_suite)

    with args.output_file:  # insure file object is flushed and closed - since it will be read in do_validate()
        junit_xml.write(args.output_file)

    if args.validate:
        do_validate(args.output_file.name)


if __name__ == '__main__':
    main()
