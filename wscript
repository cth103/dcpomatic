APPNAME = 'dvdomatic'
VERSION = '0.26pre'

def options(opt):
    opt.load('compiler_cxx')
    opt.add_option('--debug-hash', action='store_true', default = False, help = 'print hashes of data at various points')
    opt.add_option('--enable-debug', action='store_true', default = False, help = 'build with debugging information and without optimisation')
    opt.add_option('--disable-gui', action='store_true', default = False, help = 'disable building of GUI tools')

def configure(conf):
    conf.load('compiler_cxx')

    conf.env.append_value('CXXFLAGS', ['-D__STDC_CONSTANT_MACROS', '-D__STDC_LIMIT_MACROS', '-msse', '-mfpmath=sse', '-ffast-math', '-Wall'])
    conf.env.append_value('CXXFLAGS', ['-DDVDOMATIC_VERSION="%s"' % VERSION])

    conf.env.DEBUG_HASH = conf.options.debug_hash
    if conf.options.debug_hash:
        conf.env.append_value('CXXFLAGS', '-DDEBUG_HASH')
        conf.check_cc(msg = 'Checking for library libmhash', function_name = 'mhash_init', header_name = 'mhash.h', lib = 'mhash', uselib_store = 'MHASH')

    if conf.options.enable_debug:
        conf.env.append_value('CXXFLAGS', '-g')
    else:
        conf.env.append_value('CXXFLAGS', '-O3')

    conf.env.DISABLE_GUI = conf.options.disable_gui
    if conf.options.disable_gui is False:
        conf.check_cfg(package = 'glib-2.0', args = '--cflags --libs', uselib_store = 'GLIB', mandatory = True)
        conf.check_cfg(package = 'gtkmm-2.4', args = '--cflags --libs', uselib_store = 'GTKMM', mandatory = True)
        conf.check_cfg(package = 'cairomm-1.0', args = '--cflags --libs', uselib_store = 'CAIROMM', mandatory = True)

    conf.check_cfg(package = 'sigc++-2.0', args = '--cflags --libs', uselib_store = 'SIGC++', mandatory = True)
    conf.check_cfg(package = 'libavformat', args = '--cflags --libs', uselib_store = 'AVFORMAT', mandatory = True)
    conf.check_cfg(package = 'libavfilter', args = '--cflags --libs', uselib_store = 'AVFILTER', mandatory = True)
    conf.check_cfg(package = 'libavcodec', args = '--cflags --libs', uselib_store = 'AVCODEC', mandatory = True)
    conf.check_cfg(package = 'libavutil', args = '--cflags --libs', uselib_store = 'AVUTIL', mandatory = True)
    conf.check_cfg(package = 'libswscale', args = '--cflags --libs', uselib_store = 'SWSCALE', mandatory = True)
    conf.check_cfg(package = 'libswresample', args = '--cflags --libs', uselib_store = 'SWRESAMPLE', mandatory = True)
    conf.check_cfg(package = 'libpostproc', args = '--cflags --libs', uselib_store = 'POSTPROC', mandatory = True)
    conf.check_cfg(package = 'sndfile', args = '--cflags --libs', uselib_store = 'SNDFILE', mandatory = True)
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
                              """, msg = 'Checking for library libssh', lib = 'ssh', uselib_store = 'SSH')
			      
    conf.check_cxx(fragment = """
    			      #include <boost/thread.hpp>\n
    			      int main() { boost::thread t (); }\n
			      """, msg = 'Checking for boost threading library', lib = 'boost_thread', uselib_store = 'BOOST_THREAD')
    conf.check_cxx(fragment = """
    			      #include <boost/filesystem.hpp>\n
    			      int main() { boost::filesystem::copy_file ("a", "b"); }\n
			      """, msg = 'Checking for boost filesystem library', libpath = '/usr/local/lib', lib = ['boost_filesystem', 'boost_system'], uselib_store = 'BOOST_FILESYSTEM')
    conf.check_cxx(fragment = """
                              #define BOOST_TEST_MODULE Config test\n
    			      #include <boost/test/unit_test.hpp>\n
                              int main() {}
                              """, msg = 'Checking for boost unit testing library', lib = 'boost_unit_test_framework', uselib_store = 'BOOST_TEST')

def build(bld):
    bld.recurse('src')
    bld.recurse('test')

    d = { 'PREFIX' : '${PREFIX' }

    obj = bld(features = 'subst')
    obj.source = 'dvdomatic.desktop.in'
    obj.target = 'dvdomatic.desktop'
    obj.dict = d

    bld.install_files('${PREFIX}/share/applications', 'dvdomatic.desktop')
    for r in ['22x22', '32x32', '48x48', '64x64', '128x128']:
        bld.install_files('${PREFIX}/share/icons/hicolor/%s/apps' % r, 'icons/%s/dvdomatic.png' % r)


def dist(ctx):
    ctx.excl = 'TODO core *~ src/gtk/*~ src/lib/*~ .waf* build'
