#!/usr/bin/env python
import autowaf
import os

# Version of this package (even if built as a child)
LIBSURFACES_VERSION = '4.1.0'

# Library version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
LIBSURFACES_LIB_VERSION = '4.1.0'

# Variables for 'waf dist'
APPNAME = 'libsurfaces'
VERSION = LIBSURFACES_VERSION

# Mandatory variables
srcdir = '.'
blddir = 'build'

def set_options(opt):
	autowaf.set_options(opt)

def configure(conf):
	autowaf.configure(conf)

def build(bld):
	# Generic MIDI
	obj = bld.new_task_gen('cxx', 'shlib')
	obj.source = '''
		generic_midi_control_protocol.cc
		interface.cc
		midicontrollable.cc
	'''
	obj.export_incdirs = ['./generic_midi']
	obj.cxxflags     = '-DPACKAGE=\\\"ardour_genericmidi\\\"'
	obj.includes     = ['.', './generic_midi']
	obj.name         = 'libgeneric_midi'
	obj.target       = 'generic_midi'
	obj.uselib_local = 'libardour libsurfaces'
	obj.vnum         = LIBSURFACES_LIB_VERSION
	obj.install_path = os.path.join(bld.env['LIBDIR'], 'ardour3')
	
def shutdown():
	autowaf.shutdown()

