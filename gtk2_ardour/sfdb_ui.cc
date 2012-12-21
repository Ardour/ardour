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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <map>
#include <cerrno>
#include <sstream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <gtkmm/box.h>
#include <gtkmm/stock.h>
#include <glibmm/fileutils.h>

#include "pbd/convert.h"
#include "pbd/tokenizer.h"
#include "pbd/enumwriter.h"
#include "pbd/pthread_utils.h"
#include "pbd/xml++.h"

#include <gtkmm2ext/utils.h>

#include "evoral/SMF.hpp"

#include "ardour/audio_library.h"
#include "ardour/auditioner.h"
#include "ardour/audioregion.h"
#include "ardour/audiofilesource.h"
#include "ardour/smf_source.h"
#include "ardour/region_factory.h"
#include "ardour/source_factory.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"

#include "ardour_ui.h"
#include "editing.h"
#include "gui_thread.h"
#include "prompter.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "utils.h"
#include "gain_meter.h"
#include "main_clock.h"

#ifdef FREESOUND
#include "sfdb_freesound_mootcher.h"
#endif

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

using std::string;

string SoundFileBrowser::persistent_folder;

static ImportMode
string2importmode (string str)
{
	if (str == _("as new tracks")) {
		return ImportAsTrack;
	} else if (str == _("to selected tracks")) {
		return ImportToTrack;
	} else if (str == _("to region list")) {
		return ImportAsRegion;
	} else if (str == _("as new tape tracks")) {
		return ImportAsTapeTrack;
	}

	warning << string_compose (_("programming error: unknown import mode string %1"), str) << endmsg;

	return ImportAsTrack;
}

static string
importmode2string (ImportMode mode)
{
	switch (mode) {
	case ImportAsTrack:
		return _("as new tracks");
	case ImportToTrack:
		return _("to selected tracks");
	case ImportAsRegion:
		return _("to region list");
	case ImportAsTapeTrack:
		return _("as new tape tracks");
	}
	/*NOTREACHED*/
	return _("as new tracks");
}

SoundFileBox::SoundFileBox (bool persistent)
	: table (6, 2),
	  length_clock ("sfboxLengthClock", !persistent, "", false, false, true, false),
	  timecode_clock ("sfboxTimecodeClock", !persistent, "", false, false, false, false),
	  main_box (false, 6),
	  autoplay_btn (_("Auto-play"))

{
	set_name (X_("SoundFileBox"));
	set_size_request (300, -1);

	preview_label.set_markup (_("<b>Sound File Information</b>"));

	border_frame.set_label_widget (preview_label);
	border_frame.add (main_box);

	pack_start (border_frame, true, true);
	set_border_width (6);

	main_box.set_border_width (6);

	length.set_text (_("Length:"));
	length.set_alignment (1, 0.5);
	timecode.set_text (_("Timestamp:"));
	timecode.set_alignment (1, 0.5);
	format.set_text (_("Format:"));
	format.set_alignment (1, 0.5);
	channels.set_text (_("Channels:"));
	channels.set_alignment (1, 0.5);
	samplerate.set_text (_("Sample rate:"));
	samplerate.set_alignment (1, 0.5);

        preview_label.set_max_width_chars (50);
	preview_label.set_ellipsize (Pango::ELLIPSIZE_END);

	format_text.set_max_width_chars (20);
	format_text.set_ellipsize (Pango::ELLIPSIZE_END);
	format_text.set_alignment (0, 1);

	table.set_col_spacings (6);
	table.set_homogeneous (false);
	table.set_row_spacings (6);

	table.attach (channels, 0, 1, 0, 1, FILL, FILL);
	table.attach (samplerate, 0, 1, 1, 2, FILL, FILL);
	table.attach (format, 0, 1, 2, 4, FILL, FILL);
	table.attach (length, 0, 1, 4, 5, FILL, FILL);
	table.attach (timecode, 0, 1, 5, 6, FILL, FILL);

	table.attach (channels_value, 1, 2, 0, 1, FILL, FILL);
	table.attach (samplerate_value, 1, 2, 1, 2, FILL, FILL);
	table.attach (format_text, 1, 2, 2, 4, FILL, FILL);
	table.attach (length_clock, 1, 2, 4, 5, FILL, FILL);
	table.attach (timecode_clock, 1, 2, 5, 6, FILL, FILL);

	length_clock.set_mode (ARDOUR_UI::instance()->secondary_clock->mode());
	timecode_clock.set_mode (AudioClock::Timecode);

	main_box.pack_start (table, false, false);

	tags_entry.set_editable (true);
	tags_entry.set_wrap_mode(Gtk::WRAP_WORD);
	tags_entry.signal_focus_out_event().connect (sigc::mem_fun (*this, &SoundFileBox::tags_entry_left));

	Label* label = manage (new Label (_("Tags:")));
	label->set_alignment (0.0f, 0.5f);
	main_box.pack_start (*label, false, false);
	main_box.pack_start (tags_entry, true, true);

	main_box.pack_start (bottom_box, false, false);

	play_btn.set_image (*(manage (new Image (Stock::MEDIA_PLAY, ICON_SIZE_BUTTON))));
//	play_btn.set_label (_("Play"));

	stop_btn.set_image (*(manage (new Image (Stock::MEDIA_STOP, ICON_SIZE_BUTTON))));
//	stop_btn.set_label (_("Stop"));

	bottom_box.set_homogeneous (false);
	bottom_box.set_spacing (6);
	bottom_box.pack_start(play_btn, true, true);
	bottom_box.pack_start(stop_btn, true, true);
	bottom_box.pack_start(autoplay_btn, false, false);

	play_btn.signal_clicked().connect (sigc::mem_fun (*this, &SoundFileBox::audition));
	stop_btn.signal_clicked().connect (sigc::mem_fun (*this, &SoundFileBox::stop_audition));

	channels_value.set_alignment (0.0f, 0.5f);
	samplerate_value.set_alignment (0.0f, 0.5f);
}

void
SoundFileBox::set_session(Session* s)
{
	SessionHandlePtr::set_session (s);

	length_clock.set_session (s);
	timecode_clock.set_session (s);

	if (!_session) {
		play_btn.set_sensitive (false);
		stop_btn.set_sensitive (false);
	}
}

bool
SoundFileBox::setup_labels (const string& filename)
{
	if (!path.empty()) {
		// save existing tags
		tags_changed ();
	}

	path = filename;

	string error_msg;

	if(!AudioFileSource::get_soundfile_info (filename, sf_info, error_msg)) {

		preview_label.set_markup (_("<b>Sound File Information</b>"));
		format_text.set_text ("");
		channels_value.set_text ("");
		samplerate_value.set_text ("");
		tags_entry.get_buffer()->set_text ("");

		length_clock.set (0);
		timecode_clock.set (0);

		tags_entry.set_sensitive (false);
		play_btn.set_sensitive (false);

		return false;
	}

	preview_label.set_markup (string_compose ("<b>%1</b>", Glib::Markup::escape_text (Glib::path_get_basename (filename))));
	std::string n = sf_info.format_name;
	if (n.substr (0, 8) == X_("Format: ")) {
		n = n.substr (8);
	}
	format_text.set_text (n);
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

	framecnt_t const nfr = _session ? _session->nominal_frame_rate() : 25;
	double src_coef = (double) nfr / sf_info.samplerate;

	length_clock.set (sf_info.length * src_coef + 0.5, true);
	timecode_clock.set (sf_info.timecode * src_coef + 0.5, true);

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

	if (SMFSource::safe_midi_file_extension (path)) {
		error << _("Auditioning of MIDI files is not yet supported") << endmsg;
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
			afs = boost::dynamic_pointer_cast<AudioFileSource> (
					SourceFactory::createReadable (DataType::AUDIO, *_session,
							path, n, Source::Flag (0), false));

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

	PropertyList plist;

	plist.add (ARDOUR::Properties::start, 0);
	plist.add (ARDOUR::Properties::length, srclist[0]->length(srclist[0]->timeline_position()));
	plist.add (ARDOUR::Properties::name, rname);
	plist.add (ARDOUR::Properties::layer, 0);

	r = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (srclist, plist, false));

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
SoundFileBox::tags_entry_left (GdkEventFocus *)
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

SoundFileBrowser::SoundFileBrowser (Gtk::Window& parent, string title, ARDOUR::Session* s, bool persistent)
	: ArdourDialog (parent, title, false, false),
	  found_list (ListStore::create(found_list_columns)),
	  freesound_list (ListStore::create(freesound_list_columns)),
	  chooser (FILE_CHOOSER_ACTION_OPEN),
	  preview (persistent),
	  found_search_btn (_("Search")),
	  found_list_view (found_list),
	  freesound_search_btn (_("Search")),
	  freesound_list_view (freesound_list)
{
	resetting_ourselves = false;
	gm = 0;

	resetting_ourselves = false;
	gm = 0;

#ifdef GTKOSX
        chooser.add_shortcut_folder_uri("file:///Library/GarageBand/Apple Loops");
        chooser.add_shortcut_folder_uri("file:///Library/Audio/Apple Loops");
        chooser.add_shortcut_folder_uri("file:///Library/Application Support/GarageBand/Instrument Library/Sampler/Sampler Files");

        chooser.add_shortcut_folder_uri("file:///Volumes");
#endif

#ifdef FREESOUND
	mootcher = new Mootcher();
#endif

	//add the file chooser
	{
		chooser.set_border_width (12);

		audio_and_midi_filter.add_custom (FILE_FILTER_FILENAME, sigc::mem_fun (*this, &SoundFileBrowser::on_audio_and_midi_filter));
		audio_and_midi_filter.set_name (_("Audio and MIDI files"));

		audio_filter.add_custom (FILE_FILTER_FILENAME, sigc::mem_fun(*this, &SoundFileBrowser::on_audio_filter));
		audio_filter.set_name (_("Audio files"));

		midi_filter.add_custom (FILE_FILTER_FILENAME, sigc::mem_fun(*this, &SoundFileBrowser::on_midi_filter));
		midi_filter.set_name (_("MIDI files"));

		matchall_filter.add_pattern ("*.*");
		matchall_filter.set_name (_("All files"));

		chooser.add_filter (audio_and_midi_filter);
		chooser.add_filter (audio_filter);
		chooser.add_filter (midi_filter);
		chooser.add_filter (matchall_filter);
		chooser.set_select_multiple (true);
		chooser.signal_update_preview().connect(sigc::mem_fun(*this, &SoundFileBrowser::update_preview));
		chooser.signal_file_activated().connect (sigc::mem_fun (*this, &SoundFileBrowser::chooser_file_activated));
#ifdef GTKOSX
                /* some broken redraw behaviour - this is a bandaid */
                chooser.signal_selection_changed().connect (mem_fun (chooser, &Widget::queue_draw));
#endif

		if (!persistent_folder.empty()) {
			chooser.set_current_folder (persistent_folder);
		}
		notebook.append_page (chooser, _("Browse Files"));
	}

	hpacker.set_spacing (6);
	hpacker.pack_start (notebook, true, true);
	hpacker.pack_start (preview, false, false);

	get_vbox()->pack_start (hpacker, true, true);

	//add tag search
	{
		VBox* vbox;
		HBox* hbox;


		hbox = manage(new HBox);
		hbox->pack_start (found_entry);
		hbox->pack_start (found_search_btn);

		Gtk::ScrolledWindow *scroll = manage(new ScrolledWindow);
		scroll->add(found_list_view);
		scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

		vbox = manage(new VBox);
		vbox->pack_start (*hbox, PACK_SHRINK);
		vbox->pack_start (*scroll);

		found_list_view.append_column(_("Paths"), found_list_columns.pathname);

		found_list_view.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &SoundFileBrowser::found_list_view_selected));

		found_list_view.signal_row_activated().connect (sigc::mem_fun (*this, &SoundFileBrowser::found_list_view_activated));

		found_search_btn.signal_clicked().connect(sigc::mem_fun(*this, &SoundFileBrowser::found_search_clicked));
		found_entry.signal_activate().connect(sigc::mem_fun(*this, &SoundFileBrowser::found_search_clicked));

		freesound_stop_btn.signal_clicked().connect(sigc::mem_fun(*this, &SoundFileBrowser::freesound_stop_clicked));

		notebook.append_page (*vbox, _("Search Tags"));
	}

	//add freesound search
#ifdef FREESOUND
	{
		VBox* vbox;
		HBox* passbox;
		Label* label;

		passbox = manage(new HBox);
		passbox->set_spacing (6);

		label = manage (new Label);
		label->set_text (_("Tags:"));
		passbox->pack_start (*label, false, false);
		passbox->pack_start (freesound_entry, true, true);

		label = manage (new Label);
		label->set_text (_("Sort:"));
		passbox->pack_start (*label, false, false);
		passbox->pack_start (freesound_sort, false, false);
		freesound_sort.clear_items();
		
		// Order of the following must correspond with enum sortMethod
		// in sfdb_freesound_mootcher.h	
		freesound_sort.append_text(_("None"));
		freesound_sort.append_text(_("Longest"));
		freesound_sort.append_text(_("Shortest"));
		freesound_sort.append_text(_("Newest"));
		freesound_sort.append_text(_("Oldest"));
		freesound_sort.append_text(_("Most downloaded"));
		freesound_sort.append_text(_("Least downloaded"));
		freesound_sort.append_text(_("Highest rated"));
		freesound_sort.append_text(_("Lowest rated"));
		freesound_sort.set_active(0);

		passbox->pack_start (freesound_search_btn, false, false);
		passbox->pack_end   (freesound_stop_btn, false, false);
		freesound_stop_btn.set_label(_("Stop"));
		
		Gtk::ScrolledWindow *scroll = manage(new ScrolledWindow);
		scroll->add(freesound_list_view);
		scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

		vbox = manage(new VBox);
		vbox->set_spacing (3);
		vbox->pack_start (*passbox, PACK_SHRINK);
		vbox->pack_start (freesound_progress_bar, PACK_SHRINK);
		vbox->pack_start (*scroll);

		freesound_list_view.append_column(_("ID")      , freesound_list_columns.id);
		freesound_list_view.append_column(_("Filename"), freesound_list_columns.filename);
		// freesound_list_view.append_column(_("URI")     , freesound_list_columns.uri);
		freesound_list_view.append_column(_("Duration"), freesound_list_columns.duration);
		freesound_list_view.append_column(_("Size"), freesound_list_columns.filesize);
		freesound_list_view.append_column(_("Samplerate"), freesound_list_columns.smplrate);
		freesound_list_view.append_column(_("License"), freesound_list_columns.license);
		freesound_list_view.get_column(0)->set_alignment(0.5);
		freesound_list_view.get_column(1)->set_expand(true);
		freesound_list_view.get_column(2)->set_alignment(0.5);
		freesound_list_view.get_column(3)->set_alignment(0.5);
		freesound_list_view.get_column(4)->set_alignment(0.5);
		freesound_list_view.get_column(5)->set_alignment(0.5);

		freesound_list_view.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &SoundFileBrowser::freesound_list_view_selected));

		freesound_list_view.get_selection()->set_mode (SELECTION_MULTIPLE);
		freesound_list_view.signal_row_activated().connect (sigc::mem_fun (*this, &SoundFileBrowser::freesound_list_view_activated));
		freesound_search_btn.signal_clicked().connect(sigc::mem_fun(*this, &SoundFileBrowser::freesound_search_clicked));
		freesound_entry.signal_activate().connect(sigc::mem_fun(*this, &SoundFileBrowser::freesound_search_clicked));
		freesound_stop_btn.signal_clicked().connect(sigc::mem_fun(*this, &SoundFileBrowser::freesound_stop_clicked));
		notebook.append_page (*vbox, _("Search Freesound"));
	}

#endif


	notebook.set_size_request (500, -1);
	notebook.signal_switch_page().connect (
		sigc::hide_return (sigc::hide (sigc::hide (sigc::mem_fun (*this, &SoundFileBrowser::reset_options))))
		);

	set_session (s);

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
SoundFileBrowser::found_list_view_activated (const TreeModel::Path&, TreeViewColumn*)
{
	preview.audition ();
}

void
SoundFileBrowser::freesound_list_view_activated (const TreeModel::Path&, TreeViewColumn*)
{
	preview.audition ();
}

void
SoundFileBrowser::set_session (Session* s)
{
	ArdourDialog::set_session (s);
	preview.set_session (s);

	if (_session) {
		add_gain_meter ();
	} else {
		remove_gain_meter ();
	}
}

void
SoundFileBrowser::add_gain_meter ()
{
	delete gm;

	gm = new GainMeter (_session, 250);

	boost::shared_ptr<Route> r = _session->the_auditioner ();

	gm->set_controls (r, r->shared_peak_meter(), r->amp());

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
	metering_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &SoundFileBrowser::meter));
}

void
SoundFileBrowser::stop_metering ()
{
	metering_connection.disconnect();
}

void
SoundFileBrowser::meter ()
{
	if (is_mapped () && _session && gm) {
		gm->update_meters ();
	}
}

bool
SoundFileBrowser::on_audio_filter (const FileFilter::Info& filter_info)
{
	return AudioFileSource::safe_audio_file_extension (filter_info.filename);
}

bool
SoundFileBrowser::on_midi_filter (const FileFilter::Info& filter_info)
{
	return SMFSource::safe_midi_file_extension (filter_info.filename);
}

bool
SoundFileBrowser::on_audio_and_midi_filter (const FileFilter::Info& filter_info)
{
	return on_audio_filter (filter_info) || on_midi_filter (filter_info);
}

void
SoundFileBrowser::update_preview ()
{
	if (preview.setup_labels (chooser.get_preview_filename())) {
		if (preview.autoplay()) {
			Glib::signal_idle().connect (sigc::mem_fun (preview, &SoundFileBox::audition_oneshot));
		}
	}
}

void
SoundFileBrowser::found_list_view_selected ()
{
	if (!reset_options ()) {
		set_response_sensitive (RESPONSE_OK, false);
	} else {
		string file;

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
SoundFileBrowser::freesound_list_view_selected ()
{
	freesound_download_cancel = false;

#ifdef FREESOUND
	if (!reset_options ()) {
		set_response_sensitive (RESPONSE_OK, false);
	} else {

		string file;

		TreeView::Selection::ListHandle_Path rows = freesound_list_view.get_selection()->get_selected_rows ();

		if (!rows.empty()) {
			TreeIter iter = freesound_list->get_iter(*rows.begin());

			string id  = (*iter)[freesound_list_columns.id];
			string uri = (*iter)[freesound_list_columns.uri];
			string ofn = (*iter)[freesound_list_columns.filename];

			// download the sound file			
			GdkCursor *prev_cursor;
			prev_cursor = gdk_window_get_cursor (get_window()->gobj());
			gdk_window_set_cursor (get_window()->gobj(), gdk_cursor_new(GDK_WATCH));
			gdk_flush();

			file = mootcher->getAudioFile(ofn, id, uri, this);

			gdk_window_set_cursor (get_window()->gobj(), prev_cursor);

			if (file != "") {
				chooser.set_filename (file);
				set_response_sensitive (RESPONSE_OK, true);
			}
		} else {
			set_response_sensitive (RESPONSE_OK, false);
		}

		freesound_progress_bar.set_text(
				string_compose(_("found %1 matche(s)"), matches));

		preview.setup_labels (file);
	}
#endif
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

void
SoundFileBrowser::freesound_search_clicked ()
{
	freesound_search_cancel = false;
	freesound_search();
}

void
SoundFileBrowser::freesound_stop_clicked ()
{
	freesound_download_cancel = true;
	freesound_search_cancel = true;
}


void
SoundFileBrowser::freesound_search()
{
#ifdef FREESOUND
	freesound_list->clear();
	matches = 0;

	string search_string = freesound_entry.get_text ();
	enum sortMethod sort_method = (enum sortMethod) freesound_sort.get_active_row_number();

	GdkCursor *prev_cursor;
	prev_cursor = gdk_window_get_cursor (get_window()->gobj());
	gdk_window_set_cursor (get_window()->gobj(), gdk_cursor_new(GDK_WATCH));
	freesound_progress_bar.set_fraction(0.0);
	gdk_flush();

	int freesound_n_pages = 1;
	for (int page = 1; page <= 99 && page <= freesound_n_pages; page++ ) {
		
		std::string prog;
		if (freesound_n_pages > 1) {
			freesound_progress_bar.set_fraction(page/(float)freesound_n_pages);
			prog = string_compose (_("Searching Page %1 of %2, click Stop to cancel"), page, freesound_n_pages);
		} else {
			prog = _("Searching, click Stop to cancel");
		}
		freesound_progress_bar.set_text(prog);
		while (Glib::MainContext::get_default()->iteration (false)) {
			/* do nothing */
		}

		std::string theString = mootcher->searchText(
			search_string, 
			page,
#ifdef GTKOSX
			"", // OSX eats anything incl mp3
#else
			"type:wav OR type:aiff OR type:flac OR type:aif OR type:ogg OR type:oga",
#endif
			sort_method
		);

		XMLTree doc;
		doc.read_buffer( theString );
		XMLNode *root = doc.root();

		if (!root) {
			error << "no root XML node!" << endmsg;
			break;
		}

		if ( strcmp(root->name().c_str(), "response") != 0) {
			error << string_compose ("root node name == %1 != \"response\"", root->name()) << endmsg;
			break;
		}

		//find out how many pages are available to search
		XMLNode *res = root->child("num_pages");
		if (res) {
			string result = res->child("text")->content();
			freesound_n_pages = atoi(result.c_str());
		}

		XMLNode *sounds_root = root->child("sounds");
		
		if (!sounds_root) {
			error << "no child node \"sounds\" found!" << endmsg;
			break;
		}
		
		XMLNodeList sounds = sounds_root->children();
		if (sounds.size() == 0) {
			/* nothing found */
			break;
		}

		XMLNodeConstIterator niter;
		XMLNode *node;
		for (niter = sounds.begin(); niter != sounds.end(); ++niter) {
			node = *niter;
			if( strcmp( node->name().c_str(), "resource") != 0 ){
				error << string_compose ("node->name()=%1 != \"resource\"", node->name()) << endmsg;
				freesound_search_cancel = true;
				break;
			}

			// node->dump(cerr, "node:");
			
			XMLNode *id_node  = node->child ("id");
			XMLNode *uri_node = node->child ("serve");
			XMLNode *ofn_node = node->child ("original_filename");
			XMLNode *dur_node = node->child ("duration");
			XMLNode *siz_node = node->child ("filesize");
			XMLNode *srt_node = node->child ("samplerate");
			XMLNode *lic_node = node->child ("license");

			if (id_node && uri_node && ofn_node && dur_node && siz_node && srt_node) {
				
				std::string  id =  id_node->child("text")->content();
				std::string uri = uri_node->child("text")->content();
				std::string ofn = ofn_node->child("text")->content();
				std::string dur = dur_node->child("text")->content();
				std::string siz = siz_node->child("text")->content();
				std::string srt = srt_node->child("text")->content();
				std::string lic = lic_node->child("text")->content();

				std::string r;
				// cerr << "id=" << id << ",uri=" << uri << ",ofn=" << ofn << ",dur=" << dur << endl;
				
				double duration_seconds = atof(dur.c_str());
				double h, m, s;
				char duration_hhmmss[16];
				if (duration_seconds >= 99 * 60 * 60) {
					strcpy(duration_hhmmss, ">99h");
				} else {
					s = modf(duration_seconds/60, &m) * 60;
					m = modf(m/60, &h) * 60;
					sprintf(duration_hhmmss, "%02.fh:%02.fm:%04.1fs",
						h, m, s
					);
				}

				double size_bytes = atof(siz.c_str());
				char bsize[32];
				if (size_bytes < 1000) {
					sprintf(bsize, "%.0f %s", size_bytes, _("B"));
				} else if (size_bytes < 1000000 ) {
					sprintf(bsize, "%.1f %s", size_bytes / 1000.0, _("kB"));
				} else if (size_bytes < 10000000) {
					sprintf(bsize, "%.1f %s", size_bytes / 1000000.0, _("MB"));
				} else if (size_bytes < 1000000000) {
					sprintf(bsize, "%.2f %s", size_bytes / 1000000.0, _("MB"));
				} else {
					sprintf(bsize, "%.2f %s", size_bytes / 1000000000.0, _("GB"));
				}

				/* see http://www.freesound.org/help/faq/#licenses */
				char shortlicense[64];
				if(!lic.compare(0, 42, "http://creativecommons.org/licenses/by-nc/")){
					sprintf(shortlicense, "CC-BY-NC");
				} else if(!lic.compare(0, 39, "http://creativecommons.org/licenses/by/")) {
					sprintf(shortlicense, "CC-BY");
				} else if(!lic.compare("http://creativecommons.org/licenses/sampling+/1.0/")) {
					sprintf(shortlicense, "sampling+");
				} else if(!lic.compare(0, 40, "http://creativecommons.org/publicdomain/")) {
					sprintf(shortlicense, "PD");
				} else {
					snprintf(shortlicense, 64, "%s", lic.c_str());
					shortlicense[63]= '\0';
				}

 				TreeModel::iterator new_row = freesound_list->append();
				TreeModel::Row row = *new_row;
				
				row[freesound_list_columns.id      ] = id;
				row[freesound_list_columns.uri     ] = uri;
				row[freesound_list_columns.filename] = ofn;
				row[freesound_list_columns.duration] = duration_hhmmss;
				row[freesound_list_columns.filesize] = bsize;
				row[freesound_list_columns.smplrate] = srt;
				row[freesound_list_columns.license ] = shortlicense;
				matches++;

			}
		}
	
		if (freesound_search_cancel)
			break;

	}  //page "for" loop

	gdk_window_set_cursor (get_window()->gobj(), prev_cursor);

	freesound_progress_bar.set_fraction(0.0);
	freesound_progress_bar.set_text(
			string_compose(_("found %1 matche(s)"), matches));

#endif
}

vector<string>
SoundFileBrowser::get_paths ()
{
	vector<string> results;

	int n = notebook.get_current_page ();

	if (n == 0) {
		vector<string> filenames = chooser.get_filenames();
		vector<string>::iterator i;

		for (i = filenames.begin(); i != filenames.end(); ++i) {
			struct stat buf;
			if ((!stat((*i).c_str(), &buf)) && S_ISREG(buf.st_mode)) {
				results.push_back (*i);
			}
		}

	} else if (n==1){

		typedef TreeView::Selection::ListHandle_Path ListPath;

		ListPath rows = found_list_view.get_selection()->get_selected_rows ();
		for (ListPath::iterator i = rows.begin() ; i != rows.end(); ++i) {
			TreeIter iter = found_list->get_iter(*i);
			string str = (*iter)[found_list_columns.pathname];

			results.push_back (str);
		}
	} else {
#ifdef FREESOUND
		typedef TreeView::Selection::ListHandle_Path ListPath;

		ListPath rows = freesound_list_view.get_selection()->get_selected_rows ();
		for (ListPath::iterator i = rows.begin() ; i != rows.end(); ++i) {
			TreeIter iter = freesound_list->get_iter(*i);
			string id  = (*iter)[freesound_list_columns.id];
			string uri = (*iter)[freesound_list_columns.uri];
			string ofn = (*iter)[freesound_list_columns.filename];

			GdkCursor *prev_cursor;
			prev_cursor = gdk_window_get_cursor (get_window()->gobj());
			gdk_window_set_cursor (get_window()->gobj(), gdk_cursor_new(GDK_WATCH));
			gdk_flush();

			string str = mootcher->getAudioFile(ofn, id, uri, this);
			if (str != "") {
				results.push_back (str);
			}
			
			gdk_window_set_cursor (get_window()->gobj(), prev_cursor);

		}
#endif
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
	vector<string> paths = get_paths ();

	if (paths.empty()) {

		channel_combo.set_sensitive (false);
		action_combo.set_sensitive (false);
		where_combo.set_sensitive (false);
		copy_files_btn.set_active (true);
		copy_files_btn.set_sensitive (false);

		return false;

	} else {

		channel_combo.set_sensitive (true);
		action_combo.set_sensitive (true);
		where_combo.set_sensitive (true);

		/* if we get through this function successfully, this may be
		   reset at the end, once we know if we can use hard links
		   to do embedding (or if we are importing a MIDI file).
		*/

		if (Config->get_only_copy_imported_files()) {
			copy_files_btn.set_sensitive (false);
		} else {
			copy_files_btn.set_sensitive (false);
		}
	}

	bool same_size;
	bool src_needed;
	bool selection_includes_multichannel;
	bool selection_can_be_embedded_with_links = check_link_status (_session, paths);
	ImportMode mode;

	/* See if we are thinking about importing any MIDI files */
	vector<string>::iterator i = paths.begin ();
	while (i != paths.end() && SMFSource::safe_midi_file_extension (*i) == false) {
		++i;
	}
	bool const have_a_midi_file = (i != paths.end ());

	if (check_info (paths, same_size, src_needed, selection_includes_multichannel)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &SoundFileOmega::bad_file_message));
		return false;
	}

	string existing_choice;
	vector<string> action_strings;

	resetting_ourselves = true;

	if (chooser.get_filter() == &audio_filter) {

		/* AUDIO */

		if (selected_audio_track_cnt > 0) {
			if (channel_combo.get_active_text().length()) {
				ImportDisposition id = get_channel_disposition();
				
				switch (id) {
				case Editing::ImportDistinctFiles:
					if (selected_audio_track_cnt == paths.size()) {
						action_strings.push_back (importmode2string (ImportToTrack));
					}
					break;
					
				case Editing::ImportDistinctChannels:
					/* XXX it would be nice to allow channel-per-selected track
					   but its too hard we don't want to deal with all the
					   different per-file + per-track channel configurations.
					*/
					break;
					
				default:
					action_strings.push_back (importmode2string (ImportToTrack));
					break;
				}
			}
		}
		
	}  else {

		/* MIDI ONLY */

		if (selected_midi_track_cnt > 0) {
			action_strings.push_back (importmode2string (ImportToTrack));
		}
	}

	action_strings.push_back (importmode2string (ImportAsTrack));
	action_strings.push_back (importmode2string (ImportAsRegion));
	action_strings.push_back (importmode2string (ImportAsTapeTrack));

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
				channel_strings.push_back (_("all files in one track"));
				channel_strings.push_back (_("merge files"));
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

	resetting_ourselves = true;

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

	resetting_ourselves = false;

	if (src_needed) {
		src_combo.set_sensitive (true);
	} else {
		src_combo.set_sensitive (false);
	}

	/* We must copy MIDI files or those from Freesound */
	bool const must_copy = have_a_midi_file || notebook.get_current_page() == 2;
	
	if (Config->get_only_copy_imported_files()) {

		if (selection_can_be_embedded_with_links && !must_copy) {
			copy_files_btn.set_sensitive (true);
		} else {
			if (must_copy) {
				copy_files_btn.set_active (true);
			}
			copy_files_btn.set_sensitive (false);
		}

	}  else {

		if (must_copy) {
			copy_files_btn.set_active (true);
		}			
		copy_files_btn.set_sensitive (!must_copy);
	}

	return true;
}


bool
SoundFileOmega::bad_file_message()
{
	MessageDialog msg (*this,
			   string_compose (_("One or more of the selected files\ncannot be used by %1"), PROGRAM_NAME),
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
SoundFileOmega::check_info (const vector<string>& paths, bool& same_size, bool& src_needed, bool& multichannel)
{
	SoundFileInfo info;
	framepos_t sz = 0;
	bool err = false;
	string errmsg;

	same_size = true;
	src_needed = false;
	multichannel = false;

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

		if (AudioFileSource::get_soundfile_info (*i, info, errmsg)) {
			if (info.channels > 1) {
				multichannel = true;
			}
			if (sz == 0) {
				sz = info.length;
			} else {
				if (sz != info.length) {
					same_size = false;
				}
			}

			if (info.samplerate != _session->frame_rate()) {
				src_needed = true;
			}

		} else if (SMFSource::safe_midi_file_extension (*i)) {

			Evoral::SMF reader;
			reader.open(*i);
			if (reader.num_tracks() > 1) {
				multichannel = true; // "channel" == track here...
			}

			/* XXX we need err = true handling here in case
			   we can't check the file
			*/

		} else {
			err = true;
		}
	}

	return err;
}


bool
SoundFileOmega::check_link_status (const Session* s, const vector<string>& paths)
{
	std::string tmpdir(Glib::build_filename (s->session_directory().sound_path(), "linktest"));
	bool ret = false;

	if (mkdir (tmpdir.c_str(), 0744)) {
		if (errno != EEXIST) {
			return false;
		}
	}

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

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
	: SoundFileBrowser (parent, title, s, false)
{
	chooser.set_select_multiple (false);
	found_list_view.get_selection()->set_mode (SELECTION_SINGLE);
	freesound_list_view.get_selection()->set_mode (SELECTION_SINGLE);
}

void
SoundFileChooser::on_hide ()
{
	ArdourDialog::on_hide();
	stop_metering ();

	if (_session) {
		_session->cancel_audition();
	}
}

string
SoundFileChooser::get_filename ()
{
	vector<string> paths;

	paths = get_paths ();

	if (paths.empty()) {
		return string ();
	}

	if (!Glib::file_test (paths.front(), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		return string();
	}

	return paths.front();
}

SoundFileOmega::SoundFileOmega (Gtk::Window& parent, string title, ARDOUR::Session* s, 
				uint32_t selected_audio_tracks, 
				uint32_t selected_midi_tracks, 
				bool persistent,
				Editing::ImportMode mode_hint)
	: SoundFileBrowser (parent, title, s, persistent)
	, copy_files_btn ( _("Copy files to session"))
	, selected_audio_track_cnt (selected_audio_tracks)
	, selected_midi_track_cnt (selected_midi_tracks)
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
	str.push_back (_("file timestamp"));
	str.push_back (_("edit point"));
	str.push_back (_("playhead"));
	str.push_back (_("session start"));
	set_popdown_strings (where_combo, str);
	where_combo.set_active_text (str.front());

	Label* l = manage (new Label);
	l->set_markup (_("<b>Add files as ...</b>"));

	vbox = manage (new VBox);
	vbox->set_border_width (12);
	vbox->set_spacing (6);
	vbox->pack_start (*l, false, false);
	vbox->pack_start (action_combo, false, false);
	hbox = manage (new HBox);
	hbox->pack_start (*vbox, false, false);
	options.pack_start (*hbox, false, false);

	/* dummy entry for action combo so that it doesn't look odd if we
	   come up with no tracks selected.
	*/

	str.clear ();
	str.push_back (importmode2string (mode_hint));
	set_popdown_strings (action_combo, str);
	action_combo.set_active_text (str.front());
	action_combo.set_sensitive (false);

	l = manage (new Label);
	l->set_markup (_("<b>Insert at</b>"));

	vbox = manage (new VBox);
	vbox->set_border_width (12);
	vbox->set_spacing (6);
	vbox->pack_start (*l, false, false);
	vbox->pack_start (where_combo, false, false);
	hbox = manage (new HBox);
	hbox->pack_start (*vbox, false, false);
	options.pack_start (*hbox, false, false);


	l = manage (new Label);
	l->set_markup (_("<b>Mapping</b>"));

	vbox = manage (new VBox);
	vbox->set_border_width (12);
	vbox->set_spacing (6);
	vbox->pack_start (*l, false, false);
	vbox->pack_start (channel_combo, false, false);
	hbox = manage (new HBox);
	hbox->pack_start (*vbox, false, false);
	options.pack_start (*hbox, false, false);

	str.clear ();
	str.push_back (_("one track per file"));
	set_popdown_strings (channel_combo, str);
	channel_combo.set_active_text (str.front());
	channel_combo.set_sensitive (false);

	l = manage (new Label);
	l->set_markup (_("<b>Conversion quality</b>"));

	vbox = manage (new VBox);
	vbox->set_border_width (12);
	vbox->set_spacing (6);
	vbox->pack_start (*l, false, false);
	vbox->pack_start (src_combo, false, false);
	hbox = manage (new HBox);
	hbox->pack_start (*vbox, false, false);
	options.pack_start (*hbox, false, false);

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

	action_combo.signal_changed().connect (sigc::mem_fun (*this, &SoundFileOmega::reset_options_noret));
	channel_combo.signal_changed().connect (sigc::mem_fun (*this, &SoundFileOmega::reset_options_noret));

	copy_files_btn.set_active (true);

	Gtk::Label* copy_label = dynamic_cast<Gtk::Label*>(copy_files_btn.get_child());

	if (copy_label) {
		copy_label->set_size_request (175, -1);
		copy_label->set_line_wrap (true);
	}

	block_four.pack_start (copy_files_btn, false, false);

	options.pack_start (block_four, false, false);

	get_vbox()->pack_start (options, false, false);

	/* setup disposition map */

	disposition_map.insert (pair<string,ImportDisposition>(_("one track per file"), ImportDistinctFiles));
	disposition_map.insert (pair<string,ImportDisposition>(_("one track per channel"), ImportDistinctChannels));
	disposition_map.insert (pair<string,ImportDisposition>(_("merge files"), ImportMergeFiles));
	disposition_map.insert (pair<string,ImportDisposition>(_("sequence files"), ImportSerializeFiles));

	disposition_map.insert (pair<string,ImportDisposition>(_("one region per file"), ImportDistinctFiles));
	disposition_map.insert (pair<string,ImportDisposition>(_("one region per channel"), ImportDistinctChannels));
	disposition_map.insert (pair<string,ImportDisposition>(_("all files in one region"), ImportMergeFiles));
	disposition_map.insert (pair<string,ImportDisposition>(_("all files in one track"), ImportMergeFiles));

	chooser.signal_selection_changed().connect (sigc::mem_fun (*this, &SoundFileOmega::file_selection_changed));

	/* set size requests for a couple of combos to allow them to display the longest text
	   they will ever be asked to display.  This prevents them being resized when the user
	   selects a file to import, which in turn prevents the size of the dialog from jumping
	   around. */

	vector<string> t;
	t.push_back (_("one track per file"));
	t.push_back (_("one track per channel"));
	t.push_back (_("sequence files"));
	t.push_back (_("all files in one region"));
	set_popdown_strings (channel_combo, t);

	t.clear ();
	t.push_back (importmode2string (ImportAsTrack));
	t.push_back (importmode2string (ImportToTrack));
	t.push_back (importmode2string (ImportAsRegion));
	t.push_back (importmode2string (ImportAsTapeTrack));
	set_popdown_strings (action_combo, t);
}

void
SoundFileOmega::set_mode (ImportMode mode)
{
	action_combo.set_active_text (importmode2string (mode));
}

ImportMode
SoundFileOmega::get_mode () const
{
	return string2importmode (action_combo.get_active_text());
}

void
SoundFileOmega::on_hide ()
{
	ArdourDialog::on_hide();
	if (_session) {
		_session->cancel_audition();
	}
}

ImportPosition
SoundFileOmega::get_position() const
{
	string str = where_combo.get_active_text();

	if (str == _("file timestamp")) {
		return ImportAtTimestamp;
	} else if (str == _("edit point")) {
		return ImportAtEditPoint;
	} else if (str == _("playhead")) {
		return ImportAtPlayhead;
	} else {
		return ImportAtStart;
	}
}

SrcQuality
SoundFileOmega::get_src_quality() const
{
	string str = where_combo.get_active_text();

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

	string str = channel_combo.get_active_text();
	DispositionMap::const_iterator x = disposition_map.find (str);

	if (x == disposition_map.end()) {
		fatal << string_compose (_("programming error: %1 (%2)"), "unknown string for import disposition", str) << endmsg;
		/*NOTREACHED*/
	}

	return x->second;
}

void
SoundFileOmega::reset (uint32_t selected_audio_tracks, uint32_t selected_midi_tracks)
{
	selected_audio_track_cnt = selected_audio_tracks;
	selected_midi_track_cnt = selected_midi_tracks;

	if (selected_audio_track_cnt == 0 && selected_midi_track_cnt > 0) {
		chooser.set_filter (midi_filter);
	} else if (selected_midi_track_cnt == 0 && selected_audio_track_cnt > 0) {
		chooser.set_filter (audio_filter);
	} else {
		chooser.set_filter (audio_and_midi_filter);
	}

	reset_options ();
}

void
SoundFileOmega::file_selection_changed ()
{
	if (resetting_ourselves) {
		return;
	}

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

