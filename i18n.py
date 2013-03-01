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
    

def po_to_mo(dir, name, bld):
    for f in glob.glob(os.path.join(os.getcwd(), dir, 'po', '*.po')):
        lang = os.path.basename(f).replace('.po', '')
        po = os.path.join('po', '%s.po' % lang)
        mo = os.path.join('mo', lang, '%s.mo' % name)

        bld(rule = 'msgfmt ${SRC} -o ${TGT}', source = bld.path.make_node(po), target = bld.path.get_bld().make_node(mo))
        bld.install_files(os.path.join('${PREFIX}', 'share', 'locale', lang, 'LC_MESSAGES'), mo)
