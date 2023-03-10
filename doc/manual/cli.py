#!/usr/bin/python3
import subprocess
import os
import sys

print('<para>')
print('<itemizedlist>')

os.chdir('../..')
for l in subprocess.run(['run/%s' % sys.argv[1], '--help'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout.splitlines():
    l = l.strip().decode('UTF-8')
    if not l.startswith('-'):
        continue

    s = l.split()
    print('<listitem>', end='')

    e = 0
    n = 0
    if l.startswith('-') and not l.startswith('--'):
        print('<code>%s</code>, ' % s[0][:-1], end='')
        n = 1
        e = len(s[0]) + 2

    if l.find('<') != -1:
        print('<code>%s %s</code>' % (s[n], s[n+1].replace('<', '&lt;').replace('>', '&gt;')), end='')
        e += len(s[n]) + len(s[n+1]) + 1
    else:
        print('<code>%s</code>' % s[n], end='')
        e += len(s[n])

    desc = l[e:].strip()
    fixed_desc = ''
    t = False
    for i in desc:
        if i == '"' and t == False:
            fixed_desc += '&#8220;'
            t = True
        elif i == '"' and t == True:
            fixed_desc += '&#8221;'
            t = False
        else:
            fixed_desc += i

    print(' &#8212; %s</listitem>' % fixed_desc)

print('</itemizedlist>')
print('</para>', end='')
os.chdir('doc/manual')
