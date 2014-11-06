import subprocess
import os
import sys
import distutils
import distutils.spawn

APPNAME = 'dcpomatic'
VERSION = '2.0.19devel'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('winres')

    opt.add_option('--enable-debug',      action='store_true', default=False, help='build with debugging information and without optimisation')
    opt.add_option('--disable-gui',       action='store_true', default=False, help='disable building of GUI tools')
    opt.add_option('--disable-tests',     action='store_true', default=False, help='disable building of tests')
    opt.add_option('--target-windows',    action='store_true', default=False, help='set up to do a cross-compile to make a Windows package')
    opt.add_option('--target-debian',     action='store_true', default=False, help='set up to compile for a Debian/Ubuntu package')
    opt.add_option('--debian-unstable',   action='store_true', default=False, help='add extra libraries to static-build correctly on Debian unstable')
    opt.add_option('--target-centos-6',   action='store_true', default=False, help='set up to compile for a Centos 6.5 package')
    opt.add_option('--target-centos-7',   action='store_true', default=False, help='set up to compile for a Centos 7 package')
    opt.add_option('--magickpp-config',   action='store',      default='Magick++-config', help='path to Magick++-config')
    opt.add_option('--wx-config',         action='store',      default='wx-config', help='path to wx-config')
    opt.add_option('--address-sanitizer', action='store_true', default=False, help='build with address sanitizer')
    opt.add_option('--install-prefix',                         default=None,  help='prefix of where DCP-o-matic will be installed')

def static_ffmpeg(conf):
    conf.check_cfg(package='libavformat', args='--cflags', uselib_store='AVFORMAT', mandatory=True)
    conf.env.STLIB_AVFORMAT = ['avformat']
    conf.check_cfg(package='libavfilter', args='--cflags', uselib_store='AVFILTER', mandatory=True)
    conf.env.STLIB_AVFILTER = ['avfilter', 'swresample']
    conf.check_cfg(package='libavcodec', args='--cflags', uselib_store='AVCODEC', mandatory=True)
    # lzma link is needed by Centos 7, at least
    conf.env.STLIB_AVCODEC = ['avcodec']
    conf.env.LIB_AVCODEC = ['z', 'lzma']
    conf.check_cfg(package='libavutil', args='--cflags', uselib_store='AVUTIL', mandatory=True)
    conf.env.STLIB_AVUTIL = ['avutil']
    conf.check_cfg(package='libswscale', args='--cflags', uselib_store='SWSCALE', mandatory=True)
    conf.env.STLIB_SWSCALE = ['swscale']
    conf.check_cfg(package='libswresample', args='--cflags', uselib_store='SWRESAMPLE', mandatory=True)
    conf.env.STLIB_SWRESAMPLE = ['swresample']
    conf.check_cfg(package='libpostproc', args='--cflags', uselib_store='POSTPROC', mandatory=True)
    conf.env.STLIB_POSTPROC = ['postproc']

def dynamic_ffmpeg(conf):
    conf.check_cfg(package='libavformat', args='--cflags --libs', uselib_store='AVFORMAT', mandatory=True)
    conf.check_cfg(package='libavfilter', args='--cflags --libs', uselib_store='AVFILTER', mandatory=True)
    conf.check_cfg(package='libavcodec', args='--cflags --libs', uselib_store='AVCODEC', mandatory=True)
    conf.check_cfg(package='libavutil', args='--cflags --libs', uselib_store='AVUTIL', mandatory=True)
    conf.check_cfg(package='libswscale', args='--cflags --libs', uselib_store='SWSCALE', mandatory=True)
    conf.check_cfg(package='libswresample', args='--cflags --libs', uselib_store='SWRESAMPLE', mandatory=True)
    conf.check_cfg(package='libpostproc', args='--cflags --libs', uselib_store='POSTPROC', mandatory=True)

def static_openjpeg(conf):
    conf.check_cfg(package='libopenjpeg', args='--cflags', atleast_version='1.5.0', uselib_store='OPENJPEG', mandatory=True)
    conf.check_cfg(package='libopenjpeg', args='--cflags', max_version='1.5.2', mandatory=True)
    conf.env.STLIB_OPENJPEG = ['openjpeg']

def dynamic_openjpeg(conf):
    conf.check_cfg(package='libopenjpeg', args='--cflags --libs', atleast_version='1.5.0', uselib_store='OPENJPEG', mandatory=True)
    conf.check_cfg(package='libopenjpeg', args='--cflags --libs', max_version='1.5.2', mandatory=True)

def static_sub(conf):
    conf.check_cfg(package='libsub', atleast_version='0.01.0', args='--cflags', uselib_store='SUB', mandatory=True)
    conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]
    conf.env.STLIB_SUB = ['sub']

def static_dcp(conf, static_boost, static_xmlpp, static_xmlsec, static_ssh):
    conf.check_cfg(package='libdcp-1.0', atleast_version='1.0', args='--cflags', uselib_store='DCP', mandatory=True)
    conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]
    conf.env.STLIB_DCP = ['dcp-1.0', 'asdcp-libdcp-1.0', 'kumu-libdcp-1.0']
    conf.env.LIB_DCP = ['glibmm-2.4', 'ssl', 'crypto', 'bz2', 'xslt']

    if static_boost:
        conf.env.STLIB_DCP.append('boost_system')

    if static_xmlpp:
        conf.env.STLIB_DCP.append('xml++-2.6')
    else:
        conf.env.LIB_DCP.append('xml++-2.6')

    if static_xmlsec:
        conf.env.STLIB_DCP.append('xmlsec1-openssl')
        conf.env.STLIB_DCP.append('xmlsec1')
    else:
        conf.env.LIB_DCP.append('xmlsec1-openssl')
        conf.env.LIB_DCP.append('xmlsec1')

    if static_ssh:
        conf.env.STLIB_DCP.append('ssh')
    else:
        conf.env.LIB_DCP.append('ssh')

def dynamic_dcp(conf):
    conf.check_cfg(package='libdcp-1.0', atleast_version='0.92', args='--cflags --libs', uselib_store='DCP', mandatory=True)
    conf.env.DEFINES_DCP = [f.replace('\\', '') for f in conf.env.DEFINES_DCP]

def dynamic_sub(conf):
    conf.check_cfg(package='libsub', atleast_version='0.01.0', args='--cflags --libs', uselib_store='SUB', mandatory=True)
    conf.env.DEFINES_SUB = [f.replace('\\', '') for f in conf.env.DEFINES_SUB]

def dynamic_ssh(conf):
    conf.check_cc(fragment="""
                           #include <libssh/libssh.h>\n
                           int main () {\n
                           ssh_session s = ssh_new ();\n
                           return 0;\n
                           }
                           """, msg='Checking for library libssh', mandatory=True, lib='ssh', uselib_store='SSH')

def dynamic_boost(conf, lib_suffix, thread):
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
			    """, msg='Checking for boost threading library',
			    libpath='/usr/local/lib',
                            lib=[thread, 'boost_system%s' % lib_suffix],
                            uselib_store='BOOST_THREAD')

    conf.check_cxx(fragment="""
    			    #include <boost/filesystem.hpp>\n
    			    int main() { boost::filesystem::copy_file ("a", "b"); }\n
			    """, msg='Checking for boost filesystem library',
                            libpath='/usr/local/lib',
                            lib=['boost_filesystem%s' % lib_suffix, 'boost_system%s' % lib_suffix],
                            uselib_store='BOOST_FILESYSTEM')

    conf.check_cxx(fragment="""
    			    #include <boost/date_time.hpp>\n
    			    int main() { boost::gregorian::day_clock::local_day(); }\n
			    """, msg='Checking for boost datetime library',
                            libpath='/usr/local/lib',
                            lib=['boost_date_time%s' % lib_suffix, 'boost_system%s' % lib_suffix],
                            uselib_store='BOOST_DATETIME')

    conf.check_cxx(fragment="""
    			    #include <boost/signals2.hpp>\n
    			    int main() { boost::signals2::signal<void (int)> x; }\n
			    """,
                            msg='Checking for boost signals2 library',
                            uselib_store='BOOST_SIGNALS2')

def static_boost(conf, lib_suffix):
    conf.env.STLIB_BOOST_THREAD = ['boost_thread']
    conf.env.STLIB_BOOST_FILESYSTEM = ['boost_filesystem%s' % lib_suffix]
    conf.env.STLIB_BOOST_DATETIME = ['boost_date_time%s' % lib_suffix, 'boost_system%s' % lib_suffix]
    conf.env.STLIB_BOOST_SIGNALS2 = ['boost_signals2']

def configure(conf):
    conf.load('compiler_cxx')
    if conf.options.target_windows:
        conf.load('winres')

    # conf.options -> conf.env
    conf.env.TARGET_WINDOWS = conf.options.target_windows
    conf.env.DISABLE_GUI = conf.options.disable_gui
    conf.env.DISABLE_TESTS = conf.options.disable_tests
    conf.env.TARGET_DEBIAN = conf.options.target_debian
    conf.env.DEBIAN_UNSTABLE = conf.options.debian_unstable
    conf.env.TARGET_CENTOS_6 = conf.options.target_centos_6
    conf.env.TARGET_CENTOS_7 = conf.options.target_centos_7
    conf.env.VERSION = VERSION
    conf.env.TARGET_OSX = sys.platform == 'darwin'
    conf.env.TARGET_LINUX = not conf.env.TARGET_WINDOWS and not conf.env.TARGET_OSX
    # true if we should build dcpomatic/libdcpomatic/libdcpomatic-wx statically
    conf.env.BUILD_STATIC = conf.options.target_debian or conf.options.target_centos_6 or conf.options.target_centos_7
    if conf.options.install_prefix is None:
        conf.env.INSTALL_PREFIX = conf.env.PREFIX
    else:
        conf.env.INSTALL_PREFIX = conf.options.install_prefix

    # Common CXXFLAGS
    conf.env.append_value('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS', '-D__STDC_LIMIT_MACROS', '-msse', '-ffast-math', '-fno-strict-aliasing',
                                       '-Wall', '-Wno-attributes', '-Wextra', '-Wno-unused-result', '-D_FILE_OFFSET_BITS=64'])

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', ['-g', '-DDCPOMATIC_DEBUG'])
    else:
        conf.env.append_value('CXXFLAGS', '-O2')

    if conf.options.address_sanitizer:
        conf.env.append_value('CXXFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer'])
        conf.env.append_value('LINKFLAGS', ['-fsanitize=address'])

    #
    # Platform-specific CFLAGS hacks and other tinkering
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
        boost_thread = 'boost_thread_win32-mt'

    # POSIX
    if conf.env.TARGET_LINUX or conf.env.TARGET_OSX:
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_POSIX')
        conf.env.append_value('CXXFLAGS', '-DPOSIX_LOCALE_PREFIX="%s/share/locale"' % conf.env['INSTALL_PREFIX'])
        conf.env.append_value('CXXFLAGS', '-DPOSIX_ICON_PREFIX="%s/share/dcpomatic2"' % conf.env['INSTALL_PREFIX'])
        boost_lib_suffix = ''
        boost_thread = 'boost_thread'
        conf.env.append_value('LINKFLAGS', '-pthread')

    # Linux
    if conf.env.TARGET_LINUX:
        conf.env.append_value('CXXFLAGS', '-mfpmath=sse')
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_LINUX')

    if conf.env.TARGET_DEBIAN:
        # libxml2 seems to be linked against this on Ubuntu but it doesn't mention it in its .pc file
        conf.check_cfg(package='liblzma', args='--cflags --libs', uselib_store='LZMA', mandatory=True)

    if conf.env.TARGET_CENTOS_6 or conf.env.TARGET_CENTOS_7:
        # libavcodec seems to be linked against this on Centos
        conf.check_cfg(package='liblzma', args='--cflags --libs', uselib_store='LZMA', mandatory=True)
        
    if not conf.env.DISABLE_GUI and conf.env.TARGET_LINUX:
        conf.check_cfg(package='gtk+-2.0', args='--cflags --libs', uselib_store='GTK', mandatory=True)

    # OSX
    if conf.env.TARGET_OSX:
        conf.env.append_value('CXXFLAGS', ['-DDCPOMATIC_OSX', '-Wno-unused-function', '-Wno-unused-parameter'])
        conf.env.append_value('LINKFLAGS', '-headerpad_max_install_names')

    #
    # Dependencies.
    # There's probably a neater way of expressing these, but I've gone for brute force for now.
    #

    if conf.env.TARGET_DEBIAN:
        conf.check_cfg(package='libcxml', atleast_version='0.11', args='--cflags', uselib_store='CXML', mandatory=True)
        conf.env.STLIB_CXML = ['cxml']
        conf.check_cfg(package='libxml++-2.6', args='--cflags --libs', uselib_store='XMLPP', mandatory=True)
        conf.check_cfg(package='libcurl', args='--cflags --libs', uselib_store='CURL', mandatory=True)
        static_ffmpeg(conf)
        static_openjpeg(conf)
        static_sub(conf)
        static_dcp(conf, False, False, False, False)
        dynamic_boost(conf, boost_lib_suffix, boost_thread)

    if conf.env.TARGET_CENTOS_6:
        # Centos 6.5's boost is too old, so we build a new version statically in the chroot
        conf.check_cfg(package='libcxml', atleast_version='0.08', args='--cflags --libs-only-L', uselib_store='CXML', mandatory=True)
        conf.env.STLIB_CXML = ['cxml', 'boost_filesystem']
        conf.check_cfg(package='libcurl', args='--cflags --libs-only-L', uselib_store='CURL', mandatory=True)
        conf.env.STLIB_CURL = ['curl']
        conf.env.LIB_CURL = ['ssh2', 'idn']
        static_ffmpeg(conf)
        static_openjpeg(conf)
        static_sub(conf)
        static_dcp(conf, True, True, True, True)
        static_boost(conf, boost_lib_suffix)

    if conf.env.TARGET_CENTOS_7:
        # Centos 7's boost is ok so we link it dynamically
        conf.check_cfg(package='libcxml', atleast_version='0.08', args='--cflags', uselib_store='CXML', mandatory=True)
        conf.env.STLIB_CXML = ['cxml']
        conf.check_cfg(package='libcurl', args='--cflags --libs', uselib_store='CURL', mandatory=True)
        conf.env.LIB_SSH = ['gssapi_krb5']
        conf.env.LIB_XMLPP = ['xml2']
        conf.env.LIB_XMLSEC = ['ltdl']
        static_ffmpeg(conf)
        static_openjpeg(conf)
        static_sub(conf)
        static_dcp(conf, False, True, True, True)
        dynamic_boost(conf, boost_lib_suffix, boost_thread)

    if conf.env.TARGET_WINDOWS:
        conf.check_cfg(package='libxml++-2.6', args='--cflags --libs', uselib_store='XMLPP', mandatory=True)
        conf.check_cfg(package='libcurl', args='--cflags --libs', uselib_store='CURL', mandatory=True)
        conf.check_cxx(fragment="""
    			        #include <boost/locale.hpp>\n
    			        int main() { std::locale::global (boost::locale::generator().generate ("")); }\n
			        """, msg='Checking for boost locale library',
                                libpath='/usr/local/lib',
                                lib=['boost_locale%s' % boost_lib_suffix, 'boost_system%s' % boost_lib_suffix],
                                uselib_store='BOOST_LOCALE')
        dynamic_boost(conf, boost_lib_suffix, boost_thread)
        dynamic_ffmpeg(conf)
        dynamic_openjpeg(conf)
        dynamic_dcp(conf)
        dynamic_sub(conf)
        dynamic_ssh(conf)

    # Not packaging; just a straight build
    if not conf.env.TARGET_WINDOWS and not conf.env.TARGET_DEBIAN and not conf.env.TARGET_CENTOS_6 and not conf.env.TARGET_CENTOS_7:
        conf.check_cfg(package='libcxml', atleast_version='0.08', args='--cflags --libs', uselib_store='CXML', mandatory=True)
        conf.check_cfg(package='libxml++-2.6', args='--cflags --libs', uselib_store='XMLPP', mandatory=True)
        conf.check_cfg(package='libcurl', args='--cflags --libs', uselib_store='CURL', mandatory=True)
        dynamic_boost(conf, boost_lib_suffix, boost_thread)
        dynamic_ffmpeg(conf)
        dynamic_dcp(conf)
        dynamic_sub(conf)
        dynamic_openjpeg(conf)
        dynamic_ssh(conf)

    # Dependencies which are always dynamically linked
    conf.check_cfg(package='sndfile', args='--cflags --libs', uselib_store='SNDFILE', mandatory=True)
    conf.check_cfg(package='glib-2.0', args='--cflags --libs', uselib_store='GLIB', mandatory=True)
    if distutils.spawn.find_executable(conf.options.magickpp_config):
        conf.check_cfg(package='', path=conf.options.magickpp_config, args='--cppflags --cxxflags --libs', uselib_store='MAGICK', mandatory=True)
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_IMAGE_MAGICK')
    else:
        conf.check_cfg(package='GraphicsMagick++', args='--cflags --libs', uselib_store='MAGICK', mandatory=True)
        conf.env.append_value('CXXFLAGS', '-DDCPOMATIC_GRAPHICS_MAGICK')
        
    conf.check_cfg(package='libzip', args='--cflags --libs', uselib_store='ZIP', mandatory=True)
    conf.check_cfg(package='pangomm-1.4', args='--cflags --libs', uselib_store='PANGOMM', mandatory=True)
    conf.check_cfg(package='cairomm-1.0', args='--cflags --libs', uselib_store='CAIROMM', mandatory=True)

    conf.check_cc(fragment="""
                           #include <glib.h>
                           int main() { g_format_size (1); }
                           """, msg='Checking for g_format_size ()',
                           uselib='GLIB',
                           define_name='HAVE_G_FORMAT_SIZE',
                           mandatory=False)

    conf.find_program('msgfmt', var='MSGFMT')
    
    datadir = conf.env.DATADIR
    if not datadir:
        datadir = os.path.join(conf.env.PREFIX, 'share')
    
    conf.define('LOCALEDIR', os.path.join(datadir, 'locale'))
    conf.define('DATADIR', datadir)

    conf.recurse('src')
    if not conf.env.DISABLE_TESTS:
        conf.recurse('test')

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
        bld.install_files('${PREFIX}/share/icons/hicolor/%s/apps' % r, 'icons/%s/dcpomatic2.png' % r)

    if not bld.env.TARGET_WINDOWS:
        bld.install_files('${PREFIX}/share/dcpomatic2', 'icons/taskbar_icon.png')

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
    os.system('etags src/lib/*.cc src/lib/*.h src/wx/*.cc src/wx/*.h src/tools/*.cc src/tools/*.h test/*.cc')
