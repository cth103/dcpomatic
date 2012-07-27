import subprocess
import os
import sys

APPNAME = 'dvdomatic'
VERSION = '0.30pre'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('winres')

    opt.add_option('--debug-hash', action='store_true', default = False, help = 'print hashes of data at various points')
    opt.add_option('--enable-debug', action='store_true', default = False, help = 'build with debugging information and without optimisation')
    opt.add_option('--disable-gui', action='store_true', default = False, help = 'disable building of GUI tools')
    opt.add_option('--disable-player', action='store_true', default = False, help = 'disable building of the player components')
    opt.add_option('--ffmpeg-083', action='store_true', default = False, help = 'Use FFmpeg version in Ubuntu 12.04')
    opt.add_option('--target-windows', action='store_true', default = False, help = 'set up to do a cross-compile to Windows')

def configure(conf):
    conf.load('compiler_cxx')
    if conf.options.target_windows:
        conf.load('winres')

    conf.env.append_value('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS', '-msse', '-mfpmath=sse', '-ffast-math', '-fno-strict-aliasing', '-Wall', '-Wno-attributes'])
    conf.env.append_value('CXXFLAGS', ['-DDVDOMATIC_VERSION="%s"' % VERSION])

    if conf.options.target_windows:
        conf.env.append_value('CXXFLAGS', ['-DDVDOMATIC_WINDOWS', '-DWIN32_LEAN_AND_MEAN'])
        conf.options.disable_player = True
        conf.check(lib = 'ws2_32', uselib_store = 'WINSOCK2', msg = "Checking for library winsock2")
        boost_lib_suffix = '-mt'
        boost_thread = 'boost_thread_win32-mt'
    else:
        conf.env.append_value('CXXFLAGS', '-DDVDOMATIC_POSIX')
        boost_lib_suffix = ''
        boost_thread = 'boost_thread'

    conf.env.DEBUG_HASH = conf.options.debug_hash
    conf.env.TARGET_WINDOWS = conf.options.target_windows
    conf.env.DISABLE_GUI = conf.options.disable_gui
    conf.env.DISABLE_PLAYER = conf.options.disable_player
    conf.env.VERSION = VERSION

    if conf.options.disable_player:
        conf.env.append_value('CXXFLAGS', '-DDVDOMATIC_DISABLE_PLAYER')

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', '-g')
    else:
        conf.env.append_value('CXXFLAGS', '-O3')

    if conf.options.ffmpeg_083:
        conf.env.append_value('CXXFLAGS', '-DDVDOMATIC_FFMPEG_0_8_3')

    conf.check_cfg(package = 'sigc++-2.0', args = '--cflags --libs', uselib_store = 'SIGC++', mandatory = True)
    conf.check_cfg(package = 'libavformat', args = '--cflags --libs', uselib_store = 'AVFORMAT', mandatory = True)
    conf.check_cfg(package = 'libavfilter', args = '--cflags --libs', uselib_store = 'AVFILTER', mandatory = True)
    conf.check_cfg(package = 'libavcodec', args = '--cflags --libs', uselib_store = 'AVCODEC', mandatory = True)
    conf.check_cfg(package = 'libavutil', args = '--cflags --libs', uselib_store = 'AVUTIL', mandatory = True)
    conf.check_cfg(package = 'libswscale', args = '--cflags --libs', uselib_store = 'SWSCALE', mandatory = True)
    conf.check_cfg(package = 'libswresample', args = '--cflags --libs', uselib_store = 'SWRESAMPLE', mandatory = True)
    conf.check_cfg(package = 'libpostproc', args = '--cflags --libs', uselib_store = 'POSTPROC', mandatory = True)
    conf.check_cfg(package = 'sndfile', args = '--cflags --libs', uselib_store = 'SNDFILE', mandatory = True)
    conf.check_cfg(package = 'libdcp', args = '--cflags --libs', uselib_store = 'DCP', mandatory = True)
    conf.check_cfg(package = 'glib-2.0', args = '--cflags --libs', uselib_store = 'GLIB', mandatory = True)
    conf.check_cfg(package = '', path = 'Magick++-config', args = '--cppflags --cxxflags --libs', uselib_store = 'MAGICK', mandatory = True)
    conf.check_cc(msg = 'Checking for library libtiff', function_name = 'TIFFOpen', header_name = 'tiffio.h', lib = 'tiff', uselib_store = 'TIFF')
    conf.check_cc(fragment  = """
    			      #include <stdio.h>\n
			      #include <openjpeg.h>\n
			      int main () {\n
			      void* p = (void *) opj_image_create;\n
			      return 0;\n
			      }
			      """, msg = 'Checking for library openjpeg', lib = 'openjpeg', uselib_store = 'OPENJPEG')

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

def dist(ctx):
    ctx.excl = 'TODO core *~ src/gtk/*~ src/lib/*~ .waf* build .git'

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
    
