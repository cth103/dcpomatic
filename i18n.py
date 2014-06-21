import glob
import os
from waflib import Logs

def command(c):
    print(c)
    os.system(c)

def pot(dir, sources, name):
    s = ""
    for f in sources.split():
        t = f.strip()
        if len(t) > 0:
            s += (os.path.join(dir, t)) + " "

    Logs.info('Making %s.pot' % os.path.join('build', dir, name))
    d = os.path.join('build', dir)
    try:
        os.makedirs(d)
    except:
        pass

    command('xgettext -d %s -s --keyword=_ --keyword=S_ --add-comments=/ -p %s -o %s.pot %s' % (name, d, name, s))

def pot_merge(dir, name):
    for f in glob.glob(os.path.join(os.getcwd(), dir, 'po', '*.po')):
        command('msgmerge %s %s.pot -o %s' % (f, os.path.join('build', dir, name), f))

def po_to_mo(dir, name, bld):
    for f in glob.glob(os.path.join(os.getcwd(), dir, 'po', '*.po')):
        lang = os.path.basename(f).replace('.po', '')
        po = os.path.join('po', '%s.po' % lang)
        mo = os.path.join('mo', lang, '%s.mo' % name)

        bld(rule = 'msgfmt -f ${SRC} -o ${TGT}', source = bld.path.make_node(po), target = bld.path.get_bld().make_node(mo))
        bld.install_files(os.path.join('${PREFIX}', 'share', 'locale', lang, 'LC_MESSAGES'), mo)
