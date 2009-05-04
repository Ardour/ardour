#!/usr/bin/env python
import autowaf
import Options
import os
import commands

# Variables for 'waf dist'
VERSION = '3.0pre0'
APPNAME = 'ardour'

# Mandatory variables
srcdir = '.'
blddir = 'build'

children = [
	'libs/pbd',
	'libs/midi++2',
	'libs/evoral',
	'libs/vamp-sdk',
	'libs/vamp-plugins',
	'libs/taglib',
	'libs/rubberband',
	'libs/surfaces',
	'libs/ardour',
	'libs/gtkmm2ext',
	'gtk2_ardour'
]


# Version stuff

def fetch_svn_revision (path):
	cmd = "LANG= svn info " + path + " | awk '/^Revision:/ { print $2}'"
	return commands.getoutput(cmd)

def fetch_git_revision (path):
	cmd = "LANG= git log --abbrev HEAD^..HEAD " + path
	output = commands.getoutput(cmd).splitlines()
	rev = output[0].replace ("commit", "git")[0:10]
	for line in output:
		try:
			if "git-svn-id" in line:
				line = line.split('@')[1].split(' ')
				rev = line[0]
		except:
			pass
	return rev

def create_stored_revision():
	rev = ""
	if os.path.exists('.svn'):
		rev = fetch_svn_revision('.');
	elif os.path.exists('.git'):
		rev = fetch_git_revision('.');
	elif os.path.exists('libs/ardour/svn_revision.cc'):
		print "Using packaged svn revision"
		return
	else:
		print "Missing libs/ardour/svn_revision.cc.  Blame the packager."
		sys.exit(-1)

	try:
		text =  '#include <ardour/svn_revision.h>\n'
		text += 'namespace ARDOUR { extern const char* svn_revision = \"' + rev + '\"; }\n'
		print 'Writing svn revision info to libs/ardour/svn_revision.cc'
		o = file('libs/ardour/svn_revision.cc', 'w')
		o.write(text)
		o.close()
	except IOError:
		print 'Could not open libs/ardour/svn_revision.cc for writing\n'
		sys.exit(-1)


# Waf stages

def set_options(opt):
	autowaf.set_options(opt)
	opt.add_option('--arch', type='string', dest='arch',
			help='Architecture-specific compiler flags')
	opt.add_option('--aubio', action='store_true', default=True, dest='aubio',
			help="Use Paul Brossier's aubio library for feature detection (if available)")
	opt.add_option('--audiounits', action='store_true', default=False, dest='audiounits',
			help='Compile with Apple\'s AudioUnit library (experimental)')
	opt.add_option('--coreaudio', action='store_true', default=False, dest='coreaudio',
			help='Compile with Apple\'s CoreAudio library')
	opt.add_option('--fpu-optimization', action='store_true', default=True, dest='fpu_optimization',
			help='Build runtime checked assembler code')
	opt.add_option('--freedesktop', action='store_true', default=False, dest='freedesktop',
			help='Install MIME type, icons and .desktop file as per freedesktop.org standards')
	opt.add_option('--freesound', action='store_true', default=False, dest='freesound',
			help='Include Freesound database lookup')
	opt.add_option('--gtkosx', action='store_true', default=False, dest='gtkosx',
			help='Compile for use with GTK-OSX, not GTK-X11')
	opt.add_option('--lv2', action='store_true', default=False, dest='lv2',
			help='Compile with support for LV2 (if slv2 is available)')
	opt.add_option('--nls', action='store_true', default=True, dest='nls',
			help='Enable i18n (native language support)')
	opt.add_option('--surfaces', action='store_true', default=True, dest='surfaces',
			help='Build support for control surfaces')
	opt.add_option('--syslibs', action='store_true', default=True, dest='syslibs',
			help='Use existing system versions of various libraries instead of internal ones')
	opt.add_option('--tranzport', action='store_true', default=True, dest='tranzport',
			help='Compile with support for Frontier Designs Tranzport (if libusb is available)')
	opt.add_option('--universal', action='store_true', default=False, dest='universal',
			help='Compile as universal binary (requires that external libraries are universal)')
	opt.add_option('--versioned', action='store_true', default=False, dest='versioned',
			help='Add revision information to executable name inside the build directory')
	opt.add_option('--vst', action='store_true', default=False, dest='vst',
			help='Compile with support for VST')
	opt.add_option('--wiimote', action='store_true', default=False, dest='wiimote',
			help='Build the wiimote control surface')
	opt.add_option('--windows-key', type='string', dest='windows_key',
			help='Set X Modifier (Mod1,Mod2,Mod3,Mod4,Mod5) for "Windows" key [Default: Mod4]', default='Mod4><Super')
	for i in children:
		opt.sub_options(i)

def sub_config_and_use(conf, name, has_objects = True):
	conf.sub_config(name)
	autowaf.set_local_lib(conf, name, has_objects)

def configure(conf):
	create_stored_revision()
	autowaf.set_recursive()
	autowaf.configure(conf)
	autowaf.check_pkg(conf, 'glib-2.0', uselib_store='GLIB', atleast_version='2.2')
	autowaf.check_pkg(conf, 'glibmm-2.4', uselib_store='GLIBMM', atleast_version='2.14.0')
	for i in children:
		sub_config_and_use(conf, i)

	# Fix utterly braindead FLAC include path to not smash assert.h
	conf.env['CPPPATH_FLAC'] = []

	autowaf.print_summary(conf)
	opts = Options.options
	autowaf.display_header('Ardour Configuration')
	autowaf.display_msg(conf, 'Architecture flags', opts.arch)
	autowaf.display_msg(conf, 'Aubio', bool(conf.env['HAVE_AUBIO']))
	autowaf.display_msg(conf, 'AudioUnits', opts.audiounits)
	autowaf.display_msg(conf, 'CoreAudio', opts.coreaudio)
	autowaf.display_msg(conf, 'FPU Optimization', opts.fpu_optimization)
	autowaf.display_msg(conf, 'Freedesktop Files', opts.freedesktop)
	autowaf.display_msg(conf, 'Freesound', opts.freesound)
	autowaf.display_msg(conf, 'GtkOSX', opts.gtkosx)
	autowaf.display_msg(conf, 'LV2 Support', bool(conf.env['HAVE_SLV2']))
	autowaf.display_msg(conf, 'Rubberband', bool(conf.env['HAVE_RUBBERBAND']))
	autowaf.display_msg(conf, 'Samplerate', bool(conf.env['HAVE_SAMPLERATE']))
	autowaf.display_msg(conf, 'Soundtouch', bool(conf.env['HAVE_SOUNDTOUCH']))
	autowaf.display_msg(conf, 'Translation', opts.nls)
	autowaf.display_msg(conf, 'Surfaces', opts.surfaces)
	autowaf.display_msg(conf, 'System Libraries', opts.syslibs)
	autowaf.display_msg(conf, 'Tranzport', opts.tranzport)
	autowaf.display_msg(conf, 'Universal Binary', opts.universal)
	autowaf.display_msg(conf, 'Versioned Binary', opts.versioned)
	autowaf.display_msg(conf, 'VST Support', opts.vst)
	autowaf.display_msg(conf, 'Wiimote Support', opts.wiimote)
	autowaf.display_msg(conf, 'Windows Key', opts.windows_key)

def build(bld):
	autowaf.set_recursive()
	for i in children:
		bld.add_subdirs(i)

def shutdown():
	autowaf.shutdown()

