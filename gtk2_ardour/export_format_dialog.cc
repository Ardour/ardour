/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include "ardour/session.h"
#include "ardour/export_format_specification.h"

#include "export_format_dialog.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;

ExportFormatDialog::ExportFormatDialog (FormatPtr format, bool new_dialog) :
  ArdourDialog (new_dialog ? _("New Export Format Profile") : _("Edit Export Format Profile")),
  format (format),
  manager (format),
  original_state (format->get_state()),

  applying_changes_from_engine (0),

  name_label (_("Label: "), Gtk::ALIGN_LEFT),
  name_generated_part ("", Gtk::ALIGN_LEFT),

  normalize_checkbox (_("Normalize to:")),
  normalize_adjustment (0.00, -90.00, 0.00, 0.1, 0.2),
  normalize_db_label (_("dBFS"), Gtk::ALIGN_LEFT),

  silence_table (2, 4),
  trim_start_checkbox (_("Trim silence at start")),
  silence_start_checkbox (_("Add silence at start:")),
  silence_start_clock ("silence_start", true, "", true, false, true),

  trim_end_checkbox (_("Trim silence at end")),
  silence_end_checkbox (_("Add silence at end:")),
  silence_end_clock ("silence_end", true, "", true, false, true),

  upload_checkbox(_("Upload to Soundcloud")),

  format_table (3, 4),
  compatibility_label (_("Compatibility"), Gtk::ALIGN_LEFT),
  quality_label (_("Quality"), Gtk::ALIGN_LEFT),
  format_label (_("File format"), Gtk::ALIGN_LEFT),
  sample_rate_label (_("Sample rate"), Gtk::ALIGN_LEFT),
  src_quality_label (_("Sample rate conversion quality:"), Gtk::ALIGN_RIGHT),

  encoding_options_label ("", Gtk::ALIGN_LEFT),

  /* Changing encoding options from here on */

  sample_format_label (_("Sample Format"), Gtk::ALIGN_LEFT),
  dither_label (_("Dithering"), Gtk::ALIGN_LEFT),

  with_cue (_("Create CUE file for disk-at-once CD/DVD creation")),
  with_toc (_("Create TOC file for disk-at-once CD/DVD creation")),

  tag_checkbox (_("Tag file with session's metadata"))
{

	/* Pack containers in dialog */

	get_vbox()->pack_start (silence_table, false, false, 6);
	get_vbox()->pack_start (format_table, false, false, 6);
	get_vbox()->pack_start (encoding_options_vbox, false, false, 0);
	get_vbox()->pack_start (cue_toc_vbox, false, false, 0);
	get_vbox()->pack_start (name_hbox, false, false, 6);

	/* Name, new and remove */

	name_hbox.pack_start (name_label, false, false, 0);
	name_hbox.pack_start (name_entry, false, false, 0);
	name_hbox.pack_start (name_generated_part, true, true, 0);
	name_entry.set_width_chars(20);
	update_description();
	manager.DescriptionChanged.connect(
		*this, invalidator (*this),
		boost::bind (&ExportFormatDialog::update_description, this), gui_context());

	/* Normalize */

	normalize_hbox.pack_start (normalize_checkbox, false, false, 0);
	normalize_hbox.pack_start (normalize_spinbutton, false, false, 6);
	normalize_hbox.pack_start (normalize_db_label, false, false, 0);

	normalize_spinbutton.configure (normalize_adjustment, 0.1, 2);

	/* Silence  */

	silence_table.set_row_spacings (6);
	silence_table.set_col_spacings (12);

	silence_table.attach (normalize_hbox, 0, 3, 0, 1);

	silence_table.attach (trim_start_checkbox, 0, 1, 1, 2);
	silence_table.attach (silence_start_checkbox, 1, 2, 1, 2);
	silence_table.attach (silence_start_clock, 2, 3, 1, 2);

	silence_table.attach (trim_end_checkbox, 0, 1, 2, 3);
	silence_table.attach (silence_end_checkbox, 1, 2, 2, 3);
	silence_table.attach (silence_end_clock, 2, 3, 2, 3);

	get_vbox()->pack_start (upload_checkbox, false, false);
	/* Format table */

	init_format_table();

	/* Encoding options */

	init_encoding_option_widgets();

	encoding_options_table.set_spacings (1);

	encoding_options_vbox.pack_start (encoding_options_label, false, false, 0);
	encoding_options_vbox.pack_start (encoding_options_table, false, false, 12);

	Pango::AttrList bold;
	Pango::Attribute b = Pango::Attribute::create_attr_weight (Pango::WEIGHT_BOLD);
	bold.insert (b);
	encoding_options_label.set_attributes (bold);

	/* Buttons */

	revert_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	revert_button->signal_clicked().connect (sigc::mem_fun(*this, &ExportFormatDialog::revert));
	close_button = add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_APPLY);
	close_button->set_sensitive (false);
	close_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportFormatDialog::end_dialog));
	manager.CompleteChanged.connect (*this, invalidator (*this), boost::bind (&Gtk::Button::set_sensitive, close_button, _1), gui_context());

	with_cue.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_with_cue));
	with_toc.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_with_toc));

	cue_toc_vbox.pack_start (with_cue, false, false);
	cue_toc_vbox.pack_start (with_toc, false, false);

	/* Load state before hooking up the rest of the signals */

	load_state (format);

	/* Name entry */

	name_entry.signal_changed().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_name));

	/* Normalize, silence and src_quality signals */

	trim_start_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_trim_start_selection));
	trim_end_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_trim_end_selection));

	normalize_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));
	normalize_spinbutton.signal_value_changed().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_normalize_selection));

	silence_start_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_start_selection));
	silence_start_clock.ValueChanged.connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_start_selection));

	silence_end_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_end_selection));
	silence_end_clock.ValueChanged.connect (sigc::mem_fun (*this, &ExportFormatDialog::update_silence_end_selection));

	src_quality_combo.signal_changed().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_src_quality_selection));

	/* Format table signals */

	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *>(compatibility_view.get_column_cell_renderer (0));
	toggle->signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_compatibility_selection));
	compatibility_select_connection = compatibility_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &ExportFormatDialog::prohibit_compatibility_selection));

	quality_view.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &ExportFormatDialog::update_quality_selection));
	format_view.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &ExportFormatDialog::update_format_selection));
	sample_rate_view.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &ExportFormatDialog::update_sample_rate_selection));

	/* Encoding option signals */

	sample_format_view.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &ExportFormatDialog::update_sample_format_selection));
	dither_type_view.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &ExportFormatDialog::update_dither_type_selection));

	tag_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportFormatDialog::update_tagging_selection));

	/* Finalize */

	show_all_children();
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

	if (sample_rate_view.get_selection()->count_selected_rows() == 0) {
		Gtk::ListStore::Children::iterator it;
		for (it = sample_rate_list->children().begin(); it != sample_rate_list->children().end(); ++it) {
			if ((framecnt_t) (*it)->get_value (sample_rate_cols.ptr)->rate == _session->nominal_frame_rate()) {
				sample_rate_view.get_selection()->select (it);
				break;
			}
		}
	}
}

void
ExportFormatDialog::load_state (FormatPtr spec)
{
	name_entry.set_text (spec->name());

	normalize_checkbox.set_active (spec->normalize());
	normalize_spinbutton.set_value (spec->normalize_target());

	trim_start_checkbox.set_active (spec->trim_beginning());
	silence_start = spec->silence_beginning_time();
	silence_start_checkbox.set_active (spec->silence_beginning_time().not_zero());

	trim_end_checkbox.set_active (spec->trim_end());
	silence_end = spec->silence_end_time();
	silence_end_checkbox.set_active (spec->silence_end_time().not_zero());

	with_cue.set_active (spec->with_cue());
	with_toc.set_active (spec->with_toc());

	for (Gtk::ListStore::Children::iterator it = src_quality_list->children().begin(); it != src_quality_list->children().end(); ++it) {
		if (it->get_value (src_quality_cols.id) == spec->src_quality()) {
			src_quality_combo.set_active (it);
			break;
		}
	}

	for (Gtk::ListStore::Children::iterator it = format_list->children().begin(); it != format_list->children().end(); ++it) {
		boost::shared_ptr<ARDOUR::ExportFormat> format_in_list = it->get_value (format_cols.ptr);
		if (format_in_list->get_format_id() == spec->format_id() &&
		    // BWF has the same format id with wav, so we need to check this.
		    format_in_list->has_broadcast_info() == spec->has_broadcast_info()) {

			format_in_list->set_selected (true);
			break;
		}
	}

	for (Gtk::ListStore::Children::iterator it = sample_rate_list->children().begin(); it != sample_rate_list->children().end(); ++it) {
		if (it->get_value (sample_rate_cols.ptr)->rate == spec->sample_rate()) {
			it->get_value (sample_rate_cols.ptr)->set_selected (true);
			break;
		}
	}

	if (spec->sample_format()) {
		for (Gtk::ListStore::Children::iterator it = sample_format_list->children().begin(); it != sample_format_list->children().end(); ++it) {
			if (it->get_value (sample_format_cols.ptr)->format == spec->sample_format()) {
				it->get_value (sample_format_cols.ptr)->set_selected (true);
				break;
			}
		}

		for (Gtk::ListStore::Children::iterator it = dither_type_list->children().begin(); it != dither_type_list->children().end(); ++it) {
			if (it->get_value (dither_type_cols.ptr)->type == spec->dither_type()) {
				it->get_value (dither_type_cols.ptr)->set_selected (true);
				break;
			}
		}
	}

	tag_checkbox.set_active (spec->tag());
	upload_checkbox.set_active (spec->upload());
}

void
ExportFormatDialog::init_format_table ()
{

	format_table.set_spacings (1);

	format_table.attach (compatibility_label, 0, 1, 0, 1);
	format_table.attach (quality_label, 1, 2, 0, 1);
	format_table.attach (format_label, 2, 3, 0, 1);
	format_table.attach (sample_rate_label, 3, 4, 0, 1);

	format_table.attach (compatibility_view, 0, 1, 1, 2);
	format_table.attach (quality_view, 1, 2, 1, 2);
	format_table.attach (format_view, 2, 3, 1, 2);
	format_table.attach (sample_rate_view, 3, 4, 1, 2);

	format_table.attach (src_quality_label, 0, 3, 2, 3);
	format_table.attach (src_quality_combo, 3, 4, 2, 3);

	compatibility_view.set_headers_visible (false);
	quality_view.set_headers_visible (false);
	format_view.set_headers_visible (false);
	sample_rate_view.set_headers_visible (false);

	/*** Table entries ***/

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;

	/* Compatibilities */

	compatibility_list = Gtk::ListStore::create (compatibility_cols);
	compatibility_view.set_model (compatibility_list);

	ExportFormatManager::CompatList const & compat_list = manager.get_compatibilities();

	for (ExportFormatManager::CompatList::const_iterator it = compat_list.begin(); it != compat_list.end(); ++it) {
		iter = compatibility_list->append();
		row = *iter;

		row[compatibility_cols.ptr] = *it;
		row[compatibility_cols.selected] = false;
		row[compatibility_cols.label] = (*it)->name();

		WeakCompatPtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_compatibility_selection, this, _1, ptr), gui_context());
	}

	compatibility_view.append_column_editable ("", compatibility_cols.selected);

	Gtk::CellRendererText* text_renderer = Gtk::manage (new Gtk::CellRendererText);
	text_renderer->property_editable() = false;

	Gtk::TreeView::Column* column = compatibility_view.get_column (0);
	column->pack_start (*text_renderer);
	column->add_attribute (text_renderer->property_text(), compatibility_cols.label);

	/* Qualities */

	quality_list = Gtk::ListStore::create (quality_cols);
	quality_view.set_model (quality_list);

	ExportFormatManager::QualityList const & qualities = manager.get_qualities ();

	for (ExportFormatManager::QualityList::const_iterator it = qualities.begin(); it != qualities.end(); ++it) {
		iter = quality_list->append();
		row = *iter;

		row[quality_cols.ptr] = *it;
		row[quality_cols.color] = "white";
		row[quality_cols.label] = (*it)->name();

		WeakQualityPtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_quality_selection, this, _1, ptr), gui_context());
		(*it)->CompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_quality_compatibility, this, _1, ptr), gui_context());
	}

	quality_view.append_column ("", quality_cols.label);

	/* Formats */

	format_list = Gtk::ListStore::create (format_cols);
	format_view.set_model (format_list);

	ExportFormatManager::FormatList const & formats = manager.get_formats ();

	for (ExportFormatManager::FormatList::const_iterator it = formats.begin(); it != formats.end(); ++it) {
		iter = format_list->append();
		row = *iter;

		row[format_cols.ptr] = *it;
		row[format_cols.color] = "white";
		row[format_cols.label] = (*it)->name();

		WeakFormatPtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_format_selection, this, _1, ptr), gui_context());
		(*it)->CompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_format_compatibility, this, _1, ptr), gui_context());

		/* Encoding options */

		boost::shared_ptr<HasSampleFormat> hsf;

		if ((hsf = boost::dynamic_pointer_cast<HasSampleFormat> (*it))) {
			hsf->SampleFormatSelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_format_selection, this, _1, _2), gui_context());
			hsf->SampleFormatCompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_format_compatibility, this, _1, _2), gui_context());

			hsf->DitherTypeSelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_dither_type_selection, this, _1, _2), gui_context());
			hsf->DitherTypeCompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_dither_type_compatibility, this, _1, _2), gui_context());
		}
	}

	format_view.append_column ("", format_cols.label);

	/* Sample Rates */

	sample_rate_list = Gtk::ListStore::create (sample_rate_cols);
	sample_rate_view.set_model (sample_rate_list);

	ExportFormatManager::SampleRateList const & rates = manager.get_sample_rates ();

	for (ExportFormatManager::SampleRateList::const_iterator it = rates.begin(); it != rates.end(); ++it) {
		iter = sample_rate_list->append();
		row = *iter;

		row[sample_rate_cols.ptr] = *it;
		row[sample_rate_cols.color] = "white";
		row[sample_rate_cols.label] = (*it)->name();

		WeakSampleRatePtr ptr (*it);
		(*it)->SelectChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_rate_selection, this, _1, ptr), gui_context());
		(*it)->CompatibleChanged.connect (*this, invalidator (*this), boost::bind (&ExportFormatDialog::change_sample_rate_compatibility, this, _1, ptr), gui_context());
	}

	sample_rate_view.append_column ("", sample_rate_cols.label);

	/* Color rendering */

	Gtk::TreeViewColumn * label_col;
	Gtk::CellRendererText * renderer;

	label_col = quality_view.get_column(0);
	renderer = dynamic_cast<Gtk::CellRendererText*> (quality_view.get_column_cell_renderer (0));
	label_col->add_attribute(renderer->property_foreground(), quality_cols.color);

	label_col = format_view.get_column(0);
	renderer = dynamic_cast<Gtk::CellRendererText*> (format_view.get_column_cell_renderer (0));
	label_col->add_attribute(renderer->property_foreground(), format_cols.color);

	label_col = sample_rate_view.get_column(0);
	renderer = dynamic_cast<Gtk::CellRendererText*> (sample_rate_view.get_column_cell_renderer (0));
	label_col->add_attribute(renderer->property_foreground(), sample_rate_cols.color);

	/* SRC Qualities */

	src_quality_list = Gtk::ListStore::create (src_quality_cols);
	src_quality_combo.set_model (src_quality_list);

	iter = src_quality_list->append();
	row = *iter;
	row[src_quality_cols.id] = ExportFormatBase::SRC_SincBest;
	row[src_quality_cols.label] = _("Best (sinc)");

	iter = src_quality_list->append();
	row = *iter;
	row[src_quality_cols.id] = ExportFormatBase::SRC_SincMedium;
	row[src_quality_cols.label] = _("Medium (sinc)");

	iter = src_quality_list->append();
	row = *iter;
	row[src_quality_cols.id] = ExportFormatBase::SRC_SincFast;
	row[src_quality_cols.label] = _("Fast (sinc)");

	iter = src_quality_list->append();
	row = *iter;
	row[src_quality_cols.id] = ExportFormatBase::SRC_Linear;
	row[src_quality_cols.label] = _("Linear");

	iter = src_quality_list->append();
	row = *iter;
	row[src_quality_cols.id] = ExportFormatBase::SRC_ZeroOrderHold;
	row[src_quality_cols.label] = _("Zero order hold");

	src_quality_combo.pack_start (src_quality_cols.label);
	src_quality_combo.set_active (0);
}

void
ExportFormatDialog::init_encoding_option_widgets ()
{
	Gtk::TreeViewColumn * label_col;
	Gtk::CellRendererText * renderer;

	sample_format_list = Gtk::ListStore::create (sample_format_cols);
	sample_format_view.set_model (sample_format_list);
	sample_format_view.set_headers_visible (false);
	sample_format_view.append_column ("", sample_format_cols.label);
	label_col = sample_format_view.get_column(0);
	renderer = dynamic_cast<Gtk::CellRendererText*> (sample_format_view.get_column_cell_renderer (0));
	label_col->add_attribute(renderer->property_foreground(), sample_format_cols.color);

	dither_type_list = Gtk::ListStore::create (dither_type_cols);
	dither_type_view.set_model (dither_type_list);
	dither_type_view.set_headers_visible (false);
	dither_type_view.append_column ("", dither_type_cols.label);
	label_col = dither_type_view.get_column(0);
	renderer = dynamic_cast<Gtk::CellRendererText*> (dither_type_view.get_column_cell_renderer (0));
	label_col->add_attribute(renderer->property_foreground(), dither_type_cols.color);

}

void
ExportFormatDialog::update_compatibility_selection (std::string const & path)
{

	Gtk::TreeModel::iterator iter = compatibility_view.get_model ()->get_iter (path);
	ExportFormatCompatibilityPtr ptr = iter->get_value (compatibility_cols.ptr);
	bool state = iter->get_value (compatibility_cols.selected);

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

template<typename ColsT>
void
ExportFormatDialog::update_selection (Glib::RefPtr<Gtk::ListStore> & list, Gtk::TreeView & view, ColsT & cols)
{
	if (applying_changes_from_engine) {
		return;
	}

	Gtk::ListStore::Children::iterator it;
	Glib::RefPtr<Gtk::TreeSelection> selection = view.get_selection();

	for (it = list->children().begin(); it != list->children().end(); ++it) {
		bool selected = selection->is_selected (it);
		it->get_value (cols.ptr)->set_selected (selected);
	}
}

void
ExportFormatDialog::change_compatibility_selection (bool select, WeakCompatPtr compat)
{
	++applying_changes_from_engine;

	ExportFormatCompatibilityPtr ptr = compat.lock();

	for (Gtk::ListStore::Children::iterator it = compatibility_list->children().begin(); it != compatibility_list->children().end(); ++it) {
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

	ExportFormatPtr ptr = format.lock();

	if (select && ptr) {
		change_encoding_options (ptr);
	}
}

void
ExportFormatDialog::change_sample_rate_selection (bool select, WeakSampleRatePtr rate)
{
	change_selection<ExportFormatManager::SampleRateState, SampleRateCols> (select, rate, sample_rate_list, sample_rate_view, sample_rate_cols);

	if (select) {
		ExportFormatManager::SampleRatePtr ptr = rate.lock();
		if (ptr && _session) {
			src_quality_combo.set_sensitive ((uint32_t) ptr->rate != _session->frame_rate());
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

template<typename T, typename ColsT>
void
ExportFormatDialog::change_selection (bool select, boost::weak_ptr<T> w_ptr, Glib::RefPtr<Gtk::ListStore> & list, Gtk::TreeView & view, ColsT & cols)
{
	++applying_changes_from_engine;

	boost::shared_ptr<T> ptr = w_ptr.lock();

	Gtk::ListStore::Children::iterator it;
	Glib::RefPtr<Gtk::TreeSelection> selection;

	selection = view.get_selection();

	if (!ptr) {
		selection->unselect_all();
	} else {
		for (it = list->children().begin(); it != list->children().end(); ++it) {
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

template<typename T, typename ColsT>
void
ExportFormatDialog::change_compatibility (bool compatibility, boost::weak_ptr<T> w_ptr, Glib::RefPtr<Gtk::ListStore> & list, ColsT & cols,
                                          std::string const & c_incompatible, std::string const & c_compatible)
{
	boost::shared_ptr<T> ptr = w_ptr.lock();

	Gtk::ListStore::Children::iterator it;
	for (it = list->children().begin(); it != list->children().end(); ++it) {
		if (it->get_value (cols.ptr) == ptr) {
			it->set_value (cols.color, compatibility ? c_compatible : c_incompatible);
			break;
		}
	}
}

void
ExportFormatDialog::update_with_cue ()
{
	manager.select_with_cue (with_cue.get_active());
}

void
ExportFormatDialog::update_with_toc ()
{
	manager.select_with_toc (with_toc.get_active());
}

void
ExportFormatDialog::update_description()
{
	std::string text = ": " + format->description(false);
	name_generated_part.set_text(text);
}

void
ExportFormatDialog::update_name ()
{
	manager.set_name (name_entry.get_text());
}

void
ExportFormatDialog::update_trim_start_selection ()
{
	manager.select_trim_beginning (trim_start_checkbox.get_active());
}

void
ExportFormatDialog::update_trim_end_selection ()
{
	manager.select_trim_end (trim_end_checkbox.get_active());
}

void
ExportFormatDialog::update_normalize_selection ()
{
	manager.select_normalize (normalize_checkbox.get_active());
	manager.select_normalize_target (normalize_spinbutton.get_value ());
}

void
ExportFormatDialog::update_silence_start_selection ()
{
	update_time (silence_start, silence_start_clock);
	AnyTime zero;
	zero.type = AnyTime::Timecode;
	manager.select_silence_beginning (silence_start_checkbox.get_active() ? silence_start : zero);
}

void
ExportFormatDialog::update_silence_end_selection ()
{
	update_time (silence_end, silence_end_clock);
	AnyTime zero;
	zero.type = AnyTime::Timecode;
	manager.select_silence_end (silence_end_checkbox.get_active() ? silence_end : zero);
}

void
ExportFormatDialog::update_clock (AudioClock & clock, ARDOUR::AnyTime const & time)
{
	// TODO position
	clock.set (_session->convert_to_frames (time), true);

	AudioClock::Mode mode(AudioClock::Timecode);

	switch (time.type) {
	  case AnyTime::Timecode:
		mode = AudioClock::Timecode;
		break;
	  case AnyTime::BBT:
		mode = AudioClock::BBT;
		break;
	  case AnyTime::Frames:
		mode = AudioClock::Frames;
		break;
	  case AnyTime::Seconds:
		mode = AudioClock::MinSec;
		break;
	}

	clock.set_mode (mode);
}

void
ExportFormatDialog::update_time (AnyTime & time, AudioClock const & clock)
{
	if (!_session) {
		return;
	}

	framecnt_t frames = clock.current_duration();

	switch (clock.mode()) {
	  case AudioClock::Timecode:
		time.type = AnyTime::Timecode;
		_session->timecode_time (frames, time.timecode);
		break;
	  case AudioClock::BBT:
		time.type = AnyTime::BBT;
		_session->bbt_time (frames, time.bbt);
		break;
	  case AudioClock::MinSec:
		time.type = AnyTime::Seconds;
		time.seconds = (double) frames / _session->frame_rate();
		break;
	  case AudioClock::Frames:
		time.type = AnyTime::Frames;
		time.frames = frames;
		break;
	}
}

void
ExportFormatDialog::update_src_quality_selection ()
{
	Gtk::TreeModel::const_iterator iter = src_quality_combo.get_active();
	ExportFormatBase::SRCQuality quality = iter->get_value (src_quality_cols.id);
	manager.select_src_quality (quality);
}

void
ExportFormatDialog::update_tagging_selection ()
{
	manager.select_tagging (tag_checkbox.get_active());
}

void
ExportFormatDialog::change_encoding_options (ExportFormatPtr ptr)
{
	empty_encoding_option_table ();

	boost::shared_ptr<ARDOUR::ExportFormatLinear> linear_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatOggVorbis> ogg_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatFLAC> flac_ptr;
	boost::shared_ptr<ARDOUR::ExportFormatBWF> bwf_ptr;

	if ((linear_ptr = boost::dynamic_pointer_cast<ExportFormatLinear> (ptr))) {
		show_linear_enconding_options (linear_ptr);
	} else if ((ogg_ptr = boost::dynamic_pointer_cast<ExportFormatOggVorbis> (ptr))) {
		show_ogg_enconding_options (ogg_ptr);
	} else if ((flac_ptr = boost::dynamic_pointer_cast<ExportFormatFLAC> (ptr))) {
		show_flac_enconding_options (flac_ptr);
	} else if ((bwf_ptr = boost::dynamic_pointer_cast<ExportFormatBWF> (ptr))) {
		show_bwf_enconding_options (bwf_ptr);
	} else {
		std::cout << "Unrecognized format!" << std::endl;
	}
}

void
ExportFormatDialog::empty_encoding_option_table ()
{
	encoding_options_table.foreach (sigc::bind (sigc::mem_fun (*this, &ExportFormatDialog::remove_widget), &encoding_options_table));
}

void
ExportFormatDialog::remove_widget (Gtk::Widget & to_remove, Gtk::Container * remove_from)
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
ExportFormatDialog::show_ogg_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatOggVorbis> /*ptr*/)
{
	encoding_options_label.set_label (_("Ogg Vorbis options"));

	encoding_options_table.resize (1, 1);
	encoding_options_table.attach (tag_checkbox, 0, 1, 0, 1);

	update_tagging_selection ();

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
	encoding_options_table.attach (tag_checkbox, 0, 2, 2, 3);

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
ExportFormatDialog::fill_sample_format_lists (boost::shared_ptr<ARDOUR::HasSampleFormat> ptr)
{
	/* Fill lists */

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;

	sample_format_list->clear ();

	HasSampleFormat::SampleFormatList const & formats = ptr->get_sample_formats ();

	for (HasSampleFormat::SampleFormatList::const_iterator it = formats.begin(); it != formats.end(); ++it) {
		iter = sample_format_list->append();
		row = *iter;

		row[sample_format_cols.ptr] = *it;
		row[sample_format_cols.color] = (*it)->compatible() ? "white" : "red";
		row[sample_format_cols.label] = (*it)->name();

		if ((*it)->selected()) {
			sample_format_view.get_selection()->select (iter);
		}
	}

	dither_type_list->clear ();

	HasSampleFormat::DitherTypeList const & types = ptr->get_dither_types ();

	for (HasSampleFormat::DitherTypeList::const_iterator it = types.begin(); it != types.end(); ++it) {
		iter = dither_type_list->append();
		row = *iter;

		row[dither_type_cols.ptr] = *it;
		row[dither_type_cols.color] = "white";
		row[dither_type_cols.label] = (*it)->name();

		if ((*it)->selected()) {
			dither_type_view.get_selection()->select (iter);
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
	compatibility_view.get_selection()->unselect_all ();
	compatibility_select_connection.block (false);
}
