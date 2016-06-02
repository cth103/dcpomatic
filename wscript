#
#    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>
#
#    This file is part of DCP-o-matic.
#
#    DCP-o-matic is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    DCP-o-matic is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.
#

import subprocess
import os
import shlex
import sys
import glob
import distutils
import distutils.spawn
from waflib import Logs, Context

APPNAME = 'dcpomatic'
VERSION = '2.8.8'

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
                                       '-fno-strict-aliasing',
                                       '-Wall',
                                       '-Wno-attributes',
                                       '-Wextra',
                                       # Remove auto_ptr warnings from libxml++-2.6
                                       '-Wno-deprecated-declarations',
                                       '-D_FILE_OFFSET_BITS=64'])

    gcc = conf.env['CC_VERSION']
    if int(gcc[0]) >= 4 and int(gcc[1]) > 1:
        conf.env.append_value('CXXFLAGS', ['-Wno-unused-result'])

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', ['-g', '-DDCPOMATIC_DEBUG', '-fno-omit-frame-pointer'])
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
        boost_thread = 'boost_thread-mt'
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
        conf.env.append_value('CXXFLAGS', ['-DDCPOMATIC_OSX', '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-local-typedef'])
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
        conf.check_cfg(package='libcurl', args='--cflags --libs', atleast_version='7.19.1', uselib_store='CURL', mandatory=True)

    # libicu
    if conf.check_cfg(package='icu-i18n', args='--cflags --libs', uselib_store='ICU', mandatory=False) is None:
        if conf.check_cfg(package='icu', args='--cflags --libs', uselib_store='ICU', mandatory=False) is None:
            conf.check_cxx(fragment="""
                            #include <unicode/ucsdet.h>
                            int main(void) {
                                UErrorCode status = U_ZERO_ERROR;
                                UCharsetDetector* detector = ucsdet_open (&status);
                                return 0; }\n
                            """,
                       mandatory=True,
                       msg='Checking for libicu',
                       okmsg='yes',
                       libpath=['/usr/local/lib', '/usr/lib', '/usr/lib/x86_64-linux-gnu'],
                       lib=['icuio', 'icui18n', 'icudata', 'icuuc'],
                       uselib_store='ICU')

    # libsamplerate
    conf.check_cfg(package='samplerate', args='--cflags --libs', uselib_store='SAMPLERATE', mandatory=True)

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

    # See if we are using the MagickCore or MagickLib namespaces
    conf.check_cxx(fragment="""
                            #include <Magick++/Include.h>\n
                            using namespace MagickCore;\n
                            int main () { return 0; }\n
                            """,
                   mandatory=False,
                   msg='Checking for MagickCore namespace',
                   okmsg='yes',
                   includes=conf.env['INCLUDES_MAGICK'],
                   define_name='DCPOMATIC_HAVE_MAGICKCORE_NAMESPACE')

    conf.check_cxx(fragment="""
                            #include <Magick++/Include.h>\n
                            using namespace MagickLib;\n
                            int main () { return 0; }\n
                            """,
                   mandatory=False,
                   msg='Checking for MagickLib namespace',
                   okmsg='yes',
                   includes=conf.env['INCLUDES_MAGICK'],
                   define_name='DCPOMATIC_HAVE_MAGICKLIB_NAMESPACE')

    # libzip
    conf.check_cfg(package='libzip', args='--cflags --libs', uselib_store='ZIP', mandatory=True)

    # fontconfig
    conf.check_cfg(package='fontconfig', args='--cflags --libs', uselib_store='FONTCONFIG', mandatory=True)

    # pangomm
    conf.check_cfg(package='pangomm-1.4', args='--cflags --libs', uselib_store='PANGOMM', mandatory=True)

    # cairomm
    conf.check_cfg(package='cairomm-1.0', args='--cflags --libs', uselib_store='CAIROMM', mandatory=True)

    # See if we have Cairo::ImageSurface::format_stride_for_width
    conf.check_cxx(fragment="""
                            #include <cairomm/cairomm.h>
                            int main(void) {
                                Cairo::ImageSurface::format_stride_for_width (Cairo::FORMAT_ARGB, 1024);\n
                                return 0; }\n
                            """,
                       mandatory=False,
                       msg='Checking for format_stride_for_width',
                       okmsg='yes',
                       includes=conf.env['INCLUDES_CAIROMM'],
                       define_name='DCPOMATIC_HAVE_FORMAT_STRIDE_FOR_WIDTH')

    # libcxml
    if conf.options.static_cxml:
        conf.check_cfg(package='libcxml', atleast_version='0.15.1', args='--cflags', uselib_store='CXML', mandatory=True)
        conf.env.STLIB_CXML = ['cxml']
    else:
        conf.check_cfg(package='libcxml', atleast_version='0.15.1', args='--cflags --libs', uselib_store='CXML', mandatory=True)

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
        conf.check_cfg(package='libdcp-1.0', atleast_version='1.3.3', args='--cflags', uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]
        conf.env.STLIB_DCP = ['dcp-1.0', 'asdcp-cth', 'kumu-cth', 'openjp2']
        conf.env.LIB_DCP = ['glibmm-2.4', 'ssl', 'crypto', 'bz2', 'xslt']
    else:
        conf.check_cfg(package='libdcp-1.0', atleast_version='1.3.3', args='--cflags --libs', uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]

    # libsub
    if conf.options.static_sub:
        conf.check_cfg(package='libsub-1.0', atleast_version='1.1.12', args='--cflags', uselib_store='SUB', mandatory=True)
        conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]
        conf.env.STLIB_SUB = ['sub-1.0']
    else:
        conf.check_cfg(package='libsub-1.0', atleast_version='1.1.12', args='--cflags --libs', uselib_store='SUB', mandatory=True)
        conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]

    # libxml++
    if conf.options.static_xmlpp:
        conf.env.STLIB_XMLPP = ['xml++-2.6']
        conf.env.LIB_XMLPP = ['xml2']
    else:
        conf.check_cfg(package='libxml++-2.6', args='--cflags --libs', uselib_store='XMLPP', mandatory=True)

    # libxmlsec
    if conf.options.static_xmlsec:
        if conf.check_cxx(lib='xmlsec1-openssl', mandatory=False):
            conf.env.STLIB_XMLSEC = ['xmlsec1-openssl', 'xmlsec1']
        else:
            conf.env.STLIB_XMLSEC = ['xmlsec1']
    else:
        conf.env.LIB_XMLSEC = ['xmlsec1-openssl', 'xmlsec1']

    # FFmpeg
    if conf.options.static_ffmpeg:
        names = ['avformat', 'avfilter', 'avcodec', 'avutil', 'swscale', 'postproc']
        for name in names:
            static = subprocess.Popen(shlex.split('pkg-config --static --libs lib%s' % name), stdout=subprocess.PIPE).communicate()[0].decode('utf-8')
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
            conf.env['INCLUDES_%s' % name.upper()] = include
            conf.env['LIBPATH_%s' % name.upper()] = libpath
    else:
        conf.check_cfg(package='libavformat', args='--cflags --libs', uselib_store='AVFORMAT', mandatory=True)
        conf.check_cfg(package='libavfilter', args='--cflags --libs', uselib_store='AVFILTER', mandatory=True)
        conf.check_cfg(package='libavcodec', args='--cflags --libs', uselib_store='AVCODEC', mandatory=True)
        conf.check_cfg(package='libavutil', args='--cflags --libs', uselib_store='AVUTIL', mandatory=True)
        conf.check_cfg(package='libswscale', args='--cflags --libs', uselib_store='SWSCALE', mandatory=True)
        conf.check_cfg(package='libpostproc', args='--cflags --libs', uselib_store='POSTPROC', mandatory=True)

    # Check to see if we have our version of FFmpeg that allows us to get at EBUR128 results
    conf.check_cxx(fragment="""
                            extern "C" {\n
                            #include <libavfilter/f_ebur128.h>\n
                            }\n
                            int main () { av_ebur128_get_true_peaks (0); }\n
                            """,
                   msg='Checking for EBUR128-patched FFmpeg',
                   libpath=conf.env['LIBPATH_AVFORMAT'],
                   lib='avfilter avutil swresample',
                   includes=conf.env['INCLUDES_AVFORMAT'],
                   define_name='DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG',
                   mandatory=False)

    # Check to see if we have our AVSubtitleRect has a pict member
    # Older versions (e.g. that shipped with Ubuntu 16.04) do
    conf.check_cxx(fragment="""
                            extern "C" {\n
                            #include <libavcodec/avcodec.h>\n
                            }\n
                            int main () { AVSubtitleRect r; r.pict; }\n
                            """,
                   msg='Checking for AVSubtitleRect::pict',
                   cxxflags='-Wno-unused-result -Wno-unused-value -Wdeprecated-declarations -Werror',
                   libpath=conf.env['LIBPATH_AVCODEC'],
                   lib='avcodec',
                   includes=conf.env['INCLUDES_AVCODEC'],
                   define_name='DCPOMATIC_HAVE_AVSUBTITLERECT_PICT',
                   mandatory=False)

    # Check to see if we have our AVComponentDescriptor has a depth_minus1 member
    # Older versions (e.g. that shipped with Ubuntu 16.04) do
    conf.check_cxx(fragment="""
                            extern "C" {\n
                            #include <libavutil/pixdesc.h>\n
                            }\n
                            int main () { AVComponentDescriptor d; d.depth_minus1; }\n
                            """,
                   msg='Checking for AVComponentDescriptor::depth_minus1',
                   cxxflags='-Wno-unused-result -Wno-unused-value -Wdeprecated-declarations -Werror',
                   libpath=conf.env['LIBPATH_AVUTIL'],
                   lib='avutil',
                   includes=conf.env['INCLUDES_AVUTIL'],
                   define_name='DCPOMATIC_HAVE_AVCOMPONENTDESCRIPTOR_DEPTH_MINUS1',
                   mandatory=False)

    # Hack: the previous two check_cxx calls end up copying their (necessary) cxxflags
    # to these variables.  We don't want to use these for the actual build, so clearn them out.
    conf.env['CXXFLAGS_AVCODEC'] = []
    conf.env['CXXFLAGS_AVUTIL'] = []

    # Boost
    if conf.options.static_boost:
        conf.env.STLIB_BOOST_THREAD = ['boost_thread']
        conf.env.STLIB_BOOST_FILESYSTEM = ['boost_filesystem%s' % boost_lib_suffix]
        conf.env.STLIB_BOOST_DATETIME = ['boost_date_time%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix]
        conf.env.STLIB_BOOST_SIGNALS2 = ['boost_signals2']
        conf.env.STLIB_BOOST_SYSTEM = ['boost_system']
        conf.env.STLIB_BOOST_REGEX = ['boost_regex']
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

        conf.check_cxx(fragment="""
    			    #include <boost/regex.hpp>\n
    			    int main() { boost::regex re ("foo"); }\n
			    """,
                       msg='Checking for boost regex library',
                       lib=['boost_regex%s' % boost_lib_suffix],
                       uselib_store='BOOST_REGEX')

    # libxml++ requires glibmm and versions of glibmm 2.45.31 and later
    # must be built with -std=c++11 as they use c++11
    # features and c++11 is not (yet) the default in gcc.
    glibmm_version = conf.cmd_and_log(['pkg-config', '--modversion', 'glibmm-2.4'], output=Context.STDOUT, quiet=Context.BOTH)
    s = glibmm_version.split('.')
    v = (int(s[0]) << 16) | (int(s[1]) << 8) | int(s[2])
    if v >= 0x022D1F:
        conf.env.append_value('CXXFLAGS', '-std=c++11')

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
    bld.recurse('graphics')

    if not bld.env.DISABLE_TESTS:
        bld.recurse('test')
    if bld.env.TARGET_WINDOWS:
        bld.recurse('platform/windows')
    if bld.env.TARGET_LINUX:
        bld.recurse('platform/linux')
    if bld.env.TARGET_OSX:
        bld.recurse('platform/osx')

    if not bld.env.TARGET_WINDOWS:
        bld.install_files('${PREFIX}/share/dcpomatic2', 'fonts/LiberationSans-Regular.ttf')
        bld.install_files('${PREFIX}/share/dcpomatic2', 'fonts/LiberationSans-Italic.ttf')
        bld.install_files('${PREFIX}/share/dcpomatic2', 'fonts/LiberationSans-Bold.ttf')

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
    os.system('etags src/lib/*.cc src/lib/*.h src/wx/*.cc src/wx/*.h src/tools/*.cc')

def cppcheck(bld):
    os.system('cppcheck --enable=all --quiet .')
