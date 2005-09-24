# -*- python -*-

import os
import sys
import re
import shutil
import glob
import errno
import time
import SCons.Node.FS

SConsignFile()
EnsureSConsVersion(0, 96)

version = '1.9beta1'

subst_dict = { }

#
# Command-line options
#

opts = Options('scache.conf')
opts.AddOptions(
    BoolOption('ALTIVEC', 'Compile using Altivec instructions', 0),
  ('ARCH', 'Set architecture-specific compilation flags by hand (all flags as 1 argument)',''),
    BoolOption('SYSLIBS', 'USE AT YOUR OWN RISK: CANCELS ALL SUPPORT FROM ARDOUR AUTHORS: Use existing system versions of various libraries instead of internal ones', 0),
    BoolOption('DEBUG', 'Set to build with debugging information and no optimizations', 0),
    PathOption('DESTDIR', 'Set the intermediate install "prefix"', '/'),
    BoolOption('DEVBUILD', 'Use shared libardour (developers only)', 0),
    BoolOption('SIGCCVSBUILD', 'Use if building sigc++ with a new configure.ac (developers only)', 0),
    BoolOption('NLS', 'Set to turn on i18n support', 1),
    BoolOption('NOARCH', 'Do not use architecture-specific compilation flags', 0),
    PathOption('PREFIX', 'Set the install "prefix"', '/usr/local'),
    BoolOption('VST', 'Compile with support for VST', 0),
    BoolOption('VERSIONED', 'Add version information to ardour/gtk executable name inside the build directory', 0),
    BoolOption('USE_SSE_EVERYWHERE', 'Ask the compiler to use x86/SSE instructions and also our hand-written x86/SSE optimizations when possible (off by default)', 0),
    BoolOption('BUILD_SSE_OPTIMIZATIONS', 'Use our hand-written x86/SSE optimizations when possible (off by default)', 0)
  )


#----------------------------------------------------------------------
# a handy helper that provides a way to merge compile/link information
# from multiple different "environments"
#----------------------------------------------------------------------
#
class LibraryInfo(Environment):
    def __init__(self,*args,**kw):
        Environment.__init__ (self,*args,**kw)
        
    def Merge (self,others):
        for other in others:
            self.Append (LIBS = other.get ('LIBS',[]))
            self.Append (LIBPATH = other.get ('LIBPATH', []))	
            self.Append (CPPPATH = other.get('CPPPATH', []))
            self.Append (LINKFLAGS = other.get('LINKFLAGS', []))


env = LibraryInfo (options = opts,
                   CPPPATH = [ '.' ],
                   VERSION = version,
                   TARBALL='ardour-' + version + '.tar.bz2',
                   DISTFILES = [ ],
                   DISTTREE  = '#ardour-' + version,
                   DISTCHECKDIR = '#ardour-' + version + '/check'
                   )


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

# po_helper
#
# this is not a builder. we can't list the .po files as a target,
# because then scons -c will remove them (even Precious doesn't alter
# this). this function is called whenever a .mo file is being
# built, and will conditionally update the .po file if necessary.
#

def po_helper(po,pot):
    args = [ 'msgmerge',
             '--update',
             po,
             pot,
             ]
    print 'Updating ' + po
    return os.spawnvp (os.P_WAIT, 'msgmerge', args)

# mo_builder: builder function for (binary) message catalogs (.mo)
#
# first source:  .po file
# second source: .pot file
#

def mo_builder(target,source,env):
    po_helper (source[0].get_path(), source[1].get_path())
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

    installenv.Alias ('potupdate', buildenv.PotBuild (potfile, sources))

    p_oze = [ os.path.basename (po) for po in glob.glob ('po/*.po') ]
    languages = [ po.replace ('.po', '') for po in p_oze ]
    m_oze = [ po.replace (".po", ".mo") for po in p_oze ]
    
    for mo in m_oze[:]:
        po = 'po/' + mo.replace (".mo", ".po")
        installenv.Alias ('install', buildenv.MoBuild (mo, [ po, potfile ]))
        
    for lang in languages[:]:
        modir = (os.path.join (install_prefix, 'share/locale/' + lang + '/LC_MESSAGES/'))
        moname = domain + '.mo'
        installenv.Alias('install', installenv.InstallAs (os.path.join (modir, moname), lang + '.mo'))

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
      o.close ();
   except IOError:
      print "Could not open", target[0].get_path(), " for writing\n"
      sys.exit (-1)

   text  = "#ifndef __" + env['DOMAIN'] + "_version_h__\n";
   text += "#define __" + env['DOMAIN'] + "_version_h__\n";
   text += "extern int " + env['DOMAIN'] + "_major_version;\n"
   text += "extern int " + env['DOMAIN'] + "_minor_version;\n"
   text += "extern int " + env['DOMAIN'] + "_micro_version;\n"
   text += "#endif /* __" + env['DOMAIN'] + "_version_h__ */\n";

   try:
      o = file (target[1].get_path(), 'w')
      o.write (text)
      o.close ();
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
    # build ID is composed of a representation of the date of the last CVS transaction
    # for this (SConscript) file
    
    try:
        o = file (source[0].get_dir().get_path() +  '/CVS/Entries', "r")
    except IOError:
        print "Could not CVS/Entries for reading"
        return -1

    last_date = ""        
    lines = o.readlines()
    for line in lines:
        if line[0:12] == '/SConscript/':
            parts = line.split ("/")
            last_date = parts[3]
            break
    o.close ()

    if last_date == "":
        print "No SConscript CVS update info found - versioned executable cannot be built"
        return -1

    tag = time.strftime ('%Y%M%d%H%m', time.strptime (last_date));
    print "The current build ID is " + tag

    tagged_executable = source[0].get_path() + '-' + tag

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
    return p.close ();

def tarballer (target, source, env):            
    cmd = 'tar -jcf ' + str (target[0]) +  ' ' + str(source[0]) + "  --exclude '*~'"
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

# ----------------------------------------------------------------------
# Construction environment setup
# ----------------------------------------------------------------------

libraries = { }

libraries['core'] = LibraryInfo (CPPPATH = [ '#libs'])

libraries['sndfile'] = LibraryInfo()
libraries['sndfile'].ParseConfig('pkg-config --cflags --libs sndfile')

libraries['lrdf'] = LibraryInfo()
libraries['lrdf'].ParseConfig('pkg-config --cflags --libs lrdf')

libraries['raptor'] = LibraryInfo()
libraries['raptor'].ParseConfig('pkg-config --cflags --libs raptor')

libraries['samplerate'] = LibraryInfo()
libraries['samplerate'].ParseConfig('pkg-config --cflags --libs samplerate')

libraries['jack'] = LibraryInfo()
libraries['jack'].ParseConfig('pkg-config --cflags --libs jack')

libraries['xml'] = LibraryInfo()
libraries['xml'].ParseConfig('pkg-config --cflags --libs libxml-2.0')

libraries['glib2'] = LibraryInfo()
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs glib-2.0')
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs gobject-2.0')
libraries['glib2'].ParseConfig ('pkg-config --cflags --libs gmodule-2.0')

libraries['gtk2'] = LibraryInfo()
libraries['gtk2'].ParseConfig ('pkg-config --cflags --libs gtk+-2.0')

libraries['pango'] = LibraryInfo()
libraries['pango'].ParseConfig ('pkg-config --cflags --libs pango')

libraries['libgnomecanvas2'] = LibraryInfo()
libraries['libgnomecanvas2'].ParseConfig ('pkg-config --cflags --libs libgnomecanvas-2.0')

libraries['ardour'] = LibraryInfo (LIBS='ardour', LIBPATH='#libs/ardour', CPPPATH='#libs/ardour')
libraries['midi++2'] = LibraryInfo (LIBS='midi++', LIBPATH='#libs/midi++2', CPPPATH='#libs/midi++2')
libraries['pbd3']    = LibraryInfo (LIBS='pbd', LIBPATH='#libs/pbd3', CPPPATH='#libs/pbd3')
libraries['gtkmm2ext'] = LibraryInfo (LIBS='gtkmm2ext', LIBPATH='#libs/gtkmm2ext', CPPPATH='#libs/gtkmm2ext')
#libraries['cassowary'] = LibraryInfo(LIBS='cassowary', LIBPATH='#libs/cassowary', CPPPATH='#libs/cassowary')

libraries['fst'] = LibraryInfo()
if env['VST']:
    libraries['fst'].ParseConfig('pkg-config --cflags --libs libfst')

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
    libraries['sysmidi'] = LibraryInfo (LINKFLAGS= '-framework CoreMIDI -framework CoreFoundation -framework CoreAudio -framework CoreServices -framework AudioUnit -bind_at_load')
    env['SYSMIDI'] = 'CoreMIDI'
    subst_dict['%MIDITAG%'] = "ardour"
    subst_dict['%MIDITYPE%'] = "coremidi"

env = conf.Finish()

if env['SYSLIBS']:

    libraries['sigc2'] = LibraryInfo()
    libraries['sigc2'].ParseConfig('pkg-config --cflags --libs sigc++-2.0')

    libraries['gtkmm2'] = LibraryInfo()
    libraries['gtkmm2'].ParseConfig ('pkg-config --cflags --libs gtkmm-2.0')

    libraries['soundtouch'] = LibraryInfo(LIBS='SoundTouch')

    coredirs = [
        'templates'
    ]

    subdirs = [
#	'libs/cassowary',
        'libs/pbd3',
        'libs/midi++2',
        'libs/ardour',
        'templates'
        ]

    gtk_subdirs = [
        'libs/gtkmm2ext',
        'gtk2_ardour',
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

    libraries['soundtouch'] = LibraryInfo(LIBS='soundtouch',
                                          LIBPATH='#libs/soundtouch',
                                          CPPPATH=['#libs', '#libs/soundtouch'])

    coredirs = [
        'libs/soundtouch',
        'templates'
    ]

    subdirs = [
#	'libs/cassowary',
        'libs/sigc++2',
        'libs/pbd3',
        'libs/midi++2',
        'libs/ardour'
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
        ]

opts.Save('scache.conf', env)
Help(opts.GenerateHelpText(env))

if os.environ.has_key('PATH'):
    env.Append(PATH = os.environ['PATH'])

if os.environ.has_key('PKG_CONFIG_PATH'):
    env.Append(PKG_CONFIG_PATH = os.environ['PKG_CONFIG_PATH'])

if os.environ.has_key('CC'):
    env['CC'] = os.environ['CC']

if os.environ.has_key('CXX'):
    env['CXX'] = os.environ['CXX']

if os.environ.has_key('DISTCC_HOSTS'):
    env['ENV']['DISTCC_HOSTS'] = os.environ['DISTCC_HOSTS']
    env['ENV']['HOME'] = os.environ['HOME']
    
final_prefix = '$PREFIX'
install_prefix = '$DESTDIR/$PREFIX'

if env['PREFIX'] == '/usr':
    final_config_prefix = '/etc'
else:
    final_config_prefix = env['PREFIX'] + '/etc'

config_prefix = '$DESTDIR' + final_config_prefix

#
# Compiler flags and other system-dependent stuff
#

opt_flags = []
debug_flags = [ '-g' ]

# guess at the platform, used to define compiler flags

config_guess = os.popen("tools/config.guess").read()[:-1]

config_cpu = 0;
config_arch = 1;
config_kernel = 2;
config_os = 3;
config = config_guess.split ("-")

#
# on OS X darwinports puts things in /opt/local by default
#
if config[config_arch] == 'apple':
    if os.path.isdir('/opt/local/lib'):
        libraries['core'].Append (LIBPATH = [ '/opt/local/lib' ])
    if os.path.isdir('/opt/local/include'):
        libraries['core'].Append (CPPPATH = [ '/opt/local/include' ])
if config[config_cpu] == 'powerpc':
    #
    # Apple/PowerPC optimization options
    #
    # -mcpu=7450 does not reliably work with gcc 3.*
    #
    if env['NOARCH'] == 0:
        if env['ALTIVEC'] == 1:
	    if config[config_arch] == 'apple':
                opt_flags.extend ([ "-mcpu=7450", "-faltivec"])
            else:
	        opt_flags.extend ([ "-mcpu=7400", "-maltivec", "-mabi=altivec"]) 
	else:
            opt_flags.extend([ "-mcpu=750", "-mmultiple" ])
        opt_flags.extend (["-mhard-float", "-mpowerpc-gfxopt"])

elif ((re.search ("i[0-9]86", config[config_cpu]) != None) or (re.search ("x86_64", config[config_cpu]) != None)):

    build_host_supports_sse = 0
    
    if env['NOARCH'] == 0:

        debug_flags.append ("-DARCH_X86")
        opt_flags.append ("-DARCH_X86")

        if config[config_kernel] == 'linux' :

            flag_line = os.popen ("cat /proc/cpuinfo | grep '^flags'").read()[:-1]
            x86_flags = flag_line.split (": ")[1:][0].split (' ')

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

        if env['USE_SSE_EVERYWHERE'] == 1:
                opt_flags.extend (["-msse", "-mfpmath=sse"])
                debug_flags.extend (["-msse", "-mfpmath=sse"])
                if build_host_supports_sse != 1:
                    print "\nWarning: you are building Ardour with SSE support even though your system does not support these instructions. (This may not be an error, especially if you are a package maintainer)"

        if env['BUILD_SSE_OPTIMIZATIONS'] == 1:
                opt_flags.append ("-DBUILD_SSE_OPTIMIZATIONS")
                debug_flags.append ("-DBUILD_SSE_OPTIMIZATIONS")
                if build_host_supports_sse != 1:
                    print "\nWarning: you are building Ardour with SSE support even though your system does not support these instructions. (This may not be an error, especially if you are a package maintainer)"
                    
# end of processor-specific section

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
    "-fstrength-reduce"
    ]

if env['DEBUG'] == 1:
    env.Append(CCFLAGS=" ".join (debug_flags))
else:
    env.Append(CCFLAGS=" ".join (opt_flags))

env.Append(CCFLAGS="-Wall")

if env['VST']:
    env.Append(CCFLAGS="-DVST_SUPPORT")


#
# everybody needs this
#

env.Merge ([ libraries['core'] ])

#
# i18n support 
#

conf = Configure (env)

if env['NLS']:
    print 'Checking for internationalization support ...'
    have_gettext = conf.TryAction(Action('xgettext --version'))
    if have_gettext[0] != 1:
        print 'This system is not configured for internationalized applications (no xgettext command). An english-only version will be built\n'
        env['NLS'] = 0
        
    if conf.CheckCHeader('libintl.h') == None:
        print 'This system is not configured for internationalized applications (no libintl.h). An english-only version will be built\n'
        env['NLS'] = 0


env = conf.Finish()

if env['NLS'] == 1:
    env.Append(CCFLAGS="-DENABLE_NLS")


Export('env install_prefix final_prefix config_prefix final_config_prefix libraries i18n version')

#
# the configuration file may be system dependent
#

conf = env.Configure ()

if conf.CheckCHeader('/System/Library/Frameworks/CoreAudio.framework/Versions/A/Headers/CoreAudio.h'):
    subst_dict['%JACK_BACKEND%'] = "coreaudio:Built-in Audio:in"
else:
    subst_dict['%JACK_BACKEND%'] = "alsa_pcm:playback_"

# posix_memalign available
if not conf.CheckFunc('posix_memalign'):
    print 'Did not find posix_memalign(), using malloc'
    env.Append(CCFLAGS='-DNO_POSIX_MEMALIGN')


env = conf.Finish()

rcbuild = env.SubstInFile ('ardour.rc','ardour.rc.in', SUBST_DICT = subst_dict)

env.Alias('install', env.Install(os.path.join(config_prefix, 'ardour'), 'ardour_system.rc'))
env.Alias('install', env.Install(os.path.join(config_prefix, 'ardour'), 'ardour.rc'))

Default (rcbuild)

# source tarball

Precious (env['DISTTREE'])

#
# note the special "cleanfirst" source name. this triggers removal
# of the existing disttree
#

env.Distribute (env['DISTTREE'],
                [ 'SConstruct',
                  'COPYING', 'PACKAGER_README', 'README',
                  'ardour.rc.in',
                  'ardour_system.rc',
                  'tools/config.guess'
                  ] +
                glob.glob ('DOCUMENTATION/AUTHORS*') +
                glob.glob ('DOCUMENTATION/CONTRIBUTORS*') +
                glob.glob ('DOCUMENTATION/TRANSLATORS*') +
                glob.glob ('DOCUMENTATION/BUILD*') +
                glob.glob ('DOCUMENTATION/FAQ*') +
                glob.glob ('DOCUMENTATION/README*')
                )
                
srcdist = env.Tarball(env['TARBALL'], env['DISTTREE'])
env.Alias ('srctar', srcdist)
#
# don't leave the distree around 
#
env.AddPreAction (env['DISTTREE'], Action ('rm -rf ' + str (File (env['DISTTREE']))))
env.AddPostAction (srcdist, Action ('rm -rf ' + str (File (env['DISTTREE']))))

#
# the subdirs
# 

for subdir in coredirs:
    SConscript (subdir + '/SConscript')

for sublistdir in [subdirs, gtk_subdirs]:
	for subdir in sublistdir:
	        SConscript (subdir + '/SConscript')

# cleanup
env.Clean ('scrub', [ 'scache.conf', '.sconf_temp', '.sconsign.dblite', 'config.log'])

