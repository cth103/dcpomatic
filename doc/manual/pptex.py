#!/usr/bin/python

# Farcical script to remove newlines after
# \begin{sidebar} in dblatex' .tex output;
#
# farcical because I'm sure this can be done
# in 1 line of sed, and also because I'm sure
# the whole need for this script could be fixed
# in the ardour.sty (but I don't know how).

import sys
import os
import tempfile
import shutil

f = open(sys.argv[1])
t = tempfile.NamedTemporaryFile(delete = False)
remove_next = False
while 1:
    l = f.readline()
    if l == '':
        break

    if not remove_next:
        print>>t,l,

    remove_next = False

    if l.strip() == '\\begin{sidebar}':
        remove_next = True

f.close()
t.close()
shutil.move(t.name, sys.argv[1])

