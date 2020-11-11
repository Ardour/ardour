/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtkmm/frame.h>
#include <gtkmm/stock.h>

#include "ardour/export_format_specification.h"
#include "ardour/session.h"

#include "widgets/tooltips.h"

#include "export_format_dialog.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

ExportFormatDialog::ExportFormatDialog (FormatPtr format, bool new_dialog)
	: ArdourDialog (new_dialog ? _("New Export Format Profile") : _("Edit Export Format Profile"))

	, format (format)
	, manager (format)
	, original_state (format->get_state ())

	, applying_changes_from_engine (0)

	, name_label (_("Label: "), Gtk::ALIGN_LEFT)
	, name_generated_part ("", Gtk::ALIGN_LEFT)

	, normalize_checkbox (_("Normalize:"))
	, normalize_peak_rb (_("Peak"))
	, normalize_loudness_rb (_("Loudness"))
	, normalize_dbfs_adjustment ( 0.00, -90.00, 0.00, 0.1, 0.2)
	, normalize_lufs_adjustment (-23.0, -90.00, 0.00, 0.1, 1.0)
	, normalize_dbtp_adjustment ( -1.0, -90.00, 0.00, 0.1, 0.2)

	, normalize_dbfs_label (_("dBFS"), Gtk::ALIGN_LEFT)
	, normalize_lufs_label (_("LUFS"), Gtk::ALIGN_LEFT)
	, normalize_dbtp_label (_("dBTP"), Gtk::ALIGN_LEFT)

	, silence_table (2, 4)
	, trim_start_checkbox (_("Trim silence at start"))
	, silence_start_checkbox (_("Add silence at start:"))
	, silence_start_clock ("silence_start", true, "", true, false, true)

	, trim_end_checkbox (_("Trim silence at end"))
	, silence_end_checkbox (_("Add silence at end:"))
	, silence_end_clock ("silence_end", true, "", true, false, true)

	, command_label (_("Command to run post-export\n(%f=file path, %d=directory, %b=basename, see tooltip for more):"), Gtk::ALIGN_LEFT)

	, format_table (3, 4)
	, compatibility_label (_("Compatibility"), Gtk::ALIGN_LEFT)
	, quality_label (_("Quality"), Gtk::ALIGN_LEFT)
	, format_label (_("File format"), Gtk::ALIGN_LEFT)
	, sample_rate_label (_("Sample rate"), Gtk::ALIGN_LEFT)

	, src_quality_label (_("Sample rate conversion quality:"), Gtk::ALIGN_RIGHT)

	/* Watermarking */
	, watermark_heading (_("Preview / Watermark"), Gtk::ALIGN_LEFT)

	, demo_noise_mode_label (_("Mode:"), Gtk::ALIGN_LEFT)
	, demo_noise_level_label (_("Noise Level:"), Gtk::ALIGN_LEFT)
	, demo_noise_dbfs_unit (_("dBFS"), Gtk::ALIGN_LEFT)

	, demo_noise_dbfs_adjustment (-20.0, -90.00, -6.00, 1, 5)
	, demo_noise_dbfs_spinbutton (demo_noise_dbfs_adjustment, 1, 0)

	/* Changing encoding options from here on */
	, encoding_options_label ("", Gtk::ALIGN_LEFT)

	/* Changing encoding options from here on */
	, sample_format_label (_("Sample Format"), Gtk::ALIGN_LEFT)
	, dither_label (_("Dithering"), Gtk::ALIGN_LEFT)

	, with_cue (_("Create CUE file for disk-at-once CD/DVD creation"))
	, with_toc (_("Create TOC file for disk-at-once CD/DVD creation"))
	, with_mp4chaps (_("Create chapter mark file for MP4 chapter marks"))
	, tag_checkbox (_("Tag file with session's metadata"))
{
	/* Name, new and remove */

	name_hbox.pack_start (name_label, false, false, 0);
	name_hbox.pack_start (name_entry, false, false, 0);
	name_hbox.pack_start (name_generated_part, true, true, 0);
	name_entry.set_width_chars (20);
	update_description ();
	manager.DescriptionChanged.connect (
	    *this, invalidator (*this),
	    boost::bind (&ExportFormatDialog::update_description, this), gui_context ());

	/* Normalize */

	normalize_tp_limiter.append_text (_("limit to"));
	normalize_tp_limiter.append_text (_("constrain to"));

	Gtk::RadioButtonGroup normalize_group = normalize_loudness_rb.get_group ();
	normalize_peak_rb.set_group (normalize_group);

	normalize_table.set_row_spacings (4);
	normalize_table.set_col_spacings (4);

	normalize_table.attach (normalize_checkbox,        0, 1, 0, 1);
	normalize_table.attach (normalize_peak_rb,         1, 2, 0, 1);
	normalize_table.attach (normalize_dbfs_spinbutton, 2, 3, 0, 1);
	normalize_table.attach (normalize_dbfs_label,      3, 4, 0, 1);

	normalize_table.attach (normalize_loudness_rb,     1, 2, 1, 2);
	normalize_table.attach (normalize_lufs_spinbutton, 2, 3, 1, 2);
	normalize_table.attach (normalize_lufs_label,      3, 4, 1, 2);
	normalize_table.attach (normalize_tp_limiter,      4, 5, 1, 2);
	normalize_table.attach (normalize_dbtp_spinbutton, 5, 6, 1, 2);
	normalize_table.attach (normalize_dbtp_label,      6, 7, 1, 2);

	ArdourWidgets::set_tooltip (normalize_loudness_rb,
	                            _("Normalize to EBU-R128 LUFS target loudness without exceeding the given true-peak limit. EBU-R128 normalization is only available for mono and stereo targets, true-peak works for any channel layout."));

	normalize_dbfs_spinbutton.configure (normalize_dbfs_adjustment, 0.1, 2);
	normalize_lufs_spinbutton.configure (normalize_lufs_adjustment, 0.1, 2);
	normalize_dbtp_spinbutton.configure (normalize_dbtp_adjustment, 0.1, 2);

	/* Silence  */

	silence_table.set_row_spacings (6);
	silence_table.set_col_spacings (12);

	silence_table.attach (normalize_table, 0, 3, 0, 1);

	silence_table.attach (trim_start_checkbox, 0, 1, 1, 2);
	silence_table.attach (silence_start_checkbox, 1, 2, 1, 2);
	silence_table.attach (silence_start_clock, 2, 3, 1, 2);

	silence_table.attach (trim_end_checkbox, 0, 1, 2, 3);
	silence_table.attach (silence_end_checkbox, 1, 2, 2, 3);
	silence_table.attach (silence_end_clock, 2, 3, 2, 3);

	/* post export */
	command_box.pack_start (command_label, false, false);
	command_box.pack_start (command_entry, false, false, 6);

	ArdourWidgets::set_tooltip (command_entry,
	                            _(
	                              "%a Artist name\n"
	                              "%b File's base-name\n"
	                              "%c Copyright\n"
	                              "%d File's directory\n"
	                              "%f File's full absolute path\n"
	                              "%l Lyricist\n"
	                              "%n Session name\n"
	                              "%o Conductor\n"
	                              "%t Title\n"
	                              "%z Organization\n"
	                              "%A Album\n"
	                              "%C Comment\n"
	                              "%E Engineer\n"
	                              "%G Genre\n"
	                              "%L Total track count\n"
	                              "%M Mixer\n"
	                              "%N Timespan name\n"
	                              "%O Composer\n"
	                              "%P Producer\n"
	                              "%S Disc subtitle\n"
	                              "%T Track number\n"
	                              "%Y Year\n"
	                              "%Z Country"));

	/* Format table */

	init_format_table ();

	/* SRC */

	src_quality_box.pack_start (src_quality_label, true, true);
	src_quality_box.pack_start (src_quality_combo, false, false);

	/* Watermark */

	watermark_options_table.attach (watermark_heading,          0, 3, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	watermark_options_table.attach (demo_noise_mode_label,      0, 1, 1, 2, Gtk::FILL, Gtk::SHRINK);
	watermark_options_table.attach (demo_noise_combo,           1, 3, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	watermark_options_table.attach (demo_noise_level_label,     0, 1, 2, 3, Gtk::FILL, Gtk::SHRINK);
	watermark_options_table.attach (demo_noise_dbfs_spinbutton, 1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK);
	watermark_options_table.attach (demo_noise_dbfs_unit,       2, 3, 2, 3, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);

	/* Encoding options */

	init_encoding_option_widgets ();

	encoding_options_table.set_spacings (1);

	encoding_options_vbox.pack_start (encoding_options_label, false, false, 0);
	encoding_options_vbox.pack_start (encoding_options_table, false, false, 12);
	encoding_options_vbox.pack_end (src_quality_box, false, false);

	Pango::AttrList  bold;
	Pango::Attribute b = Pango::Attribute::create_attr_weight (Pango::WEIGHT_BOLD);
	bold.insert (b);
	encoding_options_label.set_attributes (bold);

	/* Codec options */

	codec_quality_list = Gtk::ListStore::create (codec_quality_cols);
	codec_quality_combo.set_model (codec_quality_list);
	codec_quality_combo.pack_start (codec_quality_cols.label);
	//codec_quality_combo.set_active (0);

	/* Buttons */

	revert_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	revert_button->signal_clicked ().connect (sigc::mem_fun (*this, &ExportFormatDialog::revert));
	close_button = add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_APPLY);
	close_button->set_sensitive (false);
	close_button->signal_clicked ().connect (sigc::mem_fun (*this, &ExportFormatDialog::end_dialog));
	manager.CompleteChanged.connect (*this, invalidator (*this), boost::bind (&Gtk::Button::set_sensitive, close_button, _1), gui_context ());

	with_cue.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_with_cue));
	with_toc.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_with_toc));
	with_mp4chaps.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_with_mp4chaps));
	command_entry.signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_command));

	metadata_table.attach (tag_checkbox,  0, 1, 0, 1);
	metadata_table.attach (with_mp4chaps, 0, 1, 1, 2);
	metadata_table.attach (with_cue,      1, 2, 0, 1);
	metadata_table.attach (with_toc,      1, 2, 1, 2);

	/* Load state before hooking up the rest of the signals */

	load_state (format);

	/* Name entry */

	name_entry.signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_name));

	/* Normalize, silence and src_quality signals */

	trim_start_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_trim_start_selection));
	trim_end_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_trim_end_selection));

	normalize_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_peak_rb.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_loudness_rb.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_tp_limiter.signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_dbfs_spinbutton.signal_value_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_lufs_spinbutton.signal_value_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_dbtp_spinbutton.signal_value_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));

	silence_start_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_start_selection));
	silence_start_clock.ValueChanged.connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_start_selection));

	silence_end_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_end_selection));
	silence_end_clock.ValueChanged.connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_end_selection));

	src_quality_combo.signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_src_quality_selection));
	codec_quality_combo.signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_codec_quality_selection));

	watermark_heading.set_attributes (bold);
	demo_noise_combo.signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_demo_noise_selection));
	demo_noise_dbfs_spinbutton.signal_value_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_demo_noise_selection));

	/* Format table signals */

	Gtk::CellRendererToggle* toggle = dynamic_cast<Gtk::CellRendererToggle*> (compatibility_view.get_column_cell_renderer (0));
	toggle->signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_compatibility_selection));
	compatibility_select_connection = compatibility_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::prohibit_compatibility_selection));

	quality_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_quality_selection));
	format_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_format_selection));
	sample_rate_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_sample_rate_selection));

	/* Encoding option signals */

	sample_format_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_sample_format_selection));
	dither_type_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_dither_type_selection));

	tag_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_tagging_selection));

	/* Pack containers in dialog */
	Gtk::Frame* f;
	Gtk::Table* g = manage (new Gtk::Table);
	g->set_spacings (6);
	get_vbox ()->pack_start (*g);

	g->attach (name_hbox, 0, 2, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);

	f = Gtk::manage (new Gtk::Frame (_("Pre Process")));
	f->add (silence_table);
	g->attach (*f, 0, 1, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	f = Gtk::manage (new Gtk::Frame (_("Watermark")));
	f->add (watermark_options_table);
	g->attach (*f, 1, 2, 1, 2, Gtk::EXPAND | Gtk::FILL, Gtk::FILL);

	f = Gtk::manage (new Gtk::Frame (_("Format")));
	f->add (format_table);
	g->attach (*f, 0, 1, 2, 3);

	f = Gtk::manage (new Gtk::Frame (_("Encoding")));
	f->add (encoding_options_vbox);
	g->attach (*f, 1, 2, 2, 3);

	f = Gtk::manage (new Gtk::Frame (_("Metadata")));
	f->add (metadata_table);
	g->attach (*f, 0, 2, 3, 4, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);

	f = Gtk::manage (new Gtk::Frame (_("Post Export")));
	f->add (command_box);
	g->attach (*f, 0, 2, 4, 5, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);

	/* Finalize */

	show_all_children ();
	update_normalize_sensitivity ();
}

ExportFormatDialog::~ExportFormatDialog ()
{
}

void
ExportFormatDialog::revert ()
{
	++applying_changes_from_engine;

	format->set_state (original_state);
	load_state (format);

	--applying_changes_from_engine;
}

void
ExportFormatDialog::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);
	silence_start_clock.set_session (s);
	silence_end_clock.set_session (s);

	if (!_session) {
		return;
	}

	update_clock (silence_start_clock, silence_start);
	update_clock (silence_end_clock, silence_end);

	/* Select native samplerate if no selection is yet made */

	if (sample_rate_view.get_selection ()->count_selected_rows () == 0) {
		Gtk::ListStore::Children::iterator it;
		for (it = sample_rate_list->children ().begin (); it != sample_rate_list->children ().end (); ++it) {
			if ((samplecnt_t) (*it)->get_value (sample_rate_cols.ptr)->rate == _session->nominal_sample_rate ()) {
				sample_rate_view.get_selection ()->select (it);
				break;
			}
		}
	}
}

void
ExportFormatDialog::load_state (FormatPtr spec)
{
	name_entry.set_text (spec->name ());

	normalize_checkbox.set_active (spec->normalize ());
	normalize_peak_rb.set_active (!spec->normalize_loudness ());
	normalize_tp_limiter.set_active (spec->use_tp_limiter () ? 0 : 1);
	normalize_loudness_rb.set_active (spec->normalize_loudness ());
	normalize_dbfs_spinbutton.set_value (spec->normalize_dbfs ());
	normalize_lufs_spinbutton.set_value (spec->normalize_lufs ());
	normalize_dbtp_spinbutton.set_value (spec->normalize_dbtp ());

	trim_start_checkbox.set_active (spec->trim_beginning ());
	silence_start = spec->silence_beginning_time ();
	silence_start_checkbox.set_active (spec->silence_beginning_time ().not_zero ());

	trim_end_checkbox.set_active (spec->trim_end ());
	silence_end = spec->silence_end_time ();
	silence_end_checkbox.set_active (spec->silence_end_time ().not_zero ());

	with_cue.set_active (spec->with_cue ());
	with_toc.set_active (spec->with_toc ());
	with_mp4chaps.set_active (spec->with_mp4chaps ());

	demo_noise_combo.set_active (0);
	for (Gtk::ListStore::Children::iterator it = demo_noise_list->children ().begin (); it != demo_noise_list->children ().end (); ++it) {
		if (it->get_value (demo_noise_cols.interval) == spec->demo_noise_interval () && it->get_value (demo_noise_cols.duration) == spec->demo_noise_duration ()) {
			demo_noise_combo.set_active (it);
			break;
		}
	}

	demo_noise_dbfs_spinbutton.set_value (spec->demo_noise_level ());
	update_demo_noise_sensitivity ();

	for (Gtk::ListStore::Children::iterator it = src_quality_list->children ().begin (); it != src_quality_list->children ().end (); ++it) {
		if (it->get_value (src_quality_cols.id) == spec->src_quality ()) {
			src_quality_combo.set_active (it);
			break;
		}
	}

	for (Gtk::ListStore::Children::iterator it = codec_quality_list->children ().begin (); it != codec_quality_list->children ().end (); ++it) {
		if (it->get_value (codec_quality_cols.quality) == spec->codec_quality ()) {
			codec_quality_combo.set_active (it);
			break;
		}
	}

	for (Gtk::ListStore::Children::iterator it = format_list->children ().begin (); it != format_list->children ().end (); ++it) {
		boost::shared_ptr<ARDOUR::ExportFormat> format_in_list = it->get_value (format_cols.ptr);
		if (format_in_list->get_format_id () == spec->format_id () &&
		    // BWF has the same format id with wav, so we need to check this.
		    format_in_list->has_broadcast_info () == spec->has_broadcast_info ()) {
			format_in_list->set_selected (true);
			break;
		}
	}

	for (Gtk::ListStore::Children::iterator it = sample_rate_list->children ().begin (); it != sample_rate_list->children ().end (); ++it) {
		if (it->get_value (sample_rate_cols.ptr)->rate == spec->sample_rate ()) {
			it->get_value (sample_rate_cols.ptr)->set_selected (true);
			break;
		}
	}

	if (spec->sample_format ()) {
		for (Gtk::ListStore::Children::iterator it = sample_format_list->children ().begin (); it != sample_format_list->children ().end (); ++it) {
			if (it->get_value (sample_format_cols.ptr)->format == spec->sample_format ()) {
				it->get_value (sample_format_cols.ptr)->set_selected (true);
				break;
			}
		}

		for (Gtk::ListStore::Children::iterator it = dither_type_list->children ().begin (); it != dither_type_list->children ().end (); ++it) {
			if (it->get_value (dither_type_cols.ptr)->type == spec->dither_type ()) {
				it->get_value (dither_type_cols.ptr)->set_selected (true);
				break;
			}
		}
	}

	update_normalize_sensitivity ();
	tag_checkbox.set_active (spec->tag ());
	command_entry.set_text (spec->command ());
}

void
ExportFormatDialog::init_format_table ()
{
	format_table.set_spacings (1);

	format_table.attach (compatibility_label, 0, 1, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	format_table.attach (quality_label,       1, 2, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	format_table.attach (format_label,        2, 3, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	format_table.attach (sample_rate_label,   3, 4, 0, 1, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);

	format_table.attach (compatibility_view,  0, 1, 1, 2);
	format_table.attach (quality_view,        1, 2, 1, 2);
	format_table.attach (format_view,         2, 3, 1, 2);
	format_table.attach (sample_rate_view,    3, 4, 1, 2);

	compatibility_view.set_headers_visible (false);
	quality_view.set_headers_visible (false);
	format_view.set_headers_visible (false);
	sample_rate_view.set_headers_visible (false);

	/*** Table entries ***/

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row      row;

	/* Compatibilities */

	compatibility_list = Gtk::ListStore::create (compatibility_cols);
	compatibility_view.set_model (compatibility_list);

	ExportFormatManager::CompatList const& compat_list = manager.get_compatibilities ();

	for (ExportFormatManager::CompatList::const_iterator it = compat_list.begin (); it != compat_list.end (); ++it) {
		iter = compatibility_list->append ();
		row  = *iter;

		row[compatibility_cols.ptr]      = *it;
		row[compatibility_cols.selected] = false;
		row[compatibility_cols.label]    = (*it)->name ();

		WeakCompatPtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_compatibility_selection, this, _1, ptr), gui_context ());
	}

	compatibility_view.append_column_editable ("", compatibility_cols.selected);

	Gtk::CellRendererText* text_renderer = Gtk::manage (new Gtk::CellRendererText);
	text_renderer->property_editable ()  = false;

	Gtk::TreeView::Column* column = compatibility_view.get_column (0);
	column->pack_start (*text_renderer);
	column->add_attribute (text_renderer->property_text (), compatibility_cols.label);

	/* Qualities */

	quality_list = Gtk::ListStore::create (quality_cols);
	quality_view.set_model (quality_list);

	ExportFormatManager::QualityList const& qualities = manager.get_qualities ();

	for (ExportFormatManager::QualityList::const_iterator it = qualities.begin (); it != qualities.end (); ++it) {
		iter = quality_list->append ();
		row  = *iter;

		row[quality_cols.ptr]   = *it;
		row[quality_cols.color] = "white";
		row[quality_cols.label] = (*it)->name ();

		WeakQualityPtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_quality_selection, this, _1, ptr), gui_context ());
		(*it)->CompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_quality_compatibility, this, _1, ptr), gui_context ());
	}

	quality_view.append_column ("", quality_cols.label);

	/* Formats */

	format_list = Gtk::ListStore::create (format_cols);
	format_view.set_model (format_list);

	ExportFormatManager::FormatList const& formats = manager.get_formats ();

	for (ExportFormatManager::FormatList::const_iterator it = formats.begin (); it != formats.end (); ++it) {
		iter = format_list->append ();
		row  = *iter;

		row[format_cols.ptr]   = *it;
		row[format_cols.color] = "white";
		row[format_cols.label] = (*it)->name ();

		WeakFormatPtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_format_selection, this, _1, ptr), gui_context ());
		(*it)->CompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_format_compatibility, this, _1, ptr), gui_context ());

		/* Encoding options */

		boost::shared_ptr<HasSampleFormat> hsf;

		if ((hsf = boost::dynamic_pointer_cast<HasSampleFormat> (*it))) {
			hsf->SampleFormatSelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_format_selection, this, _1, _2), gui_context ());
			hsf->SampleFormatCompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_format_compatibility, this, _1, _2), gui_context ());

			hsf->DitherTypeSelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_dither_type_selection, this, _1, _2), gui_context ());
			hsf->DitherTypeCompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_dither_type_compatibility, this, _1, _2), gui_context ());
		}
	}

	format_view.append_column ("", format_cols.label);

	/* Sample Rates */

	sample_rate_list = Gtk::ListStore::create (sample_rate_cols);
	sample_rate_view.set_model (sample_rate_list);

	ExportFormatManager::SampleRateList const& rates = manager.get_sample_rates ();

	for (ExportFormatManager::SampleRateList::const_iterator it = rates.begin (); it != rates.end (); ++it) {
		iter = sample_rate_list->append ();
		row  = *iter;

		row[sample_rate_cols.ptr]   = *it;
		row[sample_rate_cols.color] = "white";
		row[sample_rate_cols.label] = (*it)->name ();

		WeakSampleRatePtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_rate_selection, this, _1, ptr), gui_context ());
		(*it)->CompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_rate_compatibility, this, _1, ptr), gui_context ());
	}

	sample_rate_view.append_column ("", sample_rate_cols.label);

	/* Color rendering */

	Gtk::TreeViewColumn*   label_col;
	Gtk::CellRendererText* renderer;

	label_col = quality_view.get_column (0);
	renderer  = dynamic_cast<Gtk::CellRendererText*> (quality_view.get_column_cell_renderer (0));
	label_col->add_attribute (renderer->property_foreground (), quality_cols.color);

	label_col = format_view.get_column (0);
	renderer  = dynamic_cast<Gtk::CellRendererText*> (format_view.get_column_cell_renderer (0));
	label_col->add_attribute (renderer->property_foreground (), format_cols.color);

	label_col = sample_rate_view.get_column (0);
	renderer  = dynamic_cast<Gtk::CellRendererText*> (sample_rate_view.get_column_cell_renderer (0));
	label_col->add_attribute (renderer->property_foreground (), sample_rate_cols.color);

	/* SRC Qualities */

	src_quality_list = Gtk::ListStore::create (src_quality_cols);
	src_quality_combo.set_model (src_quality_list);

	iter                        = src_quality_list->append ();
	row                         = *iter;
	row[src_quality_cols.id]    = ExportFormatBase::SRC_SincBest;
	row[src_quality_cols.label] = _("Best (sinc)");

	iter                        = src_quality_list->append ();
	row                         = *iter;
	row[src_quality_cols.id]    = ExportFormatBase::SRC_SincMedium;
	row[src_quality_cols.label] = _("Medium (sinc)");

	iter                        = src_quality_list->append ();
	row                         = *iter;
	row[src_quality_cols.id]    = ExportFormatBase::SRC_SincFast;
	row[src_quality_cols.label] = _("Fast (sinc)");

	iter                        = src_quality_list->append ();
	row                         = *iter;
	row[src_quality_cols.id]    = ExportFormatBase::SRC_Linear;
	row[src_quality_cols.label] = _("Linear");

	iter                        = src_quality_list->append ();
	row                         = *iter;
	row[src_quality_cols.id]    = ExportFormatBase::SRC_ZeroOrderHold;
	row[src_quality_cols.label] = _("Zero order hold");

	src_quality_combo.pack_start (src_quality_cols.label);
	src_quality_combo.set_active (0);

	/* Demo Noise Optoins */

	demo_noise_list = Gtk::ListStore::create (demo_noise_cols);
	demo_noise_combo.set_model (demo_noise_list);

	iter                          = demo_noise_list->append ();
	row                           = *iter;
	row[demo_noise_cols.duration] = 0;
	row[demo_noise_cols.interval] = 0;
	row[demo_noise_cols.label]    = _("No Watermark");

	iter                          = demo_noise_list->append ();
	row                           = *iter;
	row[demo_noise_cols.duration] = 500;
	row[demo_noise_cols.interval] = 15000;
	row[demo_noise_cols.label]    = _("1/2 sec white noise every 15 sec");

	iter                          = demo_noise_list->append ();
	row                           = *iter;
	row[demo_noise_cols.duration] = 1000;
	row[demo_noise_cols.interval] = 30000;
	row[demo_noise_cols.label]    = _("1 sec white noise every 30 sec");

	iter                          = demo_noise_list->append ();
	row                           = *iter;
	row[demo_noise_cols.duration] = 1000;
	row[demo_noise_cols.interval] = 1200000;
	row[demo_noise_cols.label]    = _("1 sec white noise every 2 mins");

	demo_noise_combo.pack_start (demo_noise_cols.label);
	demo_noise_combo.set_active (0);

	ArdourWidgets::set_tooltip (demo_noise_combo,
	                            _("This option allows to add noise, to send complete mixes to the clients for preview but watermarked. White noise is injected after analysis, right before the sample-format conversion or encoding. The first noise burst happens at 1/3 of the interval. Note: there is currently no limiter."));
}

void
ExportFormatDialog::init_encoding_option_widgets ()
{
	Gtk::TreeViewColumn*   label_col;
	Gtk::CellRendererText* renderer;

	sample_format_list = Gtk::ListStore::create (sample_format_cols);
	sample_format_view.set_model (sample_format_list);
	sample_format_view.set_headers_visible (false);
	sample_format_view.append_column ("", sample_format_cols.label);
	label_col = sample_format_view.get_column (0);
	renderer  = dynamic_cast<Gtk::CellRendererText*> (sample_format_view.get_column_cell_renderer (0));
	label_col->add_attribute (renderer->property_foreground (), sample_format_cols.color);

	dither_type_list = Gtk::ListStore::create (dither_type_cols);
	dither_type_view.set_model (dither_type_list);
	dither_type_view.set_headers_visible (false);
	dither_type_view.append_column ("", dither_type_cols.label);
	label_col = dither_type_view.get_column (0);
	renderer  = dynamic_cast<Gtk::CellRendererText*> (dither_type_view.get_column_cell_renderer (0));
	label_col->add_attribute (renderer->property_foreground (), dither_type_cols.color);
}

void
ExportFormatDialog::update_compatibility_selection (std::string const& path)
{
	Gtk::TreeModel::iterator     iter  = compatibility_view.get_model ()->get_iter (path);
	ExportFormatCompatibilityPtr ptr   = iter->get_value (compatibility_cols.ptr);
	bool                         state = iter->get_value (compatibility_cols.selected);

	iter->set_value (compatibility_cols.selected, state);
	ptr->set_selected (state);
}

void
ExportFormatDialog::update_quality_selection ()
{
	update_selection<QualityCols> (quality_list, quality_view, quality_cols);
}

void
ExportFormatDialog::update_format_selection ()
{
	update_selection<FormatCols> (format_list, format_view, format_cols);
}

void
ExportFormatDialog::update_sample_rate_selection ()
{
	update_selection<SampleRateCols> (sample_rate_list, sample_rate_view, sample_rate_cols);
}

void
ExportFormatDialog::update_sample_format_selection ()
{
	update_selection<SampleFormatCols> (sample_format_list, sample_format_view, sample_format_cols);
}

void
ExportFormatDialog::update_dither_type_selection ()
{
	update_selection<DitherTypeCols> (dither_type_list, dither_type_view, dither_type_cols);
}

template <typename ColsT>
void
ExportFormatDialog::update_selection (Glib::RefPtr<Gtk::ListStore>& list, Gtk::TreeView& view, ColsT& cols)
{
	if (applying_changes_from_engine) {
		return;
	}

	Gtk::ListStore::Children::iterator it;
	Glib::RefPtr<Gtk::TreeSelection>   selection = view.get_selection ();

	for (it = list->children ().begin (); it != list->children ().end (); ++it) {
		bool selected = selection->is_selected (it);
		it->get_value (cols.ptr)->set_selected (selected);
	}

	set_codec_quality_selection ();
}

void
ExportFormatDialog::change_compatibility_selection (bool select, WeakCompatPtr compat)
{
	++applying_changes_from_engine;

	ExportFormatCompatibilityPtr ptr = compat.lock ();

	for (Gtk::ListStore::Children::iterator it = compatibility_list->children ().begin (); it != compatibility_list->children ().end (); ++it) {
		if (it->get_value (compatibility_cols.ptr) == ptr) {
			it->set_value (compatibility_cols.selected, select);
			break;
		}
	}

	--applying_changes_from_engine;
}

void
ExportFormatDialog::change_quality_selection (bool select, WeakQualityPtr quality)
{
	change_selection<ExportFormatManager::QualityState, QualityCols> (select, quality, quality_list, quality_view, quality_cols);
}

void
ExportFormatDialog::change_format_selection (bool select, WeakFormatPtr format)
{
	change_selection<ExportFormat, FormatCols> (select, format, format_list, format_view, format_cols);

	ExportFormatPtr ptr = format.lock ();

	if (select && ptr) {
		change_encoding_options (ptr);
	}
}

void
ExportFormatDialog::change_sample_rate_selection (bool select, WeakSampleRatePtr rate)
{
	change_selection<ExportFormatManager::SampleRateState, SampleRateCols> (select, rate, sample_rate_list, sample_rate_view, sample_rate_cols);

	if (select) {
		ExportFormatManager::SampleRatePtr ptr = rate.lock ();
		if (ptr && _session) {
			src_quality_combo.set_sensitive ((uint32_t)ptr->rate != _session->sample_rate () && ptr->rate != ExportFormatBase::SR_Session);
		}
	}
}

void
ExportFormatDialog::change_sample_format_selection (bool select, WeakSampleFormatPtr format)
{
	change_selection<HasSampleFormat::SampleFormatState, SampleFormatCols> (select, format, sample_format_list, sample_format_view, sample_format_cols);
}

void
ExportFormatDialog::change_dither_type_selection (bool select, WeakDitherTypePtr type)
{
	change_selection<HasSampleFormat::DitherTypeState, DitherTypeCols> (select, type, dither_type_list, dither_type_view, dither_type_cols);
}

template <typename T, typename ColsT>
void
ExportFormatDialog::change_selection (bool select, boost::weak_ptr<T> w_ptr, Glib::RefPtr<Gtk::ListStore>& list, Gtk::TreeView& view, ColsT& cols)
{
	++applying_changes_from_engine;

	boost::shared_ptr<T> ptr = w_ptr.lock ();

	Gtk::ListStore::Children::iterator it;
	Glib::RefPtr<Gtk::TreeSelection>   selection;

	selection = view.get_selection ();

	if (!ptr) {
		selection->unselect_all ();
	} else {
		for (it = list->children ().begin (); it != list->children ().end (); ++it) {
			if (it->get_value (cols.ptr) == ptr) {
				if (select) {
					selection->select (it);
				} else {
					selection->unselect (it);
				}
				break;
			}
		}
	}

	--applying_changes_from_engine;
}

void
ExportFormatDialog::change_quality_compatibility (bool compatibility, WeakQualityPtr quality)
{
	change_compatibility<ExportFormatManager::QualityState, QualityCols> (compatibility, quality, quality_list, quality_cols);
}

void
ExportFormatDialog::change_format_compatibility (bool compatibility, WeakFormatPtr format)
{
	change_compatibility<ExportFormat, FormatCols> (compatibility, format, format_list, format_cols);
}

void
ExportFormatDialog::change_sample_rate_compatibility (bool compatibility, WeakSampleRatePtr rate)
{
	change_compatibility<ExportFormatManager::SampleRateState, SampleRateCols> (compatibility, rate, sample_rate_list, sample_rate_cols);
}

void
ExportFormatDialog::change_sample_format_compatibility (bool compatibility, WeakSampleFormatPtr format)
{
	change_compatibility<HasSampleFormat::SampleFormatState, SampleFormatCols> (compatibility, format, sample_format_list, sample_format_cols);
}

void
ExportFormatDialog::change_dither_type_compatibility (bool compatibility, WeakDitherTypePtr type)
{
	change_compatibility<HasSampleFormat::DitherTypeState, DitherTypeCols> (compatibility, type, dither_type_list, dither_type_cols, "red");
}

template <typename T, typename ColsT>
void
ExportFormatDialog::change_compatibility (bool compatibility, boost::weak_ptr<T> w_ptr, Glib::RefPtr<Gtk::ListStore>& list, ColsT& cols,
                                          std::string const& c_incompatible, std::string const& c_compatible)
{
	boost::shared_ptr<T> ptr = w_ptr.lock ();

	Gtk::ListStore::Children::iterator it;
	for (it = list->children ().begin (); it != list->children ().end (); ++it) {
		if (it->get_value (cols.ptr) == ptr) {
			it->set_value (cols.color, compatibility ? c_compatible : c_incompatible);
			break;
		}
	}
}

void
ExportFormatDialog::update_with_cue ()
{
	manager.select_with_cue (with_cue.get_active ());
}

void
ExportFormatDialog::update_with_toc ()
{
	manager.select_with_toc (with_toc.get_active ());
}

void
ExportFormatDialog::update_with_mp4chaps ()
{
	manager.select_with_mp4chaps (with_mp4chaps.get_active ());
}

void
ExportFormatDialog::update_command ()
{
	manager.set_command (command_entry.get_text ());
}

void
ExportFormatDialog::update_description ()
{
	std::string text = ": " + format->description (false);
	name_generated_part.set_text (text);
}

void
ExportFormatDialog::update_name ()
{
	manager.set_name (name_entry.get_text ());
}

void
ExportFormatDialog::update_trim_start_selection ()
{
	manager.select_trim_beginning (trim_start_checkbox.get_active ());
}

void
ExportFormatDialog::update_trim_end_selection ()
{
	manager.select_trim_end (trim_end_checkbox.get_active ());
}

void
ExportFormatDialog::update_normalize_sensitivity ()
{
	bool en       = normalize_checkbox.get_active ();
	bool loudness = normalize_loudness_rb.get_active ();
	normalize_tp_limiter.set_sensitive (loudness && en);
	normalize_dbfs_spinbutton.set_sensitive (!loudness && en);
	normalize_lufs_spinbutton.set_sensitive (loudness && en);
	normalize_dbtp_spinbutton.set_sensitive (loudness && en);
}

void
ExportFormatDialog::update_normalize_selection ()
{
	manager.select_normalize (normalize_checkbox.get_active ());
	manager.select_normalize_loudness (normalize_loudness_rb.get_active ());
	manager.select_normalize_dbfs (normalize_dbfs_spinbutton.get_value ());
	manager.select_tp_limiter (normalize_tp_limiter.get_active_row_number () == 0);
	manager.select_normalize_lufs (normalize_lufs_spinbutton.get_value ());
	manager.select_normalize_dbtp (normalize_dbtp_spinbutton.get_value ());
	update_normalize_sensitivity ();
}

void
ExportFormatDialog::update_silence_start_selection ()
{
	update_time (silence_start, silence_start_clock);
	AnyTime zero;
	zero.type = AnyTime::Timecode;
	manager.select_silence_beginning (silence_start_checkbox.get_active () ? silence_start : zero);
}

void
ExportFormatDialog::update_silence_end_selection ()
{
	update_time (silence_end, silence_end_clock);
	AnyTime zero;
	zero.type = AnyTime::Timecode;
	manager.select_silence_end (silence_end_checkbox.get_active () ? silence_end : zero);
}

void
ExportFormatDialog::update_clock (AudioClock& clock, ARDOUR::AnyTime const& time)
{
	// TODO position
	clock.set (timepos_t (_session->convert_to_samples (time)), true);

	AudioClock::Mode mode (AudioClock::Timecode);

	switch (time.type) {
		case AnyTime::Timecode:
			mode = AudioClock::Timecode;
			break;
		case AnyTime::BBT:
			mode = AudioClock::BBT;
			break;
		case AnyTime::Samples:
			mode = AudioClock::Samples;
			break;
		case AnyTime::Seconds:
			mode = AudioClock::MinSec;
			break;
	}

	clock.set_mode (mode);
}

void
ExportFormatDialog::update_time (AnyTime& time, AudioClock const& clock)
{
	if (!_session) {
		return;
	}

	samplecnt_t samples = clock.current_duration().samples();

	switch (clock.mode ()) {
	case AudioClock::Timecode:
		time.type = AnyTime::Timecode;
		_session->timecode_time (samples, time.timecode);
		break;
	case AudioClock::BBT:
		time.type = AnyTime::BBT;
		_session->bbt_time (timepos_t (samples), time.bbt);
		break;
	case AudioClock::Seconds:
	case AudioClock::MinSec:
		time.type    = AnyTime::Seconds;
		time.seconds = (double)samples / _session->sample_rate ();
		break;
	case AudioClock::Samples:
		time.type    = AnyTime::Samples;
		time.samples = samples;
		break;
	}
}

void
ExportFormatDialog::update_src_quality_selection ()
{
	Gtk::TreeModel::const_iterator iter    = src_quality_combo.get_active ();
	ExportFormatBase::SRCQuality   quality = iter->get_value (src_quality_cols.id);
	manager.select_src_quality (quality);
}

void
ExportFormatDialog::update_demo_noise_sensitivity ()
{
	Gtk::TreeModel::const_iterator iter = demo_noise_combo.get_active ();
	if (!iter) {
		demo_noise_dbfs_spinbutton.set_sensitive (false);
		return;
	}
	int duration = iter->get_value (demo_noise_cols.duration);
	int interval = iter->get_value (demo_noise_cols.interval);
	demo_noise_dbfs_spinbutton.set_sensitive (interval != 0 && duration != 0);
}

void
ExportFormatDialog::update_demo_noise_selection ()
{
	Gtk::TreeModel::const_iterator iter = demo_noise_combo.get_active ();
	if (!iter) {
		demo_noise_dbfs_spinbutton.set_sensitive (false);
		return;
	}
	int duration = iter->get_value (demo_noise_cols.duration);
	int interval = iter->get_value (demo_noise_cols.interval);
	int level    = demo_noise_dbfs_spinbutton.get_value ();
	demo_noise_dbfs_spinbutton.set_sensitive (interval != 0 && duration != 0);

	manager.select_demo_noise_duration (duration);
	manager.select_demo_noise_interval (interval);
	manager.select_demo_noise_level (level);
}

void
ExportFormatDialog::update_codec_quality_selection ()
{
	Gtk::TreeModel::const_iterator iter = codec_quality_combo.get_active ();
	if (!iter) {
		return;
	}
	int quality = iter->get_value (codec_quality_cols.quality);
	manager.select_codec_quality (quality);
}

void
ExportFormatDialog::update_tagging_selection ()
{
	manager.select_tagging (tag_checkbox.get_active ());
}

void
ExportFormatDialog::change_encoding_options (ExportFormatPtr ptr)
{
	empty_encoding_option_table ();

	boost::shared_ptr<ARDOUR::ExportFormatLinear>    linear_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatOggVorbis> ogg_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatFLAC>      flac_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatBWF>       bwf_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatFFMPEG>    ffmpeg_ptr;

	if ((linear_ptr = boost::dynamic_pointer_cast<ExportFormatLinear> (ptr))) {
		show_linear_enconding_options (linear_ptr);
	} else if ((ogg_ptr = boost::dynamic_pointer_cast<ExportFormatOggVorbis> (ptr))) {
		show_ogg_enconding_options (ogg_ptr);
	} else if ((flac_ptr = boost::dynamic_pointer_cast<ExportFormatFLAC> (ptr))) {
		show_flac_enconding_options (flac_ptr);
	} else if ((bwf_ptr = boost::dynamic_pointer_cast<ExportFormatBWF> (ptr))) {
		show_bwf_enconding_options (bwf_ptr);
	} else if ((ffmpeg_ptr = boost::dynamic_pointer_cast<ExportFormatFFMPEG> (ptr))) {
		show_ffmpeg_enconding_options (ffmpeg_ptr);
	} else {
		std::cout << "Unrecognized format!" << std::endl;
	}

	tag_checkbox.set_sensitive (ptr->supports_tagging ());
}

void
ExportFormatDialog::empty_encoding_option_table ()
{
	encoding_options_table.foreach (sigc::bind (sigc::mem_fun (*this, &ExportFormatDialog::remove_widget), &encoding_options_table));
}

void
ExportFormatDialog::remove_widget (Gtk::Widget& to_remove, Gtk::Container* remove_from)
{
	remove_from->remove (to_remove);
}

void
ExportFormatDialog::show_linear_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatLinear> ptr)
{
	/* Set label and pack table */

	encoding_options_label.set_label (_("Linear encoding options"));

	encoding_options_table.resize (2, 2);
	encoding_options_table.attach (sample_format_label, 0, 1, 0, 1);
	encoding_options_table.attach (dither_label, 1, 2, 0, 1);
	encoding_options_table.attach (sample_format_view, 0, 1, 1, 2);
	encoding_options_table.attach (dither_type_view, 1, 2, 1, 2);

	fill_sample_format_lists (boost::dynamic_pointer_cast<HasSampleFormat> (ptr));

	show_all_children ();
}

void
ExportFormatDialog::show_ogg_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatOggVorbis> ptr)
{
	encoding_options_label.set_label (_("Ogg Vorbis options"));

	encoding_options_table.resize (2, 1);
	encoding_options_table.attach (codec_quality_combo, 0, 1, 0, 1);
	fill_codec_quality_lists (ptr);
	show_all_children ();
}

void
ExportFormatDialog::show_flac_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatFLAC> ptr)
{
	encoding_options_label.set_label (_("FLAC options"));

	encoding_options_table.resize (3, 2);
	encoding_options_table.attach (sample_format_label, 0, 1, 0, 1);
	encoding_options_table.attach (dither_label, 1, 2, 0, 1);
	encoding_options_table.attach (sample_format_view, 0, 1, 1, 2);
	encoding_options_table.attach (dither_type_view, 1, 2, 1, 2);

	fill_sample_format_lists (boost::dynamic_pointer_cast<HasSampleFormat> (ptr));

	show_all_children ();
}

void
ExportFormatDialog::show_bwf_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatBWF> ptr)
{
	encoding_options_label.set_label (_("Broadcast Wave options"));

	encoding_options_table.resize (2, 2);
	encoding_options_table.attach (sample_format_label, 0, 1, 0, 1);
	encoding_options_table.attach (dither_label, 1, 2, 0, 1);
	encoding_options_table.attach (sample_format_view, 0, 1, 1, 2);
	encoding_options_table.attach (dither_type_view, 1, 2, 1, 2);

	fill_sample_format_lists (boost::dynamic_pointer_cast<HasSampleFormat> (ptr));

	show_all_children ();
}

void
ExportFormatDialog::show_ffmpeg_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatFFMPEG> ptr)
{
	encoding_options_label.set_label (_("FFMPEG/MP3 options"));
	encoding_options_table.resize (1, 1);
	encoding_options_table.attach (codec_quality_combo, 0, 1, 0, 1);
	fill_codec_quality_lists (ptr);
	show_all_children ();
}

void
ExportFormatDialog::fill_sample_format_lists (boost::shared_ptr<ARDOUR::HasSampleFormat> ptr)
{
	/* Fill lists */

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row      row;

	sample_format_list->clear ();

	HasSampleFormat::SampleFormatList const& formats = ptr->get_sample_formats ();

	for (HasSampleFormat::SampleFormatList::const_iterator it = formats.begin (); it != formats.end (); ++it) {
		iter = sample_format_list->append ();
		row  = *iter;

		row[sample_format_cols.ptr]   = *it;
		row[sample_format_cols.color] = (*it)->compatible () ? "white" : "red";
		row[sample_format_cols.label] = (*it)->name ();

		if ((*it)->selected ()) {
			sample_format_view.get_selection ()->select (iter);
		}
	}

	dither_type_list->clear ();

	HasSampleFormat::DitherTypeList const& types = ptr->get_dither_types ();

	for (HasSampleFormat::DitherTypeList::const_iterator it = types.begin (); it != types.end (); ++it) {
		iter = dither_type_list->append ();
		row  = *iter;

		row[dither_type_cols.ptr]   = *it;
		row[dither_type_cols.color] = "white";
		row[dither_type_cols.label] = (*it)->name ();

		if ((*it)->selected ()) {
			dither_type_view.get_selection ()->select (iter);
		}
	}
}

void
ExportFormatDialog::fill_codec_quality_lists (boost::shared_ptr<ARDOUR::HasCodecQuality> ptr)
{
	HasCodecQuality::CodecQualityList const& codecs = ptr->get_codec_qualities ();

	codec_quality_list->clear ();
	for (HasCodecQuality::CodecQualityList::const_iterator it = codecs.begin (); it != codecs.end (); ++it) {
		Gtk::TreeModel::iterator iter   = codec_quality_list->append ();
		Gtk::TreeModel::Row      row    = *iter;
		row[codec_quality_cols.quality] = (*it)->quality;
		row[codec_quality_cols.label]   = (*it)->name;
	}
	set_codec_quality_selection ();
}

void
ExportFormatDialog::set_codec_quality_selection ()
{
	for (Gtk::ListStore::Children::iterator it = codec_quality_list->children ().begin (); it != codec_quality_list->children ().end (); ++it) {
		if (it->get_value (codec_quality_cols.quality) == format->codec_quality ()) {
			codec_quality_combo.set_active (it);
			break;
		}
	}
}

void
ExportFormatDialog::end_dialog ()
{
	hide_all ();
}

void
ExportFormatDialog::prohibit_compatibility_selection ()
{
	compatibility_select_connection.block (true);
	compatibility_view.get_selection ()->unselect_all ();
	compatibility_select_connection.block (false);
}
