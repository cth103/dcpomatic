#!/usr/bin/python

import os
import sys
import datetime
import shutil
import copy

class Version:
    def __init__(self, s):
        self.pre = False
        self.beta = None

        if s.startswith("'"):
            s = s[1:]
        if s.endswith("'"):
            s = s[0:-1]
        
        if s.endswith('pre'):
            s = s[0:-3]
            self.pre = True

        b = s.find("beta")
        if b != -1:
            self.beta = int(s[b+4:])

        p = s.split('.')
        self.major = int(p[0])
        self.minor = int(p[1])

    def bump(self):
        self.minor += 1
        self.pre = False
        self.beta = None

    def bump_and_to_pre(self):
        self.bump()
        self.pre = True
        self.beta = None

    def to_release(self):
        self.pre = False
        self.beta = None

    def bump_beta(self):
        if self.pre:
            self.pre = False
            self.beta = 1
        elif self.beta is not None:
            self.beta += 1
        elif self.beta is None:
            self.beta = 1

    def __str__(self):
        s = '%d.%02d' % (self.major, self.minor)
        if self.beta is not None:
            s += 'beta%d' % self.beta
        elif self.pre:
            s += 'pre'

        return s
        
def rewrite_wscript(method):
    f = open('wscript', 'rw')
    o = open('wscript.tmp', 'w')
    version = None
    while 1:
        l = f.readline()
        if l == '':
            break

        s = l.split()
        if len(s) == 3 and s[0] == "VERSION":
            version = Version(s[2])
            method(version)
            print "Writing %s" % version
            print >>o,"VERSION = '%s'" % version
        else:
            print >>o,l,
    f.close()
    o.close()

    os.rename('wscript.tmp', 'wscript')
    return version

def append_to_changelog(version):
    f = open('ChangeLog', 'r')
    c = f.read()
    f.close()

    f = open('ChangeLog', 'w')
    now = datetime.datetime.now()
    f.write('%d-%02d-%02d  Carl Hetherington  <cth@carlh.net>\n\n\t* Version %s released.\n\n' % (now.year, now.month, now.day, version))
    f.write(c)
