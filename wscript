import subprocess
import os
import sys

APPNAME = 'dvdomatic'
VERSION = '0.70pre'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('winres')

    opt.add_option('--enable-debug', action='store_true', default = False, help = 'build with debugging information and without optimisation')
    opt.add_option('--disable-gui', action='store_true', default = False, help = 'disable building of GUI tools')
    opt.add_option('--target-windows', action='store_true', default = False, help = 'set up to do a cross-compile to Windows')
    opt.add_option('--static', action='store_true', default = False, help = 'build statically, and link statically to libdcp and FFmpeg')

def configure(conf):
    conf.load('compiler_cxx')
    if conf.options.target_windows:
        conf.load('winres')

    conf.env.append_value('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS', '-msse', '-mfpmath=sse', '-ffast-math', '-fno-strict-aliasing', '-Wall', '-Wno-attributes', '-Wextra'])

    if conf.options.target_windows:
        conf.env.append_value('CXXFLAGS', ['-DDVDOMATIC_WINDOWS', '-DWIN32_LEAN_AND_MEAN', '-DBOOST_USE_WINDOWS_H', '-DUNICODE'])
        wxrc = os.popen('wx-config --rescomp').read().split()[1:]
        conf.env.append_value('WINRCFLAGS', wxrc)
        if conf.options.enable_debug:
            conf.env.append_value('CXXFLAGS', ['-mconsole'])
            conf.env.append_value('LINKFLAGS', ['-mconsole'])
        conf.check(lib = 'ws2_32', uselib_store = 'WINSOCK2', msg = "Checking for library winsock2")
        boost_lib_suffix = '-mt'
        boost_thread = 'boost_thread_win32-mt'
    else:
        conf.env.append_value('CXXFLAGS', '-DDVDOMATIC_POSIX')
        boost_lib_suffix = ''
        boost_thread = 'boost_thread'
        conf.env.append_value('LINKFLAGS', '-pthread')
        # libxml2 seems to be linked against this on Ubuntu, but it doesn't mention it in its .pc file
        conf.env.append_value('LIB', 'lzma')

    conf.env.TARGET_WINDOWS = conf.options.target_windows
    conf.env.DISABLE_GUI = conf.options.disable_gui
    conf.env.STATIC = conf.options.static
    conf.env.VERSION = VERSION

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', ['-g', '-DDVDOMATIC_DEBUG'])
    else:
        conf.env.append_value('CXXFLAGS', '-O2')

    if not conf.options.static:
        conf.check_cfg(package = 'libdcp', atleast_version = '0.34', args = '--cflags --libs', uselib_store = 'DCP', mandatory = True)
        conf.check_cfg(package = 'libavformat', args = '--cflags --libs', uselib_store = 'AVFORMAT', mandatory = True)
        conf.check_cfg(package = 'libavfilter', args = '--cflags --libs', uselib_store = 'AVFILTER', mandatory = True)
        conf.check_cfg(package = 'libavcodec', args = '--cflags --libs', uselib_store = 'AVCODEC', mandatory = True)
        conf.check_cfg(package = 'libavutil', args = '--cflags --libs', uselib_store = 'AVUTIL', mandatory = True)
        conf.check_cfg(package = 'libswscale', args = '--cflags --libs', uselib_store = 'SWSCALE', mandatory = True)
        conf.check_cfg(package = 'libswresample', args = '--cflags --libs', uselib_store = 'SWRESAMPLE', mandatory = False)
        conf.check_cfg(package = 'libpostproc', args = '--cflags --libs', uselib_store = 'POSTPROC', mandatory = True)
    else:
        # This is hackio grotesquio for static builds (ie for .deb packages).  We need to link some things
        # statically and some dynamically, or things get horribly confused and the dynamic linker (I think)
        # crashes horribly.  These calls do what the check_cfg calls would have done, but specify the
        # different bits as static or dynamic as required.  It'll break if you look at it funny, but
        # I think anyone else who builds would do so dynamically.
        conf.env.HAVE_DCP = 1
        conf.env.STLIB_DCP = ['dcp', 'asdcp-libdcp', 'kumu-libdcp']
        conf.env.LIB_DCP = ['glibmm-2.4', 'xml++-2.6', 'ssl', 'crypto', 'bz2']
        conf.env.HAVE_AVFORMAT = 1
        conf.env.STLIB_AVFORMAT = ['avformat']
        conf.env.HAVE_AVFILTER = 1
        conf.env.STLIB_AVFILTER = ['avfilter', 'swresample']
        conf.env.HAVE_AVCODEC = 1
        conf.env.STLIB_AVCODEC = ['avcodec']
        conf.env.LIB_AVCODEC = ['x264', 'z']
        conf.env.HAVE_AVUTIL = 1
        conf.env.STLIB_AVUTIL = ['avutil']
        conf.env.HAVE_SWSCALE = 1
        conf.env.STLIB_SWSCALE = ['swscale']
        conf.env.HAVE_SWRESAMPLE = 1
        conf.env.STLIB_SWRESAMPLE = ['swresample']
        conf.env.HAVE_POSTPROC = 1
        conf.env.STLIB_POSTPROC = ['postproc']

        # This doesn't seem to be set up, and we need it otherwise resampling support
        # won't be included.  Hack upon a hack, obviously
        conf.env.append_value('CXXFLAGS', ['-DHAVE_SWRESAMPLE=1'])

    conf.check_cfg(package = 'sndfile', args = '--cflags --libs', uselib_store = 'SNDFILE', mandatory = True)
    conf.check_cfg(package = 'glib-2.0', args = '--cflags --libs', uselib_store = 'GLIB', mandatory = True)
    conf.check_cfg(package = '', path = 'Magick++-config', args = '--cppflags --cxxflags --libs', uselib_store = 'MAGICK', mandatory = True)

    openjpeg_fragment = """
    			#include <stdio.h>\n
			#include <openjpeg.h>\n
			int main () {\n
			void* p = (void *) opj_image_create;\n
			return 0;\n
			}
			"""

    if conf.options.static:
        conf.check_cc(fragment = openjpeg_fragment, msg = 'Checking for library openjpeg', stlib = 'openjpeg', uselib_store = 'OPENJPEG')
    else:
        conf.check_cc(fragment = openjpeg_fragment, msg = 'Checking for library openjpeg', lib = 'openjpeg', uselib_store = 'OPENJPEG')

    conf.check_cc(fragment  = """
                              #include <libssh/libssh.h>\n
                              int main () {\n
                              ssh_session s = ssh_new ();\n
                              return 0;\n
                              }
                              """, msg = 'Checking for library libssh', mandatory = False, lib = 'ssh', uselib_store = 'SSH')

    conf.check_cxx(fragment = """
    			      #include <boost/thread.hpp>\n
    			      int main() { boost::thread t (); }\n
			      """, msg = 'Checking for boost threading library',
                              lib = [boost_thread, 'boost_system%s' % boost_lib_suffix],
                              uselib_store = 'BOOST_THREAD')

    conf.check_cxx(fragment = """
    			      #include <boost/filesystem.hpp>\n
    			      int main() { boost::filesystem::copy_file ("a", "b"); }\n
			      """, msg = 'Checking for boost filesystem library',
                              libpath = '/usr/local/lib',
                              lib = ['boost_filesystem%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix],
                              uselib_store = 'BOOST_FILESYSTEM')

    conf.check_cxx(fragment = """
    			      #include <boost/date_time.hpp>\n
    			      int main() { boost::gregorian::day_clock::local_day(); }\n
			      """, msg = 'Checking for boost datetime library',
                              libpath = '/usr/local/lib',
                              lib = ['boost_date_time%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix],
                              uselib_store = 'BOOST_DATETIME')

    conf.check_cxx(fragment = """
    			      #include <boost/signals2.hpp>\n
    			      int main() { boost::signals2::signal<void (int)> x; }\n
			      """,
                              msg = 'Checking for boost signals2 library',
                              uselib_store = 'BOOST_SIGNALS2')

    conf.check_cc(fragment = """
                             #include <glib.h>
                             int main() { g_format_size (1); }
                             """, msg = 'Checking for g_format_size ()',
                             uselib = 'GLIB',
                             define_name = 'HAVE_G_FORMAT_SIZE',
                             mandatory = False)

    conf.recurse('src')
    conf.recurse('test')

def build(bld):
    create_version_cc(VERSION)

    bld.recurse('src')
    bld.recurse('test')
    if bld.env.TARGET_WINDOWS:
        bld.recurse('windows')

    d = { 'PREFIX' : '${PREFIX' }

    obj = bld(features = 'subst')
    obj.source = 'dvdomatic.desktop.in'
    obj.target = 'dvdomatic.desktop'
    obj.dict = d

    bld.install_files('${PREFIX}/share/applications', 'dvdomatic.desktop')
    for r in ['22x22', '32x32', '48x48', '64x64', '128x128']:
        bld.install_files('${PREFIX}/share/icons/hicolor/%s/apps' % r, 'icons/%s/dvdomatic.png' % r)

    bld.add_post_fun(post)

def dist(ctx):
    ctx.excl = 'TODO core *~ src/wx/*~ src/lib/*~ .waf* build .git deps alignment hacks sync *.tar.bz2 *.exe .lock* *build-windows doc/manual/pdf doc/manual/html'

def create_version_cc(version):
    if os.path.exists('.git'):
        cmd = "LANG= git log --abbrev HEAD^..HEAD ."
        output = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE).communicate()[0].splitlines()
        o = output[0].decode('utf-8')
        commit = o.replace ("commit ", "")[0:10]
    else:
        commit = 'release'

    try:
        text =  '#include "version.h"\n'
        text += 'char const * dvdomatic_git_commit = \"%s\";\n' % commit
        text += 'char const * dvdomatic_version = \"%s\";\n' % version
        print('Writing version information to src/lib/version.cc')
        o = open('src/lib/version.cc', 'w')
        o.write(text)
        o.close()
    except IOError:
        print('Could not open src/lib/version.cc for writing\n')
        sys.exit(-1)
    
def post(ctx):
    if ctx.cmd == 'install':
        ctx.exec_command('/sbin/ldconfig')
