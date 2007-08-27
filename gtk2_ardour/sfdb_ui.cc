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

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtk;
using namespace Editing;

Glib::ustring SoundFileBrowser::persistent_folder;

SoundFileBox::SoundFileBox ()
	:
	_session(0),
	current_pid(0),
	main_box (false, 3),
	bottom_box (true, 4),
	play_btn (Stock::MEDIA_PLAY),
	stop_btn (Stock::MEDIA_STOP),
	apply_btn ()
{
	set_name (X_("SoundFileBox"));
	
	set_size_request (250, 250);
	
	Label* label = manage (new Label);
	label->set_markup (_("<b>Soundfile Info</b>"));

	border_frame.set_label_widget (*label);
	border_frame.add (main_box);

	Label* tag_label = manage(new Label(_("comma seperated tags")));

	pack_start (border_frame, FALSE, FALSE);
	set_border_width (4);

	main_box.set_border_width (4);

	Gtk::Image* w = manage (new Image (Stock::APPLY, ICON_SIZE_BUTTON));
	apply_btn.set_image (*w);
	apply_btn.set_label (_("Set tags"));

	main_box.pack_start(length, false, false);
	main_box.pack_start(format, false, false);
	main_box.pack_start(channels, false, false);
	main_box.pack_start(samplerate, false, false);
	main_box.pack_start(timecode, false, false);
	main_box.pack_start(*tag_label, false, false);

	HBox* hbox = manage (new HBox);
	hbox->pack_start (apply_btn, false, false);
	hbox->pack_start (tags_entry, true, true);

	main_box.pack_start(*hbox, false, false);
	main_box.pack_start(bottom_box, false, false);

	bottom_box.set_homogeneous(true);
	bottom_box.pack_start(play_btn);
	bottom_box.pack_start(stop_btn);

	play_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::audition));
	stop_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::stop_btn_clicked));
	apply_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::apply_btn_clicked));
	tags_entry.signal_activate().connect (mem_fun (*this, &SoundFileBox::apply_btn_clicked));

	length.set_alignment (0.0f, 0.0f);
	format.set_alignment (0.0f, 0.0f);
	channels.set_alignment (0.0f, 0.0f);
	samplerate.set_alignment (0.0f, 0.0f);
	timecode.set_alignment (0.0f, 0.0f);

	stop_btn.set_no_show_all (true);
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
SoundFileBox::setup_labels (const Glib::ustring& filename) 
{
	path = filename;

	string error_msg;

	if(!AudioFileSource::get_soundfile_info (filename, sf_info, error_msg)) {
		length.set_text (_("Length: n/a"));
		format.set_text (_("Format: n/a"));
		channels.set_text (_("Channels: n/a"));
		samplerate.set_text (_("Sample rate: n/a"));
		timecode.set_text (_("Timecode: n/a"));
		tags_entry.set_text ("");
		
		tags_entry.set_sensitive (false);
		play_btn.set_sensitive (false);
		apply_btn.set_sensitive (false);
		
		return false;
	}

	length.set_text (string_compose(_("Length: %1"), length2string(sf_info.length, sf_info.samplerate)));
	format.set_text (sf_info.format_name);
	channels.set_text (string_compose(_("Channels: %1"), sf_info.channels));
	samplerate.set_text (string_compose(_("Samplerate: %1"), sf_info.samplerate));
	timecode.set_text (string_compose (_("Timecode: %1"), length2string(sf_info.timecode, sf_info.samplerate)));

	// this is a hack that is fixed in trunk, i think (august 26th, 2007)

	vector<string> tags = Library->get_tags (string ("//") + filename);
	
	stringstream tag_string;
	for (vector<string>::iterator i = tags.begin(); i != tags.end(); ++i) {
		if (i != tags.begin()) {
			tag_string << ", ";
		}
		tag_string << *i;
	}
	tags_entry.set_text (tag_string.str());
	
	tags_entry.set_sensitive (true);
	if (_session) {
		play_btn.set_sensitive (true);
	}
	apply_btn.set_sensitive (true);
	
	return true;
}

bool
SoundFileBox::tags_entry_left (GdkEventFocus* event)
{	
	apply_btn_clicked ();
	return true;
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

	typedef std::map<Glib::ustring, boost::shared_ptr<AudioRegion> > RegionCache; 
	static  RegionCache region_cache;
	RegionCache::iterator the_region;

	if ((the_region = region_cache.find (path)) == region_cache.end()) {

		SourceList srclist;
		boost::shared_ptr<AudioFileSource> afs;
		
		for (int n = 0; n < sf_info.channels; ++n) {
			try {
				afs = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createReadable (*_session, path, n, AudioFileSource::Flag (0)));
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

	if (tag_string.empty()) {
		return;
	}

	vector<string> tags;

	if (!PBD::tokenize (tag_string, string(","), std::back_inserter (tags), true)) {
		warning << _("SoundFileBox: Could not tokenize string: ") << tag_string << endmsg;
		return;
	}
	
	Library->set_tags (string ("//") + path, tags);
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

SoundFileBrowser::SoundFileBrowser (string title, ARDOUR::Session* s, int selected_tracks)
	: ArdourDialog (title, false),
	  found_list (ListStore::create(found_list_columns)),
	  chooser (FILE_CHOOSER_ACTION_OPEN),
	  found_list_view (found_list),
	  found_search_btn (_("Search")),
	  split_files (_("Split non-mono files")),
	  merge_stereo (_("Use files as single stereo track")),
	  as_tracks (rgroup1, _("Use files as new tracks")),
	  to_tracks (rgroup1, _("Add files to selected tracks")),
	  as_regions (rgroup1, _("Add files to region list")),
	  as_tape_tracks (rgroup1, _("Add files as new tape tracks")),
	  import (rgroup2, _("Copy to Ardour-native files")),
	  embed (rgroup2, _("Use file without copying")),
	  mode (ImportAsTrack),
	  selected_track_cnt (selected_tracks)
{

	VBox* vbox;
	HBox* hbox;

	vbox = manage (new VBox);
	vbox->pack_start (preview, false, false);


	hbox = manage (new HBox);
	hbox->set_spacing (6);
	hbox->pack_start (notebook, true, true);
	hbox->pack_start (*vbox, false, false);

	build_options ();
	rebuild_options ();
	
	get_vbox()->pack_start (*hbox, true, true);
	get_vbox()->pack_start (options, false, false);

	hbox = manage(new HBox);
	hbox->pack_start (found_entry);
	hbox->pack_start (found_search_btn);
	
	vbox = manage(new VBox);
	vbox->pack_start (*hbox, PACK_SHRINK);
	vbox->pack_start (found_list_view);
	found_list_view.append_column(_("Paths"), found_list_columns.pathname);
	
	chooser.set_border_width (12);

	notebook.append_page (chooser, _("Search Files"));
	notebook.append_page (*vbox, _("Search Tags"));

	found_list_view.get_selection()->set_mode (SELECTION_MULTIPLE);
	found_list_view.signal_row_activated().connect (mem_fun (*this, &SoundFileBrowser::found_list_view_activated));

	custom_filter.add_custom (FILE_FILTER_FILENAME, mem_fun(*this, &SoundFileBrowser::on_custom));
	custom_filter.set_name (_("Probable audio files"));

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

	add_button (Stock::OK, RESPONSE_OK);
	add_button (Stock::CANCEL, RESPONSE_CANCEL);

	set_response_sensitive (RESPONSE_OK, false);
	
	set_session (s);
}

SoundFileBrowser::~SoundFileBrowser ()
{
	persistent_folder = chooser.get_current_folder();
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
	preview.set_session(s);
}

bool
SoundFileBrowser::on_custom (const FileFilter::Info& filter_info)
{
	return AudioFileSource::safe_file_extension(filter_info.filename);
}

void
SoundFileBrowser::update_preview ()
{
	preview.setup_labels (chooser.get_filename());
	set_response_sensitive (RESPONSE_OK, true);
}

void
SoundFileBrowser::found_list_view_selected ()
{
	Glib::ustring file;
	
	TreeView::Selection::ListHandle_Path rows = found_list_view.get_selection()->get_selected_rows ();
	
	if (!rows.empty()) {
		TreeIter iter = found_list->get_iter(*rows.begin());
		file = (*iter)[found_list_columns.pathname];
		chooser.set_filename (file);
	}
	preview.setup_labels (file);
	set_response_sensitive (RESPONSE_OK, true);
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

vector<Glib::ustring>
SoundFileBrowser::get_paths ()
{
	vector<Glib::ustring> results;
	
	int n = notebook.get_current_page ();
	
	if (n == 0) {
		vector<Glib::ustring> filenames = chooser.get_filenames();
		vector<Glib::ustring>::iterator i;
		for (i = filenames.begin(); i != filenames.end(); ++i) {
			struct stat buf;
			if ((!stat((*i).c_str(), &buf)) && S_ISREG(buf.st_mode)) {
				results.push_back (*i);
			}
		}
		return results;
		
	} else {
		
		typedef TreeView::Selection::ListHandle_Path ListPath;
		
		ListPath rows = found_list_view.get_selection()->get_selected_rows ();
		for (ListPath::iterator i = rows.begin() ; i != rows.end(); ++i) {
			TreeIter iter = found_list->get_iter(*i);
			Glib::ustring str = (*iter)[found_list_columns.pathname];
			
			results.push_back (str);
		}
		return results;
	}
}

void
SoundFileBrowser::rebuild_options ()
{
	vector<Glib::ustring> paths = get_paths ();

	selection_includes_multichannel = check_multichannel_status (paths);
	selection_can_be_embedded_with_links = check_link_status (s, paths);

	if (Profile->get_sae()) {
		if (selection_can_be_embedded_with_links) {
			block_four.pack_start (import, false, false);
			block_four.pack_start (embed, false, false);
		} else {
			Label* message = manage (new Label);
			message->set_text (string_compose (_("%1 will be imported into Ardour's native format"),
							 (selected_track_cnt > 1 ? "These files" : "This file")));
			
			block_four.pack_start (*message, false, false);
		}
	} else {
		block_four.pack_start (import, false, false);
		block_four.pack_start (embed, false, false);
	}

	if (selected_track_cnt == 0) {
		to_tracks.set_sensitive (false);
	} else {
		to_tracks.set_sensitive (true);
	}
	
	mode_changed ();
}

void
SoundFileBrowser::build_options (VBox& options)
{
	block_two.set_border_width (12);
	block_three.set_border_width (12);
	block_four.set_border_width (12);
	
	block_two.pack_start (as_tracks, false, false);
	block_two.pack_start (to_tracks, false, false);
	block_two.pack_start (as_regions, false, false);
	block_two.pack_start (as_tape_tracks, false, false);

	as_tracks.signal_toggled().connect (mem_fun (*this, &SoundFileOptionsDialog::mode_changed));
	to_tracks.signal_toggled().connect (mem_fun (*this, &SoundFileOptionsDialog::mode_changed));
	as_regions.signal_toggled().connect (mem_fun (*this, &SoundFileOptionsDialog::mode_changed));
	as_tape_tracks.signal_toggled().connect (mem_fun (*this, &SoundFileOptionsDialog::mode_changed));
	
	block_three.pack_start (merge_stereo, false, false);
	block_three.pack_start (split_files, false, false);

	options.set_spacing (12);
	options.pack_start (block_two, false, false);
	options.pack_start (block_three, false, false);
	options.pack_start (block_four, false, false);
}

void
SoundFileOptionsBrowser::mode_changed ()
{
	if (as_tracks.get_active()) {
		mode = ImportAsTrack;
	} else if (to_tracks.get_active()) {
		mode = ImportToTrack;
	} else if (as_regions.get_active()) {
		mode = ImportAsRegion;
	} else {
		mode = ImportAsTapeTrack;
	}

	if ((mode == ImportAsTrack || mode == ImportAsTapeTrack) && paths.size() == 2) {
		merge_stereo.set_sensitive (true);
	} else {
		merge_stereo.set_sensitive (false);
	}

	if (selection_includes_multichannel) {
		split_files.set_sensitive (true);
	} else {
		split_files.set_sensitive (false);
	}
}	


bool
SoundFileOptionsBrowser::check_multichannel_status (const vector<Glib::ustring>& paths)
{
	SNDFILE* sf;
	SF_INFO info;
	
	for (vector<Glib::ustring>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

		info.format = 0; // libsndfile says to clear this before sf_open().
		
		if ((sf = sf_open ((char*) (*i).c_str(), SFM_READ, &info)) != 0) { 
			sf_close (sf);
			if (info.channels > 1) {
				return true;
			}
		}
	}

	return false;
}

bool
SoundFileOptionsBrowser::check_link_status (const Session& s, const vector<Glib::ustring>& paths)
{
	string tmpdir = s.sound_dir();
	bool ret = false;

	tmpdir += "/linktest";

	if (mkdir (tmpdir.c_str(), 0744)) {
		if (errno != EEXIST) {
			return false;
		}
	}
	
	for (vector<Glib::ustring>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

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

SoundFileChooser::SoundFileChooser (string title, ARDOUR::Session* s)
	: ArdourDialog (title, false),
	  browser (title, s)
{
	set_default_size (700, 300);

	get_vbox()->pack_start (browser, false, false);

	add_button (Stock::OPEN, RESPONSE_OK);
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	
	browser.chooser.set_select_multiple (false);
	browser.found_list_view.get_selection()->set_mode (SELECTION_SINGLE);

	show_all ();
}

Glib::ustring
SoundFileChooser::get_filename ()
{
	vector<Glib::ustring> paths;

	paths = browser.get_paths ();

	if (paths.empty()) {
		return Glib::ustring ();
	}
	
	if (!Glib::file_test (paths.front(), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		return Glib::ustring();
	}
	
	return paths.front();
}




