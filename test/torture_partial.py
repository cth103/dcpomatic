#!/usr/bin/python

import sys
import os
import shutil
import random

import guessdcp

if len(sys.argv) < 2:
    print 'Syntax: %s <film>' % sys.argv[0]
    sys.exit(1)

film = sys.argv[1]

print 'Creating reference Film'
os.system('makedcp -n %s' % film)

videos = os.listdir(os.path.join(film, 'video'))
assert(len(videos) == 1)

full_size = os.path.getsize(os.path.join(film, 'video', videos[0]))
print 'Video MXF is %d bytes long' % full_size

while 1:
    film_copy = '%s-copy' % film

    try:
        shutil.rmtree(film_copy)
    except:
        pass

    print 'Copying %s to %s' % (film, film_copy)
    shutil.copytree(film, film_copy)
    old_dcp = guessdcp.path(film_copy)
    print 'Removing %s and log' % old_dcp
    shutil.rmtree(old_dcp)
    os.remove(os.path.join(film_copy, 'log'))

    truncated_size = random.randint(1, full_size)
    print 'Truncating video MXF to %d' % truncated_size
    videos = os.listdir(os.path.join(film_copy, 'video'))
    assert(len(videos) == 1)
    os.system('ls -l %s' % os.path.join(film_copy, 'video'))
    os.system('truncate %s --size %d' % (os.path.join(film_copy, 'video', videos[0]), truncated_size))
    os.system('ls -l %s' % os.path.join(film_copy, 'video'))

    print 'Rebuilding'
    os.system('makedcp -n %s' % film_copy)

    print 'Checking'
    r = os.system('dcpdiff %s %s' % (guessdcp.path(film), guessdcp.path(film_copy)))
    if r != 0:
        print 'FAIL'
        sys.exit(1)

    print 'OK'
    print

    print 'Deleting copy'
    shutil.rmtree(film_copy)
    
