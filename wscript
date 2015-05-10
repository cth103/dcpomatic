#
#    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

import subprocess
import os
import shlex
import sys
import distutils
import distutils.spawn
from waflib import Logs

APPNAME = 'dcpomatic'
VERSION = '2.0.46devel'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('winres')

    opt.add_option('--enable-debug',      action='store_true', default=False, help='build with debugging information and without optimisation')
    opt.add_option('--disable-gui',       action='store_true', default=False, help='disable building of GUI tools')
    opt.add_option('--disable-tests',     action='store_true', default=False, help='disable building of tests')
    opt.add_option('--install-prefix',                         default=None,  help='prefix of where DCP-o-matic will be installed')
    opt.add_option('--target-windows',    action='store_true', default=False, help='set up to do a cross-compile to make a Windows package')
    opt.add_option('--static-dcpomatic',  action='store_true', default=False, help='link to components of DCP-o-matic statically')
    opt.add_option('--static-boost',      action='store_true', default=False, help='link statically to Boost')
    opt.add_option('--static-openjpeg',   action='store_true', default=False, help='link statically to OpenJPEG')
    opt.add_option('--static-wxwidgets',  action='store_true', default=False, help='link statically to wxWidgets')
    opt.add_option('--static-ffmpeg',     action='store_true', default=False, help='link statically to FFmpeg')
    opt.add_option('--static-xmlpp',      action='store_true', default=False, help='link statically to libxml++')
    opt.add_option('--static-xmlsec',     action='store_true', default=False, help='link statically to xmlsec')
    opt.add_option('--static-ssh',        action='store_true', default=False, help='link statically to libssh')
    opt.add_option('--static-cxml',       action='store_true', default=False, help='link statically to libcxml')
    opt.add_option('--static-dcp',        action='store_true', default=False, help='link statically to libdcp')
    opt.add_option('--static-sub',        action='store_true', default=False, help='link statically to libsub')
    opt.add_option('--static-curl',       action='store_true', default=False, help='link statically to libcurl')
    opt.add_option('--workaround-gssapi', action='store_true', default=False, help='link to gssapi_krb5')

def configure(conf):
    conf.load('compiler_cxx')
    if conf.options.target_windows:
        conf.load('winres')

    # Save conf.options that we need elsewhere in conf.env
    conf.env.DISABLE_GUI = conf.options.disable_gui
    conf.env.DISABLE_TESTS = conf.options.disable_tests
    conf.env.TARGET_WINDOWS = conf.options.target_windows
    conf.env.TARGET_OSX = sys.platform == 'darwin'
    conf.env.TARGET_LINUX = not conf.env.TARGET_WINDOWS and not conf.env.TARGET_OSX
    conf.env.VERSION = VERSION
    conf.env.DEBUG = conf.options.enable_debug
    conf.env.STATIC_DCPOMATIC = conf.options.static_dcpomatic
    if conf.options.install_prefix is None:
        conf.env.INSTALL_PREFIX = conf.env.PREFIX
    else:
        conf.env.INSTALL_PREFIX = conf.options.install_prefix

    # Common CXXFLAGS
    conf.env.append_value('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS',
                                       '-D__STDC_LIMIT_MACROS',
                                       '-D__STDC_FORMAT_MACROS',
                                       '-msse',
                                       '-ffast-math',
                                       '-fno-strict-aliasing',
                                       '-Wall',
                                       '-Wno-attributes',
                                       '-Wextra',
                                       '-Wno-unused-result',
                                       '-D_FILE_OFFSET_BITS=64'])

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', ['-g', '-DDCPOMATIC_DEBUG'])
    else:
        conf.env.append_value('CXXFLAGS', '-O2')

    #
    # Windows/Linux/OS X specific
    #

    # Windows
    if conf.env.TARGET_WINDOWS:
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_WINDOWS')
        conf.env.append_value('CXXFLAGS', '-DWIN32_LEAN_AND_MEAN')
        conf.env.append_value('CXXFLAGS', '-DBOOST_USE_WINDOWS_H')
        conf.env.append_value('CXXFLAGS', '-DUNICODE')
        conf.env.append_value('CXXFLAGS', '-DBOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN')
        conf.env.append_value('CXXFLAGS', '-mfpmath=sse')
        conf.env.append_value('CXXFLAGS', '-Wno-deprecated-declarations')
        wxrc = os.popen('wx-config --rescomp').read().split()[1:]
        conf.env.append_value('WINRCFLAGS', wxrc)
        if conf.options.enable_debug:
            conf.env.append_value('CXXFLAGS', ['-mconsole'])
            conf.env.append_value('LINKFLAGS', ['-mconsole'])
        conf.check(lib='ws2_32', uselib_store='WINSOCK2', msg="Checking for library winsock2")
        conf.check(lib='bfd', uselib_store='BFD', msg="Checking for library bfd")
        conf.check(lib='dbghelp', uselib_store='DBGHELP', msg="Checking for library dbghelp")
        conf.check(lib='iberty', uselib_store='IBERTY', msg="Checking for library iberty")
        conf.check(lib='shlwapi', uselib_store='SHLWAPI', msg="Checking for library shlwapi")
        conf.check(lib='mswsock', uselib_store='MSWSOCK', msg="Checking for library mswsock")
        boost_lib_suffix = '-mt'
        boost_thread = 'boost_thread_win32-mt'
        conf.check_cxx(fragment="""
                               #include <boost/locale.hpp>\n
                               int main() { std::locale::global (boost::locale::generator().generate ("")); }\n
                               """,
                               msg='Checking for boost locale library',
                               libpath='/usr/local/lib',
                               lib=['boost_locale%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix],
                               uselib_store='BOOST_LOCALE')

    # POSIX
    if conf.env.TARGET_LINUX or conf.env.TARGET_OSX:
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_POSIX')
        boost_lib_suffix = ''
        boost_thread = 'boost_thread'
        conf.env.append_value('LINKFLAGS', '-pthread')

    # Linux
    if conf.env.TARGET_LINUX:
        conf.env.append_value('CXXFLAGS', '-mfpmath=sse')
        conf.env.append_value('CXXFLAGS', '-DLINUX_LOCALE_PREFIX="%s/share/locale"' % conf.env['INSTALL_PREFIX'])
        conf.env.append_value('CXXFLAGS', '-DLINUX_SHARE_PREFIX="%s/share/dcpomatic2"' % conf.env['INSTALL_PREFIX'])
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_LINUX')

    if not conf.env.DISABLE_GUI:
        conf.check_cfg(package='gtk+-2.0', args='--cflags --libs', uselib_store='GTK', mandatory=True)

    # OSX
    if conf.env.TARGET_OSX:
        conf.env.append_value('CXXFLAGS', ['-DDCPOMATIC_OSX', '-Wno-unused-function', '-Wno-unused-parameter'])
        conf.env.append_value('LINKFLAGS', '-headerpad_max_install_names')

    #
    # Dependencies.
    #

    # It should be possible to use check_cfg for both dynamic and static linking, but
    # e.g. pkg-config --libs --static foo returns some libraries that should be statically
    # linked and others that should be dynamic.  This doesn't work too well with waf
    # as it wants them separate.

    # libcurl
    if conf.options.static_curl:
        conf.env.STLIB_CURL = ['curl']
        conf.env.LIB_CURL = ['ssh2', 'idn']
    else:
        conf.check_cfg(package='libcurl', args='--cflags --libs', uselib_store='CURL', mandatory=True)


    # libsndfile
    conf.check_cfg(package='sndfile', args='--cflags --libs', uselib_store='SNDFILE', mandatory=True)

    # glib
    conf.check_cfg(package='glib-2.0', args='--cflags --libs', uselib_store='GLIB', mandatory=True)

    # ImageMagick / GraphicsMagick
    if distutils.spawn.find_executable('Magick++-config'):
        conf.check_cfg(package='', path='Magick++-config', args='--cppflags --cxxflags --libs', uselib_store='MAGICK', mandatory=True)
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_IMAGE_MAGICK')
    else:
        image = conf.check_cfg(package='ImageMagick++', args='--cflags --libs', uselib_store='MAGICK', mandatory=False)
        graphics = conf.check_cfg(package='GraphicsMagick++', args='--cflags --libs', uselib_store='MAGICK', mandatory=False)
        if image is None and graphics is None:
            Logs.pprint('RED', 'Neither ImageMagick++ nor GraphicsMagick++ found: one or the other is required')
        if image is not None:
            conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_IMAGE_MAGICK')
        if graphics is not None:
            conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_GRAPHICS_MAGICK')
        
    # libzip
    conf.check_cfg(package='libzip', args='--cflags --libs', uselib_store='ZIP', mandatory=True)

    # pangomm
    conf.check_cfg(package='pangomm-1.4', args='--cflags --libs', uselib_store='PANGOMM', mandatory=True)

    # cairomm
    conf.check_cfg(package='cairomm-1.0', args='--cflags --libs', uselib_store='CAIROMM', mandatory=True)

    # libcxml
    if conf.options.static_cxml:
        conf.check_cfg(package='libcxml', atleast_version='0.08', args='--cflags', uselib_store='CXML', mandatory=True)
        conf.env.STLIB_CXML = ['cxml']
    else:
        conf.check_cfg(package='libcxml', atleast_version='0.08', args='--cflags --libs', uselib_store='CXML', mandatory=True)

    # libssh
    if conf.options.static_ssh:
        conf.env.STLIB_SSH = ['ssh']
        if conf.options.workaround_gssapi:
            conf.env.LIB_SSH = ['gssapi_krb5']
    else:
        conf.check_cc(fragment="""
                               #include <libssh/libssh.h>\n
                               int main () {\n
                               ssh_session s = ssh_new ();\n
                               return 0;\n
                               }
                               """,
                      msg='Checking for library libssh',
                      mandatory=True,
                      lib='ssh',
                      uselib_store='SSH')

    # libdcp
    if conf.options.static_dcp:
        conf.check_cfg(package='libdcp-1.0', atleast_version='1.00.0', args='--cflags', uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]
        conf.env.STLIB_DCP = ['dcp-1.0', 'asdcp-libdcp-1.0', 'kumu-libdcp-1.0']
        conf.env.LIB_DCP = ['glibmm-2.4', 'ssl', 'crypto', 'bz2', 'xslt']
    else:
        conf.check_cfg(package='libdcp-1.0', atleast_version='1.00.0', args='--cflags --libs', uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]

    # libsub
    if conf.options.static_sub:
        conf.check_cfg(package='libsub-1.0', atleast_version='1.00.0', args='--cflags', uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]
        conf.env.STLIB_SUB = ['sub-1.0']
    else:
        conf.check_cfg(package='libsub-1.0', atleast_version='1.00.0', args='--cflags --libs', uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]

    # libxml++
    if conf.options.static_xmlpp:
        conf.env.STLIB_XMLPP = ['xml++-2.6']
    else:
        conf.check_cfg(package='libxml++-2.6', args='--cflags --libs', uselib_store='XMLPP', mandatory=True)

    # libxmlsec
    if conf.options.static_xmlsec:
        conf.env.STLIB_XMLSEC = ['xmlsec1-openssl', 'xmlsec1']
    else:
        conf.env.LIB_XMLSEC = ['xmlsec1-openssl', 'xmlsec1']

    # OpenJPEG
    if conf.options.static_openjpeg:
        conf.check_cfg(package='libopenjpeg', args='--cflags', atleast_version='1.5.0', uselib_store='OPENJPEG', mandatory=True)
        conf.check_cfg(package='libopenjpeg', args='--cflags', max_version='1.5.2', mandatory=True)
        conf.env.STLIB_OPENJPEG = ['openjpeg']
    else:
        conf.check_cfg(package='libopenjpeg', args='--cflags --libs', atleast_version='1.5.0', uselib_store='OPENJPEG', mandatory=True)
        conf.check_cfg(package='libopenjpeg', args='--cflags --libs', max_version='1.5.2', mandatory=True)

    # FFmpeg
    if conf.options.static_ffmpeg:
        names = ['avformat', 'avfilter', 'avcodec', 'avutil', 'swscale', 'swresample', 'postproc']
        for name in names:
            static = subprocess.Popen(shlex.split('pkg-config --static --libs lib%s' % name), stdout=subprocess.PIPE).communicate()[0]
            libs = []
            stlibs = []
            include = []
            libpath = []
            for s in static.split():
                if s.startswith('-L'):
                    libpath.append(s[2:])
                elif s.startswith('-I'):
                    include.append(s[2:])
                elif s.startswith('-l'):
                    if s[2:] not in names:
                        libs.append(s[2:])
                    else:
                        stlibs.append(s[2:])

            conf.env['LIB_%s' % name.upper()] = libs
            conf.env['STLIB_%s' % name.upper()] = stlibs
            conf.env['INCLUDE_%s' % name.upper()] = include
            conf.env['LIBPATH_%s' % name.upper()] = libpath
    else:
        conf.check_cfg(package='libavformat', args='--cflags --libs', uselib_store='AVFORMAT', mandatory=True)
        conf.check_cfg(package='libavfilter', args='--cflags --libs', uselib_store='AVFILTER', mandatory=True)
        conf.check_cfg(package='libavcodec', args='--cflags --libs', uselib_store='AVCODEC', mandatory=True)
        conf.check_cfg(package='libavutil', args='--cflags --libs', uselib_store='AVUTIL', mandatory=True)
        conf.check_cfg(package='libswscale', args='--cflags --libs', uselib_store='SWSCALE', mandatory=True)
        conf.check_cfg(package='libswresample', args='--cflags --libs', uselib_store='SWRESAMPLE', mandatory=True)
        conf.check_cfg(package='libpostproc', args='--cflags --libs', uselib_store='POSTPROC', mandatory=True)

    # Boost
    if conf.options.static_boost:
        conf.env.STLIB_BOOST_THREAD = ['boost_thread']
        conf.env.STLIB_BOOST_FILESYSTEM = ['boost_filesystem%s' % boost_lib_suffix]
        conf.env.STLIB_BOOST_DATETIME = ['boost_date_time%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix]
        conf.env.STLIB_BOOST_SIGNALS2 = ['boost_signals2']
        conf.env.STLIB_BOOST_SYSTEM = ['boost_system']
    else:
        conf.check_cxx(fragment="""
                            #include <boost/version.hpp>\n
                            #if BOOST_VERSION < 104500\n
                            #error boost too old\n
                            #endif\n
                            int main(void) { return 0; }\n
                            """,
                       mandatory=True,
                       msg='Checking for boost library >= 1.45',
                       okmsg='yes',
                       errmsg='too old\nPlease install boost version 1.45 or higher.')

        conf.check_cxx(fragment="""
    			    #include <boost/thread.hpp>\n
    			    int main() { boost::thread t (); }\n
			    """,
                       msg='Checking for boost threading library',
                       libpath='/usr/local/lib',
                       lib=[boost_thread, 'boost_system%s' % boost_lib_suffix],
                       uselib_store='BOOST_THREAD')

        conf.check_cxx(fragment="""
    			    #include <boost/filesystem.hpp>\n
    			    int main() { boost::filesystem::copy_file ("a", "b"); }\n
			    """,
                       msg='Checking for boost filesystem library',
                       libpath='/usr/local/lib',
                       lib=['boost_filesystem%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix],
                       uselib_store='BOOST_FILESYSTEM')

        conf.check_cxx(fragment="""
    			    #include <boost/date_time.hpp>\n
    			    int main() { boost::gregorian::day_clock::local_day(); }\n
			    """,
                       msg='Checking for boost datetime library',
                       libpath='/usr/local/lib',
                       lib=['boost_date_time%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix],
                       uselib_store='BOOST_DATETIME')

        conf.check_cxx(fragment="""
    			    #include <boost/signals2.hpp>\n
    			    int main() { boost::signals2::signal<void (int)> x; }\n
			    """,
                       msg='Checking for boost signals2 library',
                       uselib_store='BOOST_SIGNALS2')

    # Other stuff

    conf.find_program('msgfmt', var='MSGFMT')
    
    datadir = conf.env.DATADIR
    if not datadir:
        datadir = os.path.join(conf.env.PREFIX, 'share')
    
    conf.define('LOCALEDIR', os.path.join(datadir, 'locale'))
    conf.define('DATADIR', datadir)

    conf.recurse('src')
    if not conf.env.DISABLE_TESTS:
        conf.recurse('test')

    Logs.pprint('YELLOW', '')
    if conf.env.TARGET_WINDOWS:
        Logs.pprint('YELLOW', '\t' + 'Target'.ljust(25) + ': Windows')
    elif conf.env.TARGET_LINUX:
        Logs.pprint('YELLOW', '\t' + 'Target'.ljust(25) + ': Linux')
    elif conf.env.TARGET_OSX:
        Logs.pprint('YELLOW', '\t' + 'Target'.ljust(25) + ': OS X')

    def report(name, variable):
        linkage = ''
        if variable:
            linkage = 'static'
        else:
            linkage = 'dynamic'
        Logs.pprint('YELLOW', '\t%s: %s' % (name.ljust(25), linkage))

    report('DCP-o-matic libraries', conf.options.static_dcpomatic)
    report('Boost', conf.options.static_boost)
    report('OpenJPEG', conf.options.static_openjpeg)
    report('wxWidgets', conf.options.static_wxwidgets)
    report('FFmpeg', conf.options.static_ffmpeg)
    report('libxml++', conf.options.static_xmlpp)
    report('xmlsec', conf.options.static_xmlsec)
    report('libssh', conf.options.static_ssh)
    report('libcxml', conf.options.static_cxml)
    report('libdcp', conf.options.static_dcp)
    report('libcurl', conf.options.static_curl)

    Logs.pprint('YELLOW', '')

def build(bld):
    create_version_cc(VERSION, bld.env.CXXFLAGS)

    bld.recurse('src')
    if not bld.env.DISABLE_TESTS:
        bld.recurse('test')
    if bld.env.TARGET_WINDOWS:
        bld.recurse('platform/windows')
    if bld.env.TARGET_LINUX:
        bld.recurse('platform/linux')
    if bld.env.TARGET_OSX:
        bld.recurse('platform/osx')

    for r in ['22x22', '32x32', '48x48', '64x64', '128x128']:
        bld.install_files('${PREFIX}/share/icons/hicolor/%s/apps' % r, 'icons/%s/dcpomatic.png' % r)

    if not bld.env.TARGET_WINDOWS:
        bld.install_files('${PREFIX}/share/dcpomatic', 'icons/taskbar_icon.png')

    bld.add_post_fun(post)

def git_revision():
    if not os.path.exists('.git'):
        return None

    cmd = "LANG= git log --abbrev HEAD^..HEAD ."
    output = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE).communicate()[0].splitlines()
    o = output[0].decode('utf-8')
    return o.replace("commit ", "")[0:10]

def dist(ctx):
    r = git_revision()
    if r is not None:
        f = open('.git_revision', 'w')
        print >>f,r
    f.close()

    ctx.excl = """
               TODO core *~ src/wx/*~ src/lib/*~ builds/*~ doc/manual/*~ src/tools/*~ *.pyc .waf* build .git
               deps alignment hacks sync *.tar.bz2 *.exe .lock* *build-windows doc/manual/pdf doc/manual/html
               GRSYMS GRTAGS GSYMS GTAGS
               """

def create_version_cc(version, cxx_flags):
    commit = git_revision()
    if commit is None and os.path.exists('.git_revision'):
        f = open('.git_revision', 'r')
        commit = f.readline().strip()
    
    if commit is None:
        commit = 'release'

    try:
        text =  '#include "version.h"\n'
        text += 'char const * dcpomatic_git_commit = \"%s\";\n' % commit
        text += 'char const * dcpomatic_version = \"%s\";\n' % version

        t = ''
        for f in cxx_flags:
            f = f.replace('"', '\\"')
            t += f + ' '
        text += 'char const * dcpomatic_cxx_flags = \"%s\";\n' % t[:-1]

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

def pot(bld):
    bld.recurse('src')

def pot_merge(bld):
    bld.recurse('src')

def tags(bld):
    os.system('etags src/lib/*.cc src/lib/*.h src/wx/*.cc src/wx/*.h src/tools/*.cc src/tools/*.h')
