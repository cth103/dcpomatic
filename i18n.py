import glob
import os
from waflib import Logs

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
