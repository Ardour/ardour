# -*- python -*-

#
# and there we have it, or do we?
#

import os
import os.path
import sys
import re
import shutil
import glob
import errno
import time
import platform
import string
import commands
from sets import Set
import SCons.Node.FS

SConsignFile()
EnsureSConsVersion(0, 96)

ardour_version = '2.8.13'

subst_dict = { }

#
# Command-line options
#

opts = Variables('scache.conf')
opts.AddVariables(
    ('ARCH', 'Set architecture-specific compilation flags by hand (all flags as 1 argument)',''),
    ('WINDOWS_KEY', 'Set X Modifier (Mod1,Mod2,Mod3,Mod4,Mod5) for "Windows" key', 'Mod4><Super'),
    ('PROGRAM_NAME', 'Set program name (default is "Ardour")', 'Ardour'),
    ('DIST_LIBDIR', 'Explicitly set library dir. If not set, Fedora-style defaults are used (typically lib or lib64)', ''),
    PathVariable('DESTDIR', 'Set the intermediate install "prefix"', '/'),
    PathVariable('PREFIX', 'Set the install "prefix"', '/usr/local'),
    EnumVariable('DIST_TARGET', 'Build target for cross compiling packagers', 'auto', 
          allowed_values=('auto', 'i386', 'i686', 'x86_64', 'powerpc', 'tiger', 'panther', 'leopard', 'none' ), ignorecase=2),
    BoolVariable('AUDIOUNITS', 'Compile with Apple\'s AudioUnit library. (experimental)', 0),
    BoolVariable('COREAUDIO', 'Compile with Apple\'s CoreAudio library', 0),
    BoolVariable('GTKOSX', 'Compile for use with GTK-OSX, not GTK-X11', 0),
    BoolVariable('OLDFONTS', 'Old school font sizes', 0),
    BoolVariable('DEBUG', 'Set to build with debugging information and no optimizations', 0),
    BoolVariable('STL_DEBUG', 'Set to build with Standard Template Library Debugging', 0),
    BoolVariable('DMALLOC', 'Compile and link using the dmalloc library', 0),
    BoolVariable('EXTRA_WARN', 'Compile with -Wextra, -ansi, and -pedantic.  Might break compilation.  For pedants', 0),
    BoolVariable('FFT_ANALYSIS', 'Include FFT analysis window', 1),
    BoolVariable('FREESOUND', 'Include Freesound database lookup', 1),
    BoolVariable('FPU_OPTIMIZATION', 'Build runtime checked assembler code', 1),
    BoolVariable('LIBLO', 'Compile with support for liblo library', 1),
    BoolVariable('NLS', 'Set to turn on i18n support', 1),
    BoolVariable('SURFACES', 'Build support for control surfaces', 1),
    BoolVariable('WIIMOTE', 'Build the wiimote control surface', 0),
    BoolVariable('SYSLIBS', 'USE AT YOUR OWN RISK: CANCELS ALL SUPPORT FROM ARDOUR AUTHORS: Use existing system versions of various libraries instead of internal ones', 0),
    BoolVariable('UNIVERSAL', 'Compile as universal binary.  Requires that external libraries are already universal.', 0),
    BoolVariable('VERSIONED', 'Add revision information to ardour/gtk executable name inside the build directory', 0),
    BoolVariable('VST', 'Compile with support for VST', 0),
    BoolVariable('LV2', 'Compile with support for LV2 (if Lilv is available)', 1),
    BoolVariable('LV2_UI', 'Compile with support for LV2 UIs (if Suil is available)', 1),
    BoolVariable('GPROFILE', 'Compile with support for gprofile (Developers only)', 0),
    BoolVariable('FREEDESKTOP', 'Install MIME type, icons and .desktop file as per the freedesktop.org spec (requires xdg-utils and shared-mime-info). "scons uninstall" removes associations in desktop database', 0),
    BoolVariable('TRANZPORT', 'Compile with support for Frontier Designs (if libusb is available)', 1),
    BoolVariable('AUBIO', "Use Paul Brossier's aubio library for feature detection", 1),
    BoolVariable('AUSTATE', "Build with support for AU settings & presets saving/loading", 0),
    BoolVariable('WITH_CARBON', "Build with support for Carbon AU GUIs", 1),
)

#----------------------------------------------------------------------
# a handy helper that provides a way to merge compile/link information
# from multiple different "environments"
#----------------------------------------------------------------------
#
class LibraryInfo(Environment):
    def __init__(self,*args,**kw):
        Environment.__init__ (self,*args,**kw)
        self.ENV_update(os.environ)
    
    def Merge (self,others):
        for other in others:
            self.Append (LIBS = other.get ('LIBS',[]))
            self.Append (LIBPATH = other.get ('LIBPATH', []))
            self.Append (CPPPATH = other.get('CPPPATH', []))
            self.Append (LINKFLAGS = other.get('LINKFLAGS', []))
            self.Append (CCFLAGS = other.get('CCFLAGS', []))
	self.Replace(LIBPATH = list(Set(self.get('LIBPATH', []))))
	self.Replace(CPPPATH = list(Set(self.get('CPPPATH',[]))))
        #doing LINKFLAGS breaks -framework
        #doing LIBS break link order dependency
    
    def ENV_update(self, src_ENV):
        for k in src_ENV.keys():
            if k in self['ENV'].keys() and k in [ 'PATH', 'LD_LIBRARY_PATH',
                                                  'LIB', 'PKG_CONFIG_PATH', 'INCLUDE' ]:
                self['ENV'][k]=SCons.Util.AppendPath(self['ENV'][k], src_ENV[k])
            else:
                self['ENV'][k]=src_ENV[k]

env = LibraryInfo (options = opts,
                   CPPPATH = [ '.' ],
                   VERSION = ardour_version,
                   TARBALL='ardour-' + ardour_version + '.tar.bz2',
                   DISTFILES = [ ],
                   DISTTREE  = '#ardour-' + ardour_version,
                   DISTCHECKDIR = '#ardour-' + ardour_version + '/check'
                   )

env.ENV_update(os.environ)

#----------------------------------------------------------------------
# Builders
#----------------------------------------------------------------------

# Handy subst-in-file builder
#

def do_subst_in_file(targetfile, sourcefile, dict):
    """Replace all instances of the keys of dict with their values.
    For example, if dict is {'%VERSION%': '1.2345', '%BASE%': 'MyProg'},
    then all instances of %VERSION% in the file will be replaced with 1.2345 etc.
    """
    try:
        f = open(sourcefile, 'rb')
        contents = f.read()
        f.close()
    except:
        raise SCons.Errors.UserError, "Can't read source file %s"%sourcefile
    for (k,v) in dict.items():
        contents = re.sub(k, v, contents)
    try:
        f = open(targetfile, 'wb')
        f.write(contents)
        f.close()
    except:
        raise SCons.Errors.UserError, "Can't write target file %s"%targetfile
    return 0 # success

def subst_in_file(target, source, env):
    if not env.has_key('SUBST_DICT'):
        raise SCons.Errors.UserError, "SubstInFile requires SUBST_DICT to be set."
    d = dict(env['SUBST_DICT']) # copy it
    for (k,v) in d.items():
        if callable(v):
            d[k] = env.subst(v())
        elif SCons.Util.is_String(v):
            d[k]=env.subst(v)
        else:
            raise SCons.Errors.UserError, "SubstInFile: key %s: %s must be a string or callable"%(k, repr(v))
    for (t,s) in zip(target, source):
        return do_subst_in_file(str(t), str(s), d)

def subst_in_file_string(target, source, env):
    """This is what gets printed on the console."""
    return '\n'.join(['Substituting vars from %s into %s'%(str(s), str(t))
                      for (t,s) in zip(target, source)])

def subst_emitter(target, source, env):
    """Add dependency from substituted SUBST_DICT to target.
    Returns original target, source tuple unchanged.
    """
    d = env['SUBST_DICT'].copy() # copy it
    for (k,v) in d.items():
        if callable(v):
            d[k] = env.subst(v())
        elif SCons.Util.is_String(v):
            d[k]=env.subst(v)
    Depends(target, SCons.Node.Python.Value(d))
    # Depends(target, source) # this doesn't help the install-sapphire-linux.sh problem
    return target, source

subst_action = Action (subst_in_file, subst_in_file_string)
env['BUILDERS']['SubstInFile'] = Builder(action=subst_action, emitter=subst_emitter)

#
# internationalization
#

# po_builder: builder function to copy po files to the parent directory while updating them
#
# first source:  .po file
# second source: .pot file
#

def po_builder(target,source,env):
    os.spawnvp (os.P_WAIT, 'cp', ['cp', str(source[0]), str(target[0])])
    args = [ 'msgmerge',
             '--update',
             str(target[0]),
             str(source[1])
             ]
    print 'Updating ' + str(target[0])
    return os.spawnvp (os.P_WAIT, 'msgmerge', args)

po_bld = Builder (action = po_builder)
env.Append(BUILDERS = {'PoBuild' : po_bld})

# mo_builder: builder function for (binary) message catalogs (.mo)
#
# first source:  .po file
#

def mo_builder(target,source,env):
    args = [ 'msgfmt',
             '-c',
             '-o',
             target[0].get_path(),
             source[0].get_path()
             ]
    return os.spawnvp (os.P_WAIT, 'msgfmt', args)

mo_bld = Builder (action = mo_builder)
env.Append(BUILDERS = {'MoBuild' : mo_bld})

# pot_builder: builder function for message templates (.pot)
#
# source: list of C/C++ etc. files to extract messages from
#

def pot_builder(target,source,env):
    args = [ 'xgettext',
             '--keyword=_',
             '--keyword=N_',
             '--from-code=UTF-8',
             '-o', target[0].get_path(),
             "--default-domain=" + env['PACKAGE'],
             '--copyright-holder="Paul Davis"' ]
    args += [ src.get_path() for src in source ]
    
    return os.spawnvp (os.P_WAIT, 'xgettext', args)

pot_bld = Builder (action = pot_builder)
env.Append(BUILDERS = {'PotBuild' : pot_bld})

#
# utility function, not a builder
#

def i18n (buildenv, sources, installenv):
    domain = buildenv['PACKAGE']
    potfile = buildenv['POTFILE']

    #
    # on glibc systems, libintl is part of libc. not true on OS X
    #
    
    if re.search ("darwin[0-9]", config[config_kernel]):
        buildenv.Merge ([ libraries['intl'] ])

    installenv.Alias ('potupdate', buildenv.PotBuild (potfile, sources))
    
    p_oze = [ os.path.basename (po) for po in glob.glob ('po/*.po') ]
    languages = [ po.replace ('.po', '') for po in p_oze ]
    
    for po_file in p_oze:
        buildenv.PoBuild(po_file, ['po/'+po_file, potfile])
        mo_file = po_file.replace (".po", ".mo")
        installenv.Alias ('install', buildenv.MoBuild (mo_file, po_file))
        installenv.Alias ('msgupdate', buildenv.MoBuild (mo_file, po_file))
    
    for lang in languages:
        modir = (os.path.join (install_prefix, 'share/locale/' + lang + '/LC_MESSAGES/'))
        moname = domain + '.mo'
        installenv.Alias('install', installenv.InstallAs (os.path.join (modir, moname), lang + '.mo'))


def fetch_svn_revision (path):
    cmd = "LANG= "
    cmd += "svn info "
    cmd += path
    cmd += " 2>/dev/null | awk '/^Revision:/ { print $2}'"
    return commands.getoutput (cmd)

def create_stored_revision (target = None, source = None, env = None):
    if os.path.exists('.svn'):    
        rev = fetch_svn_revision ('.');
        try:
            text  = "#include <ardour/svn_revision.h>\n"
            text += "namespace ARDOUR {\n";
            text += "extern const char* svn_revision = \"" + rev + "\";\n";
            text += "}\n";
            print '============> writing svn revision info to libs/ardour/svn_revision.cc\n'
            o = file ('libs/ardour/svn_revision.cc', 'w')
            o.write (text)
            o.close ()
        except IOError:
            print "Could not open libs/ardour/svn_revision.cc for writing\n"
            sys.exit (-1)
    else:
        if not os.path.exists('libs/ardour/ardour/svn_revision.h'):    
            print "This release of ardour is missing libs/ardour/ardour/svn_revision.h. Blame the packager."
            sys.exit (-1)

#
# A generic builder for version.cc files
#
# note: requires that DOMAIN, MAJOR, MINOR, MICRO are set in the construction environment
# note: assumes one source files, the header that declares the version variables
#

def version_builder (target, source, env):

    text  = "int " + env['DOMAIN'] + "_major_version = " + str (env['MAJOR']) + ";\n"
    text += "int " + env['DOMAIN'] + "_minor_version = " + str (env['MINOR']) + ";\n"
    text += "int " + env['DOMAIN'] + "_micro_version = " + str (env['MICRO']) + ";\n"
    
    try:
        o = file (target[0].get_path(), 'w')
        o.write (text)
        o.close ()
    except IOError:
        print "Could not open", target[0].get_path(), " for writing\n"
        sys.exit (-1)

    text  = "#ifndef __" + env['DOMAIN'] + "_version_h__\n"
    text += "#define __" + env['DOMAIN'] + "_version_h__\n"
    text += "extern const char* " + env['DOMAIN'] + "_revision;\n"
    text += "extern int " + env['DOMAIN'] + "_major_version;\n"
    text += "extern int " + env['DOMAIN'] + "_minor_version;\n"
    text += "extern int " + env['DOMAIN'] + "_micro_version;\n"
    text += "#endif /* __" + env['DOMAIN'] + "_version_h__ */\n"
    
    try:
        o = file (target[1].get_path(), 'w')
        o.write (text)
        o.close ()
    except IOError:
        print "Could not open", target[1].get_path(), " for writing\n"
        sys.exit (-1)
        
    return None

version_bld = Builder (action = version_builder)
env.Append (BUILDERS = {'VersionBuild' : version_bld})

#
# a builder that makes a hard link from the 'source' executable to a name with
# a "build ID" based on the most recent CVS activity that might be reasonably
# related to version activity. this relies on the idea that the SConscript
# file that builds the executable is updated with new version info and committed
# to the source code repository whenever things change.
#

def versioned_builder(target,source,env):
    w, r = os.popen2( "LANG= svn info 2>/dev/null | awk '/^Revision:/ { print $2}'")
    
    last_revision = r.readline().strip()
    w.close()
    r.close()
    if last_revision == "":
        print "No SVN info found - versioned executable cannot be built"
        return -1
    
    print "The current build ID is " + last_revision
    
    tagged_executable = source[0].get_path() + '-' + last_revision
    
    if os.path.exists (tagged_executable):
        print "Replacing existing executable with the same build tag."
        os.unlink (tagged_executable)
    
    return os.link (source[0].get_path(), tagged_executable)

verbuild = Builder (action = versioned_builder)
env.Append (BUILDERS = {'VersionedExecutable' : verbuild})

#
# source tar file builder
#

def distcopy (target, source, env):
    treedir = str (target[0])
    
    try:
        os.mkdir (treedir)
    except OSError, (errnum, strerror):
        if errnum != errno.EEXIST:
            print 'mkdir ', treedir, ':', strerror
    
    cmd = 'tar cf - '
    #
    # we don't know what characters might be in the file names
    # so quote them all before passing them to the shell
    #
    all_files = ([ str(s) for s in source ])
    cmd += " ".join ([ "'%s'" % quoted for quoted in all_files])
    cmd += ' | (cd ' + treedir + ' && tar xf -)'
    p = os.popen (cmd)
    return p.close ()

def tarballer (target, source, env):
    cmd = 'tar -jcf ' + str (target[0]) +  ' ' + str(source[0]) + "  --exclude '*~'" + " --exclude .svn --exclude '.svn/*'"
    print 'running ', cmd, ' ... '
    p = os.popen (cmd)
    return p.close ()

dist_bld = Builder (action = distcopy,
                    target_factory = SCons.Node.FS.default_fs.Entry,
                    source_factory = SCons.Node.FS.default_fs.Entry,
                    multi = 1)

tarball_bld = Builder (action = tarballer,
                       target_factory = SCons.Node.FS.default_fs.Entry,
                       source_factory = SCons.Node.FS.default_fs.Entry)

env.Append (BUILDERS = {'Distribute' : dist_bld})
env.Append (BUILDERS = {'Tarball' : tarball_bld})

####################
# push environment
####################

def pushEnvironment(context):
    if os.environ.has_key('PATH'):
	context['ENV']['PATH'] = os.environ['PATH']
	
    if os.environ.has_key('PKG_CONFIG_PATH'):
	context['ENV']['PKG_CONFIG_PATH'] = os.environ['PKG_CONFIG_PATH']
	    
    if os.environ.has_key('CC'):
	context['CC'] = os.environ['CC']
		
    if os.environ.has_key('CXX'):
	context['CXX'] = os.environ['CXX']

    if os.environ.has_key('DISTCC_HOSTS'):
	context['ENV']['DISTCC_HOSTS'] = os.environ['DISTCC_HOSTS']
	context['ENV']['HOME'] = os.environ['HOME']

pushEnvironment (env)

#######################
# Dependency Checking #
#######################

deps = \
{
	'glib-2.0'             : '2.10.1',
	'gthread-2.0'          : '2.10.1',
	'gtk+-2.0'             : '2.8.1',
	'libxml-2.0'           : '2.6.0',
	'samplerate'           : '0.1.0',
	'raptor2'              : '2.0.0',
	'lrdf'                 : '0.4.0',
	'jack'                 : '0.120.0',
	'libgnomecanvas-2.0'   : '2.0',
	'sndfile'              : '1.0.18',
        'aubio'                : '0.3.0',
	'liblo'                : '0.24'
}

def DependenciesRequiredMessage():
	print 'You do not have the necessary dependencies required to build ardour'
	print 'Please consult http://ardour.org/building for more information'

def CheckPKGConfig(context, version):
    context.Message( 'Checking for pkg-config version >= %s... ' %version )
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result( ret )
    return ret

def CheckPKGVersion(context, name, version):
    context.Message( 'Checking for %s... ' % name )
    ret = context.TryAction('pkg-config --atleast-version=%s %s' %(version,name) )[0]
    context.Result( ret )
    return ret

def CheckPKGExists(context, name):
    context.Message ('Checking for %s...' % name)
    ret = context.TryAction('pkg-config --exists %s' % name)[0]
    context.Result (ret)
    return ret

conf = Configure(env, custom_tests = { 'CheckPKGConfig' : CheckPKGConfig,
                                       'CheckPKGVersion' : CheckPKGVersion })

# I think a more recent version is needed on win32
min_pkg_config_version = '0.8.0'

if not conf.CheckPKGConfig(min_pkg_config_version):
     print 'pkg-config >= %s not found.' % min_pkg_config_version
     Exit(1)

for pkg, version in deps.iteritems():
	if not conf.CheckPKGVersion( pkg, version ):
		print '%s >= %s not found.' %(pkg, version)
		DependenciesRequiredMessage()
		Exit(1)

env = conf.Finish()

# ----------------------------------------------------------------------
# Construction environment setup
# ----------------------------------------------------------------------

libraries = { }

libraries['core'] = LibraryInfo (CCFLAGS = '-Ilibs')

conf = env.Configure (custom_tests = { 'CheckPKGExists' : CheckPKGExists,
                                       'CheckPKGVersion' : CheckPKGVersion }
                      )
                      

if conf.CheckPKGExists ('fftw3f'):
    libraries['fftw3f'] = LibraryInfo()
    libraries['fftw3f'].ParseConfig('pkg-config --cflags --libs fftw3f')

if conf.CheckPKGExists ('fftw3'):
    libraries['fftw3'] = LibraryInfo()
    libraries['fftw3'].ParseConfig('pkg-config --cflags --libs fftw3')

if conf.CheckPKGExists ('aubio'):
    libraries['aubio'] = LibraryInfo()
    libraries['aubio'].ParseConfig('pkg-config --cflags --libs aubio')

env = conf.Finish ()

if env['FFT_ANALYSIS']:
        #
        # Check for fftw3 header as well as the library
        #

        conf = Configure(libraries['fftw3'])

        if conf.CheckHeader ('fftw3.h') == False:
            print ('Ardour cannot be compiled without the FFTW3 headers, which do not seem to be installed')
            sys.exit (1)            
        conf.Finish()

if env['FREESOUND']:
    if conf.CheckPKGVersion('libcurl', '7.0.0'):
        print 'FREESOUND support cannot be built without the development libraries for CURL 7.X.X or later'
        env['FREESOUND'] = 0;
    else:
	libraries['curl'] = LibraryInfo()
        libraries['curl'].ParseConfig('pkg-config --cflags --libs libcurl')

if env['LV2']:
	conf = env.Configure(custom_tests = { 'CheckPKGVersion' : CheckPKGVersion})
	
	if conf.CheckPKGVersion('lilv-0', '0.5.0'):
		libraries['lilv'] = LibraryInfo()
		libraries['lilv'].ParseConfig('pkg-config --cflags --libs lilv-0')
		env.Append (CCFLAGS="-DHAVE_LV2")
	else:
		print 'LV2 support is not enabled (Lilv not found or older than 0.4.0)'
		env['LV2'] = 0

	if conf.CheckPKGVersion('lilv-0', '0.14.0'):
		env.Append (CCFLAGS="-DHAVE_NEW_LILV")

	if env['LV2_UI']:
		if conf.CheckPKGVersion('suil-0', '0.4.0'):
			libraries['suil'] = LibraryInfo()
			libraries['suil'].ParseConfig('pkg-config --cflags --libs suil-0')
			env.Append (CCFLAGS="-DHAVE_SUIL")
			env['LV2_UI'] = 1
		else:
			print 'LV2 UI support is not enabled (Suil not found or older than 0.4.0'
			env['LV2_UI'] = 0

	conf.Finish()
else:
	env['LV2_UI'] = 0
	print 'LV2 support is not enabled.  Build with \'scons LV2=1\' to enable.'

if not env['WIIMOTE']:
	print 'WIIMOTE not enabled. Build with \'scons WIIMOTE=1\' to enable support.'

libraries['jack'] = LibraryInfo()
libraries['jack'].ParseConfig('pkg-config --cflags --libs jack')

libraries['xml'] = LibraryInfo()
libraries['xml'].ParseConfig('pkg-config --cflags --libs libxml-2.0')

libraries['xslt'] = LibraryInfo()
libraries['xslt'].ParseConfig('pkg-config --cflags --libs libxslt')

libraries['lrdf'] = LibraryInfo()
libraries['lrdf'].ParseConfig('pkg-config --cflags --libs lrdf')

libraries['liblo'] = LibraryInfo()
libraries['liblo'].ParseConfig('pkg-config --cflags --libs liblo')

libraries['raptor'] = LibraryInfo()
libraries['raptor'].ParseConfig('pkg-config --cflags --libs raptor2')

libraries['sndfile'] = LibraryInfo()
libraries['sndfile'].ParseConfig ('pkg-config --cflags --libs sndfile')

libraries['samplerate'] = LibraryInfo()
libraries['samplerate'].ParseConfig('pkg-config --cflags --libs samplerate')

libraries['glib2'] = LibraryInfo()
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs glib-2.0')
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs gobject-2.0')
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs gmodule-2.0')
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs gthread-2.0')

libraries['freetype2'] = LibraryInfo()
libraries['freetype2'].ParseConfig ('pkg-config --cflags --libs freetype2')

libraries['gtk2'] = LibraryInfo()
libraries['gtk2'].ParseConfig ('pkg-config --cflags --libs gtk+-2.0')

libraries['pango'] = LibraryInfo()
libraries['pango'].ParseConfig ('pkg-config --cflags --libs pango')

libraries['libgnomecanvas2'] = LibraryInfo()
libraries['libgnomecanvas2'].ParseConfig ('pkg-config --cflags --libs libgnomecanvas-2.0')

#libraries['flowcanvas'] = LibraryInfo(LIBS='flowcanvas', LIBPATH='#/libs/flowcanvas', CPPPATH='#libs/flowcanvas')

# The Ardour Control Protocol Library

libraries['ardour_cp'] = LibraryInfo (LIBS='ardour_cp', LIBPATH='#libs/surfaces/control_protocol',
                                      CPPPATH='#libs/surfaces/control_protocol')

# The Ardour backend/engine

libraries['ardour'] = LibraryInfo (LIBS='ardour', LIBPATH='#libs/ardour', CPPPATH='#libs/ardour')
libraries['midi++2'] = LibraryInfo (LIBS='midi++', LIBPATH='#libs/midi++2', CPPPATH='#libs/midi++2')
libraries['pbd']    = LibraryInfo (LIBS='pbd', LIBPATH='#libs/pbd', CPPPATH='#libs/pbd')
libraries['gtkmm2ext'] = LibraryInfo (LIBS='gtkmm2ext', LIBPATH='#libs/gtkmm2ext', CPPPATH='#libs/gtkmm2ext')


# SCons should really do this for us

conf = env.Configure ()

have_cxx = conf.TryAction (Action (str(env['CXX']) + ' --version'))
if have_cxx[0] != 1:
    print "This system has no functional C++ compiler. You cannot build Ardour from source without one."
    sys.exit (1)
else:
    print "Congratulations, you have a functioning C++ compiler."

env = conf.Finish()


#
# Compiler flags and other system-dependent stuff
#

opt_flags = []
if env['GPROFILE'] == 1:
    debug_flags = [ '-g', '-pg' ]
else:
    debug_flags = [ '-g' ]

# guess at the platform, used to define compiler flags

config_guess = os.popen("tools/config.guess").read()[:-1]

config_cpu = 0
config_arch = 1
config_kernel = 2
config_os = 3
config = config_guess.split ("-")

print "system triple: " + config_guess

# Autodetect
print 'dist target: ', env['DIST_TARGET'], '\n'
if env['DIST_TARGET'] == 'auto':
    if config[config_arch] == 'apple':
        # The [.] matches to the dot after the major version, "." would match any character
        if re.search ("darwin[0-7][.]", config[config_kernel]) != None:
            env['DIST_TARGET'] = 'panther'
        if re.search ("darwin8[.]", config[config_kernel]) != None:
            env['DIST_TARGET'] = 'tiger'
        else:
            env['DIST_TARGET'] = 'leopard'
    else:
        if re.search ("x86_64", config[config_cpu]) != None:
            env['DIST_TARGET'] = 'x86_64'
        elif re.search("i[0-5]86", config[config_cpu]) != None:
            env['DIST_TARGET'] = 'i386'
        elif re.search("powerpc", config[config_cpu]) != None:
            env['DIST_TARGET'] = 'powerpc'
        else:
            env['DIST_TARGET'] = 'i686'
    print "\n*******************************"
    print "detected DIST_TARGET = " + env['DIST_TARGET']
    print "*******************************\n"

if env['DIST_TARGET'] != 'tiger' and env['DIST_TARGET'] != 'leopard':
	# make sure this is all disabled for non-OS X builds
	env['GTKOSX'] = 0
	env['COREAUDIO'] = 0
	env['AUDIOUNITS'] = 0
	env['AUSTATE'] = 0
	env['WITH_CARBON'] = 0

if config[config_cpu] == 'powerpc' and env['DIST_TARGET'] != 'none':
    # Apple/PowerPC optimization options
    #
    # -mcpu=7450 does not reliably work with gcc 3.*
    #
    if env['DIST_TARGET'] == 'panther' or env['DIST_TARGET'] == 'tiger':
        if config[config_arch] == 'apple':
            ## opt_flags.extend ([ "-mcpu=7450", "-faltivec"])
            # to support g3s but still have some optimization for above
            opt_flags.extend ([ "-mcpu=G3", "-mtune=7450"])
        else:
            opt_flags.extend ([ "-mcpu=7400", "-maltivec", "-mabi=altivec"])
    else:
        opt_flags.extend([ "-mcpu=750", "-mmultiple" ])
    opt_flags.extend (["-mhard-float", "-mpowerpc-gfxopt"])
    #opt_flags.extend (["-Os"])

elif ((re.search ("i[0-9]86", config[config_cpu]) != None) or (re.search ("x86_64", config[config_cpu]) != None)) and env['DIST_TARGET'] != 'none':
    print 'Config CPU is', config[config_cpu], '\n'
    
    build_host_supports_sse = 0
    
    #
    # ARCH_X86 means anything in the x86 family from i386 to x86_64
    # USE_X86_64_ASM is used to distingush 32 and 64 bit assembler
    #

    if (re.search ("(i[0-9]86|x86_64)", config[config_cpu]) != None):
        debug_flags.append ("-DARCH_X86")
        opt_flags.append ("-DARCH_X86")
    
    if config[config_kernel] == 'linux' :
        
        if env['DIST_TARGET'] != 'i386':
            
            flag_line = os.popen ("cat /proc/cpuinfo | grep '^flags'").read()[:-1]
            x86_flags = flag_line.split (": ")[1:][0].split ()
            
            if "mmx" in x86_flags:
                opt_flags.append ("-mmmx")
            if "sse" in x86_flags:
                build_host_supports_sse = 1
            if "3dnow" in x86_flags:
                opt_flags.append ("-m3dnow")
            if config[config_cpu] == "i586":
                opt_flags.append ("-march=i586")
            elif config[config_cpu] == "i686":
                opt_flags.append ("-march=i686")
    
    if ((env['DIST_TARGET'] == 'i686') or (env['DIST_TARGET'] == 'x86_64')) and build_host_supports_sse:
        opt_flags.extend (["-msse", "-mfpmath=sse", "-DUSE_XMMINTRIN"])
        debug_flags.extend (["-msse", "-mfpmath=sse", "-DUSE_XMMINTRIN"])

    if config[config_cpu] == "i586":
        opt_flags.append ("-march=i586")
    elif config[config_cpu] == "i686" and config[config_arch] != 'apple':
        opt_flags.append ("-march=i686")

    if (env['VST']):
        #
        # everything must be 32 bit for VST (we're not replicating Cakewalk's hack, yet ...)
        # 
        opt_flags.extend(["-m32"])
        debug_flags.extend(["-m32"])

# end of processor-specific section

# optimization section
if env['FPU_OPTIMIZATION']:
    if env['DIST_TARGET'] == 'tiger' or env['DIST_TARGET'] == 'leopard':
        opt_flags.append ("-DBUILD_VECLIB_OPTIMIZATIONS");
        debug_flags.append ("-DBUILD_VECLIB_OPTIMIZATIONS");
        libraries['core'].Append(LINKFLAGS= '-framework Accelerate')
    elif env['DIST_TARGET'] == 'i686' or env['DIST_TARGET'] == 'x86_64':
        opt_flags.append ("-DBUILD_SSE_OPTIMIZATIONS")
        debug_flags.append ("-DBUILD_SSE_OPTIMIZATIONS")
        if env['DIST_TARGET'] == 'x86_64' and not env['VST']:
            opt_flags.append ("-DUSE_X86_64_ASM")
            debug_flags.append ("-DUSE_X86_64_ASM")
        if build_host_supports_sse != 1:
            print "\nWarning: you are building Ardour with SSE support even though your system does not support these instructions. (This may not be anerror, especially if you are a package maintainer)"
# end optimization section

# handle x86/x86_64 libdir properly

if env['DIST_LIBDIR'] == '':
    if env['DIST_TARGET'] == 'x86_64':
        env['LIBDIR']='lib64'
    else:
        env['LIBDIR']='lib'
else:
    env['LIBDIR'] = env['DIST_LIBDIR']

#
# no VST on x86_64
#

if env['DIST_TARGET'] == 'x86_64' and env['VST']:
    print "\n\n=================================================="
    print "You cannot use VST plugins with a 64 bit host. Please run scons with VST=0"
    print "\nIt is theoretically possible to build a 32 bit host on a 64 bit system."
    print "However, this is tricky and not recommended for beginners."
    sys.exit (-1)

#
# a single way to test if we're on OS X
#

if env['DIST_TARGET'] in [ 'panther','tiger','leopard' ]:
    env['IS_OSX'] = 1
    # force tiger or later, to avoid issues on PPC which defaults
    # back to 10.1 if we don't tell it otherwise.
    env.Append (CCFLAGS="-DMAC_OS_X_VERSION_MIN_REQUIRED=1040")

    #if env['DIST_TARGET'] == 'leopard':
        # need this to really build against the 10.4 SDK when building on leopard
        # ideally this would be configurable, but lets just do that later when we need it
        #env.Append(CCFLAGS="-mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk")
        #env.Append(LINKFLAGS="-mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk")

else:
    env['IS_OSX'] = 0

#
# save off guessed arch element in an env
#
env.Append(CONFIG_ARCH=config[config_arch])


#
# ARCH="..." overrides all
#

if env['ARCH'] != '':
    opt_flags = env['ARCH'].split()

#
# prepend boiler plate optimization flags
#

opt_flags[:0] = [
    "-O3",
    "-fomit-frame-pointer",
    "-ffast-math",
    "-fstrength-reduce",
    "-pipe"
    ]

if env['DEBUG'] == 1:
    env.Append(CCFLAGS=" ".join (debug_flags))
    env.Append(LINKFLAGS=" ".join (debug_flags))
else:
    env.Append(CCFLAGS=" ".join (opt_flags))
    env.Append(LINKFLAGS=" ".join (opt_flags))

if env['STL_DEBUG'] == 1:
    env.Append(CXXFLAGS="-D_GLIBCXX_DEBUG")

if env['UNIVERSAL'] == 1:
    env.Append(CCFLAGS="-arch i386 -arch ppc")
    env.Append(LINKFLAGS="-arch i386 -arch ppc")


#
# warnings flags
#

env.Append(CCFLAGS="-Wall")
env.Append(CXXFLAGS="-Woverloaded-virtual")

if env['EXTRA_WARN']:
    env.Append(CCFLAGS="-Wextra -pedantic -ansi")
    env.Append(CXXFLAGS="-ansi")
#    env.Append(CFLAGS="-iso")

if env['LIBLO']:
    env.Append(CCFLAGS="-DHAVE_LIBLO")


# It appears that SCons propagates CCFLAGS into CXXFLAGS so 
# we need only set these once.
#
# the program name is defined everywhere
#
env.Append(CCFLAGS='-DPROGRAM_NAME=\\"' + env['PROGRAM_NAME'] + '\\"')

#
# we deal with threads and big files
#
env.Append(CCFLAGS="-D_REENTRANT -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64")

#
# we use inttypes.h format macros anywhere we want
#
env.Append(CCFLAGS="-D__STDC_FORMAT_MACROS")

#
# fix scons nitpickiness on APPLE
#

def prep_libcheck(topenv, libinfo):
    if os.path.exists (os.path.expanduser ('~/gtk/inst')):
	#
        # build-gtk-stack puts the GTK stack under ~/gtk/inst
        # build-ardour-stack puts other Ardour deps under ~/a3/inst
        #
        # things need to build with this in mind
        #
        GTKROOT = os.path.expanduser ('~/gtk/inst')
        libinfo.Append(CPPPATH= GTKROOT + "/include", LIBPATH= GTKROOT + "/lib")
        libinfo.Append(CXXFLAGS="-I" + GTKROOT + "/include", LINKFLAGS="-L" + GTKROOT + "/lib")
        ARDOURDEP_ROOT = os.path.expanduser ('~/a3/inst')
        libinfo.Append(CPPPATH= ARDOURDEP_ROOT + "/include", LIBPATH= ARDOURDEP_ROOT + "/lib")
        libinfo.Append(CXXFLAGS="-I" + ARDOURDEP_ROOT + "/include", LINKFLAGS="-L" + ARDOURDEP_ROOT + "/lib")
	    
prep_libcheck(env, env)


#
# these are part of the Ardour source tree because they are C++
# 

libraries['vamp'] = LibraryInfo (LIBS='vampsdk',
                                 LIBPATH='#libs/vamp-sdk',
                                 CPPPATH='#libs/vamp-sdk')
libraries['vamphost'] = LibraryInfo (LIBS='vamphostsdk',
                                 LIBPATH='#libs/vamp-sdk',
                                 CPPPATH='#libs/vamp-sdk')

env['RUBBERBAND'] = False

conf = Configure (env)

if conf.CheckHeader ('fftw3.h'):
    env['RUBBERBAND'] = True
    libraries['rubberband'] = LibraryInfo (LIBS='rubberband',
                                           LIBPATH='#libs/rubberband',
                                           CPPPATH='#libs/rubberband',
                                           CCFLAGS='-DUSE_RUBBERBAND')
else:
    print ""
    print "-------------------------------------------------------------------------"
    print "You do not have the FFTW single-precision development package installed."
    print "This prevents Ardour from using the Rubberband library for timestretching"
    print "and pitchshifting. It will fall back on SoundTouch for timestretch, and "
    print "pitchshifting will not be available."
    print "-------------------------------------------------------------------------"
    print ""

conf.Finish()

#
# Check for libusb

libraries['usb'] = LibraryInfo ()
prep_libcheck(env, libraries['usb'])

conf = Configure (libraries['usb'])
if conf.CheckLib ('usb', 'usb_interrupt_write'):
    have_libusb = True
else:
    have_libusb = False

# check for linux/input.h while we're at it for powermate
if conf.CheckHeader('linux/input.h'):
    have_linux_input = True
else:
    have_linux_input = False

libraries['usb'] = conf.Finish ()

#
# Check for wiimote dependencies

if env['WIIMOTE']:
    wiimoteConf = env.Configure ( )
    if not wiimoteConf.CheckHeader('cwiid.h'):
	print 'WIIMOTE configured but you are missing libcwiid!'
        sys.exit(1)
    if not wiimoteConf.CheckHeader('bluetooth/bluetooth.h'):
        print 'WIIMOTE configured but you are missing the libbluetooth headers which you need to compile wiimote support!'
        sys.exit(1)
    wiimoteConf.Finish()


#
# need a way to see if the installed version of libsndfile supports
# FLAC ....
# 

# boost (we don't link against boost, just use some header files)

libraries['boost'] = LibraryInfo ()
prep_libcheck(env, libraries['boost'])
libraries['boost'].Append(CPPPATH="/usr/local/include", LIBPATH="/usr/local/lib")
conf = Configure (libraries['boost'])
if conf.CheckHeader ('boost/shared_ptr.hpp', language='CXX') == False:
        print "Boost header files do not appear to be installed. You also might be running a buggy version of scons. Try scons 0.97 if you can."
        sys.exit (1)
    
libraries['boost'] = conf.Finish ()

#
# Check for dmalloc

libraries['dmalloc'] = LibraryInfo ()
prep_libcheck(env, libraries['dmalloc'])

#
# look for the threaded version
#

conf = Configure (libraries['dmalloc'])
if conf.CheckLib ('dmallocth', 'dmalloc_shutdown'):
    have_libdmalloc = True
else:
    have_libdmalloc = False

libraries['dmalloc'] = conf.Finish ()

#
# ensure FREEDESKTOP target is doable..
#

conf = env.Configure ()
if env['FREEDESKTOP']:
	have_update_mime_database = conf.TryAction (Action ('update-mime-database -v'))
	if have_update_mime_database[0] != 1:
		print "Warning. You have no update-mime-database command in your PATH. FREEDESKTOP is now disabled."
		env['FREEDESKTOP'] = 0
	have_gtk_update_icon_cache = conf.TryAction (Action ('gtk-update-icon-cache -?'))
	if have_gtk_update_icon_cache[0] != 1:
		print "Warning. You have no gtk-update-icon-cache command in your PATH. FREEDESKTOP is now disabled."
		env['FREEDESKTOP'] = 0
	have_update_desktop_database = conf.TryAction (Action ('update-desktop-database -?'))
	if have_update_desktop_database[0] != 1:
		print "Warning. You have no update-desktop-database command in your PATH. FREEDESKTOP is now disabled."
		env['FREEDESKTOP'] = 0
env = conf.Finish()

#
# Audio/MIDI library (needed for MIDI, since audio is all handled via JACK)
#

conf = Configure(env)

if conf.CheckCHeader('alsa/asoundlib.h'):
    libraries['sysmidi'] = LibraryInfo (LIBS='asound')
    env['SYSMIDI'] = 'ALSA Sequencer'
    subst_dict['%MIDITAG%'] = "seq"
    subst_dict['%MIDITYPE%'] = "alsa/sequencer"
elif conf.CheckCHeader('/System/Library/Frameworks/CoreMIDI.framework/Headers/CoreMIDI.h'):
    # this line is needed because scons can't handle -framework in ParseConfig() yet.
    if env['GTKOSX']:
        # We need Carbon as well as the rest
        libraries['sysmidi'] = LibraryInfo (
	    	LINKFLAGS = ' -framework CoreMIDI -framework CoreFoundation -framework CoreAudio -framework CoreServices -framework AudioUnit -framework AudioToolbox -framework Carbon -bind_at_load' )
    else:
        libraries['sysmidi'] = LibraryInfo (
		LINKFLAGS = ' -framework CoreMIDI -framework CoreFoundation -framework CoreAudio -framework CoreServices -framework AudioUnit -framework AudioToolbox -bind_at_load' )
    env['SYSMIDI'] = 'CoreMIDI'
    subst_dict['%MIDITAG%'] = "ardour"
    subst_dict['%MIDITYPE%'] = "coremidi"
else:
    print "It appears you don't have the required MIDI libraries installed. For Linux this means you are missing the development package for ALSA libraries."
    sys.exit (1)

pname = env['PROGRAM_NAME']
subst_dict['%MIDI_DEVICE_NAME%'] = pname.lower()

env = conf.Finish()

if env['GTKOSX']:
    clearlooks_version = 'libs/clearlooks-newer'
else:
    clearlooks_version = 'libs/clearlooks-newer'

if env['SYSLIBS']:

    syslibdeps = \
    {
        'sigc++-2.0'           : '2.0',
        'gtkmm-2.4'            : '2.8',
        'libgnomecanvasmm-2.6' : '2.12.0'
    }

    conf = Configure(env, custom_tests = { 'CheckPKGConfig' : CheckPKGConfig,
                    'CheckPKGVersion' : CheckPKGVersion })

    for pkg, version in syslibdeps.iteritems():
        if not conf.CheckPKGVersion( pkg, version ):
            print '%s >= %s not found.' %(pkg, version)
            DependenciesRequiredMessage()
            Exit(1)
    
    env = conf.Finish()
    
    libraries['sigc2'] = LibraryInfo()
    libraries['sigc2'].ParseConfig('pkg-config --cflags --libs sigc++-2.0')
    libraries['glibmm2'] = LibraryInfo()
    libraries['glibmm2'].ParseConfig('pkg-config --cflags --libs glibmm-2.4')
    libraries['cairomm'] = LibraryInfo()
    libraries['cairomm'].ParseConfig('pkg-config --cflags --libs cairomm-1.0')
    libraries['gdkmm2'] = LibraryInfo()
    libraries['gdkmm2'].ParseConfig ('pkg-config --cflags --libs gdkmm-2.4')
    libraries['gtkmm2'] = LibraryInfo()
    libraries['gtkmm2'].ParseConfig ('pkg-config --cflags --libs gtkmm-2.4')
    libraries['atkmm'] = LibraryInfo()
    libraries['atkmm'].ParseConfig ('pkg-config --cflags --libs atkmm-1.6')
    libraries['pangomm'] = LibraryInfo()
    libraries['pangomm'].ParseConfig ('pkg-config --cflags --libs pangomm-1.4')
    libraries['libgnomecanvasmm'] = LibraryInfo()
    libraries['libgnomecanvasmm'].ParseConfig ('pkg-config --cflags --libs libgnomecanvasmm-2.6')

#    libraries['libglademm'] = LibraryInfo()
#    libraries['libglademm'].ParseConfig ('pkg-config --cflags --libs libglademm-2.4')

#    libraries['flowcanvas'] = LibraryInfo(LIBS='flowcanvas', LIBPATH='#/libs/flowcanvas', CPPPATH='#libs/flowcanvas')

    libraries['appleutility'] = LibraryInfo(LIBS='libappleutility',
                                            LIBPATH='#libs/appleutility',
                                            CPPPATH='#libs/appleutility')
    
    coredirs = [
        'templates',
        'manual'
    ]
    
    subdirs = [
        'libs/pbd',
        'libs/midi++2',
        'libs/ardour',
        'libs/vamp-sdk',
        'libs/vamp-plugins/',
    # these are unconditionally included but have
    # tests internally to avoid compilation etc
    # if VST is not set
        'libs/fst',
        'vst',
    # this is unconditionally included but has
    # tests internally to avoid compilation etc
    # if COREAUDIO is not set
        'libs/appleutility'
        ]
    
    gtk_subdirs = [
#        'libs/flowcanvas',
        'libs/gtkmm2ext',
        'gtk2_ardour',
        clearlooks_version
        ]

else:
    libraries['sigc2'] = LibraryInfo(LIBS='sigc++2',
                                    LIBPATH='#libs/sigc++2',
                                    CPPPATH='#libs/sigc++2')
    libraries['glibmm2'] = LibraryInfo(LIBS='glibmm2',
                                    LIBPATH='#libs/glibmm2',
                                    CPPPATH='#libs/glibmm2')
    libraries['pangomm'] = LibraryInfo(LIBS='pangomm',
                                    LIBPATH='#libs/gtkmm2/pango',
                                    CPPPATH='#libs/gtkmm2/pango')
    libraries['cairomm'] = LibraryInfo(LIBS='cairomm',
                                    LIBPATH='#libs/cairomm',
                                    CPPPATH='#libs/cairomm')
    libraries['atkmm'] = LibraryInfo(LIBS='atkmm',
                                     LIBPATH='#libs/gtkmm2/atk',
                                     CPPPATH='#libs/gtkmm2/atk')
    libraries['gdkmm2'] = LibraryInfo(LIBS='gdkmm2',
                                      LIBPATH='#libs/gtkmm2/gdk',
                                      CPPPATH='#libs/gtkmm2/gdk')
    libraries['gtkmm2'] = LibraryInfo(LIBS='gtkmm2',
                                     LIBPATH="#libs/gtkmm2/gtk",
                                     CPPPATH='#libs/gtkmm2/gtk/')
    libraries['libgnomecanvasmm'] = LibraryInfo(LIBS='libgnomecanvasmm',
                                                LIBPATH='#libs/libgnomecanvasmm',
                                                CPPPATH='#libs/libgnomecanvasmm')
    
#    libraries['libglademm'] = LibraryInfo(LIBS='libglademm',
#                                          LIBPATH='#libs/libglademm',
#                                          CPPPATH='#libs/libglademm')
    libraries['appleutility'] = LibraryInfo(LIBS='libappleutility',
                                            LIBPATH='#libs/appleutility',
                                            CPPPATH='#libs/appleutility')

    coredirs = [
        'templates',
	'manual'
    ]
    
    subdirs = [
        'libs/sigc++2',
        'libs/pbd',
        'libs/midi++2',
        'libs/ardour',
        'libs/vamp-sdk',
        'libs/vamp-plugins/',
    # these are unconditionally included but have
    # tests internally to avoid compilation etc
    # if VST is not set
        'libs/fst',
        'vst',
    # this is unconditionally included but has
    # tests internally to avoid compilation etc
    # if COREAUDIO is not set
        'libs/appleutility'
        ]
    
    gtk_subdirs = [
        'libs/glibmm2',
        'libs/gtkmm2/pango',
        'libs/gtkmm2/atk',
        'libs/gtkmm2/gdk',
        'libs/gtkmm2/gtk',
        'libs/libgnomecanvasmm',
        'libs/gtkmm2ext',
        'gtk2_ardour',
        clearlooks_version
        ]

#
# * always build the LGPL control protocol lib, since we link against it from libardour
# * ditto for generic MIDI
# * tranzport & wiimote check whether they should build internally, but we need them here
#   so that they are included in the tarball
#

surface_subdirs = [ 'libs/surfaces/control_protocol',
                    'libs/surfaces/generic_midi',
                    'libs/surfaces/tranzport',
                    'libs/surfaces/mackie',
                    'libs/surfaces/powermate',
		    'libs/surfaces/wiimote'
                    ]

if env['SURFACES']:
    if have_libusb:
        env['TRANZPORT'] = 1
    else:
        env['TRANZPORT'] = 0
        print 'Disabled building Tranzport code because libusb could not be found'

    if have_linux_input:
        env['POWERMATE'] = 1
    else:
        env['POWERMATE'] = 0
        print 'Disabled building Powermate code because linux/input.h could not be found'

    if os.access ('libs/surfaces/sony9pin', os.F_OK):
        surface_subdirs += [ 'libs/surfaces/sony9pin' ]
else:
    env['POWERMATE'] = 0
    env['TRANZPORT'] = 0

#
# timestretch libraries
#

timefx_subdirs = []
if env['RUBBERBAND']:
    timefx_subdirs += ['libs/rubberband']

#
# Tools
#
if env['IS_OSX'] == 0 :
	tools_subdirs = [ 'tools/sanity_check' ]
else:
	tools_subdirs = [ ]


opts.Save('scache.conf', env)
Help(opts.GenerateHelpText(env))

final_prefix = '$PREFIX'

if env['DESTDIR'] :
    install_prefix = '$DESTDIR/$PREFIX'
else:
    install_prefix = env['PREFIX']

subst_dict['%INSTALL_PREFIX%'] = install_prefix;
subst_dict['%FINAL_PREFIX%'] = final_prefix;
subst_dict['%PREFIX%'] = final_prefix;

if env['PREFIX'] == '/usr':
    final_config_prefix = '/etc'
else:
    final_config_prefix = env['PREFIX'] + '/etc'

config_prefix = '$DESTDIR' + final_config_prefix

#
# everybody needs this
#

env.Merge ([ libraries['core'] ])


#
# i18n support
#

conf = Configure (env)
if env['NLS']:
    nls_error = 'This system is not configured for internationalized applications.  An english-only version will be built:'
    print 'Checking for internationalization support ...'
    have_gettext = conf.TryAction(Action('xgettext --version'))
    if have_gettext[0] != 1:
        nls_error += ' No xgettext command.'
        env['NLS'] = 0
    else:
        print "Found xgettext"
    
    have_msgmerge = conf.TryAction(Action('msgmerge --version'))
    if have_msgmerge[0] != 1:
        nls_error += ' No msgmerge command.'
        env['NLS'] = 0
    else:
        print "Found msgmerge"
    
    if not conf.CheckCHeader('libintl.h'):
        nls_error += ' No libintl.h.'
        env['NLS'] = 0
        
    if env['NLS'] == 0:
        print nls_error
    else:
	if config[config_arch] == 'apple':
	   libraries['intl'] = LibraryInfo (LIBS='intl')
        else:
	   libraries['intl'] = LibraryInfo ()
        print "International version will be built."

env = conf.Finish()

if env['NLS'] == 1:
    env.Append(CCFLAGS="-DENABLE_NLS")

Export('env install_prefix final_prefix config_prefix final_config_prefix libraries i18n ardour_version subst_dict')

#
# the configuration file may be system dependent
#

conf = env.Configure ()

if conf.CheckCHeader('/System/Library/Frameworks/CoreAudio.framework/Versions/A/Headers/CoreAudio.h'):
    subst_dict['%JACK_INPUT%'] = "coreaudio:Built-in Audio:in"
    subst_dict['%JACK_OUTPUT%'] = "coreaudio:Built-in Audio:out"
else:
    subst_dict['%JACK_INPUT%'] = "alsa_pcm:playback_"
    subst_dict['%JACK_OUTPUT%'] = "alsa_pcm:capture_"

# posix_memalign available
if not conf.CheckFunc('posix_memalign'):
    print 'Did not find posix_memalign(), using malloc'
    env.Append(CCFLAGS='-DNO_POSIX_MEMALIGN')


env = conf.Finish()

# Which GTK tooltips API

gtktestenv = env.Clone ()
gtktestenv.Merge ([
        libraries['gtk2']
        ])

conf = gtktestenv.Configure ()

if conf.CheckFunc('gtk_widget_set_tooltip_text'):
    env.Append (CXXFLAGS='-DGTK_NEW_TOOLTIP_API')

conf.Finish ()


# generate the per-user and system rc files from the same source

sysrcbuild = env.SubstInFile ('ardour_system.rc','ardour.rc.in', SUBST_DICT = subst_dict)

# add to the substitution dictionary

subst_dict['%VERSION%'] = ardour_version[0:3]
subst_dict['%EXTRA_VERSION%'] = ardour_version[3:]
subst_dict['%REVISION_STRING%'] = ''
if os.path.exists('.svn'):
    subst_dict['%REVISION_STRING%'] = '.' + fetch_svn_revision ('.') + 'svn'

# specbuild = env.SubstInFile ('ardour.spec','ardour.spec.in', SUBST_DICT = subst_dict)

the_revision = env.Command ('frobnicatory_decoy', [], create_stored_revision)
remove_ardour = env.Command ('frobnicatory_decoy2', [],
                             [ Delete ('$PREFIX/etc/ardour2'),
                               Delete ('$PREFIX/lib/ardour2'),
                               Delete ('$PREFIX/bin/ardour2'),
                               Delete ('$PREFIX/share/ardour2')])

env.Alias('revision', the_revision)
env.Alias('install', env.Install(os.path.join(config_prefix, 'ardour2'), 'ardour_system.rc'))
env.Alias('uninstall', remove_ardour)

Default (sysrcbuild)

# source tarball

Precious (env['DISTTREE'])

env.Distribute (env['DISTTREE'],
               [ 'SConstruct', 
                  'COPYING', 'PACKAGER_README', 'README',
                  'ardour.rc.in',
                  'tools/config.guess',
                  'icons/icon/ardour_icon_mac_mask.png',
                  'icons/icon/ardour_icon_mac.png',
                  'icons/icon/ardour_icon_tango_16px_blue.png',
                  'icons/icon/ardour_icon_tango_16px_red.png',
                  'icons/icon/ardour_icon_tango_22px_blue.png',
                  'icons/icon/ardour_icon_tango_22px_red.png',
                  'icons/icon/ardour_icon_tango_32px_blue.png',
                  'icons/icon/ardour_icon_tango_32px_red.png',
                  'icons/icon/ardour_icon_tango_48px_blue.png',
                  'icons/icon/ardour_icon_tango_48px_red.png'
                  ] +
                glob.glob ('ardour.1*') +
                glob.glob ('libs/clearlooks-newer/*.c') +
                glob.glob ('libs/clearlooks-newer/*.h') +
                glob.glob ('libs/clearlooks-newer/SConscript')
                )

srcdist = env.Tarball(env['TARBALL'], [ env['DISTTREE'], the_revision ])
env.Alias ('srctar', srcdist)

#
# don't leave the distree around
#

env.AddPreAction (env['DISTTREE'], Action ('rm -rf ' + str (File (env['DISTTREE']))))
env.AddPostAction (srcdist, Action ('rm -rf ' + str (File (env['DISTTREE']))))

#
# Update revision info before going into subdirs
#

create_stored_revision()

#
# the subdirs
#

for subdir in coredirs:
    SConscript (subdir + '/SConscript')

for sublistdir in [ subdirs, timefx_subdirs, gtk_subdirs, surface_subdirs, tools_subdirs ]:
    for subdir in sublistdir:
        SConscript (subdir + '/SConscript')

# cleanup
env.Clean ('scrub', [ 'scache.conf', '.sconf_temp', '.sconsign.dblite', 'config.log'])

