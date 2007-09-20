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

#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <gtkmm/box.h>
#include <gtkmm/stock.h>
#include <glibmm/fileutils.h>

#include <pbd/convert.h>
#include <pbd/tokenizer.h>

#include <gtkmm2ext/utils.h>

#include <ardour/audio_library.h>
#include <ardour/auditioner.h>
#include <ardour/audioregion.h>
#include <ardour/audiofilesource.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>
#include <ardour/profile.h>

#include "ardour_ui.h"
#include "editing.h"
#include "gui_thread.h"
#include "prompter.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "utils.h"
#include "gain_meter.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

using Glib::ustring;

ustring SoundFileBrowser::persistent_folder;

static int reset_depth = 0;

SoundFileBox::SoundFileBox ()
	: _session(0),
	  table (6, 2),
	  length_clock ("sfboxLengthClock", false, "EditCursorClock", false, true, false),
	  timecode_clock ("sfboxTimecodeClock", false, "EditCursorClock", false, false, false),
	  main_box (false, 6),
	  autoplay_btn (_("Auto-play"))
	
{
	HBox* hbox;
	VBox* vbox;

	set_name (X_("SoundFileBox"));
	set_size_request (300, -1);

	preview_label.set_markup (_("<b>Soundfile Info</b>"));

	border_frame.set_label_widget (preview_label);
	border_frame.add (main_box);

	pack_start (border_frame, true, true);
	set_border_width (6);

	main_box.set_border_width (6);
	main_box.set_spacing (12);

	length.set_text (_("Length:"));
	timecode.set_text (_("Timestamp:"));
	format.set_text (_("Format:"));
	channels.set_text (_("Channels:"));
	samplerate.set_text (_("Sample rate:"));

	table.set_col_spacings (6);
	table.set_homogeneous (false);
	table.set_row_spacings (6);

	table.attach (channels, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	table.attach (samplerate, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	table.attach (format, 0, 1, 2, 4, FILL|EXPAND, (AttachOptions) 0);
	table.attach (length, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	table.attach (timecode, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);

	table.attach (channels_value, 1, 2, 0, 1, FILL, (AttachOptions) 0);
	table.attach (samplerate_value, 1, 2, 1, 2, FILL, (AttachOptions) 0);
	table.attach (format_text, 1, 2, 2, 4, FILL, AttachOptions (0));
	table.attach (length_clock, 1, 2, 4, 5, FILL, (AttachOptions) 0);
	table.attach (timecode_clock, 1, 2, 5, 6, FILL, (AttachOptions) 0);

	length_clock.set_mode (ARDOUR_UI::instance()->secondary_clock.mode());
	timecode_clock.set_mode (AudioClock::SMPTE);

	hbox = manage (new HBox);
	hbox->pack_start (table, false, false);
	main_box.pack_start (*hbox, false, false);

	tags_entry.set_editable (true);
	tags_entry.signal_focus_out_event().connect (mem_fun (*this, &SoundFileBox::tags_entry_left));
	hbox = manage (new HBox);
	hbox->pack_start (tags_entry, true, true);

	vbox = manage (new VBox);

	Label* label = manage (new Label (_("Tags:")));
	label->set_alignment (0.0f, 0.5f);
	vbox->set_spacing (6);
	vbox->pack_start(*label, false, false);
	vbox->pack_start(*hbox, true, true);

	main_box.pack_start(*vbox, true, true);
	main_box.pack_start(bottom_box, false, false);
	
	play_btn.set_image (*(manage (new Image (Stock::MEDIA_PLAY, ICON_SIZE_BUTTON))));
	play_btn.set_label (_("Play (double click)"));

	stop_btn.set_image (*(manage (new Image (Stock::MEDIA_STOP, ICON_SIZE_BUTTON))));
	stop_btn.set_label (_("Stop"));
	
	bottom_box.set_homogeneous (false);
	bottom_box.set_spacing (6);
	bottom_box.pack_start(play_btn, true, true);
	bottom_box.pack_start(stop_btn, true, true);
	bottom_box.pack_start(autoplay_btn, false, false);

	play_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::audition));
	stop_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::stop_audition));

	length.set_alignment (0.0f, 0.5f);
	format.set_alignment (0.0f, 0.5f);
	channels.set_alignment (0.0f, 0.5f);
	samplerate.set_alignment (0.0f, 0.5f);
	timecode.set_alignment (0.0f, 0.5f);

	channels_value.set_alignment (0.0f, 0.5f);
	samplerate_value.set_alignment (0.0f, 0.5f);
}

void
SoundFileBox::set_session(Session* s)
{
	_session = s;

	if (!_session) {
		play_btn.set_sensitive (false);
		stop_btn.set_sensitive (false);
	} 


	length_clock.set_session (s);
	timecode_clock.set_session (s);
}

bool
SoundFileBox::setup_labels (const ustring& filename) 
{
	if (!path.empty()) {
		// save existing tags
		tags_changed ();
	}

	path = filename;

	string error_msg;

	if(!AudioFileSource::get_soundfile_info (filename, sf_info, error_msg)) {

		preview_label.set_markup (_("<b>Soundfile Info</b>"));
		format_text.set_text (_("n/a"));
		channels_value.set_text (_("n/a"));
		samplerate_value.set_text (_("n/a"));
		tags_entry.get_buffer()->set_text ("");

		length_clock.set (0);
		timecode_clock.set (0);
		
		tags_entry.set_sensitive (false);
		play_btn.set_sensitive (false);
		
		return false;
	}

	preview_label.set_markup (string_compose ("<b>%1</b>", Glib::path_get_basename (filename)));
	format_text.set_text (sf_info.format_name);
	channels_value.set_text (to_string (sf_info.channels, std::dec));

	if (_session && sf_info.samplerate != _session->frame_rate()) {
		samplerate.set_markup (string_compose ("<b>%1</b>", _("Sample rate:")));
		samplerate_value.set_markup (string_compose (X_("<b>%1 Hz</b>"), sf_info.samplerate));
		samplerate_value.set_name ("NewSessionSR1Label");
		samplerate.set_name ("NewSessionSR1Label");
	} else {
		samplerate.set_text (_("Sample rate:"));
		samplerate_value.set_text (string_compose (X_("%1 Hz"), sf_info.samplerate));
		samplerate_value.set_name ("NewSessionSR2Label");
		samplerate.set_name ("NewSessionSR2Label");
	}

	length_clock.set (sf_info.length, true);
	timecode_clock.set (sf_info.timecode, true);

	// this is a hack that is fixed in trunk, i think (august 26th, 2007)

	vector<string> tags = Library->get_tags (string ("//") + filename);
	
	stringstream tag_string;
	for (vector<string>::iterator i = tags.begin(); i != tags.end(); ++i) {
		if (i != tags.begin()) {
			tag_string << ", ";
		}
		tag_string << *i;
	}
	tags_entry.get_buffer()->set_text (tag_string.str());
	
	tags_entry.set_sensitive (true);
	if (_session) {
		play_btn.set_sensitive (true);
	}
	
	return true;
}

bool
SoundFileBox::autoplay() const
{
	return autoplay_btn.get_active();
}

bool
SoundFileBox::audition_oneshot()
{
	audition ();
	return false;
}

void
SoundFileBox::audition ()
{
	if (!_session) {
		return;
	}
	
	_session->cancel_audition();

	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		warning << string_compose(_("Could not read file: %1 (%2)."), path, strerror(errno)) << endmsg;
		return;
	}

	boost::shared_ptr<Region> r;
	SourceList srclist;
	boost::shared_ptr<AudioFileSource> afs;
	bool old_sbp = AudioSource::get_build_peakfiles ();

	/* don't even think of building peakfiles for these files */

	AudioSource::set_build_peakfiles (false);

	for (int n = 0; n < sf_info.channels; ++n) {
		try {
			afs = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createReadable (*_session, path, n, AudioFileSource::Flag (0), false));
			
			srclist.push_back(afs);
			
		} catch (failed_constructor& err) {
			error << _("Could not access soundfile: ") << path << endmsg;
			AudioSource::set_build_peakfiles (old_sbp);
			return;
		}
	}

	AudioSource::set_build_peakfiles (old_sbp);
			
	if (srclist.empty()) {
		return;
	}
	
	afs = boost::dynamic_pointer_cast<AudioFileSource> (srclist[0]);
	string rname = region_name_from_path (afs->path(), false);
	r = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (srclist, 0, srclist[0]->length(), rname, 0, Region::DefaultFlags, false));

	_session->audition_region(r);
}

void
SoundFileBox::stop_audition ()
{
	if (_session) {
		_session->cancel_audition();
	}
}

bool
SoundFileBox::tags_entry_left (GdkEventFocus *ev)
{
	tags_changed ();
	return false;
}

void
SoundFileBox::tags_changed ()
{
	string tag_string = tags_entry.get_buffer()->get_text ();

	if (tag_string.empty()) {
		return;
	}

	vector<string> tags;

	if (!PBD::tokenize (tag_string, string(",\n"), std::back_inserter (tags), true)) {
		warning << _("SoundFileBox: Could not tokenize string: ") << tag_string << endmsg;
		return;
	}

	save_tags (tags);
}

void
SoundFileBox::save_tags (const vector<string>& tags)
{
	Library->set_tags (string ("//") + path, tags);
	Library->save_changes ();
}

SoundFileBrowser::SoundFileBrowser (Gtk::Window& parent, string title, ARDOUR::Session* s)
	: ArdourDialog (parent, title, false, false),
	  found_list (ListStore::create(found_list_columns)),
	  chooser (FILE_CHOOSER_ACTION_OPEN),
	  found_list_view (found_list),
	  found_search_btn (_("Search"))
{
	VBox* vbox;
	HBox* hbox;

	gm = 0;

	set_session (s);
	resetting_ourselves = false;
	
	hpacker.set_spacing (6);
	hpacker.pack_start (notebook, true, true);
	hpacker.pack_start (preview, false, false);

	get_vbox()->pack_start (hpacker, true, true);

	hbox = manage(new HBox);
	hbox->pack_start (found_entry);
	hbox->pack_start (found_search_btn);
	
	vbox = manage(new VBox);
	vbox->pack_start (*hbox, PACK_SHRINK);
	vbox->pack_start (found_list_view);
	found_list_view.append_column(_("Paths"), found_list_columns.pathname);
	
	chooser.set_border_width (12);

	notebook.append_page (chooser, _("Browse Files"));
	notebook.append_page (*vbox, _("Search Tags"));

	notebook.set_size_request (500, -1);

	found_list_view.get_selection()->set_mode (SELECTION_MULTIPLE);
	found_list_view.signal_row_activated().connect (mem_fun (*this, &SoundFileBrowser::found_list_view_activated));

	custom_filter.add_custom (FILE_FILTER_FILENAME, mem_fun(*this, &SoundFileBrowser::on_custom));
	custom_filter.set_name (_("Audio files"));

	matchall_filter.add_pattern ("*.*");
	matchall_filter.set_name (_("All files"));

	chooser.add_filter (custom_filter);
	chooser.add_filter (matchall_filter);
	chooser.set_select_multiple (true);
	chooser.signal_update_preview().connect(mem_fun(*this, &SoundFileBrowser::update_preview));
	chooser.signal_file_activated().connect (mem_fun (*this, &SoundFileBrowser::chooser_file_activated));

	if (!persistent_folder.empty()) {
		chooser.set_current_folder (persistent_folder);
	}

	found_list_view.get_selection()->signal_changed().connect(mem_fun(*this, &SoundFileBrowser::found_list_view_selected));
	
	found_search_btn.signal_clicked().connect(mem_fun(*this, &SoundFileBrowser::found_search_clicked));
	found_entry.signal_activate().connect(mem_fun(*this, &SoundFileBrowser::found_search_clicked));

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::APPLY, RESPONSE_APPLY);
	add_button (Stock::OK, RESPONSE_OK);
	
}

SoundFileBrowser::~SoundFileBrowser ()
{
	persistent_folder = chooser.get_current_folder();
}


void
SoundFileBrowser::on_show ()
{
	ArdourDialog::on_show ();
	start_metering ();
}

void
SoundFileBrowser::clear_selection ()
{
	chooser.unselect_all ();
	found_list_view.get_selection()->unselect_all ();
}

void
SoundFileBrowser::chooser_file_activated ()
{
	preview.audition ();
}

void
SoundFileBrowser::found_list_view_activated (const TreeModel::Path& path, TreeViewColumn* col)
{
	preview.audition ();
}

void
SoundFileBrowser::set_session (Session* s)
{
	ArdourDialog::set_session (s);
	preview.set_session (s);
	if (s) {
		add_gain_meter ();
	} else {
		remove_gain_meter ();
	}
}

void
SoundFileBrowser::add_gain_meter ()
{
	if (gm) {
		delete gm;
	}

	gm = new GainMeter (session->the_auditioner(), *session);

	meter_packer.set_border_width (12);
	meter_packer.pack_start (*gm, false, true);
	hpacker.pack_end (meter_packer, false, false);
	meter_packer.show_all ();
	start_metering ();
}

void
SoundFileBrowser::remove_gain_meter ()
{
	if (gm) {
		meter_packer.remove (*gm);
		hpacker.remove (meter_packer);
		delete gm;
		gm = 0;
	}
}

void
SoundFileBrowser::start_metering ()
{
	metering_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun(*this, &SoundFileBrowser::meter));
}

void
SoundFileBrowser::stop_metering ()
{
	metering_connection.disconnect();
}

void
SoundFileBrowser::meter ()
{
	if (is_mapped () && session && gm) {
		gm->update_meters ();
	}
}

bool
SoundFileBrowser::on_custom (const FileFilter::Info& filter_info)
{
	return AudioFileSource::safe_file_extension (filter_info.filename);
}

void
SoundFileBrowser::update_preview ()
{
	preview.setup_labels (chooser.get_filename());

	if (preview.autoplay()) {
		Glib::signal_idle().connect (mem_fun (preview, &SoundFileBox::audition_oneshot));
	}
}

void
SoundFileBrowser::found_list_view_selected ()
{
	cerr << "file selected\n";

	if (!reset_options ()) {
		set_response_sensitive (RESPONSE_OK, false);
	} else {
		ustring file;

		TreeView::Selection::ListHandle_Path rows = found_list_view.get_selection()->get_selected_rows ();
		
		if (!rows.empty()) {
			TreeIter iter = found_list->get_iter(*rows.begin());
			file = (*iter)[found_list_columns.pathname];
			chooser.set_filename (file);
			set_response_sensitive (RESPONSE_OK, true);
		} else {
			set_response_sensitive (RESPONSE_OK, false);
		}
		
		preview.setup_labels (file);
	}
}

void
SoundFileBrowser::found_search_clicked ()
{
	string tag_string = found_entry.get_text ();

	vector<string> tags;

	if (!PBD::tokenize (tag_string, string(","), std::back_inserter (tags), true)) {
		warning << _("SoundFileBrowser: Could not tokenize string: ") << tag_string << endmsg;
		return;
	}
	
	vector<string> results;
	Library->search_members_and (results, tags);
	
	found_list->clear();
	for (vector<string>::iterator i = results.begin(); i != results.end(); ++i) {
		TreeModel::iterator new_row = found_list->append();
		TreeModel::Row row = *new_row;
		string path = Glib::filename_from_uri (string ("file:") + *i);
		row[found_list_columns.pathname] = path;
	}
}

vector<ustring>
SoundFileBrowser::get_paths ()
{
	vector<ustring> results;
	
	int n = notebook.get_current_page ();
	
	if (n == 0) {
		vector<ustring> filenames = chooser.get_filenames();
		vector<ustring>::iterator i;

		for (i = filenames.begin(); i != filenames.end(); ++i) {
			struct stat buf;
			if ((!stat((*i).c_str(), &buf)) && S_ISREG(buf.st_mode)) {
				results.push_back (*i);
			}
		}
		
	} else {
		
		typedef TreeView::Selection::ListHandle_Path ListPath;
		
		ListPath rows = found_list_view.get_selection()->get_selected_rows ();
		for (ListPath::iterator i = rows.begin() ; i != rows.end(); ++i) {
			TreeIter iter = found_list->get_iter(*i);
			ustring str = (*iter)[found_list_columns.pathname];
			
			results.push_back (str);
		}
	}

	return results;
}

void
SoundFileOmega::reset_options_noret ()
{
	if (!resetting_ourselves) {
		(void) reset_options ();
	}
}

bool
SoundFileOmega::reset_options ()
{
	vector<ustring> paths = get_paths ();

	reset_depth++;

	if (reset_depth > 4) {
		abort ();
	}

	cerr << "got " << paths.size() << " paths  at depth = " << reset_depth << endl;

	if (paths.empty()) {

		channel_combo.set_sensitive (false);
		action_combo.set_sensitive (false);
		where_combo.set_sensitive (false);
		copy_files_btn.set_sensitive (false);

		reset_depth--;
		return false;

	} else {

		channel_combo.set_sensitive (true);
		action_combo.set_sensitive (true);
		where_combo.set_sensitive (true);
		copy_files_btn.set_sensitive (true);

	}

	bool same_size;
	bool src_needed;
	bool selection_includes_multichannel;
	bool selection_can_be_embedded_with_links = check_link_status (*session, paths);
	ImportMode mode;

	if (check_info (paths, same_size, src_needed, selection_includes_multichannel)) {
		Glib::signal_idle().connect (mem_fun (*this, &SoundFileOmega::bad_file_message));
		reset_depth--;
		return false;
	}

	ustring existing_choice;
	vector<string> action_strings;

	if (selected_track_cnt > 0) {
		if (channel_combo.get_active_text().length()) {
			ImportDisposition id = get_channel_disposition();
			
			switch (id) {
			case Editing::ImportDistinctFiles:
				if (selected_track_cnt == paths.size()) {
					action_strings.push_back (_("to selected tracks"));
				}
				break;
				
			case Editing::ImportDistinctChannels:
				/* XXX it would be nice to allow channel-per-selected track
				   but its too hard we don't want to deal with all the 
				   different per-file + per-track channel configurations.
				*/
				break;
				
			default:
				action_strings.push_back (_("to selected tracks"));
				break;
			}
		} 
	}

	action_strings.push_back (_("as new tracks"));
	action_strings.push_back (_("to the region list"));
	action_strings.push_back (_("as new tape tracks"));

	resetting_ourselves = true;

	existing_choice = action_combo.get_active_text();

	set_popdown_strings (action_combo, action_strings);

	/* preserve any existing choice, if possible */


	if (existing_choice.length()) {
		vector<string>::iterator x;
		for (x = action_strings.begin(); x != action_strings.end(); ++x) {
			if (*x == existing_choice) {
				action_combo.set_active_text (existing_choice);
				break;
			}
		}
		if (x == action_strings.end()) {
			action_combo.set_active_text (action_strings.front());
		}
	} else {
		action_combo.set_active_text (action_strings.front());
	}

	resetting_ourselves = false;

	if ((mode = get_mode()) == ImportAsRegion) {
		where_combo.set_sensitive (false);
	} else {
		where_combo.set_sensitive (true);
	}

	vector<string> channel_strings;
	
	if (mode == ImportAsTrack || mode == ImportAsTapeTrack || mode == ImportToTrack) {
		channel_strings.push_back (_("one track per file"));

		if (selection_includes_multichannel) {
			channel_strings.push_back (_("one track per channel"));
		}

		if (paths.size() > 1) {
			/* tape tracks are a single region per track, so we cannot
			   sequence multiple files.
			*/
			if (mode != ImportAsTapeTrack) {
				channel_strings.push_back (_("sequence files"));
			}
			if (same_size) {
				channel_strings.push_back (_("all files in one region"));
			}
			
		}

	} else {
		channel_strings.push_back (_("one region per file"));

		if (selection_includes_multichannel) {
			channel_strings.push_back (_("one region per channel"));
		}

		if (paths.size() > 1) {
			if (same_size) {
				channel_strings.push_back (_("all files in one region"));
			}
		}
	}

	existing_choice = channel_combo.get_active_text();

	set_popdown_strings (channel_combo, channel_strings);

	/* preserve any existing choice, if possible */

	if (existing_choice.length()) {
		vector<string>::iterator x;
		for (x = channel_strings.begin(); x != channel_strings.end(); ++x) {
			if (*x == existing_choice) {
				channel_combo.set_active_text (existing_choice);
				break;
			}
		}
		if (x == channel_strings.end()) {
			channel_combo.set_active_text (channel_strings.front());
		}
	} else {
		channel_combo.set_active_text (channel_strings.front());
	}

	if (src_needed) {
		src_combo.set_sensitive (true);
	} else {
		src_combo.set_sensitive (false);
	}
	
	if (Profile->get_sae()) {
		if (selection_can_be_embedded_with_links) {
			copy_files_btn.set_sensitive (true);
		} else {
			copy_files_btn.set_sensitive (false);
		}
	} 
	
	reset_depth--;
	return true;
}	


bool
SoundFileOmega::bad_file_message()
{
	MessageDialog msg (*this, 
			   _("One or more of the selected files\ncannot be used by Ardour"),
			   true,
			   Gtk::MESSAGE_INFO,
			   Gtk::BUTTONS_OK);
	msg.run ();
	resetting_ourselves = true;
	chooser.unselect_uri (chooser.get_preview_uri());
	resetting_ourselves = false;

	return false;
}

bool
SoundFileOmega::check_info (const vector<ustring>& paths, bool& same_size, bool& src_needed, bool& multichannel)
{
	SNDFILE* sf;
	SF_INFO info;
	nframes64_t sz = 0;
	bool err = false;

	same_size = true;
	src_needed = false;
	multichannel = false;

	for (vector<ustring>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

		info.format = 0; // libsndfile says to clear this before sf_open().
		
		if ((sf = sf_open ((char*) (*i).c_str(), SFM_READ, &info)) != 0) { 
			sf_close (sf);

			if (info.channels > 1) {
				multichannel = true;
			}

			if (sz == 0) {
				sz = info.frames;
			} else {
				if (sz != info.frames) {
					same_size = false;
				}
			}

			if ((nframes_t) info.samplerate != session->frame_rate()) {
				src_needed = true;
			}

		} else {
			err = true;
		}
	}

	return err;
}

bool
SoundFileOmega::check_link_status (const Session& s, const vector<ustring>& paths)
{
	string tmpdir = s.sound_dir();
	bool ret = false;

	tmpdir += "/linktest";

	if (mkdir (tmpdir.c_str(), 0744)) {
		if (errno != EEXIST) {
			return false;
		}
	}
	
	for (vector<ustring>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

		char tmpc[MAXPATHLEN+1];

		snprintf (tmpc, sizeof(tmpc), "%s/%s", tmpdir.c_str(), Glib::path_get_basename (*i).c_str());

		/* can we link ? */

		if (link ((*i).c_str(), tmpc)) {
			goto out;
		}
		
		unlink (tmpc);
	}

	ret = true;

  out:
	rmdir (tmpdir.c_str());
	return ret;
}

SoundFileChooser::SoundFileChooser (Gtk::Window& parent, string title, ARDOUR::Session* s)
	: SoundFileBrowser (parent, title, s)
{
	set_size_request (780, 300);
	chooser.set_select_multiple (false);
	found_list_view.get_selection()->set_mode (SELECTION_SINGLE);
}

void
SoundFileChooser::on_hide ()
{
	ArdourDialog::on_hide();
	stop_metering ();

	if (session) {
		session->cancel_audition();
	}
}

ustring
SoundFileChooser::get_filename ()
{
	vector<ustring> paths;

	paths = get_paths ();

	if (paths.empty()) {
		return ustring ();
	}
	
	if (!Glib::file_test (paths.front(), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		return ustring();
	}

	return paths.front();
}

SoundFileOmega::SoundFileOmega (Gtk::Window& parent, string title, ARDOUR::Session* s, int selected_tracks)
	: SoundFileBrowser (parent, title, s),
	  copy_files_btn ( _("Copy files to session")),
	  selected_track_cnt (selected_tracks)
{
	VBox* vbox;
	HBox* hbox;
	vector<string> str;

	set_size_request (-1, 450);
	
	block_two.set_border_width (12);
	block_three.set_border_width (12);
	block_four.set_border_width (12);
	
	options.set_spacing (12);

	str.clear ();
	str.push_back (_("use file timestamp"));
	str.push_back (_("at edit cursor"));
	str.push_back (_("at playhead"));
	str.push_back (_("at session start"));
	set_popdown_strings (where_combo, str);
	where_combo.set_active_text (str.front());

	Label* l = manage (new Label);
	l->set_text (_("Add files:"));
	
	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->set_spacing (6);
	hbox->pack_start (*l, false, false);
	hbox->pack_start (action_combo, false, false);
	vbox = manage (new VBox);
	vbox->pack_start (*hbox, false, false);
	options.pack_start (*vbox, false, false);

	l = manage (new Label);
	l->set_text (_("Insert:"));

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->set_spacing (6);
	hbox->pack_start (*l, false, false);
	hbox->pack_start (where_combo, false, false);
	vbox = manage (new VBox);
	vbox->pack_start (*hbox, false, false);
	options.pack_start (*vbox, false, false);


	l = manage (new Label);
	l->set_text (_("Mapping:"));

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->set_spacing (6);
	hbox->pack_start (*l, false, false);
	hbox->pack_start (channel_combo, false, false);
	vbox = manage (new VBox);
	vbox->pack_start (*hbox, false, false);
	options.pack_start (*vbox, false, false);

	str.clear ();
	str.push_back (_("one track per file"));
	set_popdown_strings (channel_combo, str);
	channel_combo.set_active_text (str.front());
	channel_combo.set_sensitive (false);

	l = manage (new Label);
	l->set_text (_("Conversion Quality:"));

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->set_spacing (6);
	hbox->pack_start (*l, false, false);
	hbox->pack_start (src_combo, false, false);
	vbox = manage (new VBox);
	vbox->pack_start (*hbox, false, false);
	options.pack_start (*vbox, false, false);

	str.clear ();
	str.push_back (_("Best"));
	str.push_back (_("Good"));
	str.push_back (_("Quick"));
	str.push_back (_("Fast"));
	str.push_back (_("Fastest"));

	set_popdown_strings (src_combo, str);
	src_combo.set_active_text (str.front());
	src_combo.set_sensitive (false);

	reset_options ();

	action_combo.signal_changed().connect (mem_fun (*this, &SoundFileOmega::reset_options_noret));
	
	copy_files_btn.set_active (true);

	block_four.pack_start (copy_files_btn, false, false);

	options.pack_start (block_four, false, false);

	get_vbox()->pack_start (options, false, false);

	/* setup disposition map */

	disposition_map.insert (pair<ustring,ImportDisposition>(_("one track per file"), ImportDistinctFiles));
	disposition_map.insert (pair<ustring,ImportDisposition>(_("one track per channel"), ImportDistinctChannels));
	disposition_map.insert (pair<ustring,ImportDisposition>(_("merge files"), ImportMergeFiles));
	disposition_map.insert (pair<ustring,ImportDisposition>(_("sequence files"), ImportSerializeFiles));

	disposition_map.insert (pair<ustring,ImportDisposition>(_("one region per file"), ImportDistinctFiles));
	disposition_map.insert (pair<ustring,ImportDisposition>(_("one region per channel"), ImportDistinctChannels));
	disposition_map.insert (pair<ustring,ImportDisposition>(_("all files in one region"), ImportMergeFiles));

	chooser.signal_selection_changed().connect (mem_fun (*this, &SoundFileOmega::file_selection_changed));
}

ImportMode
SoundFileOmega::get_mode () const
{
	ustring str = action_combo.get_active_text();

	if (str == _("as new tracks")) {
		return ImportAsTrack;
	} else if (str == _("to the region list")) {
		return ImportAsRegion;
	} else if (str == _("to selected tracks")) {
		return ImportToTrack;
	} else {
		return ImportAsTapeTrack;
	}
}

void
SoundFileOmega::on_hide ()
{
	ArdourDialog::on_hide();
	if (session) {
		session->cancel_audition();
	}
}

ImportPosition
SoundFileOmega::get_position() const
{
	ustring str = where_combo.get_active_text();

	if (str == _("use file timestamp")) {
		return ImportAtTimestamp;
	} else if (str == _("at edit cursor")) {
		return ImportAtEditCursor;
	} else if (str == _("at playhead")) {
		return ImportAtPlayhead;
	} else {
		return ImportAtStart;
	}
}

SrcQuality
SoundFileOmega::get_src_quality() const
{
	ustring str = where_combo.get_active_text();

	if (str == _("Best")) {
		return SrcBest;
	} else if (str == _("Good")) {
		return SrcGood;
	} else if (str == _("Quick")) {
		return SrcQuick;
	} else if (str == _("Fast")) {
		return SrcFast;
	} else {
		return SrcFastest;
	}
}

ImportDisposition
SoundFileOmega::get_channel_disposition () const
{
	/* we use a map here because the channel combo can contain different strings
	   depending on the state of the other combos. the map contains all possible strings
	   and the ImportDisposition enum that corresponds to it.
	*/

	ustring str = channel_combo.get_active_text();
	DispositionMap::const_iterator x = disposition_map.find (str);

	if (x == disposition_map.end()) {
		fatal << string_compose (_("programming error: %1 (%2)"), "unknown string for import disposition", str) << endmsg;
		/*NOTREACHED*/
	}

	return x->second;
}

void
SoundFileOmega::reset (int selected_tracks)
{
	selected_track_cnt = selected_tracks;
	reset_options ();
}	

void
SoundFileOmega::file_selection_changed ()
{
	if (resetting_ourselves) {
		return;
	}

	cerr << "file selection changed\n";

	if (!reset_options ()) {
		set_response_sensitive (RESPONSE_OK, false);
	} else {
		if (chooser.get_filenames().size() > 0) {
			set_response_sensitive (RESPONSE_OK, true);
		} else {
			set_response_sensitive (RESPONSE_OK, false);
		}
	}
}

