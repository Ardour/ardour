/*
    Copyright (C) 1999-2005 Paul Davis 

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

#include <unistd.h>
#include <utility>

#include <fstream>

#include <samplerate.h>
#include <pbd/pthread_utils.h>
#include <pbd/xml++.h>

#include <gtkmm2ext/utils.h>
#include <ardour/export.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/audio_track.h>
#include <ardour/audioregion.h>
#include <ardour/audioengine.h>
#include <ardour/gdither.h>
#include <ardour/utils.h>

#include "export_dialog.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "keyboard.h"

#include "i18n.h"

#define FRAME_NAME "BaseFrame"

using namespace std;

using namespace ARDOUR;
using namespace sigc;
using namespace Gtk;

static const gchar *sample_rates[] = {
	N_("22.05kHz"),
	N_("44.1kHz"),
	N_("48kHz"),
	N_("88.2kHz"),
	N_("96kHz"),
	N_("192kHz"),
	0
};

static const gchar *src_quality[] = {
	N_("best"),
	N_("fastest"),
	N_("linear"),
	N_("better"),
	N_("intermediate"),
	0
};

static const gchar *dither_types[] = {
	N_("None"),
	N_("Rectangular"),
	N_("Shaped Noise"),
	N_("Triangular"),
	0
};

static const gchar* channel_strings[] = {
	N_("stereo"), 
	N_("mono"), 
	0
};

static const gchar* cue_file_types[] = {
	N_("None"), 
	N_("CUE"),
 	N_("TOC"),
	0
};

ExportDialog::ExportDialog(PublicEditor& e, AudioRegion* r)
	: ArdourDialog ("export dialog"),
	  editor (e),
	  format_table (9, 2),
	  format_frame (_("FORMAT")),
	  channel_count_label (_("CHANNELS")),
	  header_format_label (_("FILE TYPE")),
	  bitdepth_format_label (_("SAMPLE FORMAT")),
	  endian_format_label (_("SAMPLE ENDIANNESS")),
	  sample_rate_label (_("SAMPLE RATE")),
	  src_quality_label (_("CONVERSION QUALITY")),
	  dither_type_label (_("DITHER TYPE")),
	  cue_file_label (_("CD MARKER FILE TYPE")),
	  channel_count_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  header_format_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  bitdepth_format_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  endian_format_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  sample_rate_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  src_quality_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  dither_type_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  cue_file_align(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, 0.0, 0.0),
	  cuefile_only_checkbox (_("EXPORT CD MARKER FILE ONLY")),
	  file_frame (_("EXPORT TO FILE")),
	  file_browse_button (_("Browse")),
	  ok_button (_("Export")),
	  track_selector_button (_("Specific tracks ..."))
{
	guint32 n;
	guint32 len;
	guint32 maxlen;

	audio_region = r;

	session = 0;
	
	set_title (_("ardour: export"));
	set_wmclass (_("ardour_export"), "Ardour");
	set_name ("ExportWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	file_selector = 0;
	spec.running = false;

	file_entry.set_name ("ExportFileNameEntry");

	master_list = ListStore::create (exp_cols);
	master_selector.set_model (master_list);

	master_selector.set_name ("ExportTrackSelector");
	master_selector.set_size_request (-1, 100);
	master_selector.append_column(_("Output"), exp_cols.output);
	master_selector.append_column_editable(_("Left"), exp_cols.left);
	master_selector.append_column_editable(_("Right"), exp_cols.right);
	master_selector.get_column(0)->set_min_width(100);
	
	master_selector.get_column(1)->set_min_width(40);
	master_selector.get_column(1)->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
	master_selector.get_column(2)->set_min_width(40);
	master_selector.get_column(2)->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
	master_selector.get_selection()->set_mode (Gtk::SELECTION_NONE);

	track_list = ListStore::create (exp_cols);
	track_selector.set_model (track_list);

	track_selector.set_name ("ExportTrackSelector");
	track_selector.set_size_request (-1, 130);
	track_selector.append_column(_("Output"), exp_cols.output);
	track_selector.append_column_editable(_("Left"), exp_cols.left);
	track_selector.append_column_editable(_("Right"), exp_cols.right);

	track_selector.get_column(0)->set_min_width(100);
	track_selector.get_column(1)->set_min_width(40);
	track_selector.get_column(1)->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
	track_selector.get_column(2)->set_min_width(40);
	track_selector.get_column(2)->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
	track_selector.get_selection()->set_mode (Gtk::SELECTION_NONE);

	progress_bar.set_name ("ExportProgress");

	format_frame.add (format_table);
	format_frame.set_name (FRAME_NAME);

	track_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	master_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	get_vbox()->pack_start (file_frame, false, false);

	hpacker.set_spacing (5);
	hpacker.set_border_width (5);
	hpacker.pack_start (format_frame, false, false);

	if (!audio_region) {

		master_scroll.add (master_selector);
		track_scroll.add (track_selector);

		master_scroll.set_size_request (220, 100);
		track_scroll.set_size_request (220, 100);

		
		
		/* we may hide some of these later */
		track_vpacker.pack_start (master_scroll);
		track_vpacker.pack_start (track_scroll);
		track_vpacker.pack_start (track_selector_button, Gtk::PACK_EXPAND_PADDING);

		hpacker.pack_start (track_vpacker);
	}

	get_vbox()->pack_start (hpacker);
	
	track_selector_button.set_name ("EditorGTKButton");
	track_selector_button.signal_clicked().connect (mem_fun(*this, &ExportDialog::track_selector_button_click));

	get_vbox()->pack_start (button_box, false, false);
	get_vbox()->pack_start (progress_bar, false, false);

	Gtkmm2ext::set_size_request_to_display_given_text (file_entry, X_("Kg/quite/a/reasonable/size/for/files/i/think"), 5, 8);

	file_hbox.set_spacing (5);
	file_hbox.set_border_width (5);
	file_hbox.pack_start (file_entry, true, true);
	file_hbox.pack_start (file_browse_button, false, false);

	file_frame.add (file_hbox);
	file_frame.set_border_width (5);
	file_frame.set_name (FRAME_NAME);

	/* pop_strings needs to be created on the stack because set_popdown_strings()
	 * takes a reference. */
	vector<string> pop_strings = internationalize(sample_rates);
	Gtkmm2ext::set_popdown_strings (sample_rate_combo, pop_strings);
	pop_strings = internationalize(sample_rates);
	Gtkmm2ext::set_popdown_strings (src_quality_combo, pop_strings);
	pop_strings = internationalize(dither_types);
	Gtkmm2ext::set_popdown_strings (dither_type_combo, pop_strings);
	pop_strings = internationalize(channel_strings);
	Gtkmm2ext::set_popdown_strings (channel_count_combo, pop_strings);
	pop_strings = internationalize((const char **) sndfile_header_formats_strings);
	Gtkmm2ext::set_popdown_strings (header_format_combo, pop_strings);
	pop_strings = internationalize((const char **) sndfile_bitdepth_formats_strings);
	Gtkmm2ext::set_popdown_strings (bitdepth_format_combo, pop_strings);
	pop_strings = internationalize((const char **) sndfile_endian_formats_strings);
	Gtkmm2ext::set_popdown_strings (endian_format_combo, pop_strings);
	pop_strings = internationalize(cue_file_types);
	Gtkmm2ext::set_popdown_strings (cue_file_combo, pop_strings);

	/* this will re-sensitized as soon as a non RIFF/WAV
	   header format is chosen.
	*/

	endian_format_combo.set_sensitive (false);

	/* determine longest strings at runtime */

	const guint32 FUDGE = 10; // Combo's are stupid - they steal space from the entry for the button

	maxlen = 0;
	const char *longest = "gl";
	string longest_str;

	for (n = 0; n < SNDFILE_HEADER_FORMATS; ++n) {
		if ((len = strlen (sndfile_header_formats_strings[n])) > maxlen) {
			maxlen = len;
			longest = sndfile_header_formats_strings[n];
		}
	}

	for (n = 0; n < SNDFILE_BITDEPTH_FORMATS; ++n) {
		if ((len = strlen (sndfile_bitdepth_formats_strings[n])) > maxlen) {
			maxlen = len;
			longest = sndfile_bitdepth_formats_strings[n];
		}
	}

	for (n = 0; n < SNDFILE_ENDIAN_FORMATS; ++n) {
		if ((len = strlen (sndfile_endian_formats_strings[n])) > maxlen) {
			maxlen = len;
			longest = sndfile_endian_formats_strings[n];
		}
	}

	longest_str = longest;

	/* force ascender + descender */

	longest_str[0] = 'g';
	longest_str[1] = 'l';

	Gtkmm2ext::set_size_request_to_display_given_text (header_format_combo, longest_str.c_str(), 5+FUDGE, 5);

	// TRANSLATORS: "slereg" is "stereo" with ascender and descender substituted
	Gtkmm2ext::set_size_request_to_display_given_text (channel_count_combo, _("slereg"), 5+FUDGE, 5);

/*	header_format_combo.set_focus_on_click (true);
	bitdepth_format_combo.set_focus_on_click (true);
	endian_format_combo.set_focus_on_click (true);
	channel_count_combo.set_focus_on_click (true);
	src_quality_combo.set_focus_on_click (true);
	dither_type_combo.set_focus_on_click (true);
	sample_rate_combo.set_focus_on_click (true);
	cue_file_combo.set_focus_on_click (true);
*/
	dither_type_label.set_name ("ExportFormatLabel");
	sample_rate_label.set_name ("ExportFormatLabel");
	src_quality_label.set_name ("ExportFormatLabel");
	channel_count_label.set_name ("ExportFormatLabel");
	header_format_label.set_name ("ExportFormatLabel");
	bitdepth_format_label.set_name ("ExportFormatLabel");
	endian_format_label.set_name ("ExportFormatLabel");
	cue_file_label.set_name ("ExportFormatLabel");

	header_format_combo.set_name ("ExportFormatDisplay");
	bitdepth_format_combo.set_name ("ExportFormatDisplay");
	endian_format_combo.set_name ("ExportFormatDisplay");
	channel_count_combo.set_name ("ExportFormatDisplay");
	dither_type_combo.set_name ("ExportFormatDisplay");
	src_quality_combo.set_name ("ExportFormatDisplay");
	sample_rate_combo.set_name ("ExportFormatDisplay");
	cue_file_combo.set_name ("ExportFormatDisplay");

	cuefile_only_checkbox.set_name ("ExportCheckbox");

	format_table.set_homogeneous (true);
	format_table.set_border_width (5);
	format_table.set_col_spacings (5);
	format_table.set_row_spacings (5);

	if (!audio_region) {
		channel_count_align.add(channel_count_label);
		format_table.attach (channel_count_align, 0, 1, 0, 1);
		format_table.attach (channel_count_combo, 1, 2, 0, 1);
	}

	header_format_align.add(header_format_label);
	format_table.attach (header_format_align, 0, 1, 1, 2);
	format_table.attach (header_format_combo, 1, 2, 1, 2);

	bitdepth_format_align.add(bitdepth_format_label);
	format_table.attach (bitdepth_format_align, 0, 1, 2, 3);
	format_table.attach (bitdepth_format_combo, 1, 2, 2, 3);

	endian_format_align.add(endian_format_label);
	format_table.attach (endian_format_align, 0, 1, 3, 4);
	format_table.attach (endian_format_combo, 1, 2, 3, 4);

	sample_rate_align.add(sample_rate_label);
	format_table.attach (sample_rate_align, 0, 1, 4, 5);
	format_table.attach (sample_rate_combo, 1, 2, 4, 5);

	src_quality_align.add(src_quality_label);
	format_table.attach (src_quality_align, 0, 1, 5, 6);
	format_table.attach (src_quality_combo, 1, 2, 5, 6);

	dither_type_align.add(dither_type_label);
	format_table.attach (dither_type_align, 0, 1, 6, 7);
	format_table.attach (dither_type_combo, 1, 2, 6, 7);

	cue_file_align.add(cue_file_label);
	format_table.attach (cue_file_align, 0, 1, 7, 8);
	format_table.attach (cue_file_combo, 1, 2, 7, 8);
	format_table.attach (cuefile_only_checkbox, 1, 2, 8, 9);


	button_box.set_spacing (10);
	button_box.set_homogeneous (true);

	cancel_button.add (cancel_label);

	button_box.pack_start (ok_button, false, true);
	button_box.pack_start (cancel_button, false, true);
	
	ok_button.set_name ("EditorGTKButton");
	cancel_button.set_name ("EditorGTKButton");
	file_entry.set_name ("ExportFileDisplay");

	signal_delete_event().connect (mem_fun(*this, &ExportDialog::window_closed));
	ok_button.signal_clicked().connect (mem_fun(*this, &ExportDialog::do_export));
	cancel_button.signal_clicked().connect (mem_fun(*this, &ExportDialog::end_dialog));
	
	file_browse_button.set_name ("EditorGTKButton");
	file_browse_button.signal_clicked().connect (mem_fun(*this, &ExportDialog::initiate_browse));

	channel_count_combo.signal_changed().connect (mem_fun(*this, &ExportDialog::channels_chosen));
	bitdepth_format_combo.signal_changed().connect (mem_fun(*this, &ExportDialog::bitdepth_chosen));
	header_format_combo.signal_changed().connect (mem_fun(*this, &ExportDialog::header_chosen));
	sample_rate_combo.signal_changed().connect (mem_fun(*this, &ExportDialog::sample_rate_chosen));
	cue_file_combo.signal_changed().connect (mem_fun(*this, &ExportDialog::cue_file_type_chosen));
}

ExportDialog::~ExportDialog()
{
	if (file_selector) {
		delete file_selector;
	}
}

void
ExportDialog::connect_to_session (Session *s)
{
	session = s;
	session->going_away.connect (mem_fun(*this, &Window::hide_all));

	switch (session->frame_rate()) {
	case 22050:
		sample_rate_combo.set_active_text (N_("22.05kHz"));
		break;
	case 44100:
		sample_rate_combo.set_active_text (N_("44.1kHz"));
		break;
	case 48000:
		sample_rate_combo.set_active_text (N_("48kHz"));
		break;
	case 88200:
		sample_rate_combo.set_active_text (N_("88.2kHz"));
		break;
	case 96000:
		sample_rate_combo.set_active_text (N_("96kHz"));
		break;
	case 192000:
		sample_rate_combo.set_active_text (N_("192kHz"));
		break;
	default:
		sample_rate_combo.set_active_text (N_("44.1kHz"));
		break;
	}

	src_quality_combo.set_sensitive (false);

	set_state();
}

void
ExportDialog::set_state()
{
	XMLNode* node = session->instant_xml(X_("ExportDialog"), session->path());
	XMLProperty* prop;

	if (node) {

		if ((prop = node->property (X_("sample_rate"))) != 0) {
			sample_rate_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("src_quality"))) != 0) {
			src_quality_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("dither_type"))) != 0) {
			dither_type_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("channel_count"))) != 0) {
			channel_count_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("header_format"))) != 0) {
			header_format_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("bitdepth_format"))) != 0) {
			bitdepth_format_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("endian_format"))) != 0) {
			endian_format_combo.set_active_text(prop->value());
		}
		if ((prop = node->property (X_("filename"))) != 0) {
			file_entry.set_text(prop->value());
		}
		if ((prop = node->property (X_("cue_file_type"))) != 0) {
		        cue_file_combo.set_active_text(prop->value());
		}
	}

	header_chosen ();
	bitdepth_chosen();
	channels_chosen();
	sample_rate_chosen();

	if (session->master_out()) {
		track_scroll.hide ();
	} else {
		master_scroll.hide ();
		track_selector_button.hide ();
	}

	if (!node) {
		return;
	}

	if (session->master_out()) {
		XMLNode* master = find_named_node(*node, (X_("Master")));
		int nchns;

		if (!master) {
			
			/* default is to use all */
			if (channel_count_combo.get_active_text() == _("mono")) {
				nchns = 1;
			} else {
				nchns = 2;
			}

			TreeModel::Children rows = master_selector.get_model()->children();
			for (uint32_t r = 0; r < session->master_out()->n_outputs(); ++r) {
				if (nchns == 2) {
					if (r % 2) {
						rows[r][exp_cols.right] = true;
					} else {
						rows[r][exp_cols.left] = true;
					}
				} else {
					rows[r][exp_cols.left] = true;
				}
			}

		} else {
			/* XXX use XML state */
		}
	}

	XMLNode* tracks = find_named_node(*node, (X_("Tracks")));
	if (!tracks) {
		return;
	}
	
	XMLNodeList track_list = tracks->children(X_("Track"));
	TreeModel::Children rows = track_selector.get_model()->children();
	TreeModel::Children::iterator ri = rows.begin();
	TreeModel::Row row;

	for (XMLNodeIterator it = track_list.begin(); it != track_list.end(); ++it, ++ri) {
		if (ri == rows.end()){
			break;
		}

		XMLNode* track = *it;
		row = *ri;

		if ((prop = track->property(X_("channel1"))) != 0) {
			if (prop->value() == X_("on")) {
				row[exp_cols.left] = true;
			} else {
				row[exp_cols.left] = false;
			}
		}

		if ((prop = track->property(X_("channel2"))) != 0) {
			if (prop->value() == X_("on")) {
				row[exp_cols.right] = true;
			} else {
				row[exp_cols.right] = false;
			}
		}
	}
}

void
ExportDialog::save_state()
{
	if (!session) {
		return;
	}

	XMLNode* node = new XMLNode(X_("ExportDialog"));

	node->add_property(X_("sample_rate"), sample_rate_combo.get_active_text());
	node->add_property(X_("src_quality"), src_quality_combo.get_active_text());
	node->add_property(X_("dither_type"), dither_type_combo.get_active_text());
	node->add_property(X_("channel_count"), channel_count_combo.get_active_text());
	node->add_property(X_("header_format"), header_format_combo.get_active_text());
	node->add_property(X_("bitdepth_format"), bitdepth_format_combo.get_active_text());
	node->add_property(X_("endian_format"), endian_format_combo.get_active_text());
	node->add_property(X_("filename"), file_entry.get_text());
	node->add_property(X_("cue_file_type"), cue_file_combo.get_active_text());

	XMLNode* tracks = new XMLNode(X_("Tracks"));

	TreeModel::Children rows = track_selector.get_model()->children();
	TreeModel::Row row;
	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri) {
		XMLNode* track = new XMLNode(X_("Track"));

		row = *ri;
		track->add_property(X_("channel1"), row[exp_cols.left] ? X_("on") : X_("off"));
		track->add_property(X_("channel1"), row[exp_cols.right] ? X_("on") : X_("off"));

		tracks->add_child_nocopy(*track);
	}
	node->add_child_nocopy(*tracks);
	
	session->add_instant_xml(*node, session->path());
}

void
ExportDialog::set_range (jack_nframes_t start, jack_nframes_t end)
{
	spec.start_frame = start;
	spec.end_frame = end;

	if (!audio_region) {
		// XXX: this is a hack until we figure out what is really wrong
		session->request_locate (spec.start_frame, false);
	}
}

gint
ExportDialog::progress_timeout ()
{
	progress_bar.set_fraction (spec.progress/100);
	return TRUE;
}

void*
ExportDialog::_export_region_thread (void *arg)
{
	PBD::ThreadCreated (pthread_self(), X_("Export Region"));

	static_cast<ExportDialog*>(arg)->export_region ();
	return 0;
}

void
ExportDialog::export_region ()
{
	audio_region->exportme (*session, spec);
}

void
frames_to_cd_frames_string (char* buf, jack_nframes_t when, jack_nframes_t fr)
{

  long unsigned int remainder;
  int mins, secs, frames;

	mins = when / (60 * fr);
	remainder = when - (mins * 60 * fr);
	secs = remainder / fr;
	remainder -= secs * fr;
	frames = remainder / (fr / 75);
	sprintf (buf, " %02d:%02d:%02d", mins, secs, frames);

}

struct LocationSortByStart {
    bool operator() (Location *a, Location *b) {
	    return a->start() < b->start();
    }
};

void
ExportDialog::export_toc_file (Locations::LocationList& locations, const string& path)
{
	
        string filepath = path + ".toc";
	ofstream out (filepath.c_str());
	long unsigned int last_end_time = spec.start_frame, last_start_time = spec.start_frame;
	int numtracks = 0;
	gchar buf[18];

	if (!out) {
		error << string_compose(_("Editor: cannot open \"%1\" as export file for CD toc file"), filepath) << endmsg;
		return;
	}
	out << "CD_DA" << endl;
	out << "CD_TEXT {" << endl << "  LANGUAGE_MAP {" << endl << "    0 : EN" << endl << "  }" << endl;
	out << "  LANGUAGE 0 {" << endl << "    TITLE \"" << session->name() << "\"" << endl << "  }" << endl << "}" << endl;

	Locations::LocationList::iterator i;
	Locations::LocationList temp;

	for (i = locations.begin(); i != locations.end(); ++i) {
	  if ((*i)->start() >= spec.start_frame && (*i)->end() <= spec.end_frame && (*i)->is_cd_marker() && !(*i)->is_end()) {
	    temp.push_back (*i);
	    if (!(*i)->is_mark()) {
	      numtracks ++;
	    }
	  }
	}

	if (numtracks == 0 ) {
		    /* the user supplied no track markers.
		       we now treat the session as one track.*/

		    out << endl << "TRACK AUDIO" << endl;
		   
		    out << "COPY" << endl;

		    out << "NO PRE_EMPHASIS" << endl;
   
		    /* XXX add session properties for catalog etc.
		       (so far only the session name is used) */
		    
		    out << "CD_TEXT {" << endl << "  LANGUAGE 0 {" << endl << "     TITLE \"" << session->name() << "\"" << endl;
		    out << "  }" << endl << "}" << endl;

		    out << "FILE \"" << path << "\" ";
		    out << "00:00:00 " ;
		    frames_to_cd_frames_string (buf, spec.end_frame - spec.start_frame, session->frame_rate());
		    out << buf << endl;
		    out << "START 00:00:00" << endl;

		    last_start_time = spec.start_frame;
		    last_end_time = spec.end_frame;
	} 

	if (temp.size()) {
		LocationSortByStart cmp;
		temp.sort (cmp);

		for (i = temp.begin(); i != temp.end(); ++i) {
	
		      if (!(*i)->is_mark()) {
			/*this is a track */
		        out << endl << "TRACK AUDIO" << endl;

			if ((*i)->cd_info.find("scms") != (*i)->cd_info.end())  {
			  out << "NO ";
			}
			out << "COPY" << endl;

			if ((*i)->cd_info.find("preemph") != (*i)->cd_info.end())  {
			  out << "PRE_EMPHASIS" << endl;
			} else {
			  out << "NO PRE_EMPHASIS" << endl;
			}

			if ((*i)->cd_info.find("isrc") != (*i)->cd_info.end())  {
			  out << "ISRC \"" << (*i)->cd_info["isrc"] << "\"" << endl;
			}

			out << "CD_TEXT {" << endl << "  LANGUAGE 0 {" << endl << "     TITLE \"" << (*i)->name() << "\"" << endl;
			if ((*i)->cd_info.find("performer") != (*i)->cd_info.end()) {
			  out << "     PERFORMER \"" << (*i)->cd_info["performer"]  << "\"" << endl;
			}
			if ((*i)->cd_info.find("string_composer") != (*i)->cd_info.end()) {
			  out  << "     COMPOSER \"" << (*i)->cd_info["string_composer"] << "\"" << endl;
			}

			if ((*i)->cd_info.find("isrc") != (*i)->cd_info.end()) {			  
			  out  << "     ISRC \"";
			  out << (*i)->cd_info["isrc"].substr(0,2) << "-";
			  out << (*i)->cd_info["isrc"].substr(2,3) << "-";
			  out << (*i)->cd_info["isrc"].substr(5,2) << "-";
			  out << (*i)->cd_info["isrc"].substr(7,5) << "\"" << endl;
			}

			out << "  }" << endl << "}" << endl;

			frames_to_cd_frames_string (buf, last_end_time - spec.start_frame, session->frame_rate());
			out << "FILE \"" << path << "\" " << buf;

			frames_to_cd_frames_string (buf, (*i)->end() - last_end_time, session->frame_rate());
			out << buf << endl;

			frames_to_cd_frames_string (buf, (*i)->start() - last_end_time, session->frame_rate());
			out << "START" << buf << endl;
			
			last_start_time = (*i)->start();
			last_end_time = (*i)->end();
		 

		      } else  if ((*i)->start() < last_end_time) {
			/* this is an index within a track */
			
			frames_to_cd_frames_string (buf, (*i)->start() - last_start_time, session->frame_rate());
			out << "INDEX" << buf << endl;
		      }
		}
	}
	
}

void
ExportDialog::export_cue_file (Locations::LocationList& locations, const string& path)
{
        string filepath = path + ".cue";
	ofstream out (filepath.c_str());
	gchar buf[18];
	long unsigned int last_track_end = spec.start_frame;
	int numtracks = 0, tracknum = 0, indexnum = 0;

	if (!out) {
		error << string_compose(_("Editor: cannot open \"%1\" as export file for CD cue file"), filepath) << endmsg;
		return;
	}

	Locations::LocationList::iterator i;
	Locations::LocationList temp;

	for (i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->start() >= spec.start_frame && (*i)->end() <= spec.end_frame && (*i)->is_cd_marker() && !(*i)->is_end()) {
			temp.push_back (*i);
			if (!(*i)->is_mark()) {
				numtracks++;
			}
		}
	}
	
	out << "REM Cue file generated by Ardour" << endl;
	out << "TITLE \"" << session->name() << "\"" << endl;

	if ((header_format_combo.get_active_text() == N_("WAV"))) {
		  out << "FILE " << path  << " WAVE" << endl;
	} else {
		  out << "FILE " << path  << ' ' << (header_format_combo.get_active_text()) << endl;
	}

	if (numtracks == 0) {
		    /* the user has supplied no track markers.
		       the entire export is treated as one track. 
		    */

		  numtracks++;
		  tracknum++;
		  indexnum = 0;
		  out << endl << "TRACK " << tracknum << " AUDIO" << endl;
		  out << "FLAGS " ;
		  
		  out << "DCP " << endl;		   
		  
		  /* use the session name*/
		  
		  if (session->name() != "") {
		    out << "TITLE \"" << session->name() << "\"" << endl;
		  }	      
		  
		  /* no pregap in this case */

		  out << "INDEX 00 00:00:00" << endl;
		  indexnum++;
		  out << "INDEX 01 00:00:00" << endl;
		  indexnum++;
		  last_track_end = spec.end_frame;
	}

	if (temp.size()) {
		LocationSortByStart cmp;
		temp.sort (cmp);

		for ( i = temp.begin(); i != temp.end(); ++i) {

		    if (!(*i)->is_mark() && ((*i)->start() >= last_track_end)) {
		      /* this is a track and it doesn't start inside another one*/
		      
		      tracknum++;
		      indexnum = 0;
		      out << endl << "TRACK " << tracknum << " AUDIO" << endl;
		      out << "FLAGS " ;
		      
		      if ((*i)->cd_info.find("scms") != (*i)->cd_info.end())  {
			out << "SCMS ";
		      } else {
			out << "DCP ";
		      }
		      
		      if ((*i)->cd_info.find("preemph") != (*i)->cd_info.end())  {
			out << "PRE";
		      }
		      out << endl;
		      
		      if ((*i)->cd_info.find("isrc") != (*i)->cd_info.end())  {
			out << "ISRC " << (*i)->cd_info["isrc"] << endl;
			
		      }
		      if ((*i)->name() != "") {
			out << "TITLE \"" << (*i)->name() << "\"" << endl;
		      }	      
		      
		      if ((*i)->cd_info.find("performer") != (*i)->cd_info.end()) {
			out << "PERFORMER \"" <<  (*i)->cd_info["performer"] << "\"" << endl;
		      }
		      
		      if ((*i)->cd_info.find("string_composer") != (*i)->cd_info.end()) {
			out << "SONGWRITER \"" << (*i)->cd_info["string_composer"]  << "\"" << endl;
		      }
			snprintf (buf, sizeof(buf), "INDEX %02d", indexnum);
			out << buf;
			frames_to_cd_frames_string (buf, last_track_end - spec.start_frame, session->frame_rate());
			out << buf << endl;
			indexnum++;
			last_track_end = (*i)->end();
		    } 
		    if ((tracknum > 0) && ((*i)->start() < last_track_end)) {
		      /*this is an index and it lies within a track*/
		      snprintf (buf, sizeof(buf), "INDEX %02d", indexnum);
		      out << buf;
		      frames_to_cd_frames_string (buf,(*i)->start() - spec.start_frame, session->frame_rate());
		      out << buf << endl;
		      indexnum++;
		    }
		}
	}
	
}

void
ExportDialog::do_export_cd_markers (const string& path,const string& cuefile_type)
{
	if (cuefile_type == "TOC") {
		session->locations()->apply (*this, &ExportDialog::export_toc_file, path);	
	} else {
		session->locations()->apply (*this, &ExportDialog::export_cue_file, path);
	}
}


void
ExportDialog::do_export ()
{
	ok_button.set_sensitive(false);
	save_state();

	if (cue_file_combo.get_active_text () != _("None")) {
		do_export_cd_markers (file_entry.get_text(), cue_file_combo.get_active_text ());
	}

	if (cuefile_only_checkbox.get_active()) {
		end_dialog ();
		return;
	}

	set_modal (true);
	
	spec.path = file_entry.get_text();
	spec.progress = 0;
	spec.running = true;
	spec.stop = false;
	spec.port_map.clear();
	
	if (channel_count_combo.get_active_text() == _("mono")) {
		spec.channels = 1;
	} else {
		spec.channels = 2;
	}

	spec.format = 0;

	spec.format |= sndfile_header_format_from_string (header_format_combo.get_active_text ());
	
	if ((spec.format & SF_FORMAT_WAV) == 0) {
		/* RIFF/WAV specifies endianess */
		spec.format |= sndfile_endian_format_from_string (endian_format_combo.get_active_text ());
	}

	spec.format |= sndfile_bitdepth_format_from_string (bitdepth_format_combo.get_active_text ());

	string sr_str = sample_rate_combo.get_active_text();
	if (sr_str == N_("22.05kHz")) {
		spec.sample_rate = 22050;
	} else if (sr_str == N_("44.1kHz")) {
		spec.sample_rate = 44100;
	} else if (sr_str == N_("48kHz")) {
		spec.sample_rate = 48000;
	} else if (sr_str == N_("88.2kHz")) {
		spec.sample_rate = 88200;
	} else if (sr_str == N_("96kHz")) {
		spec.sample_rate = 96000;
	} else if (sr_str == N_("192kHz")) {
		spec.sample_rate = 192000;
	} else {
		spec.sample_rate = session->frame_rate();
	}
	
	string src_str = src_quality_combo.get_active_text();
	if (src_str == _("fastest")) {
		spec.src_quality = SRC_ZERO_ORDER_HOLD;
	} else if (src_str == _("linear")) {
		spec.src_quality = SRC_LINEAR;
	} else if (src_str == _("better")) {
		spec.src_quality = SRC_SINC_FASTEST;
	} else if (src_str == _("intermediate")) {
		spec.src_quality = SRC_SINC_MEDIUM_QUALITY;
	} else {
		spec.src_quality = SRC_SINC_BEST_QUALITY;
	}

	string dither_str = dither_type_combo.get_active_text();
	if (dither_str == _("None")) {
		spec.dither_type = GDitherNone;
	} else if (dither_str == _("Rectangular")) {
		spec.dither_type = GDitherRect;
	} else if (dither_str == _("Triangular")) {
		spec.dither_type = GDitherTri;
	} else {
		spec.dither_type = GDitherShaped;
	} 

	if (!audio_region) {

		uint32_t chan=0;
		Port *last_port = 0;
		
		TreeModel::Children rows = master_selector.get_model()->children();
		TreeModel::Children::iterator ri;
		TreeModel::Row row;
		for (ri = rows.begin(); ri != rows.end(); ++ri) {
			row = *ri;
			Port* port = row[exp_cols.port];
			
			if (last_port != port) {
				chan = 0;
			}
			
			if (row[exp_cols.left]) {
				spec.port_map[0].push_back (std::pair<Port*,uint32_t>(port, chan));
			} 
			
			if (spec.channels == 2) {
				if (row[exp_cols.right]) {
					spec.port_map[1].push_back (std::pair<Port*,uint32_t>(port, chan));
				}
			}
		}

		chan = 0;

		rows = track_selector.get_model()->children();
		for (ri = rows.begin(); ri != rows.end(); ++ri) {
			row = *ri;
			
			Port* port = row[exp_cols.port];
			
			if (last_port != port) {
				chan = 0;
			}
			
			if (row[exp_cols.left]) {
				spec.port_map[0].push_back (std::pair<Port*,uint32_t>(port, chan));
			} 
			
			if (spec.channels == 2) {
				if (row[exp_cols.right]) {
					spec.port_map[1].push_back (std::pair<Port*,uint32_t>(port, chan));
				}
				
			}
			
			last_port = port;
			++chan;
		}
	}

	progress_connection = Glib::signal_timeout().connect (mem_fun(*this, &ExportDialog::progress_timeout), 100);
	cancel_label.set_text (_("Stop Export"));

	if (!audio_region) {
		if (session->start_audio_export (spec)) {
			goto out;
		}
	} else {
		pthread_t thr;
		pthread_create_and_store ("region export", &thr, 0, ExportDialog::_export_region_thread, this);
	}

	gtk_main_iteration ();
	while (spec.running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			usleep (10000);
		}
	}
	
  out:
	progress_connection.disconnect ();
	end_dialog ();
}
	

void
ExportDialog::end_dialog ()
{

	if (spec.running) {
		spec.stop = true;

		while (spec.running) {
			if (gtk_events_pending()) {
				gtk_main_iteration ();
			} else {
				usleep (10000);
			}
		}
	}

	session->engine().freewheel (false);

	hide_all ();

	if (file_selector) {
		file_selector->hide_all ();
	}

	set_modal (false);
	ok_button.set_sensitive(true);
}

void
ExportDialog::start_export ()
{
	if (session == 0) {
		return;
	}

	/* If it the filename hasn't been set before, use the
	   directory above the current session as a default
	   location for the export.  
	*/
	
	if (file_entry.get_text().length() == 0) {
		string dir = session->path();
		string::size_type last_slash;
		
		if ((last_slash = dir.find_last_of ('/')) != string::npos) {
			dir = dir.substr (0, last_slash+1);
		}
		
		file_entry.set_text (dir);
	}
	
	progress_bar.set_fraction (0);
	cancel_label.set_text (_("Cancel"));

	show_all ();

	if (session->master_out()) {
		track_scroll.hide ();
	} else {
		master_scroll.hide ();
		track_selector_button.hide ();
	}
}

void
ExportDialog::header_chosen ()
{
	if (sndfile_header_format_from_string (header_format_combo.get_active_text ()) == SF_FORMAT_WAV) {
		endian_format_combo.set_sensitive (false);
	} else {
		endian_format_combo.set_sensitive (true);
	}
}

void
ExportDialog::bitdepth_chosen ()
{
	int format = sndfile_bitdepth_format_from_string (bitdepth_format_combo.get_active_text ());	
	switch (format) {
	case SF_FORMAT_PCM_24:
	case SF_FORMAT_PCM_32:
	case SF_FORMAT_FLOAT:
		dither_type_combo.set_sensitive (false);
		break;

	default:
		dither_type_combo.set_sensitive (true);
		break;
	}
}

void
ExportDialog::cue_file_type_chosen ()
{
	if (cue_file_combo.get_active_text () != "None") {
		cuefile_only_checkbox.set_sensitive (true);
	} else {
		cuefile_only_checkbox.set_active (false);
		cuefile_only_checkbox.set_sensitive (false);
	}
}

void
ExportDialog::sample_rate_chosen ()
{
	string sr_str = sample_rate_combo.get_active_text();
	jack_nframes_t rate;

	if (sr_str == N_("22.05kHz")) {
		rate = 22050;
	} else if (sr_str == N_("44.1kHz")) {
		rate = 44100;
	} else if (sr_str == N_("48kHz")) {
		rate = 48000;
	} else if (sr_str == N_("88.2kHz")) {
		rate = 88200;
	} else if (sr_str == N_("96kHz")) {
		rate = 96000;
	} else if (sr_str == N_("192kHz")) {
		rate = 192000;
	} else {
		rate = session->frame_rate();
	}
		
	if (rate != session->frame_rate()) {
		src_quality_combo.set_sensitive (true);
	} else {
		src_quality_combo.set_sensitive (false);
	}
}

void
ExportDialog::channels_chosen ()
{
	bool mono;

	mono = (channel_count_combo.get_active_text() == _("mono"));

	if (mono) {
		track_selector.get_column(2)->set_visible(false);
		track_selector.get_column(1)->set_title(_("Export"));

		if (session->master_out()) {
			master_selector.get_column(2)->set_visible(false);
			master_selector.get_column(1)->set_title(_("Export"));
		}

	} else {
		track_selector.get_column(2)->set_visible(true);
		track_selector.get_column(1)->set_title(_("Left"));

		if (session->master_out()) {
			master_selector.get_column(2)->set_visible(true);
			master_selector.get_column(1)->set_title(_("Left"));
		}
	}

	fill_lists();
}

void
ExportDialog::fill_lists ()
{
	track_list->clear();
	master_list->clear();
	
	Session::RouteList routes = session->get_routes ();

	for (Session::RouteList::iterator ri = routes.begin(); ri != routes.end(); ++ri) {

		Route* route = (*ri);
		
		if (route->hidden()) {
			continue;
		}

		for (uint32_t i=0; i < route->n_outputs(); ++i) {
			string name;
			if (route->n_outputs() == 1) {
				name = route->name();
			} else {
				name = string_compose("%1: out-%2", route->name(), i+1);
			}

			if (route == session->master_out()) {
				TreeModel::iterator iter = master_list->append();
				TreeModel::Row row = *iter;
				row[exp_cols.output] = name;
				row[exp_cols.left] = false;
				row[exp_cols.right] = false;
				row[exp_cols.port] = route->output (i);
			} else {
				TreeModel::iterator iter = track_list->append();
				TreeModel::Row row = *iter;
				row[exp_cols.output] = name;
				row[exp_cols.left] = false;
				row[exp_cols.right] = false;
				row[exp_cols.port] = route->output (i);
			}
		}
	}
}

gint
ExportDialog::window_closed (GdkEventAny *ignored)
{
	end_dialog ();
	return TRUE;
}
void
ExportDialog::initiate_browse ()
{
	if (file_selector == 0) {
		file_selector = new FileSelection;
		file_selector->set_modal (true);

		file_selector->get_cancel_button()->signal_clicked().connect (bind (mem_fun(*this, &ExportDialog::finish_browse), -1));
		file_selector->get_ok_button()->signal_clicked().connect (bind (mem_fun(*this, &ExportDialog::finish_browse), 1));
	}
	file_selector->show_all ();
}

void
ExportDialog::finish_browse (int status)
{
	if (file_selector) {
		if (status > 0) {
			string result = file_selector->get_filename();
			
			if (result.length()) {
				file_entry.set_text (result);
			}
		}
		file_selector->hide_all();
	}
}

void
ExportDialog::track_selector_button_click ()
{
	if (track_scroll.is_visible ()) {
		track_scroll.hide ();
	} else {
		track_scroll.show_all ();
	}
}
