#!/usr/bin/env python
import autowaf
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
		print 'Writing svn revision info to libs/ardour/svn_revision.cc\n'
		o = file('libs/ardour/svn_revision.cc', 'w')
		o.write(text)
		o.close()
	except IOError:
		print 'Could not open libs/ardour/svn_revision.cc for writing\n'
		sys.exit(-1)


# Waf stages

def set_options(opt):
	autowaf.set_options(opt)
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

def build(bld):
	autowaf.set_recursive()
	for i in children:
		bld.add_subdirs(i)

def shutdown():
	autowaf.shutdown()

