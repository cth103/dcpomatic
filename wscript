#
#    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>
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

from __future__ import print_function

import subprocess
import os
import shlex
import sys
import glob
import distutils
import distutils.spawn
try:
    # python 2
    from urllib import urlencode
except ImportError:
    # python 3
    from urllib.parse import urlencode
from waflib import Logs, Context

APPNAME = 'dcpomatic'
libdcp_version = '1.8.73'
libsub_version = '1.6.42'

this_version = subprocess.Popen(shlex.split('git tag -l --points-at HEAD'), stdout=subprocess.PIPE).communicate()[0]
last_version = subprocess.Popen(shlex.split('git describe --tags --match v* --abbrev=0'), stdout=subprocess.PIPE).communicate()[0]

# Python 2/3 compatibility; I don't really understand what's going on here
if not isinstance(this_version, str):
    this_version = this_version.decode('utf-8')
if not isinstance(last_version, str):
    last_version = last_version.decode('utf-8')

if this_version == '' or this_version == 'merged-to-main':
    VERSION = '%sdevel' % last_version[1:].strip()
else:
    VERSION = this_version[1:].strip()

def options(opt):
    opt.load('compiler_cxx')
    opt.load('winres')

    opt.add_option('--enable-debug',      action='store_true', default=False, help='build with debugging information and without optimisation')
    opt.add_option('--disable-gui',       action='store_true', default=False, help='disable building of GUI tools')
    opt.add_option('--disable-tests',     action='store_true', default=False, help='disable building of tests')
    opt.add_option('--target-windows-64', action='store_true', default=False, help='set up to do a cross-compile for Windows 64-bit')
    opt.add_option('--target-windows-32', action='store_true', default=False, help='set up to do a cross-compile for Windows 32-bit')
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
    opt.add_option('--use-lld',           action='store_true', default=False, help='use lld linker')
    opt.add_option('--enable-disk',       action='store_true', default=False, help='build dcpomatic2_disk tool; requires Boost process, lwext4 and nanomsg libraries')
    opt.add_option('--warnings-are-errors', action='store_true', default=False, help='build with -Werror')
    opt.add_option('--wx-config',         help='path to wx-config')
    opt.add_option('--enable-asan',       action='store_true', help='build with asan')
    opt.add_option('--disable-more-warnings', action='store_true', default=False, help='disable some warnings raised by Xcode 15 with the 2.16 branch')

def configure(conf):
    conf.load('compiler_cxx')
    conf.load('clang_compilation_database', tooldir=['waf-tools'])
    if conf.options.target_windows_64 or conf.options.target_windows_32:
        conf.load('winres')

    # Save conf.options that we need elsewhere in conf.env
    conf.env.DISABLE_GUI = conf.options.disable_gui
    conf.env.DISABLE_TESTS = conf.options.disable_tests
    conf.env.TARGET_WINDOWS_64 = conf.options.target_windows_64
    conf.env.TARGET_WINDOWS_32 = conf.options.target_windows_32
    conf.env.TARGET_OSX = sys.platform == 'darwin'
    conf.env.TARGET_LINUX = not conf.env.TARGET_WINDOWS_64 and not conf.env.TARGET_WINDOWS_32 and not conf.env.TARGET_OSX
    conf.env.VERSION = VERSION
    conf.env.DEBUG = conf.options.enable_debug
    conf.env.STATIC_DCPOMATIC = conf.options.static_dcpomatic
    conf.env.ENABLE_DISK = conf.options.enable_disk
    if conf.options.destdir == '':
        conf.env.INSTALL_PREFIX = conf.options.prefix
    else:
        conf.env.INSTALL_PREFIX = conf.options.destdir

    conf.check_cxx(cxxflags=['-msse', '-mfpmath=sse'], msg='Checking for SSE support', mandatory=False, define_name='SSE')

    # Common CXXFLAGS
    conf.env.append_value('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS',
                                       '-D__STDC_LIMIT_MACROS',
                                       '-D__STDC_FORMAT_MACROS',
                                       '-fno-strict-aliasing',
                                       '-Wall',
                                       '-Wextra',
                                       '-Wwrite-strings',
                                       '-Wno-error=deprecated',
                                       # I tried and failed to ignore these with _Pragma
                                       '-Wno-ignored-qualifiers',
                                       '-D_FILE_OFFSET_BITS=64',
                                       '-std=c++11'])

    if conf.options.disable_more_warnings:
        # These are for Xcode 15.0.1 with the v2.16.x-era
        # dependencies; maybe they aren't necessary when building
        # v2.1{7,8}.x
        conf.env.append_value('CXXFLAGS', ['-Wno-deprecated-builtins',
                                           '-Wno-deprecated-declarations',
                                           '-Wno-enum-constexpr-conversion',
                                           '-Wno-deprecated-copy'])

    if conf.options.warnings_are_errors:
        conf.env.append_value('CXXFLAGS', '-Werror')

    if conf.env.SSE:
        conf.env.append_value('CXXFLAGS', ['-msse', '-mfpmath=sse'])

    if conf.options.enable_asan:
        conf.env.append_value('CXXFLAGS', '-fsanitize=address')
        conf.env.append_value('LINKFLAGS', '-fsanitize=address')

    if conf.env['CXX_NAME'] == 'gcc':
        gcc = conf.env['CC_VERSION']
        if int(gcc[0]) >= 8:
            # I tried and failed to ignore these with _Pragma
            conf.env.append_value('CXXFLAGS', ['-Wno-cast-function-type'])
        # Most gccs still give these warnings from boost::optional
        conf.env.append_value('CXXFLAGS', ['-Wno-maybe-uninitialized'])
        if int(gcc[0]) > 4:
            # gcc 4.8.5 on Centos 7 does not have this warning
            conf.env.append_value('CXXFLAGS', ['-Wsuggest-override'])

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', ['-g', '-DDCPOMATIC_DEBUG', '-fno-omit-frame-pointer'])
    else:
        conf.env.append_value('CXXFLAGS', '-O2')

    if conf.options.enable_disk:
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_DISK')

    if conf.options.use_lld:
        try:
            conf.find_program('ld.lld')
            conf.env.append_value('LINKFLAGS', '-fuse-ld=lld')
        except conf.errors.ConfigurationError:
            pass

    #
    # Windows/Linux/macOS specific
    #

    # Windows
    if conf.env.TARGET_WINDOWS_64 or conf.env.TARGET_WINDOWS_32:
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_WINDOWS')
        conf.env.append_value('CXXFLAGS', '-DWIN32_LEAN_AND_MEAN')
        conf.env.append_value('CXXFLAGS', '-DBOOST_USE_WINDOWS_H')
        conf.env.append_value('CXXFLAGS', '-DBOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN')
        conf.env.append_value('CXXFLAGS', '-Wcast-align')
        wxrc = os.popen('wx-config --rescomp').read().split()[1:]
        conf.env.append_value('WINRCFLAGS', wxrc)
        if conf.options.enable_debug:
            conf.env.append_value('CXXFLAGS', ['-mconsole'])
            conf.env.append_value('LINKFLAGS', ['-mconsole'])
        conf.check(lib='ws2_32', uselib_store='WINSOCK2', msg="Checking for library winsock2")
        conf.check(lib='dbghelp', uselib_store='DBGHELP', msg="Checking for library dbghelp")
        conf.check(lib='shlwapi', uselib_store='SHLWAPI', msg="Checking for library shlwapi")
        conf.check(lib='mswsock', uselib_store='MSWSOCK', msg="Checking for library mswsock")
        conf.check(lib='ole32', uselib_store='OLE32', msg="Checking for library ole32")
        conf.check(lib='dsound', uselib_store='DSOUND', msg="Checking for library dsound")
        conf.check(lib='winmm', uselib_store='WINMM', msg="Checking for library winmm")
        conf.check(lib='ksuser', uselib_store='KSUSER', msg="Checking for library ksuser")
        conf.check(lib='setupapi', uselib_store='SETUPAPI', msg="Checking for library setupapi")
        conf.check(lib='uuid', uselib_store='UUID', msg="Checking for library uuid")
        boost_lib_suffix = '-mt-x32' if conf.options.target_windows_32 else '-mt-x64'
        boost_thread = 'boost_thread' + boost_lib_suffix
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
        conf.env.append_value('CXXFLAGS', '-DLINUX_LOCALE_PREFIX="%s/share/locale"' % conf.env['INSTALL_PREFIX'])
        conf.env.append_value('CXXFLAGS', '-DLINUX_SHARE_PREFIX="%s/share"' % conf.env['INSTALL_PREFIX'])
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_LINUX')
        conf.env.append_value('CXXFLAGS', ['-Wlogical-op', '-Wcast-align'])
        conf.check(lib='dl', uselib_store='DL', msg='Checking for library dl')

    # OSX
    if conf.env.TARGET_OSX:
        conf.env.append_value('CXXFLAGS', ['-DDCPOMATIC_OSX', '-DGL_SILENCE_DEPRECATION'])
        conf.env.append_value('LINKFLAGS', '-headerpad_max_install_names')
        conf.env.append_value('LINKFLAGS', '-llzma')

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
        conf.check_cfg(package='libcurl', args='libcurl >= 7.19.1 --cflags --libs', uselib_store='CURL', mandatory=True)

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

    # libzip
    conf.check_cfg(package='libzip', args='--cflags --libs', uselib_store='ZIP', mandatory=True)
    conf.check_cxx(fragment="""
                            #include <zip.h>
                            int main() { zip_source_t* foo; (void)foo; }
                            """,
                   mandatory=False,
                   msg="Checking for zip_source_t",
                   uselib="ZIP",
                   define_name='DCPOMATIC_HAVE_ZIP_SOURCE_T'
                   )
    conf.check_cxx(fragment="""
                            #include <zip.h>
                            int main() { struct zip* zip = nullptr; zip_source_t* source = nullptr; zip_file_add(zip, "foo", source, ZIP_FL_ENC_GUESS); }
                            """,
                   mandatory=False,
                   msg="Checking for zip_file_add",
                   uselib="ZIP",
                   define_name='DCPOMATIC_HAVE_ZIP_FILE_ADD'
                   )
    conf.check_cxx(fragment="""
                            #include <zip.h>
                            int main() { int error; zip_open("foo", ZIP_RDONLY, &error); }
                            """,
                   mandatory=False,
                   msg="Checking for ZIP_RDONLY",
                   uselib="ZIP",
                   define_name='DCPOMATIC_HAVE_ZIP_RDONLY'
                   )

    # libbz2; must be explicitly linked on macOS for some reason
    conf.check_cxx(fragment="""
                            #include <bzlib.h>
                            int main() { BZ2_bzCompressInit(0, 0, 0, 0); }
                            """,
                   mandatory=True,
                   msg="Checking for libbz2",
                   okmsg='yes',
                   lib='bz2',
                   uselib_store="BZ2"
                   )

    # libz; must be explicitly linked on macOS for some reason
    conf.check_cxx(fragment="""
                            #include <zlib.h>
                            int main() { zlibVersion(); }
                            """,
                   mandatory=True,
                   msg="Checking for libz",
                   okmsg='yes',
                   lib='z',
                   uselib_store="LIBZ"
                   )

    # fontconfig
    conf.check_cfg(package='fontconfig', args='--cflags --libs', uselib_store='FONTCONFIG', mandatory=True)

    # pangomm
    conf.check_cfg(package='pangomm-1.4', args='--cflags --libs', uselib_store='PANGOMM', mandatory=True)

    # cairomm
    conf.check_cfg(package='cairomm-1.0', args='--cflags --libs', uselib_store='CAIROMM', mandatory=True)

    # leqm_nrt
    conf.check_cfg(package='leqm_nrt', args='--cflags --libs', uselib_store='LEQM_NRT', mandatory=True)

    # libcxml
    if conf.options.static_cxml:
        conf.check_cfg(package='libcxml', args='libcxml >= 0.17.0 --cflags', uselib_store='CXML', mandatory=True)
        conf.env.STLIB_CXML = ['cxml']
    else:
        conf.check_cfg(package='libcxml', args='libcxml >= 0.16.0 --cflags --libs', uselib_store='CXML', mandatory=True)

    # libssh
    if conf.options.static_ssh:
        conf.env.STLIB_SSH = ['ssh']
        if conf.options.workaround_gssapi:
            conf.env.LIB_SSH = ['gssapi_krb5']
    else:
        conf.check_cxx(fragment="""
                               #include <libssh/libssh.h>\n
                               int main () {\n
                               ssh_new ();\n
                               return 0;\n
                               }
                               """,
                      msg='Checking for library libssh',
                      mandatory=True,
                      lib='ssh',
                      uselib_store='SSH')

    # libdcp
    if conf.options.static_dcp:
        conf.check_cfg(package='libdcp-1.0', args='libdcp-1.0 >= %s --cflags' % libdcp_version, uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]
        conf.env.STLIB_DCP = ['dcp-1.0', 'asdcp-carl', 'kumu-carl', 'openjp2']
        conf.env.LIB_DCP = ['glibmm-2.4', 'ssl', 'crypto', 'bz2', 'xslt', 'xerces-c']
    else:
        conf.check_cfg(package='libdcp-1.0', args='libdcp-1.0 >= %s --cflags --libs' % libdcp_version, uselib_store='DCP', mandatory=True)
        conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]

    # libsub
    if conf.options.static_sub:
        conf.check_cfg(package='libsub-1.0', args='libsub-1.0 >= %s --cflags' % libsub_version, uselib_store='SUB', mandatory=True)
        conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]
        conf.env.STLIB_SUB = ['sub-1.0']
    else:
        conf.check_cfg(package='libsub-1.0', args='libsub-1.0 >= %s --cflags --libs' % libsub_version, uselib_store='SUB', mandatory=True)
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

    # nettle
    conf.check_cfg(package="nettle", args='--cflags --libs', uselib_store='NETTLE', mandatory=True)

    # libpng
    conf.check_cfg(package='libpng', args='--cflags --libs', uselib_store='PNG', mandatory=True)

    # libjpeg
    conf.check_cxx(fragment="""
                            #include <cstddef>
                            #include <cstdio>
                            #include <jpeglib.h>
                            int main() { struct jpeg_compress_struct compress; jpeg_create_compress (&compress); return 0; }
                            """,
                   msg='Checking for libjpeg',
                   libpath='/usr/local/lib',
                   lib=['jpeg'],
                   uselib_store='JPEG')

    # lwext4
    if conf.options.enable_disk:
        conf.check_cxx(fragment="""
                                #include <lwext4/ext4.h>\n
                                int main() { ext4_mount("ext4_fs", "/mp/", false); }\n
                                """,
                                msg='Checking for lwext4 library',
                                libpath='/usr/local/lib',
                                lib=['lwext4', 'blockdev'],
                                uselib_store='LWEXT4')

    if conf.env.TARGET_LINUX and conf.options.enable_disk:
        conf.check_cfg(package='polkit-gobject-1', args='--cflags --libs', uselib_store='POLKIT', mandatory=True)

    # nanomsg
    if conf.options.enable_disk:
        if conf.check_cfg(package='nanomsg', args='--cflags --libs', uselib_store='NANOMSG', mandatory=False) is None:
            conf.check_cfg(package='libnanomsg', args='--cflags --libs', uselib_store='NANOMSG', mandatory=True)
        if conf.env.TARGET_LINUX:
            # We link with nanomsg statically on Centos 8 so we need to link this as well
            conf.env.LIB_NANOMSG.append('anl')

    # FFmpeg
    if conf.options.static_ffmpeg:
        names = ['avformat', 'avfilter', 'avcodec', 'avutil', 'swscale', 'postproc', 'swresample']
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
        conf.check_cfg(package='libswresample', args='--cflags --libs', uselib_store='SWRESAMPLE', mandatory=True)

    # Check to see if we have our version of FFmpeg that allows us to get at EBUR128 results
    conf.check_cxx(fragment="""
                            extern "C" {\n
                            #include <libavfilter/f_ebur128.h>\n
                            }\n
                            int main () { av_ebur128_get_true_peaks (0); }\n
                            """,
                   msg='Checking for EBUR128-patched FFmpeg',
                   uselib='AVCODEC AVFILTER',
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
                   uselib='AVCODEC',
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
                   uselib='AVUTIL',
                   define_name='DCPOMATIC_HAVE_AVCOMPONENTDESCRIPTOR_DEPTH_MINUS1',
                   mandatory=False)

    # See if we have av_register_all and avfilter_register_all
    conf.check_cxx(fragment="""
                            extern "C" {\n
                            #include <libavformat/avformat.h>\n
                            #include <libavfilter/avfilter.h>\n
                            }\n
                            int main () { av_register_all(); avfilter_register_all(); }\n
                            """,
                   msg='Checking for av_register_all and avfilter_register_all',
                   uselib='AVFORMAT AVFILTER',
                   define_name='DCPOMATIC_HAVE_AVREGISTER',
                   mandatory=False)

    # Hack: the previous two check_cxx calls end up copying their (necessary) cxxflags
    # to these variables.  We don't want to use these for the actual build, so clean them out.
    conf.env['CXXFLAGS_AVCODEC'] = []
    conf.env['CXXFLAGS_AVUTIL'] = []

    if conf.env.TARGET_LINUX:
        conf.env.LIB_X11 = ['X11']

    # We support older boosts on Linux so we can use the distribution-provided package
    # on Centos 7, but it's good if we can use 1.61 for boost::dll::program_location()
    boost_version = ('1.45', '104500') if conf.env.TARGET_LINUX else ('1.61', '106800')

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
                            #if BOOST_VERSION < %s\n
                            #error boost too old\n
                            #endif\n
                            int main(void) { return 0; }\n
                            """ % boost_version[1],
                       mandatory=True,
                       msg='Checking for boost library >= %s' % boost_version[0],
                       okmsg='yes',
                       errmsg='too old\nPlease install boost version %s or higher.' % boost_version[0])

        conf.check_cxx(fragment="""
    			    #include <boost/thread.hpp>\n
			    int main() { boost::thread t; }\n
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

        # Really just checking for the header here (there's no associated library) but the test
        # program has to link with boost_system so I'm doing it this way.
        if conf.options.enable_disk:
            deps = ['boost_system%s' % boost_lib_suffix]
            if conf.env.TARGET_WINDOWS_64 or conf.env.TARGET_WINDOWS_32:
                deps.append('ws2_32')
                deps.append('boost_filesystem%s' % boost_lib_suffix)
            conf.check_cxx(fragment="""
                                #include <boost/process.hpp>\n
                                int main() { new boost::process::child("foo"); }\n
                                """,
                           cxxflags='-Wno-unused-parameter',
                           msg='Checking for boost process library',
                           lib=deps,
                           uselib_store='BOOST_PROCESS')

    # Other stuff

    conf.find_program('msgfmt', var='MSGFMT')
    conf.check(header_name='valgrind/memcheck.h', mandatory=False)

    datadir = conf.env.DATADIR
    if not datadir:
        datadir = os.path.join(conf.env.PREFIX, 'share')

    conf.define('LOCALEDIR', os.path.join(datadir, 'locale'))
    conf.define('DATADIR', datadir)

    conf.recurse('src')
    if not conf.env.DISABLE_TESTS:
        conf.recurse('test')

    Logs.pprint('YELLOW', '')
    if conf.env.TARGET_WINDOWS_64 or conf.env.TARGET_WINDOWS_32:
        Logs.pprint('YELLOW', '\t' + 'Target'.ljust(25) + ': Windows')
    elif conf.env.TARGET_LINUX:
        Logs.pprint('YELLOW', '\t' + 'Target'.ljust(25) + ': Linux')
    elif conf.env.TARGET_OSX:
        Logs.pprint('YELLOW', '\t' + 'Target'.ljust(25) + ': macOS')

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
    if bld.env.TARGET_WINDOWS_64 or bld.env.TARGET_WINDOWS_32:
        bld.recurse('platform/windows')
    if bld.env.TARGET_LINUX:
        bld.recurse('platform/linux')
    if bld.env.TARGET_OSX:
        bld.recurse('platform/osx')

    if not bld.env.TARGET_WINDOWS_64 and not bld.env.TARGET_WINDOWS_32:
        bld.install_files('${PREFIX}/share/dcpomatic2', 'fonts/LiberationSans-Regular.ttf')
        bld.install_files('${PREFIX}/share/dcpomatic2', 'fonts/LiberationSans-Italic.ttf')
        bld.install_files('${PREFIX}/share/dcpomatic2', 'fonts/LiberationSans-Bold.ttf')

    bld.add_post_fun(post)

def git_revision():
    if not os.path.exists('.git'):
        return None

    cmd = "LANG= git log --abbrev HEAD^..HEAD ."
    output = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE).communicate()[0].splitlines()
    if len(output) == 0:
        return None
    o = output[0].decode('utf-8')
    return o.replace("commit ", "")[0:10]

def dist(ctx):
    r = git_revision()
    if r is not None:
        f = open('.git_revision', 'w')
        print(r, file=f)
        f.close()

    ctx.excl = """
               TODO core *~ src/wx/*~ src/lib/*~ builds/*~ doc/manual/*~ src/tools/*~ *.pyc .waf* build .git
               deps alignment hacks sync *.tar.bz2 *.exe .lock* *build-windows doc/manual/pdf doc/manual/html
               GRSYMS GRTAGS GSYMS GTAGS compile_commands.json
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
    if ctx.cmd == 'install' and ctx.env.TARGET_LINUX:
        ctx.exec_command('/sbin/ldconfig')
        exe = os.path.join(ctx.env['INSTALL_PREFIX'], 'bin/dcpomatic2_disk_writer')
        if os.path.exists(exe):
            os.system('setcap "cap_dac_override+ep cap_sys_admin+ep" %s' % exe)

def pot(bld):
    bld.recurse('src')

def pot_merge(bld):
    bld.recurse('src')

def supporters(bld):
    r = os.system('curl -m 2 -s -f https://dcpomatic.com/supporters.cc > src/wx/supporters.cc')
    if (r >> 8) == 0:
        r = os.system('curl -s -f https://dcpomatic.com/subscribers.cc > src/wx/subscribers.cc')
    if (r >> 8) != 0:
        raise Exception("Could not download supporters lists (%d)" % (r >> 8))

def tags(bld):
    os.system('etags src/lib/*.cc src/lib/*.h src/wx/*.cc src/wx/*.h src/tools/*.cc')

def cppcheck(bld):
    os.system('cppcheck --enable=all --quiet .')
