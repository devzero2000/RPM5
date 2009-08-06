#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2009 Per Øyvind Karlsen <peroyvind@mandriva.org>
#
import sys, random, rpm, unittest, os, subprocess
from test.test_support import rmtree

DICT = {}

class Test_loadHeader(unittest.TestCase):
    def setUp(self):
	self.topdir = "%s/tmp" % os.getcwdu()
	self.package = "%s/RPMS/noarch/simple-1.0-1-foo2009.1.noarch.rpm" % self.topdir

	build = subprocess.Popen(["--define", "_topdir %s" % self.topdir, "-bb", "resources/simple.spec"],
		executable="rpm", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
	self.assertFalse(build.wait())
	self.assertTrue(os.path.isfile(self.package))

    def test_loadHeader(self):
	ts = rpm.TransactionSet()
	file = open(self.package)
	DICT['hdr'] = h = ts.hdrFromFdno(file.fileno())
	file.close()
	self.assertEqual(h['name'], "simple")
	self.assertEqual(h['epoch'], 1)
	self.assertEqual(h['version'], "1.0")
	self.assertEqual(h['release'], "1")
	self.assertEqual(h['disttag'], "foo")
	self.assertEqual(h['distepoch'], "2009.1")

    def tearDown(self):
	rmtree(self.topdir)

class Test_labelCompare(unittest.TestCase):

    def setUp(self):
	self.labels = (tuple(DICT['hdr'].sprintf("%{epoch} %{version} %{release} %{distepoch}").split()), ("1", "2.1", "2", "2010.1"))

    def test_evr(self):
	le = self.labels[0][0:3]
	ge = self.labels[1][0:3]
	self.assertEqual(rpm.labelCompare(le, ge), -1)
	self.assertEqual(rpm.labelCompare(ge, le), 1)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)

    def test_evrd(self):
	le = self.labels[0]
	ge = self.labels[1]
	self.assertEqual(rpm.labelCompare(le, ge), -1)
	self.assertEqual(rpm.labelCompare(ge, le), 1)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)

    def test_None(self):
	no = (None, None, None, None)
	yes = self.labels[0]
	# first without distepoch
	self.assertEqual(rpm.labelCompare(yes[0:3], no[0:3]), 1)
	self.assertEqual(rpm.labelCompare(no[0:3], yes[0:3]), -1)
	self.assertEqual(rpm.labelCompare(no[0:3], no[0:3]), 0)	
	# then again with distepoch
	self.assertEqual(rpm.labelCompare(yes, no), 1)
	self.assertEqual(rpm.labelCompare(no, yes), -1)
	self.assertEqual(rpm.labelCompare(no, no), 0)	

def test_main():
    from test import test_support
    test_support.run_unittest(
	    Test_loadHeader,
	    Test_labelCompare)
    test_support.reap_children()


if __name__ == "__main__":
    test_main()
