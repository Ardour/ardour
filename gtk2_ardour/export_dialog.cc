/*
    Copyright (C) 1999-2003 Paul Davis 

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

#include <gtkmm.h>
#include <gtkmm2ext/utils.h>
#include <ardour/export.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/audio_track.h>
#include <ardour/audioregion.h>
#include <ardour/audioengine.h>
#include <ardour/gdither.h>
#include <ardour/utils.h>

#include "export_dialog.h"
#include "check_mark.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "keyboard.h"

#include "i18n.h"

#define FRAME_SHADOW_STYLE Gtk::SHADOW_IN
#define FRAME_NAME "BaseFrame"

GdkPixmap* ExportDialog::check_pixmap = 0;
GdkPixmap* ExportDialog::check_mask = 0;
GdkPixmap* ExportDialog::empty_pixmap = 0;
GdkPixmap* ExportDialog::empty_mask = 0;

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

static const gchar *src_qualities[] = {
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
	  sample_rate_label (_("SAMPLE RATE")),
	  src_quality_label (_("CONVERSION QUALITY")),
	  dither_type_label (_("DITHER TYPE")),
	  cue_file_label (_("CD MARKER FILE TYPE")),
	  channel_count_label (_("CHANNELS")),
	  header_format_label (_("FILE TYPE")),
	  bitdepth_format_label (_("SAMPLE FORMAT")),
	  endian_format_label (_("SAMPLE ENDIANNESS")),
	  cuefile_only_checkbox (_("EXPORT CD MARKER FILE ONLY")),
	  file_frame (_("EXPORT TO FILE")),
	  file_browse_button (_("Browse")),
	  ok_button (_("Export")),
	  track_selector_button (_("Specific tracks ...")),
	  track_selector (3),
	  master_selector (3)
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

	add (vpacker);

	vpacker.set_border_width (10);
	vpacker.set_spacing (10);

	file_selector = 0;
	spec.running = false;

	file_entry.signal_focus_in_event().connect (slot (ARDOUR_UI::generic_focus_in_event));
	file_entry.signal_focus_out_event().connect (slot (ARDOUR_UI::generic_focus_out_event));

	file_entry.set_name ("ExportFileNameEntry");

	master_selector.set_name ("ExportTrackSelector");
	master_selector.set_size_request (-1, 100);
	master_selector.set_column_min_width (0, 100);
	master_selector.set_column_min_width (1, 40);
	master_selector.set_column_auto_resize(1, true);
	master_selector.set_column_min_width (2, 40);
	master_selector.set_column_auto_resize(2, true);
	master_selector.set_column_title (0, _("Output"));
	master_selector.column_titles_show ();
	master_selector.set_selection_mode (GTK_SELECTION_MULTIPLE);
	master_selector.button_press_event.connect (slot (*this, &ExportDialog::master_selector_button_press_event));
	
	track_selector.set_name ("ExportTrackSelector");
	track_selector.set_size_request (-1, 130);
	track_selector.set_column_min_width (0, 100);
	track_selector.set_column_min_width (1, 40);
	track_selector.set_column_auto_resize(1, true);
	track_selector.set_column_min_width (2, 40);
	track_selector.set_column_auto_resize(2, true);
	track_selector.set_column_title (0, _("Track"));
	track_selector.column_titles_show ();
	track_selector.set_selection_mode (GTK_SELECTION_MULTIPLE);
	track_selector.button_press_event.connect (slot (*this, &ExportDialog::track_selector_button_press_event));

	check_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
			gtk_widget_get_colormap(GTK_WIDGET(track_selector.gobj())),
			&check_mask, NULL, (gchar**) check_xpm);
	empty_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
			gtk_widget_get_colormap(GTK_WIDGET(track_selector.gobj())),
			&empty_mask, NULL, (gchar**) empty_xpm);

	progress_bar.set_show_text (false);
	progress_bar.set_orientation (GTK_PROGRESS_LEFT_TO_RIGHT);
	progress_bar.set_name ("ExportProgress");

	format_frame.add (format_table);
	format_frame.set_name (FRAME_NAME);

	track_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	master_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	vpacker.pack_start (file_frame, false, false);

	hpacker.set_spacing (5);
	hpacker.set_border_width (5);
	hpacker.pack_start (format_frame, false, false);

	if (!audio_region) {

		master_scroll.add (master_selector);
		track_scroll.add (track_selector);

		master_scroll.set_size_request (220, 100);
		track_scroll.set_size_request (220, 100);

		
		
		/* we may hide some of these later */
		track_vpacker.pack_start (master_scroll, true, true);
		track_vpacker.pack_start (track_scroll, true, true);
		track_vpacker.pack_start (track_selector_button, false);

		hpacker.pack_start (track_vpacker, true, true);
	}

	vpacker.pack_start (hpacker, true, true);
	
	track_selector_button.set_name ("EditorGTKButton");
	track_selector_button.signal_clicked().connect (slot (*this, &ExportDialog::track_selector_button_click));

	vpacker.pack_start (button_box, false, false);
	vpacker.pack_start (progress_bar, false, false);

	Gtkmm2ext::set_size_request_to_display_given_text (file_entry, X_("Kg/quite/a/reasonable/size/for/files/i/think"), 5, 8);

	file_hbox.set_spacing (5);
	file_hbox.set_border_width (5);
	file_hbox.pack_start (file_entry, true, true);
	file_hbox.pack_start (file_browse_button, false, false);

	file_frame.add (file_hbox);
	file_frame.set_border_width (5);
	file_frame.set_name (FRAME_NAME);

	sample_rate_combo.set_popdown_strings (internationalize(sample_rates));
	src_quality_combo.set_popdown_strings (internationalize (src_qualities));
	dither_type_combo.set_popdown_strings (internationalize (dither_types));
	channel_count_combo.set_popdown_strings (internationalize (channel_strings));
	header_format_combo.set_popdown_strings (internationalize ((const char **) sndfile_header_formats_strings));
	bitdepth_format_combo.set_popdown_strings (internationalize ((const char **) sndfile_bitdepth_formats_strings));
	endian_format_combo.set_popdown_strings (internationalize ((const char **) sndfile_endian_formats_strings));
	cue_file_combo.set_popdown_strings (internationalize (cue_file_types));

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

	Gtkmm2ext::set_size_request_to_display_given_text (*header_format_combo.get_entry(), longest_str.c_str(), 5+FUDGE, 5);

	// TRANSLATORS: "slereg" is "stereo" with ascender and descender substituted
	Gtkmm2ext::set_size_request_to_display_given_text (*channel_count_combo.get_entry(), _("slereg"), 5+FUDGE, 5);

	header_format_combo.set_use_arrows_always (true);
	bitdepth_format_combo.set_use_arrows_always (true);
	endian_format_combo.set_use_arrows_always (true);
	channel_count_combo.set_use_arrows_always (true);
	src_quality_combo.set_use_arrows_always (true);
	dither_type_combo.set_use_arrows_always (true);
	sample_rate_combo.set_use_arrows_always (true);
	cue_file_combo.set_use_arrows_always (true);

	header_format_combo.set_value_in_list (true, false);
	bitdepth_format_combo.set_value_in_list (true, false);
	endian_format_combo.set_value_in_list (true, false);
	channel_count_combo.set_value_in_list (true, false);
	src_quality_combo.set_value_in_list (true, false);
	dither_type_combo.set_value_in_list (true, false);
	sample_rate_combo.set_value_in_list (true, false);
	cue_file_combo.set_value_in_list (true, false);

	header_format_combo.get_entry()->set_editable (false);
	bitdepth_format_combo.get_entry()->set_editable (false);
	endian_format_combo.get_entry()->set_editable (false);
	channel_count_combo.get_entry()->set_editable (false);
	src_quality_combo.get_entry()->set_editable (false);
	dither_type_combo.get_entry()->set_editable (false);
	sample_rate_combo.get_entry()->set_editable (false);
	cue_file_combo.get_entry()->set_editable (false);

	dither_type_label.set_name ("ExportFormatLabel");
	sample_rate_label.set_name ("ExportFormatLabel");
	src_quality_label.set_name ("ExportFormatLabel");
	channel_count_label.set_name ("ExportFormatLabel");
	header_format_label.set_name ("ExportFormatLabel");
	bitdepth_format_label.set_name ("ExportFormatLabel");
	endian_format_label.set_name ("ExportFormatLabel");
	cue_file_label.set_name ("ExportFormatLabel");

	header_format_combo.get_entry()->set_name ("ExportFormatDisplay");
	bitdepth_format_combo.get_entry()->set_name ("ExportFormatDisplay");
	endian_format_combo.get_entry()->set_name ("ExportFormatDisplay");
	channel_count_combo.get_entry()->set_name ("ExportFormatDisplay");
	dither_type_combo.get_entry()->set_name ("ExportFormatDisplay");
	src_quality_combo.get_entry()->set_name ("ExportFormatDisplay");
	sample_rate_combo.get_entry()->set_name ("ExportFormatDisplay");
	cue_file_combo.get_entry()->set_name ("ExportFormatDisplay");

	cuefile_only_checkbox.set_name ("ExportCheckbox");

	format_table.set_homogeneous (true);
	format_table.set_border_width (5);
	format_table.set_col_spacings (5);
	format_table.set_row_spacings (5);

	if (!audio_region) {
		format_table.attach (channel_count_label, 0, 1, 0, 1);
		format_table.attach (channel_count_combo, 0, 1, 1, 2);
	}

	format_table.attach (header_format_label, 1, 2, 0, 1);
	format_table.attach (header_format_combo, 1, 2, 1, 2);

	format_table.attach (bitdepth_format_label, 0, 1, 2, 3);
	format_table.attach (bitdepth_format_combo, 0, 1, 3, 4);

	format_table.attach (endian_format_label, 1, 2, 2, 3);
	format_table.attach (endian_format_combo, 1, 2, 3, 4);

	format_table.attach (sample_rate_label, 0, 1, 4, 5);
	format_table.attach (sample_rate_combo, 0, 1, 5, 6);

	format_table.attach (src_quality_label, 1, 2, 4, 5);
	format_table.attach (src_quality_combo, 1, 2, 5, 6);

	format_table.attach (dither_type_label, 0, 1, 6, 7);
	format_table.attach (dither_type_combo, 0, 1, 7, 8);

	format_table.attach (cue_file_label, 1, 2, 6, 7);
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

	delete_event.connect (slot (*this, &ExportDialog::window_closed));
	ok_button.signal_clicked().connect (slot (*this, &ExportDialog::do_export));
	cancel_button.signal_clicked().connect (slot (*this, &ExportDialog::end_dialog));
	
	file_browse_button.set_name ("EditorGTKButton");
	file_browse_button.signal_clicked().connect (slot (*this, &ExportDialog::initiate_browse));

	channel_count_combo.get_popwin()->unmap_event.connect (slot (*this, &ExportDialog::channels_chosen));
	bitdepth_format_combo.get_popwin()->unmap_event.connect (slot (*this, &ExportDialog::bitdepth_chosen));
	header_format_combo.get_popwin()->unmap_event.connect (slot (*this, &ExportDialog::header_chosen));
	sample_rate_combo.get_popwin()->unmap_event.connect (slot (*this, &ExportDialog::sample_rate_chosen));
	cue_file_combo.get_popwin()->unmap_event.connect (slot (*this, &ExportDialog::cue_file_type_chosen));
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
	session->going_away.connect (slot (*this, &Window::hide_all));

	switch (session->frame_rate()) {
	case 22050:
		sample_rate_combo.get_entry()->set_text (N_("22.05kHz"));
		break;
	case 44100:
		sample_rate_combo.get_entry()->set_text (N_("44.1kHz"));
		break;
	case 48000:
		sample_rate_combo.get_entry()->set_text (N_("48kHz"));
		break;
	case 88200:
		sample_rate_combo.get_entry()->set_text (N_("88.2kHz"));
		break;
	case 96000:
		sample_rate_combo.get_entry()->set_text (N_("96kHz"));
		break;
	case 192000:
		sample_rate_combo.get_entry()->set_text (N_("192kHz"));
		break;
	default:
		sample_rate_combo.get_entry()->set_text (N_("44.1kHz"));
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
			sample_rate_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("src_quality"))) != 0) {
			src_quality_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("dither_type"))) != 0) {
			dither_type_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("channel_count"))) != 0) {
			channel_count_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("header_format"))) != 0) {
			header_format_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("bitdepth_format"))) != 0) {
			bitdepth_format_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("endian_format"))) != 0) {
			endian_format_combo.get_entry()->set_text(prop->value());
		}
		if ((prop = node->property (X_("filename"))) != 0) {
			file_entry.set_text(prop->value());
		}
		if ((prop = node->property (X_("cue_file_type"))) != 0) {
		        cue_file_combo.get_entry()->set_text(prop->value());
		}
	}

	header_chosen (0);
	bitdepth_chosen(0);
	channels_chosen(0);
	sample_rate_chosen(0);

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
			if (channel_count_combo.get_entry()->get_text() == _("mono")) {
				nchns = 1;
			} else {
				nchns = 2;
			}

			for (uint32_t r = 0; r < session->master_out()->n_outputs(); ++r) {
				if (nchns == 2) {
					if (r % 2) {
						master_selector.cell (r, 2).set_pixmap (check_pixmap, check_mask);
					} else {
						master_selector.cell (r, 1).set_pixmap (check_pixmap, check_mask);
					}
				} else {
					master_selector.cell (r, 1).set_pixmap (check_pixmap, check_mask);
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
	CList_Helpers::RowIterator ri = track_selector.rows().begin();
	uint32_t n = 0;
	for (XMLNodeIterator it = track_list.begin(); it != track_list.end(); ++it, ++ri, ++n) {
		if (ri == track_selector.rows().end()) {
			break;
		}

		XMLNode* track = *it;

		if ((prop = track->property(X_("channel1"))) != 0) {
			if (prop->value() == X_("on")) {
				track_selector.cell (n,1).set_pixmap (check_pixmap, check_mask);
			} else {
				track_selector.cell (n,1).set_pixmap (empty_pixmap, empty_mask);
			}
		}

		if ((prop = track->property(X_("channel2"))) != 0) {
			if (prop->value() == X_("on")) {
				track_selector.cell (n,2).set_pixmap (check_pixmap, check_mask);
			} else {
				track_selector.cell (n,2).set_pixmap (empty_pixmap, empty_mask);
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

	node->add_property(X_("sample_rate"), sample_rate_combo.get_entry()->get_text());
	node->add_property(X_("src_quality"), src_quality_combo.get_entry()->get_text());
	node->add_property(X_("dither_type"), dither_type_combo.get_entry()->get_text());
	node->add_property(X_("channel_count"), channel_count_combo.get_entry()->get_text());
	node->add_property(X_("header_format"), header_format_combo.get_entry()->get_text());
	node->add_property(X_("bitdepth_format"), bitdepth_format_combo.get_entry()->get_text());
	node->add_property(X_("endian_format"), endian_format_combo.get_entry()->get_text());
	node->add_property(X_("filename"), file_entry.get_text());
	node->add_property(X_("cue_file_type"), cue_file_combo.get_entry()->get_text());

	XMLNode* tracks = new XMLNode(X_("Tracks"));

	uint32_t n = 0;
	for (CList_Helpers::RowIterator ri = track_selector.rows().begin(); ri != track_selector.rows().end(); ++ri, ++n) {
		XMLNode* track = new XMLNode(X_("Track"));

		Gdk::Pixmap left_pixmap = track_selector.cell (n, 1).get_pixmap ();
		track->add_property(X_("channel1"), left_pixmap.gobj() == check_pixmap ? X_("on") : X_("off"));

		Gdk::Pixmap right_pixmap = track_selector.cell (n, 2).get_pixmap ();
		track->add_property(X_("channel2"), right_pixmap.gobj() == check_pixmap ? X_("on") : X_("off"));				

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
	progress_bar.set_percentage (spec.progress);
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
		error << compose(_("Editor: cannot open \"%1\" as export file for CD toc file"), filepath) << endmsg;
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
			if ((*i)->cd_info.find("composer") != (*i)->cd_info.end()) {
			  out  << "     COMPOSER \"" << (*i)->cd_info["composer"] << "\"" << endl;
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
		error << compose(_("Editor: cannot open \"%1\" as export file for CD cue file"), filepath) << endmsg;
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

	if ((header_format_combo.get_entry()->get_text() == N_("WAV"))) {
		  out << "FILE " << path  << " WAVE" << endl;
	} else {
		  out << "FILE " << path  << ' ' << (header_format_combo.get_entry()->get_text()) << endl;
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
		      
		      if ((*i)->cd_info.find("composer") != (*i)->cd_info.end()) {
			out << "SONGWRITER \"" << (*i)->cd_info["composer"]  << "\"" << endl;
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
	using namespace CList_Helpers;

	ok_button.set_sensitive(false);
	save_state();

	if (cue_file_combo.get_entry()->get_text () != _("None")) {
		do_export_cd_markers (file_entry.get_text(), cue_file_combo.get_entry()->get_text ());
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
	
	if (channel_count_combo.get_entry()->get_text() == _("mono")) {
		spec.channels = 1;
	} else {
		spec.channels = 2;
	}

	spec.format = 0;

	spec.format |= sndfile_header_format_from_string (header_format_combo.get_entry()->get_text ());
	
	if ((spec.format & SF_FORMAT_WAV) == 0) {
		/* RIFF/WAV specifies endianess */
		spec.format |= sndfile_endian_format_from_string (endian_format_combo.get_entry()->get_text ());
	}

	spec.format |= sndfile_bitdepth_format_from_string (bitdepth_format_combo.get_entry()->get_text ());

	string sr_str = sample_rate_combo.get_entry()->get_text();
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
	
	string src_str = src_quality_combo.get_entry()->get_text();
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

	string dither_str = dither_type_combo.get_entry()->get_text();
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

		uint32_t n = 0;
		uint32_t chan=0;
		Port *last_port = 0;
		
		for (RowIterator ri = master_selector.rows().begin(); ri != master_selector.rows().end(); ++ri, ++n) {
			
			Port* port = static_cast<Port*> ((*ri)->get_data ());
			
			if (last_port != port) {
				chan = 0;
			}
			
			Gdk::Pixmap left_pixmap = master_selector.cell (n, 1).get_pixmap ();
			
			if (left_pixmap.gobj() == check_pixmap) {
				spec.port_map[0].push_back (std::pair<Port*,uint32_t>(port, chan));
			} 
			
			if (spec.channels == 2) {
				
				Gdk::Pixmap right_pixmap = master_selector.cell (n, 2).get_pixmap ();
				
				if (right_pixmap.gobj() == check_pixmap) {
					spec.port_map[1].push_back (std::pair<Port*,uint32_t>(port, chan));
				}
				
			}
		}

		chan = 0;
		n = 0;

		for (RowIterator ri = track_selector.rows().begin(); ri != track_selector.rows().end(); ++ri, ++n) {
			
			Port* port = static_cast<Port*> ((*ri)->get_data ());
			
			if (last_port != port) {
				chan = 0;
			}
			
			Gdk::Pixmap left_pixmap = track_selector.cell (n, 1).get_pixmap ();
			
			if (left_pixmap.gobj() == check_pixmap) {
				spec.port_map[0].push_back (std::pair<Port*,uint32_t>(port, chan));
			} 
			
			if (spec.channels == 2) {
				
				Gdk::Pixmap right_pixmap = track_selector.cell (n, 2).get_pixmap ();
				
				if (right_pixmap.gobj() == check_pixmap) {
					spec.port_map[1].push_back (std::pair<Port*,uint32_t>(port, chan));
				}
				
			}
			
			last_port = port;
			++chan;
		}
	}

	progress_connection = Main::timeout.connect (slot (*this, &ExportDialog::progress_timeout), 100);
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
	
	progress_bar.set_percentage (0);
	cancel_label.set_text (_("Cancel"));

	show_all ();

	if (session->master_out()) {
		track_scroll.hide ();
	} else {
		master_scroll.hide ();
		track_selector_button.hide ();
	}
}

gint
ExportDialog::header_chosen (GdkEventAny* ignored)
{
	if (sndfile_header_format_from_string (header_format_combo.get_entry()->get_text ()) == SF_FORMAT_WAV) {
		endian_format_combo.set_sensitive (false);
	} else {
		endian_format_combo.set_sensitive (true);
	}
	return FALSE;
}

gint
ExportDialog::bitdepth_chosen (GdkEventAny* ignored)
{
	int format = sndfile_bitdepth_format_from_string (bitdepth_format_combo.get_entry()->get_text ());	
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

	return FALSE;
}

gint
ExportDialog::cue_file_type_chosen (GdkEventAny* ignored)
{
	if (cue_file_combo.get_entry()->get_text () != "None") {
		cuefile_only_checkbox.set_sensitive (true);
	} else {
		cuefile_only_checkbox.set_active (false);
		cuefile_only_checkbox.set_sensitive (false);
	}
       	return FALSE;
}

gint
ExportDialog::sample_rate_chosen (GdkEventAny* ignored)
{
	string sr_str = sample_rate_combo.get_entry()->get_text();
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

	return FALSE;
}

gint
ExportDialog::channels_chosen (GdkEventAny* ignored)
{
	bool mono;

	mono = (channel_count_combo.get_entry()->get_text() == _("mono"));

	if (mono) {
		track_selector.set_column_visibility (2, false);
		track_selector.set_column_title (1, _("Export"));

		if (session->master_out()) {
			master_selector.set_column_visibility (2, false);
			master_selector.set_column_title (1, _("Export"));
		}

	} else {
		track_selector.set_column_visibility (2, true);
		track_selector.set_column_title (1, _("Left"));
		track_selector.set_column_title (2, _("Right"));

		if (session->master_out()) {
			master_selector.set_column_visibility (2, true);
			master_selector.set_column_title (1, _("Left"));
			master_selector.set_column_title (2, _("Right"));
		}
	}

	track_selector.column_titles_show ();
	track_selector.clear ();
	master_selector.column_titles_show ();
	master_selector.clear ();
	
	Session::RouteList routes = session->get_routes ();

	for (Session::RouteList::iterator ri = routes.begin(); ri != routes.end(); ++ri) {

		Route* route = (*ri);
		
		if (route->hidden()) {
			continue;
		}

		for (uint32_t i=0; i < route->n_outputs(); ++i) {
			
			list<string> stupid_list;

			if (route->n_outputs() == 1) {
				stupid_list.push_back (route->name());
			} else {
				stupid_list.push_back (compose("%1: out-%2", route->name(), i+1));
			}

			stupid_list.push_back ("");
			stupid_list.push_back ("");

			if (route == session->master_out()) {
				master_selector.rows().push_back (stupid_list);
				CList_Helpers::Row row = master_selector.rows().back();
				row.set_data (route->output (i));
				master_selector.cell (row.get_row_num(), 1).set_pixmap (empty_pixmap, empty_mask);
				master_selector.cell (row.get_row_num(), 2).set_pixmap (empty_pixmap, empty_mask);
			} else {
				track_selector.rows().push_back (stupid_list);
				CList_Helpers::Row row = track_selector.rows().back();
				row.set_data (route->output (i));
				track_selector.cell (row.get_row_num(), 1).set_pixmap (empty_pixmap, empty_mask);
				track_selector.cell (row.get_row_num(), 2).set_pixmap (empty_pixmap, empty_mask);
			}
		}
	}
	
	track_selector.select_all ();
	master_selector.select_all ();

	return FALSE;
}

gint
ExportDialog::track_selector_button_press_event (GdkEventButton* ev)
{
	gint row, col;

	if (track_selector.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) == 0) {
		return FALSE;
	}

	gtk_signal_emit_stop_by_name (GTK_OBJECT(track_selector.gobj()), "button_press_event");
	
	Gdk::Pixmap pixmap = track_selector.cell (row,col).get_pixmap ();

	if (col != 0) {
		if (pixmap.gobj() == check_pixmap) {
			track_selector.cell (row,col).set_pixmap (empty_pixmap, empty_mask);
		} else {
			track_selector.cell (row,col).set_pixmap (check_pixmap, check_mask);
		}
	}
	
	return TRUE;
}

gint
ExportDialog::master_selector_button_press_event (GdkEventButton* ev)
{
	gint row, col;

	if (master_selector.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) == 0) {
		return FALSE;
	}

	gtk_signal_emit_stop_by_name (GTK_OBJECT(master_selector.gobj()), "button_press_event");
	
	if (col != 0) {
		Gdk::Pixmap pixmap = master_selector.cell (row,col).get_pixmap ();
		
		if (pixmap.gobj() == check_pixmap) {
			master_selector.cell (row,col).set_pixmap (empty_pixmap, empty_mask);
		} else {
			master_selector.cell (row,col).set_pixmap (check_pixmap, check_mask);
		}
	}
	
	return TRUE;
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

		file_selector->get_cancel_button()-.signal_clicked().connect (bind (slot (*this, &ExportDialog::finish_browse), -1));
		file_selector->get_ok_button()-.signal_clicked().connect (bind (slot (*this, &ExportDialog::finish_browse), 1));
		file_selector->map_event.connect (bind (slot (*this, &ExportDialog::change_focus_policy), true));
		file_selector->unmap_event.connect (bind (slot (*this, &ExportDialog::change_focus_policy), false));
	}
	file_selector->show_all ();
}

gint
ExportDialog::change_focus_policy (GdkEventAny *ev, bool yn)
{
	Keyboard::the_keyboard().allow_focus (yn);
	return FALSE;
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
