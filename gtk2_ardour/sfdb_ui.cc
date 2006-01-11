/*
    Copyright (C) 2005 Paul Davis 
    Written by Taybin Rutkin

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

#include <sndfile.h>

#include <pbd/basename.h>

#include <gtkmm/box.h>
#include <gtkmm/stock.h>

#include <ardour/audio_library.h>
#include <ardour/audioregion.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/sndfilesource.h>

#include "sfdb_ui.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;

std::string length2string (const int32_t frames, const int32_t sample_rate);

SoundFileBox::SoundFileBox ()
	:
	_session(0),
	current_pid(0),
	fields(Gtk::ListStore::create(label_columns)),
	main_box (false, 3),
	top_box (true, 4),
	bottom_box (true, 4),
	play_btn(_("Play")),
	stop_btn(_("Stop")),
	add_field_btn(_("Add Field...")),
	remove_field_btn(_("Remove Field"))
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
	main_box.pack_start(field_view, true, true);
	main_box.pack_start(top_box, false, false);
	main_box.pack_start(bottom_box, false, false);

	field_view.set_size_request(200, 150);
	field_view.append_column (_("Field"), label_columns.field);
	field_view.append_column_editable (_("Value"), label_columns.data);

	top_box.set_homogeneous(true);
	top_box.pack_start(add_field_btn);
	top_box.pack_start(remove_field_btn);

	remove_field_btn.set_sensitive(false);

	bottom_box.set_homogeneous(true);
	bottom_box.pack_start(play_btn);
	bottom_box.pack_start(stop_btn);

	play_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::play_btn_clicked));
	stop_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::stop_btn_clicked));

	add_field_btn.signal_clicked().connect
	                (mem_fun (*this, &SoundFileBox::add_field_clicked));
	remove_field_btn.signal_clicked().connect
	                (mem_fun (*this, &SoundFileBox::remove_field_clicked));

	field_view.get_selection()->signal_changed().connect (mem_fun (*this, &SoundFileBox::field_selected));
	Library->fields_changed.connect (mem_fun (*this, &SoundFileBox::setup_fields));

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

    SNDFILE *sf;

	sf_info.format = 0; // libsndfile says to clear this before sf_open().

    if ((sf = sf_open ((char *) filename.c_str(), SFM_READ, &sf_info)) < 0) {
        return false;
    }

	sf_close (sf);

    if (sf_info.frames == 0 && sf_info.channels == 0 &&
		sf_info.samplerate == 0 && sf_info.format == 0 &&
	   	sf_info.sections == 0) {
		/* .. ok, it's not a sound file */
	    return false;
	}

	length.set_alignment (0.0f, 0.0f);
	length.set_text (string_compose("Length: %1", length2string(sf_info.frames, sf_info.samplerate)));

	format.set_alignment (0.0f, 0.0f);
	format.set_text (string_compose("Format: %1, %2", 
				sndfile_major_format(sf_info.format),
				sndfile_minor_format(sf_info.format)));

	channels.set_alignment (0.0f, 0.0f);
	channels.set_text (string_compose("Channels: %1", sf_info.channels));

	samplerate.set_alignment (0.0f, 0.0f);
	samplerate.set_text (string_compose("Samplerate: %1", sf_info.samplerate));

	setup_fields ();

	return true;
}

void
SoundFileBox::setup_fields ()
{
	ENSURE_GUI_THREAD(mem_fun (*this, &SoundFileBox::setup_fields));

	vector<string> field_list;
	Library->get_fields(field_list);

	vector<string>::iterator i;
	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;
	for (i = field_list.begin(); i != field_list.end(); ++i) {
		string value = Library->get_field(path, *i);
		iter = fields->append();
		row = *iter;

		row[label_columns.field] = *i;
		row[label_columns.data]  = value;
	}
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

	static std::map<string, AudioRegion*> region_cache;

	if (region_cache.find (path) == region_cache.end()) {
		AudioRegion::SourceList srclist;
		SndFileSource* sfs;

		for (int n = 0; n < sf_info.channels; ++n) {
			try {
				sfs = new SndFileSource(path+":"+string_compose("%1", n), false);
				srclist.push_back(sfs);

			} catch (failed_constructor& err) {
				error << _("Could not access soundfile: ") << path << endmsg;
				return;
			}
		}

		if (srclist.empty()) {
			return;
		}

		string result;
		_session->region_name (result, PBD::basename(srclist[0]->name()), false);
		AudioRegion* a_region = new AudioRegion(srclist, 0, srclist[0]->length(), result, 0, Region::DefaultFlags, false);
		region_cache[path] = a_region;
	}

	play_btn.hide();
	stop_btn.show();

	_session->audition_region(*region_cache[path]);
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
SoundFileBox::add_field_clicked ()
{}

void
SoundFileBox::remove_field_clicked ()
{}

void
SoundFileBox::audition_status_changed (bool active)
{
    ENSURE_GUI_THREAD(bind (mem_fun (*this, &SoundFileBox::audition_status_changed), active));

	if (!active) {
		stop_btn_clicked ();
	}
}

void
SoundFileBox::field_selected ()
{}

SoundFileBrowser::SoundFileBrowser (std::string title)
	:
	ArdourDialog(title),
	chooser(Gtk::FILE_CHOOSER_ACTION_OPEN)
{
	get_vbox()->pack_start(chooser);
	chooser.set_preview_widget(preview);

	chooser.signal_update_preview().connect(mem_fun(*this, &SoundFileBrowser::update_preview));
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

SoundFileChooser::SoundFileChooser (std::string title)
	:
	SoundFileBrowser(title)
{
	add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	show_all ();
}

SoundFileOmega::SoundFileOmega (std::string title)
	:
	SoundFileBrowser(title),
	embed_btn (_("Embed")),
	import_btn (_("Import")),
	split_check (_("Split Channels"))
{
	get_action_area()->pack_start(embed_btn);
	get_action_area()->pack_start(import_btn);
	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);

	chooser.set_extra_widget(split_check);

	embed_btn.signal_clicked().connect (mem_fun (*this, &SoundFileOmega::embed_clicked));
	import_btn.signal_clicked().connect (mem_fun (*this, &SoundFileOmega::import_clicked));

	show_all ();
}

void
SoundFileOmega::embed_clicked ()
{
	Embedded (chooser.get_filenames(), split_check.get_active());
}

void
SoundFileOmega::import_clicked ()
{
	Imported (chooser.get_filenames(), split_check.get_active());
}

std::string
length2string (const int32_t frames, const int32_t sample_rate)
{
    int secs = (int) (frames / (float) sample_rate);
    int hrs =  secs / 3600;
    secs -= (hrs * 3600);
    int mins = secs / 60;
    secs -= (mins * 60);

    int total_secs = (hrs * 3600) + (mins * 60) + secs;
    int frames_remaining = frames - (total_secs * sample_rate);
    float fractional_secs = (float) frames_remaining / sample_rate;

    char duration_str[32];
    sprintf (duration_str, "%02d:%02d:%05.2f", hrs, mins, (float) secs + fractional_secs);

    return duration_str;
}

