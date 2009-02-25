#!/usr/bin/env python
import autowaf

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
	'libs/taglib',
	'libs/surfaces',
	'libs/ardour',
	'libs/gtkmm2ext',
	'gtk2_ardour'
]

def set_options(opt):
	autowaf.set_options(opt)
	for i in children:
		opt.sub_options(i)

def sub_config_and_use(conf, name, has_objects = True):
	conf.sub_config(name)
	autowaf.set_local_lib(conf, name, has_objects)

def configure(conf):
	autowaf.set_recursive()
	autowaf.configure(conf)
	#conf.env.append_value('CXXFLAGS', '-DBOOST_NO_STD_LOCALE=1')
	for i in children:
		sub_config_and_use(conf, i)

def build(bld):
	autowaf.set_recursive()
	for i in children:
		bld.add_subdirs(i)

def shutdown():
	autowaf.shutdown()

