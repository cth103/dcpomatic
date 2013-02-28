import glob
import os
from waflib import Logs

def pot(dir, sources, name, all = False):
    s = ""
    for f in sources.split('\n'):
        t = f.strip()
        if len(t) > 0:
            s += (os.path.join(dir, t)) + " "

    if all:
        Logs.info('Making %s.pot (extracting all)' % os.path.join('build', dir, name))
        os.system('xgettext -d %s -s --extract-all -p %s -o %s.pot %s' % (name, os.path.join('build', dir), name, s))
    else:
        Logs.info('Making %s.pot' % os.path.join('build', dir, name))
        os.system('xgettext -d %s -s --keyword=_ --add-comments=/ -p %s -o %s.pot %s' % (name, os.path.join('build', dir), name, s))
    

def po_to_mo(dir, name):
    for f in glob.glob(os.path.join(dir, 'po', '*.po')):
        
        lang = os.path.basename(f).replace('.po', '')
        out = os.path.join('build', dir, 'mo', lang, '%s.mo' % name)
        try:
            os.makedirs(os.path.dirname(out))
        except:
            pass

        os.system('msgfmt %s -o %s' % (f, out))
        Logs.info('%s -> %s' % (f, out))
