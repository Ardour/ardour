/*
    Copyright (C) 2005-2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <map>
#include <cerrno>
#include <sstream>

#include <gtkmm/box.h>
#include <gtkmm/stock.h>

#include <pbd/convert.h>
#include <pbd/whitespace.h>

#include <gtkmm2ext/utils.h>

#include <ardour/audio_library.h>
#include <ardour/audioregion.h>
#include <ardour/audiofilesource.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>

#include "ardour_ui.h"
#include "editing.h"
#include "gui_thread.h"
#include "prompter.h"
#include "sfdb_ui.h"
#include "utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

SoundFileBox::SoundFileBox ()
	:
	_session(0),
	current_pid(0),
	main_box (false, 3),
	top_box (true, 4),
	bottom_box (true, 4),
	play_btn(_("Play")),
	stop_btn(_("Stop")),
	apply_btn(_("Apply"))
{
	set_name (X_("SoundFileBox"));
	border_frame.set_label (_("Soundfile Info"));
	border_frame.add (main_box);

	pack_start (border_frame);
	set_border_width (4);

	main_box.set_border_width (4);

	main_box.pack_start(length, false, false);
	main_box.pack_start(format, false, false);
	main_box.pack_start(channels, false, false);
	main_box.pack_start(samplerate, false, false);
	main_box.pack_start(timecode, false, false);
	main_box.pack_start(tags_entry, true, true);
	main_box.pack_start(top_box, false, false);
	main_box.pack_start(bottom_box, false, false);

	top_box.set_homogeneous(true);
	top_box.pack_start(apply_btn);

	bottom_box.set_homogeneous(true);
	bottom_box.pack_start(play_btn);
	bottom_box.pack_start(stop_btn);

//	tags_entry.signal_focus_out_event().connect (mem_fun (*this, &SoundFileBox::tags_entry_left));
	play_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::play_btn_clicked));
	stop_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::stop_btn_clicked));
	apply_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::apply_btn_clicked));

	show_all();
	stop_btn.hide();
}

void
SoundFileBox::set_session(Session* s)
{
	_session = s;

	if (!_session) {
		play_btn.set_sensitive(false);
	} else {
		_session->AuditionActive.connect(mem_fun (*this, &SoundFileBox::audition_status_changed));
	}
}

bool
SoundFileBox::setup_labels (string filename) 
{
	path = filename;

	string error_msg;

	if(!AudioFileSource::get_soundfile_info (filename, sf_info, error_msg)) {
		return false;
	}

	length.set_alignment (0.0f, 0.0f);
	length.set_text (string_compose(_("Length: %1"), length2string(sf_info.length, sf_info.samplerate)));

	format.set_alignment (0.0f, 0.0f);
	format.set_text (sf_info.format_name);

	channels.set_alignment (0.0f, 0.0f);
	channels.set_text (string_compose(_("Channels: %1"), sf_info.channels));

	samplerate.set_alignment (0.0f, 0.0f);
	samplerate.set_text (string_compose(_("Samplerate: %1"), sf_info.samplerate));

	timecode.set_alignment (0.0f, 0.0f);
	timecode.set_text (string_compose (_("Timecode: %1"), length2string(sf_info.timecode, sf_info.samplerate)));

	vector<string> tags = Library->get_tags (filename);
	
	stringstream tag_string;
	for (vector<string>::iterator i = tags.begin(); i != tags.end(); ++i) {
		if (i != tags.begin()) {
			tag_string << ", ";
		}
		tag_string << *i;
	}
	tags_entry.set_text (tag_string.str());
	
	return true;
}

bool
SoundFileBox::tags_entry_left (GdkEventFocus* event)
{	
	apply_btn_clicked ();
	
	return true;
}

void
SoundFileBox::play_btn_clicked ()
{
	if (!_session) {
		return;
	}

	_session->cancel_audition();

	if (access(path.c_str(), R_OK)) {
		warning << string_compose(_("Could not read file: %1 (%2)."), path, strerror(errno)) << endmsg;
		return;
	}

	typedef std::map<string, boost::shared_ptr<AudioRegion> > RegionCache; 
	static  RegionCache region_cache;
	RegionCache::iterator the_region;

	if ((the_region = region_cache.find (path)) == region_cache.end()) {
		SourceList srclist;
		boost::shared_ptr<AudioFileSource> afs;
		
		for (int n = 0; n < sf_info.channels; ++n) {
			try {
				afs = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createReadable (*_session, path+":"+string_compose("%1", n), AudioFileSource::Flag (0)));
				srclist.push_back(afs);

			} catch (failed_constructor& err) {
				error << _("Could not access soundfile: ") << path << endmsg;
				return;
			}
		}

		if (srclist.empty()) {
			return;
		}

		string rname;

		_session->region_name (rname, Glib::path_get_basename(srclist[0]->name()), false);

		pair<string,boost::shared_ptr<AudioRegion> > newpair;
		pair<RegionCache::iterator,bool> res;

		newpair.first = path;
		newpair.second = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (srclist, 0, srclist[0]->length(), rname, 0, Region::DefaultFlags, false));

		res = region_cache.insert (newpair);
		the_region = res.first;
	}

	play_btn.hide();
	stop_btn.show();

	boost::shared_ptr<Region> r = boost::static_pointer_cast<Region> (the_region->second);

	_session->audition_region(r);
}

void
SoundFileBox::stop_btn_clicked ()
{
	if (_session) {
		_session->cancel_audition();
		play_btn.show();
		stop_btn.hide();
	}
}

void
SoundFileBox::apply_btn_clicked ()
{
	string tag_string = tags_entry.get_text ();

	vector<string> tags;

	static const string DELIMITERS = ",";
	
	// Skip delimiters at beginning.
    string::size_type last_pos = tag_string.find_first_not_of(DELIMITERS, 0);
    // Find first "non-delimiter".
    string::size_type pos = tag_string.find_first_of(DELIMITERS, last_pos);

    while (string::npos != pos || string::npos != last_pos) {
		string x = tag_string.substr(last_pos, pos - last_pos);
		strip_whitespace_edges (x);
		if (x.length()) {
			tags.push_back (x);
		}
        // Skip delimiters.  Note the "not_of"
        last_pos = tag_string.find_first_not_of(DELIMITERS, pos);
        // Find next "non-delimiter"
        pos = tag_string.find_first_of(DELIMITERS, last_pos);
    }
    
	Library->set_tags (path, tags);
	Library->save_changes ();
}

void
SoundFileBox::audition_status_changed (bool active)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &SoundFileBox::audition_status_changed), active));
	
	if (!active) {
		stop_btn_clicked ();
	}
}

// this needs to be kept in sync with the ImportMode enum defined in editing.h and editing_syms.h.
static const char *import_mode_strings[] = {
	N_("Add to Region list"),
	N_("Add to selected Track(s)"),
	N_("Add as new Track(s)"),
	N_("Add as new Tape Track(s)"),
	0
};

SoundFileBrowser::SoundFileBrowser (string title, ARDOUR::Session* s)
	: ArdourDialog (title, false),
	  chooser (Gtk::FILE_CHOOSER_ACTION_OPEN)
{
	set_default_size (700, 500);
	get_vbox()->pack_start(chooser);
	chooser.set_preview_widget(preview);
	chooser.set_select_multiple (true);

	chooser.signal_update_preview().connect(mem_fun(*this, &SoundFileBrowser::update_preview));

	set_session (s);
}

void
SoundFileBrowser::set_session (Session* s)
{
	preview.set_session(s);
}

void
SoundFileBrowser::update_preview ()
{
	chooser.set_preview_widget_active(preview.setup_labels(chooser.get_filename()));
}

SoundFileChooser::SoundFileChooser (string title, ARDOUR::Session* s)
	:
	SoundFileBrowser(title, s)
{
	add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	show_all ();
}

vector<string> SoundFileOmega::mode_strings;

SoundFileOmega::SoundFileOmega (string title, ARDOUR::Session* s)
	: SoundFileBrowser (title, s),
	  split_check (_("Split Channels"))
{
	if (mode_strings.empty()) {
		mode_strings = I18N (import_mode_strings);
	}

	ARDOUR_UI::instance()->tooltips().set_tip(split_check, 
			_("Create a region for each channel"));

	Gtk::Button* btn = add_button (_("Embed"), ResponseEmbed);
	ARDOUR_UI::instance()->tooltips().set_tip(*btn, 
			_("Link to an external file"));

	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);

	btn = add_button (_("Import"), ResponseImport);
	ARDOUR_UI::instance()->tooltips().set_tip(*btn, 
			_("Copy a file to the session folder"));

	Gtk::HBox *box = manage (new Gtk::HBox());

	Gtkmm2ext::set_popdown_strings (mode_combo, mode_strings);

	set_mode (Editing::ImportAsRegion);

	box->pack_start (split_check);
	box->pack_start (mode_combo);

	mode_combo.signal_changed().connect (mem_fun (*this, &SoundFileOmega::mode_changed));

	chooser.set_extra_widget (*box);
	
	show_all ();
}

bool
SoundFileOmega::get_split ()
{
	return split_check.get_active();
}

vector<Glib::ustring>
SoundFileOmega::get_paths ()
{
	return chooser.get_filenames();
}

void
SoundFileOmega::set_mode (Editing::ImportMode mode)
{
	mode_combo.set_active_text (mode_strings[(int)mode]);

	switch (mode) {
	case Editing::ImportAsRegion:
		split_check.set_sensitive (true);
		break;
	case Editing::ImportAsTrack:
		split_check.set_sensitive (true);
		break;
	case Editing::ImportToTrack:
		split_check.set_sensitive (false);
		break;
	case Editing::ImportAsTapeTrack:
		split_check.set_sensitive (true);
		break;
	}
}

Editing::ImportMode
SoundFileOmega::get_mode ()
{
	vector<string>::iterator i;
	uint32_t n;
	string str = mode_combo.get_active_text ();

	for (n = 0, i = mode_strings.begin (); i != mode_strings.end(); ++i, ++n) {
		if (str == (*i)) {
			break;
		}
	}

	if (i == mode_strings.end()) {
		fatal << string_compose (_("programming error: %1"), X_("unknown import mode string")) << endmsg;
		/*NOTREACHED*/
	}

	return (Editing::ImportMode) (n);
}

void
SoundFileOmega::mode_changed ()
{
	Editing::ImportMode mode = get_mode();

	switch (mode) {
	case Editing::ImportAsRegion:
		split_check.set_sensitive (true);
		break;
	case Editing::ImportAsTrack:
		split_check.set_sensitive (true);
		break;
	case Editing::ImportToTrack:
		split_check.set_sensitive (false);
		break;
	case Editing::ImportAsTapeTrack:
		split_check.set_sensitive (true);
		break;
	}
}
