#!/usr/bin/env python

import autowaf
import os
import glob

def build_i18n(bld,dir,name,sources):
	pwd = bld.get_curdir()
	os.chdir(pwd)

	pot_file = '%s.pot' % name

	args = [ 'xgettext',
		 '--keyword=_',
		 '--keyword=N_',
		 '--from-code=UTF-8',
		 '-o', pot_file,
		 '--copyright-holder="Paul Davis"' ]
	args += sources
	print 'Updating ', pot_file
	os.spawnvp (os.P_WAIT, 'xgettext', args)
	
	po_files = glob.glob ('po/*.po')
	
	for po_file in po_files:
		args = [ 'msgmerge',
			 '--update',
			 po_file,
			 pot_file ]
		print 'Updating ', po_file
		os.spawnvp (os.P_WAIT, 'msgmerge', args)
		
	for po_file in po_files:
		mo_file = po_file.replace ('.po', '.mo')
		args = [ 'msgfmt',
			 '-c',
			 '-o',
			 mo_file,
			 po_file ]
		print 'Generating ', po_file
		os.spawnvp (os.P_WAIT, 'msgfmt', args)
