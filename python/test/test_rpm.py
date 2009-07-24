#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2009 Per Øyvind Karlsen <peroyvind@mandriva.org>
#
import sys, random
import rpm
import unittest
import os
from test.test_support import TESTFN

class TestRPM(unittest.TestCase):
    
    def setUp(self):
	self.labels = (("1", "2.1", "1", "2010.1"), ("1", "2.1", "2", "2010.1"))

    def test_labelCompare(self):
	le = self.labels[0][0:3]
	ge = self.labels[1][0:3]
	self.assertEqual(rpm.labelCompare(le, ge), -1)
	self.assertEqual(rpm.labelCompare(ge, le), 1)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)

    def test_labelCompareDISTEPOCH(self):
	le = self.labels[0]
	ge = self.labels[1]
	self.assertEqual(rpm.labelCompare(le, ge), -1)
	self.assertEqual(rpm.labelCompare(ge, le), 1)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)
	self.assertEqual(rpm.labelCompare(ge, ge), 0)

    def test_labelCompareNone(self):
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
    test_support.run_unittest(TestRPM)

if __name__ == "__main__":
    unittest.main()
