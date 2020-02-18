#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Autowaf, useful waf utilities with support for recursive projects
# Copyright 2008-2011 David Robillard
#
# Licensed under the GNU GPL v2 or later, see COPYING file for details.

import glob
import os
import subprocess
import sys
import shutil

from waflib import Configure, Context, Logs, Node, Options, Task, Utils
from waflib.TaskGen import feature, before, after

global g_is_child
g_is_child = False

# Only run autowaf hooks once (even if sub projects call several times)
global g_step
g_step = 0

# Compute dependencies globally
#import preproc
#preproc.go_absolute = True

@feature('c', 'cxx')
@after('apply_incpaths')
def include_config_h(self):
    self.env.append_value('INCPATHS', self.bld.bldnode.abspath())

def set_options(opt, debug_by_default=False):
    "Add standard autowaf options if they havn't been added yet"
    global g_step
    if g_step > 0:
        return

    # Install directory options
    dirs_options = opt.add_option_group('Installation directories', '')

    # Move --prefix and --destdir to directory options group
    for k in ('--prefix', '--destdir'):
        option = opt.parser.get_option(k)
        if option:
            opt.parser.remove_option(k)
            dirs_options.add_option(option)

    # Standard directory options
    dirs_options.add_option('--bindir', type='string',
                            help="Executable programs [Default: PREFIX/bin]")
    dirs_options.add_option('--configdir', type='string',
                            help="Configuration data [Default: PREFIX/etc]")
    dirs_options.add_option('--datadir', type='string',
                            help="Shared data [Default: PREFIX/share]")
    dirs_options.add_option('--includedir', type='string',
                            help="Header files [Default: PREFIX/include]")
    dirs_options.add_option('--libdir', type='string',
                            help="Libraries [Default: PREFIX/lib]")
    dirs_options.add_option('--mandir', type='string',
                            help="Manual pages [Default: DATADIR/man]")
    dirs_options.add_option('--docdir', type='string',
                            help="HTML documentation [Default: DATADIR/doc]")

    # Build options
    if debug_by_default:
        opt.add_option('--optimize', action='store_false', default=True, dest='debug',
                       help="Build optimized binaries")
    else:
        opt.add_option('--debug', action='store_true', default=False, dest='debug',
                       help="Build debuggable binaries")

    opt.add_option('--pardebug', action='store_true', default=False, dest='pardebug',
                       help="Build parallel-installable debuggable libraries with D suffix")

    opt.add_option('--grind', action='store_true', default=False, dest='grind',
                   help="Run tests in valgrind")
    opt.add_option('--strict', action='store_true', default=False, dest='strict',
                   help="Use strict compiler flags and show all warnings")
    opt.add_option('--ultra-strict', action='store_true', default=False, dest='ultra_strict',
                   help="Use even stricter compiler flags (likely to trigger many warnings in library headers)")
    opt.add_option('--docs', action='store_true', default=False, dest='docs',
                   help="Build documentation - requires doxygen")

    g_step = 1

def copyfile (task):
    # a cross-platform utility for copying files as part of tasks
    src = task.inputs[0].abspath()
    tgt = task.outputs[0].abspath()
    shutil.copy2 (src, tgt)

def check_header(conf, lang, name, define='', mandatory=True):
    "Check for a header"
    includes = '' # search default system include paths
    if sys.platform == "darwin":
        includes = '/opt/local/include'

    if lang == 'c':
        check_func = conf.check_cc
    elif lang == 'cxx':
        check_func = conf.check_cxx
    else:
        Logs.error("Unknown header language `%s'" % lang)
        return

    if define != '':
        check_func(header_name=name, includes=includes,
                   define_name=define, mandatory=mandatory)
    else:
        check_func(header_name=name, includes=includes, mandatory=mandatory)

def nameify(name):
    return name.replace('/', '_').replace('++', 'PP').replace('-', '_').replace('.', '_')

def define(conf, var_name, value):
    conf.define(var_name, value)
    conf.env[var_name] = value

def check_pkg(conf, name, **args):
    "Check for a package iff it hasn't been checked for yet"
    if args['uselib_store'].lower() in conf.env['AUTOWAF_LOCAL_LIBS']:
        return
    class CheckType:
        OPTIONAL=1
        MANDATORY=2
    var_name = 'CHECKED_' + nameify(args['uselib_store'])
    check = not var_name in conf.env
    mandatory = not 'mandatory' in args or args['mandatory']
    if not check and 'atleast_version' in args:
        # Re-check if version is newer than previous check
        checked_version = conf.env['VERSION_' + name]
        if checked_version and checked_version < args['atleast_version']:
            check = True;
    if not check and mandatory and conf.env[var_name] == CheckType.OPTIONAL:
        # Re-check if previous check was optional but this one is mandatory
        check = True;
    if check:
        found = None
        pkg_var_name = 'PKG_' + name.replace('-', '_')
        pkg_name = name
        if mandatory:
            args['mandatory'] = True  # Unsmash mandatory arg
        if 'atleast_version' in args:
            if not 'msg' in args:
                args['msg'] = 'Checking for %r >= %s' %(pkg_name, args['atleast_version'])
            found = conf.check_cfg(package=pkg_name, args=[pkg_name + " >= " + args['atleast_version'], '--cflags', '--libs'], **args)
        else:
            found = conf.check_cfg(package=pkg_name, args="--cflags --libs", **args)
        if found:
            conf.env[pkg_var_name] = pkg_name
        if 'atleast_version' in args:
            conf.env['VERSION_' + name] = args['atleast_version']
    if mandatory:
        conf.env[var_name] = CheckType.MANDATORY
    else:
        conf.env[var_name] = CheckType.OPTIONAL


def normpath(path):
    if sys.platform == 'win32':
        return os.path.normpath(path).replace('\\', '/')
    else:
        return os.path.normpath(path)

def ensure_visible_symbols(bld, visible):
    if bld.env['MSVC_COMPILER']:
        if visible:
            print ('*** WARNING: MSVC does not allow symbols to be visible/exported by default while building ' + bld.name)
        else:
            pass
    else:
        if not hasattr (bld,'cxxflags'):
            bld.cxxflags = []
        if not hasattr (bld,'cflags'):
            bld.cflags = []
        if visible:
            bld.cxxflags += [ '-fvisibility=default' ]
            bld.cflags += [ '-fvisibility=default' ]
        else:
            bld.cxxflags += [ '-fvisibility=hidden' ]
            bld.cflags += [ '-fvisibility=hidden' ]

def set_basic_compiler_flags(conf, flag_dict):
    if Options.options.debug:
        conf.env.append_value('CFLAGS', flag_dict['debuggable'])
        conf.env.append_value('CXXFLAGS', flag_dict['debuggable'])
        conf.env.append_value('LINKFLAGS', flag_dict['linker-debuggable'])
    else:
        conf.env.append_value('CFLAGS', flag_dict['nondebuggable'])
        conf.env.append_value('CXXFLAGS', flag_dict['nondebuggable'])

    if Options.options.ultra_strict:
        Options.options.strict = True
        conf.env.append_value('CFLAGS', flag_dict['ultra-strict'])

    if Options.options.strict:
        conf.env.append_value('CFLAGS', flag_dict['c-strict'])
        conf.env.append_value('CXXFLAGS', flag_dict['cxx-strict'])
        conf.env.append_value('CFLAGS', flag_dict['strict'])
        conf.env.append_value('CXXFLAGS', flag_dict['strict'])

    conf.env.append_value('CFLAGS', flag_dict['show-column'])
    conf.env.append_value('CXXFLAGS', flag_dict['show-column'])

def configure(conf):
    global g_step
    if g_step > 1:
        return
    print('')
    display_header('Global Configuration')

    if Options.options.docs:
        conf.load('doxygen')

    conf.env['DOCS'] = Options.options.docs
    conf.env['DEBUG'] = Options.options.debug or Options.options.pardebug
    conf.env['PARDEBUG'] = Options.options.pardebug
    conf.env['PREFIX'] = normpath(os.path.abspath(os.path.expanduser(conf.env['PREFIX'])))

    def config_dir(var, opt, default):
        if opt:
            conf.env[var] = normpath(opt)
        else:
            conf.env[var] = normpath(default)

    opts   = Options.options
    prefix = conf.env['PREFIX']

    config_dir('BINDIR',     opts.bindir,     os.path.join(prefix, 'bin'))
    config_dir('SYSCONFDIR', opts.configdir,  os.path.join(prefix, 'etc'))
    config_dir('DATADIR',    opts.datadir,    os.path.join(prefix, 'share'))
    config_dir('INCLUDEDIR', opts.includedir, os.path.join(prefix, 'include'))
    config_dir('LIBDIR',     opts.libdir,     os.path.join(prefix, 'lib'))
    config_dir('MANDIR',     opts.mandir,     os.path.join(conf.env['DATADIR'], 'man'))
    config_dir('DOCDIR',     opts.docdir,     os.path.join(conf.env['DATADIR'], 'doc'))

    if Options.options.docs:
        doxygen = conf.find_program('doxygen')
        if not doxygen:
            conf.fatal("Doxygen is required to build with --docs")

        dot = conf.find_program('dot')
        if not dot:
            conf.fatal("Graphviz (dot) is required to build with --docs")

    conf.env.prepend_value('CFLAGS', '-I' + os.path.abspath('.'))
    conf.env.prepend_value('CXXFLAGS', '-I' + os.path.abspath('.'))

    display_msg(conf, "Install prefix", conf.env['PREFIX'])
    display_msg(conf, "Debuggable build", str(conf.env['DEBUG']))
    display_msg(conf, "Build documentation", str(conf.env['DOCS']))
    print('')

    g_step = 2

def set_local_lib(conf, name, has_objects):
    var_name = 'HAVE_' + nameify(name.upper())
    define(conf, var_name, 1)
    if has_objects:
        if type(conf.env['AUTOWAF_LOCAL_LIBS']) != dict:
            conf.env['AUTOWAF_LOCAL_LIBS'] = {}
        conf.env['AUTOWAF_LOCAL_LIBS'][name.lower()] = True
    else:
        if type(conf.env['AUTOWAF_LOCAL_HEADERS']) != dict:
            conf.env['AUTOWAF_LOCAL_HEADERS'] = {}
        conf.env['AUTOWAF_LOCAL_HEADERS'][name.lower()] = True

def append_property(obj, key, val):
    if hasattr(obj, key):
        setattr(obj, key, getattr(obj, key) + val)
    else:
        setattr(obj, key, val)

def use_lib(bld, obj, libs):
    abssrcdir = os.path.abspath('.')
    libs_list = libs.split()
    for l in libs_list:
        in_headers = l.lower() in bld.env['AUTOWAF_LOCAL_HEADERS']
        in_libs    = l.lower() in bld.env['AUTOWAF_LOCAL_LIBS']
        if in_libs:
            append_property(obj, 'use', ' lib%s ' % l.lower())
            append_property(obj, 'framework', bld.env['FRAMEWORK_' + l])
        if in_headers or in_libs:
            inc_flag = '-iquote ' + os.path.join(abssrcdir, l.lower())
            for f in ['CFLAGS', 'CXXFLAGS']:
                if not inc_flag in bld.env[f]:
                    bld.env.prepend_value(f, inc_flag)
        else:
            append_property(obj, 'uselib', ' ' + l)

@feature('c', 'cxx')
@before('apply_link')
def version_lib(self):
    if sys.platform == 'win32':
        self.vnum = None  # Prevent waf from automatically appending -0
    if self.env['PARDEBUG']:
        applicable = ['cshlib', 'cxxshlib', 'cstlib', 'cxxstlib']
        if [x for x in applicable if x in self.features]:
            self.target = self.target + 'D'

def set_lib_env(conf, name, version):
    'Set up environment for local library as if found via pkg-config.'
    NAME         = name.upper()
    major_ver    = version.split('.')[0]
    pkg_var_name = 'PKG_' + name.replace('-', '_')
    lib_name     = '%s-%s' % (name, major_ver)
    if conf.env.PARDEBUG:
        lib_name += 'D'
    conf.env[pkg_var_name]       = lib_name
    conf.env['INCLUDES_' + NAME] = ['${INCLUDEDIR}/%s-%s' % (name, major_ver)]
    conf.env['LIBPATH_' + NAME]  = [conf.env.LIBDIR]
    conf.env['LIB_' + NAME]      = [lib_name]

def display_header(title):
    Logs.pprint('BOLD', title)

def display_msg(conf, msg, status = None, color = None):
    color = 'CYAN'
    if type(status) == bool and status or status == "True":
        color = 'GREEN'
    elif type(status) == bool and not status or status == "False":
        color = 'YELLOW'
    Logs.pprint('BOLD', " *", sep='')
    Logs.pprint('NORMAL', "%s" % msg.ljust(conf.line_just - 3), sep='')
    Logs.pprint('BOLD', ":", sep='')
    Logs.pprint(color, status)

def link_flags(env, lib):
    return ' '.join(map(lambda x: env['LIB_ST'] % x, env['LIB_' + lib]))

def compile_flags(env, lib):
    return ' '.join(map(lambda x: env['CPPPATH_ST'] % x, env['INCLUDES_' + lib]))

def set_recursive():
    global g_is_child
    g_is_child = True

def is_child():
    global g_is_child
    return g_is_child

# Pkg-config file
def build_pc(bld, name, version, version_suffix, libs, subst_dict={}):
    '''Build a pkg-config file for a library.
    name           -- uppercase variable name     (e.g. 'SOMENAME')
    version        -- version string              (e.g. '1.2.3')
    version_suffix -- name version suffix         (e.g. '2')
    libs           -- string/list of dependencies (e.g. 'LIBFOO GLIB')
    '''
    pkg_prefix       = bld.env['PREFIX']
    if pkg_prefix[-1] == '/':
        pkg_prefix = pkg_prefix[:-1]

    target = name.lower()
    if version_suffix != '':
        target += '-' + version_suffix

    if bld.env['PARDEBUG']:
        target += 'D'

    target += '.pc'

    libdir = bld.env['LIBDIR']
    if libdir.startswith(pkg_prefix):
        libdir = libdir.replace(pkg_prefix, '${exec_prefix}')

    includedir = bld.env['INCLUDEDIR']
    if includedir.startswith(pkg_prefix):
        includedir = includedir.replace(pkg_prefix, '${prefix}')

    obj = bld(features     = 'subst',
              source       = '%s.pc.in' % name.lower(),
              target       = target,
              install_path = os.path.join(bld.env['LIBDIR'], 'pkgconfig'),
              exec_prefix  = '${prefix}',
              PREFIX       = pkg_prefix,
              EXEC_PREFIX  = '${prefix}',
              LIBDIR       = libdir,
              INCLUDEDIR   = includedir)

    if type(libs) != list:
        libs = libs.split()

    subst_dict[name + '_VERSION'] = version
    subst_dict[name + '_MAJOR_VERSION'] = version[0:version.find('.')]
    for i in libs:
        subst_dict[i + '_LIBS']   = link_flags(bld.env, i)
        lib_cflags = compile_flags(bld.env, i)
        if lib_cflags == '':
            lib_cflags = ' '
        subst_dict[i + '_CFLAGS'] = lib_cflags

    obj.__dict__.update(subst_dict)

def build_dir(name, subdir):
    if is_child():
        return os.path.join('build', name, subdir)
    else:
        return os.path.join('build', subdir)

# Clean up messy Doxygen documentation after it is built
def make_simple_dox(name):
    name = name.lower()
    NAME = name.upper()
    try:
        top = os.getcwd()
        os.chdir(build_dir(name, 'doc/html'))
        page = 'group__%s.html' % name
        if not os.path.exists(page):
            return
        for i in [
            ['%s_API ' % NAME, ''],
            ['%s_DEPRECATED ' % NAME, ''],
            ['group__%s.html' % name, ''],
            ['&#160;', ''],
            ['<script.*><\/script>', ''],
            ['<hr\/><a name="details" id="details"><\/a><h2>.*<\/h2>', ''],
            ['<link href=\"tabs.css\" rel=\"stylesheet\" type=\"text\/css\"\/>',
             ''],
            ['<img class=\"footer\" src=\"doxygen.png\" alt=\"doxygen\"\/>',
             'Doxygen']]:
            os.system("sed -i 's/%s/%s/g' %s" % (i[0], i[1], page))
        os.rename('group__%s.html' % name, 'index.html')
        for i in (glob.glob('*.png') +
                  glob.glob('*.html') +
                  glob.glob('*.js') +
                  glob.glob('*.css')):
            if i != 'index.html' and i != 'style.css':
                os.remove(i)
        os.chdir(top)
        os.chdir(build_dir(name, 'doc/man/man3'))
        for i in glob.glob('*.3'):
            os.system("sed -i 's/%s_API //' %s" % (NAME, i))
        for i in glob.glob('_*'):
            os.remove(i)
        os.chdir(top)
    except Exception as e:
        Logs.error("Failed to fix up %s documentation: %s" % (name, e))

# Doxygen API documentation
def build_dox(bld, name, version, srcdir, blddir, outdir=''):
    if not bld.env['DOCS']:
        return

    if is_child():
        src_dir = os.path.join(srcdir, name.lower())
        doc_dir = os.path.join(blddir, name.lower(), 'doc')
    else:
        src_dir = srcdir
        doc_dir = os.path.join(blddir, 'doc')

    subst_tg = bld(features     = 'subst',
                   source       = 'doc/reference.doxygen.in',
                   target       = 'doc/reference.doxygen',
                   install_path = '',
                   name         = 'doxyfile')

    subst_dict = {
        name + '_VERSION' : version,
        name + '_SRCDIR'  : os.path.abspath(src_dir),
        name + '_DOC_DIR' : os.path.abspath(doc_dir)
        }

    subst_tg.__dict__.update(subst_dict)

    subst_tg.post()

    docs = bld(features = 'doxygen',
               doxyfile = 'doc/reference.doxygen')

    docs.post()

    major = int(version[0:version.find('.')])
    bld.install_files(
        os.path.join('${DOCDIR}', '%s-%d' % (name.lower(), major), outdir, 'html'),
        bld.path.get_bld().ant_glob('doc/html/*'))
    for i in range(1, 8):
        bld.install_files('${MANDIR}/man%d' % i,
                          bld.path.get_bld().ant_glob('doc/man/man%d/*' % i,
                                                      excl='**/_*'))

# Version code file generation
def build_version_files(header_path, source_path, domain, major, minor, micro, exportname, visheader):
    header_path = os.path.abspath(header_path)
    source_path = os.path.abspath(source_path)
    text  = "int " + domain + "_major_version = " + str(major) + ";\n"
    text += "int " + domain + "_minor_version = " + str(minor) + ";\n"
    text += "int " + domain + "_micro_version = " + str(micro) + ";\n"
    try:
        o = open(source_path, 'w')
        o.write(text)
        o.close()
    except IOError:
        Logs.error('Failed to open %s for writing\n' % source_path)
        sys.exit(-1)

    text  = "#ifndef __" + domain + "_version_h__\n"
    text += "#define __" + domain + "_version_h__\n"
    if visheader != '':
        text += "#include \"" + visheader + "\"\n"
    text += exportname + " extern const char* " + domain + "_revision;\n"
    text += exportname + " extern int " + domain + "_major_version;\n"
    text += exportname + " extern int " + domain + "_minor_version;\n"
    text += exportname + " extern int " + domain + "_micro_version;\n"
    text += "#endif /* __" + domain + "_version_h__ */\n"
    try:
        o = open(header_path, 'w')
        o.write(text)
        o.close()
    except IOError:
        Logs.warn('Failed to open %s for writing\n' % header_path)
        sys.exit(-1)

    return None

# Internationalization with gettext
def build_i18n_pot(bld, srcdir, dir, name, sources, copyright_holder=None):
    Logs.info('Generating pot file from %s' % name)
    pot_file = '%s.pot' % name

    cmd = ['xgettext',
            '--keyword=_',
            '--keyword=N_',
            '--keyword=S_',
            '--keyword=P_:1,2',
            '--from-code=UTF-8',
            '-o', pot_file]

    if copyright_holder:
        cmd += ['--copyright-holder="%s"' % copyright_holder]

    cmd += sources
    Logs.info('Updating ' + pot_file)
    subprocess.call(cmd, cwd=os.path.join(srcdir, dir))

def build_i18n_po(bld, srcdir, dir, name, sources, copyright_holder=None):
    pwd = os.getcwd()
    os.chdir(os.path.join(srcdir, dir))
    pot_file = '%s.pot' % name
    po_files = glob.glob('po/*.po')
    for po_file in po_files:
        cmd = ['msgmerge',
               '--update',
               '--no-fuzzy-matching',
               po_file,
               pot_file]
        Logs.info('Updating ' + po_file)
        subprocess.call(cmd)
    os.chdir(pwd)

def build_i18n_mo(bld, srcdir, dir, name, sources, copyright_holder=None):
    pwd = os.getcwd()
    os.chdir(os.path.join(srcdir, dir))
    pot_file = '%s.pot' % name
    po_files = glob.glob('po/*.po')
    for po_file in po_files:
        mo_file = po_file.replace('.po', '.mo')
        cmd = ['msgfmt',
               '-c',
               '-f',
               '-o',
               mo_file,
               po_file]
        Logs.info('Generating ' + po_file)
        subprocess.call(cmd)
    os.chdir(pwd)

def build_i18n(bld, srcdir, dir, name, sources, copyright_holder=None):
    build_i18n_pot(bld, srcdir, dir, name, sources, copyright_holder)
    build_i18n_po(bld, srcdir, dir, name, sources, copyright_holder)
    build_i18n_mo(bld, srcdir, dir, name, sources, copyright_holder)

def cd_to_build_dir(ctx, appname):
    orig_dir  = os.path.abspath(os.curdir)
    top_level = (len(ctx.stack_path) > 1)
    if top_level:
        os.chdir(os.path.join('build', appname))
    else:
        os.chdir('build')
    Logs.pprint('GREEN', "Waf: Entering directory `%s'" % os.path.abspath(os.getcwd()))

def cd_to_orig_dir(ctx, child):
    if child:
        os.chdir(os.path.join('..', '..'))
    else:
        os.chdir('..')

def pre_test(ctx, appname, dirs=['src']):
    diropts  = ''
    for i in dirs:
        diropts += ' -d ' + i
    cd_to_build_dir(ctx, appname)
    clear_log = open('lcov-clear.log', 'w')
    try:
        try:
            # Clear coverage data
            subprocess.call(('lcov %s -z' % diropts).split(),
                            stdout=clear_log, stderr=clear_log)
        except:
            Logs.warn('Failed to run lcov, no coverage report will be generated')
    finally:
        clear_log.close()

def post_test(ctx, appname, dirs=['src'], remove=['*boost*', 'c++*']):
    diropts  = ''
    for i in dirs:
        diropts += ' -d ' + i
    coverage_log           = open('lcov-coverage.log', 'w')
    coverage_lcov          = open('coverage.lcov', 'w')
    coverage_stripped_lcov = open('coverage-stripped.lcov', 'w')
    try:
        try:
            base = '.'
            if g_is_child:
                base = '..'

            # Generate coverage data
            subprocess.call(('lcov -c %s -b %s' % (diropts, base)).split(),
                            stdout=coverage_lcov, stderr=coverage_log)

            # Strip unwanted stuff
            subprocess.call(
                ['lcov', '--remove', 'coverage.lcov'] + remove,
                stdout=coverage_stripped_lcov, stderr=coverage_log)

            # Generate HTML coverage output
            if not os.path.isdir('coverage'):
                os.makedirs('coverage')
            subprocess.call('genhtml -o coverage coverage-stripped.lcov'.split(),
                            stdout=coverage_log, stderr=coverage_log)

        except:
            Logs.warn('Failed to run lcov, no coverage report will be generated')
    finally:
        coverage_stripped_lcov.close()
        coverage_lcov.close()
        coverage_log.close()

        print('')
        Logs.pprint('GREEN', "Waf: Leaving directory `%s'" % os.path.abspath(os.getcwd()))
        top_level = (len(ctx.stack_path) > 1)
        if top_level:
            cd_to_orig_dir(ctx, top_level)

    print('')
    Logs.pprint('BOLD', 'Coverage:', sep='')
    print('<file://%s>\n\n' % os.path.abspath('coverage/index.html'))

def run_tests(ctx, appname, tests, desired_status=0, dirs=['src'], name='*'):
    failures = 0
    diropts  = ''
    for i in dirs:
        diropts += ' -d ' + i

    # Run all tests
    for i in tests:
        s = i
        if type(i) == type([]):
            s = ' '.join(i)
        print('')
        Logs.pprint('BOLD', '** Test', sep='')
        Logs.pprint('NORMAL', '%s' % s)
        cmd = i
        if Options.options.grind:
            cmd = 'valgrind ' + i
        if subprocess.call(cmd, shell=True) == desired_status:
            Logs.pprint('GREEN', '** Pass')
        else:
            failures += 1
            Logs.pprint('RED', '** FAIL')

    print('')
    if failures == 0:
        Logs.pprint('GREEN', '** Pass: All %s.%s tests passed' % (appname, name))
    else:
        Logs.pprint('RED', '** FAIL: %d %s.%s tests failed' % (failures, appname, name))
