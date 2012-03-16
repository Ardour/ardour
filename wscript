#!/usr/bin/env python
from waflib.extras import autowaf as autowaf
from waflib import Options
import os
import re
import string
import subprocess
import sys

# Variables for 'waf dist'
VERSION = '3.0beta3'
APPNAME = 'Ardour3'

# Mandatory variables
top = '.'
out = 'build'

children = [
        'libs/pbd',
        'libs/midi++2',
        'libs/evoral',
        'libs/vamp-sdk',
        'libs/qm-dsp',
        'libs/vamp-plugins',
        'libs/taglib',
        'libs/rubberband',
        'libs/surfaces',
        'libs/panners',
        'libs/timecode',
        'libs/ardour',
        'libs/gtkmm2ext',
        'libs/clearlooks-newer',
        'libs/audiographer',
        'gtk2_ardour',
        'templates',
        'export',
        'midi_maps',
        'manual'
]

i18n_children = [
        'gtk2_ardour',
        'libs/ardour',
        'libs/gtkmm2ext',
]

if sys.platform != 'darwin':
    children += [ 'tools/sanity_check' ]
    lxvst_default = True
else:
    children += [ 'libs/appleutility' ]
    lxvst_default = False

# Version stuff

def fetch_svn_revision (path):
    cmd = "LANG= svn info " + path + " | awk '/^Revision:/ { print $2}'"
    return subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE).communicate()[0].splitlines()

def fetch_gcc_version (CC):
    cmd = "LANG= %s --version" % CC
    output = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE).communicate()[0].splitlines()
    o = output[0].decode('utf-8')
    version = o.split(' ')[2].split('.')
    return version

def fetch_git_revision (path):
    cmd = "LANG= git log --abbrev HEAD^..HEAD " + path
    output = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE).communicate()[0].splitlines()
    o = output[0].decode('utf-8')
    rev = o.replace ("commit", "git")[0:10]
    for line in output:
        try:
            if "git-svn-id" in line:
                line = line.split('@')[1].split(' ')
                rev = line[0]
        except:
            pass
    return rev

def fetch_bzr_revision (path):
    cmd = subprocess.Popen("LANG= bzr log -l 1 " + path, stdout=subprocess.PIPE, shell=True)
    out = cmd.communicate()[0]
    svn = re.search('^svn revno: [0-9]*', out, re.MULTILINE)
    str = svn.group(0)
    chars = 'svnreio: '
    return string.lstrip(str, chars)

def create_stored_revision():
    rev = ""
    if os.path.exists('.svn'):
        rev = fetch_svn_revision('.');
    elif os.path.exists('.git'):
        rev = fetch_git_revision('.');
    elif os.path.exists('.bzr'):
        rev = fetch_bzr_revision('.');
        print("Revision: %s", rev)
    elif os.path.exists('libs/ardour/svn_revision.cc'):
        print("Using packaged svn revision")
        return
    else:
        print("Missing libs/ardour/svn_revision.cc.  Blame the packager.")
        sys.exit(-1)

    try:
        text =  '#include "ardour/svn_revision.h"\n'
        text += 'namespace ARDOUR { const char* svn_revision = \"%s\"; }\n' % rev
        print('Writing svn revision info to libs/ardour/svn_revision.cc')
        o = open('libs/ardour/svn_revision.cc', 'w')
        o.write(text)
        o.close()
    except IOError:
        print('Could not open libs/ardour/svn_revision.cc for writing\n')
        sys.exit(-1)

def set_compiler_flags (conf,opt):
    #
    # Compiler flags and other system-dependent stuff
    #

    build_host_supports_sse = False
    optimization_flags = []
    debug_flags = []

    # guess at the platform, used to define compiler flags

    config_guess = os.popen("tools/config.guess").read()[:-1]

    config_cpu = 0
    config_arch = 1
    config_kernel = 2
    config_os = 3
    config = config_guess.split ("-")

    if opt.gprofile:
        debug_flags = [ '-pg' ]
    else:
        if config[config_arch] != 'apple':
            debug_flags = [ '-rdynamic' ] # waf adds -O0 -g itself. thanks waf!

    # Autodetect
    if opt.dist_target == 'auto':
        if config[config_arch] == 'apple':
            # The [.] matches to the dot after the major version, "." would match any character
            if re.search ("darwin[0-7][.]", config[config_kernel]) != None:
                conf.env['build_target'] = 'panther'
            elif re.search ("darwin8[.]", config[config_kernel]) != None:
                conf.env['build_target'] = 'tiger'
            else:
                conf.env['build_target'] = 'leopard'
        else:
            if re.search ("x86_64", config[config_cpu]) != None:
                conf.env['build_target'] = 'x86_64'
            elif re.search("i[0-5]86", config[config_cpu]) != None:
                conf.env['build_target'] = 'i386'
            elif re.search("powerpc", config[config_cpu]) != None:
                conf.env['build_target'] = 'powerpc'
            else:
                conf.env['build_target'] = 'i686'
    else:
        conf.env['build_target'] = opt.dist_target

    if config[config_cpu] == 'powerpc' and conf.env['build_target'] != 'none':
        #
        # Apple/PowerPC optimization options
        #
        # -mcpu=7450 does not reliably work with gcc 3.*
        #
        if opt.dist_target == 'panther' or opt.dist_target == 'tiger':
            if config[config_arch] == 'apple':
                # optimization_flags.extend ([ "-mcpu=7450", "-faltivec"])
                # to support g3s but still have some optimization for above
                optimization_flags.extend ([ "-mcpu=G3", "-mtune=7450"])
            else:
                optimization_flags.extend ([ "-mcpu=7400", "-maltivec", "-mabi=altivec"])
        else:
            optimization_flags.extend([ "-mcpu=750", "-mmultiple" ])
        optimization_flags.extend (["-mhard-float", "-mpowerpc-gfxopt"])
        optimization_flags.extend (["-Os"])

    elif ((re.search ("i[0-9]86", config[config_cpu]) != None) or (re.search ("x86_64", config[config_cpu]) != None)) and conf.env['build_target'] != 'none':


        #
        # ARCH_X86 means anything in the x86 family from i386 to x86_64
        # the compile-time presence of the macro _LP64 is used to 
        # distingush 32 and 64 bit assembler
        #

        if (re.search ("(i[0-9]86|x86_64)", config[config_cpu]) != None):
            debug_flags.append ("-DARCH_X86")
            optimization_flags.append ("-DARCH_X86")

        if config[config_kernel] == 'linux' :

            #
            # determine processor flags via /proc/cpuinfo
            #

            if conf.env['build_target'] != 'i386':

                flag_line = os.popen ("cat /proc/cpuinfo | grep '^flags'").read()[:-1]
                x86_flags = flag_line.split (": ")[1:][0].split ()

                if "mmx" in x86_flags:
                    optimization_flags.append ("-mmmx")
                if "sse" in x86_flags:
                    build_host_supports_sse = True
                if "3dnow" in x86_flags:
                    optimization_flags.append ("-m3dnow")

            if config[config_cpu] == "i586":
                optimization_flags.append ("-march=i586")
            elif config[config_cpu] == "i686":
                optimization_flags.append ("-march=i686")

        if ((conf.env['build_target'] == 'i686') or (conf.env['build_target'] == 'x86_64')) and build_host_supports_sse:
            optimization_flags.extend (["-msse", "-mfpmath=sse", "-DUSE_XMMINTRIN"])
            debug_flags.extend (["-msse", "-mfpmath=sse", "-DUSE_XMMINTRIN"])

    # end of processor-specific section

    # optimization section
    if conf.env['FPU_OPTIMIZATION']:
        if conf.env['build_target'] == 'tiger' or conf.env['build_target'] == 'leopard':
            optimization_flags.append ("-DBUILD_VECLIB_OPTIMIZATIONS");
            debug_flags.append ("-DBUILD_VECLIB_OPTIMIZATIONS");
            conf.env.append_value('LINKFLAGS', "-framework Accelerate")
        elif conf.env['build_target'] == 'i686' or conf.env['build_target'] == 'x86_64':
            optimization_flags.append ("-DBUILD_SSE_OPTIMIZATIONS")
            debug_flags.append ("-DBUILD_SSE_OPTIMIZATIONS")
        if not build_host_supports_sse:
            print("\nWarning: you are building Ardour with SSE support even though your system does not support these instructions. (This may not be an error, especially if you are a package maintainer)")

    # check this even if we aren't using FPU optimization
    if not conf.is_defined('HAVE_POSIX_MEMALIGN'):
        optimization_flags.append("-DNO_POSIX_MEMALIGN")
        debug_flags.append("-DNO_POSIX_MEMALIGN")

    # end optimization section

    #
    # no VST on x86_64
    #

    if conf.env['build_target'] == 'x86_64' and opt.windows_vst:
        print("\n\n==================================================")
        print("You cannot use VST plugins with a 64 bit host. Please run waf with --windows-vst=0")
        print("\nIt is theoretically possible to build a 32 bit host on a 64 bit system.")
        print("However, this is tricky and not recommended for beginners.")
        sys.exit (-1)

    if opt.lxvst:
        if conf.env['build_target'] == 'x86_64':
            conf.env.append_value('CXXFLAGS', "-DLXVST_64BIT")
        else:
            conf.env.append_value('CXXFLAGS', "-DLXVST_32BIT")

    #
    # a single way to test if we're on OS X
    #

    if conf.env['build_target'] in ['panther', 'tiger', 'leopard' ]:
        conf.define ('IS_OSX', 1)
        # force tiger or later, to avoid issues on PPC which defaults
        # back to 10.1 if we don't tell it otherwise.
        conf.env.append_value('CFLAGS', "-DMAC_OS_X_VERSION_MIN_REQUIRED=1040")

    else:
        conf.define ('IS_OSX', 0)

    #
    # save off guessed arch element in an env
    #
    conf.define ('CONFIG_ARCH', config[config_arch])

    #
    # ARCH="..." overrides all
    #

    if opt.arch != None:
        optimization_flags = opt.arch.split()

    #
    # prepend boiler plate optimization flags that work on all architectures
    #

    optimization_flags[:0] = [
            "-O3",
            "-fomit-frame-pointer",
            "-ffast-math",
            "-fstrength-reduce",
            "-pipe"
            ]

    if opt.debug:
        conf.env.append_value('CFLAGS', debug_flags)
        conf.env.append_value('CXXFLAGS', debug_flags)
        conf.env.append_value('LINKFLAGS', debug_flags)
    else:
        conf.env.append_value('CFLAGS', optimization_flags)
        conf.env.append_value('CXXFLAGS', optimization_flags)
        conf.env.append_value('LINKFLAGS', optimization_flags)

    if opt.stl_debug:
        conf.env.append_value('CXXFLAGS', "-D_GLIBCXX_DEBUG")

    if conf.env['DEBUG_RT_ALLOC']:
        conf.env.append_value('CFLAGS', '-DDEBUG_RT_ALLOC')
        conf.env.append_value('CXXFLAGS', '-DDEBUG_RT_ALLOC')
        conf.env.append_value('LINKFLAGS', '-ldl')

    if conf.env['DEBUG_DENORMAL_EXCEPTION']:
        conf.env.append_value('CFLAGS', '-DDEBUG_DENORMAL_EXCEPTION')
        conf.env.append_value('CXXFLAGS', '-DDEBUG_DENORMAL_EXCEPTION')

    if opt.universal:
        if not Options.options.nocarbon:
            conf.env.append_value('CFLAGS', ["-arch", "i386", "-arch", "ppc"])
            conf.env.append_value('CXXFLAGS', ["-arch", "i386", "-arch", "ppc"])
            conf.env.append_value('LINKFLAGS', ["-arch", "i386", "-arch", "ppc"])
        else:
            conf.env.append_value('CFLAGS', ["-arch", "x86_64", "-arch", "i386", "-arch", "ppc"])
            conf.env.append_value('CXXFLAGS', ["-arch", "x86_64", "-arch", "i386", "-arch", "ppc"])
            conf.env.append_value('LINKFLAGS', ["-arch", "x86_64", "-arch", "i386", "-arch", "ppc"])

    #
    # warnings flags
    #

    conf.env.append_value('CFLAGS', "-Wall")
    conf.env.append_value('CXXFLAGS', [ '-Wall', '-Woverloaded-virtual'])


    #
    # more boilerplate
    #

    conf.env.append_value('CFLAGS', '-D_LARGEFILE64_SOURCE')
    conf.env.append_value('CFLAGS', '-D_FILE_OFFSET_BITS=64')
    conf.env.append_value('CXXFLAGS', '-D_LARGEFILE64_SOURCE')
    conf.env.append_value('CXXFLAGS', '-D_FILE_OFFSET_BITS=64')

    conf.env.append_value('CXXFLAGS', '-D__STDC_LIMIT_MACROS')
    conf.env.append_value('CXXFLAGS', '-D__STDC_FORMAT_MACROS')

    if opt.nls:
        conf.env.append_value('CXXFLAGS', '-DENABLE_NLS')
        conf.env.append_value('CFLAGS', '-DENABLE_NLS')

#----------------------------------------------------------------

# Waf stages

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    autowaf.set_options(opt, debug_by_default=True)
    opt.add_option('--program-name', type='string', action='store', default='Ardour', dest='program_name',
                    help='The user-visible name of the program being built')
    opt.add_option('--arch', type='string', action='store', dest='arch',
                    help='Architecture-specific compiler flags')
    opt.add_option('--no-carbon', action='store_true', default=False, dest='nocarbon',
                    help='Compile without support for AU Plugins with only CARBON UI (needed for 64bit)')
    opt.add_option('--boost-sp-debug', action='store_true', default=False, dest='boost_sp_debug',
                    help='Compile with Boost shared pointer debugging')
    opt.add_option('--dist-target', type='string', default='auto', dest='dist_target',
                    help='Specify the target for cross-compiling [auto,none,x86,i386,i686,x86_64,powerpc,tiger,leopard]')
    opt.add_option('--fpu-optimization', action='store_true', default=True, dest='fpu_optimization',
                    help='Build runtime checked assembler code (default)')
    opt.add_option('--no-fpu-optimization', action='store_false', dest='fpu_optimization')
    opt.add_option('--freedesktop', action='store_true', default=False, dest='freedesktop',
                    help='Install MIME type, icons and .desktop file as per freedesktop.org standards')
    opt.add_option('--freebie', action='store_true', default=False, dest='freebie',
                    help='Build a version suitable for distribution as a zero-cost binary')
    opt.add_option('--no-freesound', action='store_false', default=True, dest='freesound',
                    help='Do not build with Freesound database support')
    opt.add_option('--gprofile', action='store_true', default=False, dest='gprofile',
                    help='Compile for use with gprofile')
    opt.add_option('--lv2', action='store_true', default=True, dest='lv2',
                    help='Compile with support for LV2 (if Lilv+Suil is available)')
    opt.add_option('--no-lv2', action='store_false', dest='lv2',
                    help='Do not compile with support for LV2')
    opt.add_option('--lxvst', action='store_true', default=lxvst_default, dest='lxvst',
                    help='Compile with support for linuxVST plugins')
    opt.add_option('--nls', action='store_true', default=True, dest='nls',
                    help='Enable i18n (native language support) (default)')
    opt.add_option('--no-nls', action='store_false', dest='nls')
    opt.add_option('--phone-home', action='store_false', default=False, dest='phone_home')
    opt.add_option('--stl-debug', action='store_true', default=False, dest='stl_debug',
                    help='Build with debugging for the STL')
    opt.add_option('--rt-alloc-debug', action='store_true', default=False, dest='rt_alloc_debug',
                    help='Build with debugging for memory allocation in the real-time thread')
    opt.add_option('--pt-timing', action='store_true', default=False, dest='pt_timing',
                    help='Build with logging of timing in the process thread(s)')
    opt.add_option('--denormal-exception', action='store_true', default=False, dest='denormal_exception',
                    help='Raise a floating point exception if a denormal is detected')
    opt.add_option('--test', action='store_true', default=False, dest='build_tests',
                    help="Build unit tests")
    opt.add_option('--tranzport', action='store_true', default=False, dest='tranzport',
                    help='Compile with support for Frontier Designs Tranzport (if libusb is available)')
    opt.add_option('--universal', action='store_true', default=False, dest='universal',
                    help='Compile as universal binary (OS X ONLY, requires that external libraries are universal)')
    opt.add_option('--versioned', action='store_true', default=False, dest='versioned',
                    help='Add revision information to executable name inside the build directory')
    opt.add_option('--windows-vst', action='store_true', default=False, dest='windows_vst',
                    help='Compile with support for Windows VST')
    opt.add_option('--wiimote', action='store_true', default=False, dest='wiimote',
                    help='Build the wiimote control surface')
    opt.add_option('--windows-key', type='string', action='store', dest='windows_key', default='Mod4><Super',
                    help='X Modifier(s) (Mod1,Mod2, etc) for the Windows key (X11 builds only). ' +
                    'Multiple modifiers must be separated by \'><\'')
    opt.add_option('--boost-include', type='string', action='store', dest='boost_include', default='',
                    help='directory where Boost header files can be found')
    opt.add_option('--also-include', type='string', action='store', dest='also_include', default='',
                    help='additional include directory where header files can be found')
    opt.add_option('--wine-include', type='string', action='store', dest='wine_include', default='/usr/include/wine/windows',
                    help='directory where Wine\'s Windows header files can be found')
    opt.add_option('--noconfirm', action='store_true', default=False, dest='noconfirm',
                    help='Do not ask questions that require confirmation during the build')
    for i in children:
        opt.recurse(i)

def sub_config_and_use(conf, name, has_objects = True):
    conf.recurse(name)
    autowaf.set_local_lib(conf, name, has_objects)

def configure(conf):
    conf.load('compiler_c')
    conf.load('compiler_cxx')
    if not Options.options.noconfirm:
        print ('\n\nThis is a beta version of Ardour 3.0.\n\n' +
               'You are respectfully requested NOT to ask for assistance with build issues\n' +
               'and not to report issues with Ardour 3.0 on the forums at ardour.org.\n\n' +
               'Please use IRC, the bug tracker and/or the ardour mailing lists (-dev or -user)\n\n' +
               'Thanks for your co-operation with our development process.\n\n' +
               'Press Enter to continue.\n')
        sys.stdin.readline()
    conf.env['VERSION'] = VERSION
    conf.line_just = 52
    autowaf.set_recursive()
    autowaf.configure(conf)
    autowaf.display_header('Ardour Configuration')

    gcc_versions = fetch_gcc_version(str(conf.env['CC']))
    if not Options.options.debug and gcc_versions[0] == '4' and gcc_versions[1] > '4':
        print('Version 4.5 of gcc is not ready for use when compiling Ardour with optimization.')
        print('Please use a different version or re-configure with --debug')
        exit (1)

    if sys.platform == 'darwin':

        # libintl may or may not be trivially locatable
        if not os.path.isfile ('/usr/include/libintl.h'):
            # XXXX hack hack hack
            prefinclude = ' '.join ('-I', os.path.expanduser ('~/gtk/inst/include'))
            preflib = ' '.join ('-L', os.path.expanduser ('~/gtk/inst/lib'))
            conf.env.append_value('CFLAGS', [ prefinclude ])
            conf.env.append_value('CXXFLAGS',  [prefinclude ])
            conf.env.append_value('LINKFLAGS', [ preflib ])

        # this is required, potentially, for anything we link and then relocate into a bundle
        conf.env.append_value('LINKFLAGS', [ '-Xlinker', '-headerpad_max_install_names' ])

        conf.define ('HAVE_COREAUDIO', 1)
        conf.define ('AUDIOUNIT_SUPPORT', 1)
        if not Options.options.nocarbon:
            conf.define ('WITH_CARBON', 1)
        if not Options.options.freebie:
            conf.define ('AU_STATE_SUPPORT', 1)

        conf.define ('GTKOSX', 1)
        conf.define ('TOP_MENUBAR',1)
        conf.define ('GTKOSX',1)

        conf.env.append_value('CXXFLAGS_APPLEUTILITY', '-I../libs')
        #
        #       Define OSX as a uselib to use when compiling
        #       on Darwin to add all applicable flags at once
        #
        conf.env.append_value('CXXFLAGS_OSX', '-DMAC_OS_X_VERSION_MIN_REQUIRED=1040')
        conf.env.append_value('CFLAGS_OSX', '-DMAC_OS_X_VERSION_MIN_REQUIRED=1040')
        conf.env.append_value('CXXFLAGS_OSX', '-mmacosx-version-min=10.4')
        conf.env.append_value('CFLAGS_OSX', '-mmacosx-version-min=10.4')

        #conf.env.append_value('CXXFLAGS_OSX', "-isysroot /Developer/SDKs/MacOSX10.4u.sdk")
        #conf.env.append_value('CFLAGS_OSX', "-isysroot /Developer/SDKs/MacOSX10.4u.sdk")
        #conf.env.append_value('LINKFLAGS_OSX', "-isysroot /Developer/SDKs/MacOSX10.4u.sdk")

        #conf.env.append_value('LINKFLAGS_OSX', "-sysroot /Developer/SDKs/MacOSX10.4u.sdk")

        conf.env.append_value('CXXFLAGS_OSX', "-msse")
        conf.env.append_value('CFLAGS_OSX', "-msse")
        conf.env.append_value('CXXFLAGS_OSX', "-msse2")
        conf.env.append_value('CFLAGS_OSX', "-msse2")
        #
        #       TODO: The previous sse flags NEED to be based
        #       off processor type.  Need to add in a check
        #       for that.
        #
        conf.env.append_value('CXXFLAGS_OSX', '-F/System/Library/Frameworks')
        conf.env.append_value('CXXFLAGS_OSX', '-F/Library/Frameworks')

        conf.env.append_value('LINKFLAGS_OSX', ['-framework', 'AppKit'])
        conf.env.append_value('LINKFLAGS_OSX', ['-framework', 'CoreAudio'])
        conf.env.append_value('LINKFLAGS_OSX', ['-framework', 'CoreAudioKit'])
        conf.env.append_value('LINKFLAGS_OSX', ['-framework', 'CoreFoundation'])
        conf.env.append_value('LINKFLAGS_OSX', ['-framework', 'CoreServices'])

        conf.env.append_value('LINKFLAGS_OSX', ['-undefined', 'dynamic_lookup' ])
        conf.env.append_value('LINKFLAGS_OSX', ['-flat_namespace'])

        conf.env.append_value('CXXFLAGS_AUDIOUNITS', "-DAUDIOUNIT_SUPPORT")
        conf.env.append_value('LINKFLAGS_AUDIOUNITS', ['-framework', 'Audiotoolbox', '-framework', 'AudioUnit'])
        conf.env.append_value('LINKFLAGS_AUDIOUNITS', ['-framework', 'Cocoa'])

        if not Options.options.freebie:
            conf.env.append_value('CXXFLAGS_AUDIOUNITS', "-DAU_STATE_SUPPORT")
        if not Options.options.nocarbon:
            conf.env.append_value('CXXFLAGS_AUDIOUNITS', "-DWITH_CARBON")
            conf.env.append_value('LINKFLAGS_AUDIOUNITS', ['-framework', 'Carbon'])

    if Options.options.boost_include != '':
        conf.env.append_value('CXXFLAGS', '-I' + Options.options.boost_include)

    if Options.options.also_include != '':
        conf.env.append_value('CXXFLAGS', '-I' + Options.options.also_include)
        conf.env.append_value('CFLAGS', '-I' + Options.options.also_include)

    autowaf.check_header(conf, 'cxx', 'boost/signals2.hpp', mandatory = True)

    if Options.options.boost_sp_debug:
        conf.env.append_value('CXXFLAGS', '-DBOOST_SP_ENABLE_DEBUG_HOOKS')

    autowaf.check_header(conf, 'cxx', 'jack/session.h', define="JACK_SESSION", mandatory = False)

    conf.check_cxx(fragment = "#include <boost/version.hpp>\nint main(void) { return (BOOST_VERSION >= 103900 ? 0 : 1); }\n",
                  execute = "1",
                  mandatory = True,
                  msg = 'Checking for boost library >= 1.39',
                  okmsg = 'ok',
                  errmsg = 'too old\nPlease install boost version 1.39 or higher.')

    autowaf.check_pkg(conf, 'cppunit', uselib_store='CPPUNIT', atleast_version='1.12.0', mandatory=False)
    autowaf.check_pkg(conf, 'glib-2.0', uselib_store='GLIB', atleast_version='2.2')
    autowaf.check_pkg(conf, 'gthread-2.0', uselib_store='GTHREAD', atleast_version='2.2')
    autowaf.check_pkg(conf, 'glibmm-2.4', uselib_store='GLIBMM', atleast_version='2.14.0')
    autowaf.check_pkg(conf, 'sndfile', uselib_store='SNDFILE', atleast_version='1.0.18')
    autowaf.check_pkg(conf, 'giomm-2.4', uselib_store='GIOMM', atleast_version='2.2')

    for i in children:
        sub_config_and_use(conf, i)

    # Fix utterly braindead FLAC include path to not smash assert.h
    conf.env['INCLUDES_FLAC'] = []

    conf.check_cc(function_name='dlopen', header_name='dlfcn.h', linkflags='-ldl', uselib_store='DL')
    conf.check_cc(function_name='curl_global_init', header_name='curl/curl.h', linkflags='-lcurl', uselib_store='CURL')

    # Tell everyone that this is a waf build

    conf.env.append_value('CFLAGS', '-DWAF_BUILD')
    conf.env.append_value('CXXFLAGS', '-DWAF_BUILD')

    # Set up waf environment and C defines
    opts = Options.options
    if opts.debug:
        opts.phone_home = False;   # debug builds should not call home
    if opts.phone_home:
        conf.env['PHONE_HOME'] = opts.phone_home
    if opts.fpu_optimization:
        conf.env['FPU_OPTIMIZATION'] = True
    if opts.freesound:
        conf.define('FREESOUND',1)
        conf.env['FREESOUND'] = True
    if opts.nls:
        conf.define('ENABLE_NLS', 1)
        conf.env['ENABLE_NLS'] = True
    if opts.build_tests:
        conf.env['BUILD_TESTS'] = opts.build_tests
    if opts.tranzport:
        conf.env['TRANZPORT'] = 1
    if opts.windows_vst:
        conf.define('WINDOWS_VST_SUPPORT', 1)
        conf.env['WINDOWS_VST_SUPPORT'] = True
        conf.env.append_value('CFLAGS', '-I' + Options.options.wine_include)
        conf.env.append_value('CXXFLAGS', '-I' + Options.options.wine_include)
        autowaf.check_header(conf, 'cxx', 'windows.h', mandatory = True)
    if opts.lxvst:
        conf.define('LXVST_SUPPORT', 1)
        conf.env['LXVST_SUPPORT'] = True
    if bool(conf.env['JACK_SESSION']):
        conf.define('HAVE_JACK_SESSION', 1)
    if opts.wiimote:
        conf.define('WIIMOTE', 1)
        conf.env['WIIMOTE'] = True
    conf.define('WINDOWS_KEY', opts.windows_key)
    conf.env['PROGRAM_NAME'] = opts.program_name
    if opts.rt_alloc_debug:
        conf.define('DEBUG_RT_ALLOC', 1)
        conf.env['DEBUG_RT_ALLOC'] = True
    if opts.pt_timing:
        conf.define('PT_TIMING', 1)
        conf.env['PT_TIMING'] = True
    if opts.denormal_exception:
        conf.define('DEBUG_DENORMAL_EXCEPTION', 1)
        conf.env['DEBUG_DENORMAL_EXCEPTION'] = True
    if not conf.is_defined('HAVE_CPPUNIT'):
        conf.env['BUILD_TESTS'] = False

    set_compiler_flags (conf, Options.options)

    config_text = open('libs/ardour/config_text.cc', "w")
    config_text.write('''#include "ardour/ardour.h"
namespace ARDOUR {
const char* const ardour_config_info = "\\n\\
''')

    def write_config_text(title, val):
        autowaf.display_msg(conf, title, val)
        config_text.write(title + ': ')
        config_text.write(str(val))
        config_text.write("\\n\\\n")

    write_config_text('Build documentation',   conf.env['DOCS'])
    write_config_text('Debuggable build',      conf.env['DEBUG'])
    write_config_text('Install prefix',        conf.env['PREFIX'])
    write_config_text('Strict compiler flags', conf.env['STRICT'])

    write_config_text('Architecture flags',    opts.arch)
    write_config_text('Aubio',                 conf.is_defined('HAVE_AUBIO'))
    write_config_text('AudioUnits',            conf.is_defined('AUDIOUNIT_SUPPORT'))
    write_config_text('AU state support',      conf.is_defined('AU_STATE_SUPPORT'))
    write_config_text('Build target',          conf.env['build_target'])
    write_config_text('CoreAudio',             conf.is_defined('HAVE_COREAUDIO'))
    write_config_text('Debug RT allocations',  conf.is_defined('DEBUG_RT_ALLOC'))
    write_config_text('Process thread timing', conf.is_defined('PT_TIMING'))
    write_config_text('Denormal exceptions',   conf.is_defined('DEBUG_DENORMAL_EXCEPTION'))
    write_config_text('FLAC',                  conf.is_defined('HAVE_FLAC'))
    write_config_text('FPU optimization',      opts.fpu_optimization)
    write_config_text('Freedesktop files',     opts.freedesktop)
    write_config_text('Freesound',             opts.freesound)
    write_config_text('JACK session support',  conf.is_defined('JACK_SESSION'))
    write_config_text('LV2 UI embedding',      conf.is_defined('HAVE_SUIL'))
    write_config_text('LV2 support',           conf.is_defined('LV2_SUPPORT'))
    write_config_text('LV2 state support',     conf.is_defined('HAVE_NEW_LILV'))
    write_config_text('LXVST support',         conf.is_defined('LXVST_SUPPORT'))
    write_config_text('OGG',                   conf.is_defined('HAVE_OGG'))
    write_config_text('Phone home',            conf.is_defined('PHONE_HOME'))
    write_config_text('Program name',          opts.program_name)
    write_config_text('Rubberband',            conf.is_defined('HAVE_RUBBERBAND'))
    write_config_text('Samplerate',            conf.is_defined('HAVE_SAMPLERATE'))
#    write_config_text('Soundtouch',            conf.is_defined('HAVE_SOUNDTOUCH'))
    write_config_text('Translation',           opts.nls)
    write_config_text('Tranzport',             opts.tranzport)
    write_config_text('Unit tests',            conf.env['BUILD_TESTS'])
    write_config_text('Universal binary',      opts.universal)
    write_config_text('Windows VST support',   opts.windows_vst)
    write_config_text('Wiimote support',       opts.wiimote)
    write_config_text('Windows key',           opts.windows_key)

    write_config_text('C compiler flags',      conf.env['CFLAGS'])
    write_config_text('C++ compiler flags',    conf.env['CXXFLAGS'])

    config_text.write ('";\n}\n')
    config_text.close ()
    print('')

def build(bld):
    create_stored_revision()

    # add directories that contain only headers, to workaround an issue with waf

    bld.path.find_dir ('libs/evoral/evoral')
    bld.path.find_dir ('libs/vamp-sdk/vamp-sdk')
    bld.path.find_dir ('libs/surfaces/control_protocol/control_protocol')
    bld.path.find_dir ('libs/timecode/timecode')
    bld.path.find_dir ('libs/rubberband/rubberband')
    bld.path.find_dir ('libs/gtkmm2ext/gtkmm2ext')
    bld.path.find_dir ('libs/ardour/ardour')
    bld.path.find_dir ('libs/taglib/taglib')
    bld.path.find_dir ('libs/pbd/pbd')

    autowaf.set_recursive()

    for i in children:
        bld.recurse(i)

    # ideally, we'd like to use the OS-provided MIDI API
    # for default ports. that doesn't work on at least
    # Fedora (Nov 9th, 2009) so use JACK MIDI on linux.

    if sys.platform == 'darwin':
        rc_subst_dict = {
                'MIDITAG'    : 'control',
                'MIDITYPE'   : 'coremidi',
                'JACK_INPUT' : 'auditioner'
                }
    else:
        rc_subst_dict = {
                'MIDITAG'    : 'control',
                'MIDITYPE'   : 'jack',
                'JACK_INPUT' : 'auditioner'
                }

    obj              = bld(features = 'subst')
    obj.source       = 'ardour.rc.in'
    obj.target       = 'ardour_system.rc'
    obj.dict         = rc_subst_dict
    obj.install_path = '${SYSCONFDIR}/ardour3'

def i18n(bld):
    bld.recurse (i18n_children)

def i18n_pot(bld):
    bld.recurse (i18n_children)

def i18n_po(bld):
    bld.recurse (i18n_children)

def i18n_mo(bld):
    bld.recurse (i18n_children)
