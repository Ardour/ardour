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

#ifndef __export_format_dialog_h__
#define __export_format_dialog_h__

#include "ardour/types.h"
#include "ardour/export_format_manager.h"
#include "ardour/export_format_compatibility.h"
#include "ardour/export_formats.h"

#include "pbd/xml++.h"
#include "pbd/signals.h"

#include "ardour_dialog.h"
#include "audio_clock.h"

#include <gtkmm.h>

class ExportFormatDialog : public ArdourDialog, public PBD::ScopedConnectionList {
  private:

	typedef ARDOUR::WeakExportFormatCompatibilityPtr WeakCompatPtr;
	typedef ARDOUR::WeakExportFormatPtr WeakFormatPtr;
	typedef ARDOUR::ExportFormatManager::WeakQualityPtr WeakQualityPtr;
	typedef ARDOUR::ExportFormatManager::WeakSampleRatePtr WeakSampleRatePtr;
	typedef ARDOUR::ExportFormatManager::WeakSampleFormatPtr WeakSampleFormatPtr;
	typedef ARDOUR::ExportFormatManager::WeakDitherTypePtr WeakDitherTypePtr;

	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;


  public:

	explicit ExportFormatDialog (FormatPtr format, bool new_dialog = false);
	~ExportFormatDialog ();

	void set_session (ARDOUR::Session* s);

  private:

	FormatPtr                   format;
	ARDOUR::ExportFormatManager manager;

	XMLNode & original_state;

	ARDOUR::AnyTime silence_start;
	ARDOUR::AnyTime silence_end;

	void end_dialog ();
	void revert ();

	/*** Init functions ***/

	void load_state (FormatPtr spec);
	void init_format_table ();
	void init_encoding_option_widgets ();

	/*** Interactive selections ***/

	/* These are connected to signals from GUI components, and should change element states  */

	void update_compatibility_selection (std::string const & path);
	void update_quality_selection ();
	void update_format_selection ();
	void update_sample_rate_selection ();
	void update_sample_format_selection ();
	void update_dither_type_selection ();

	template<typename ColsT>
	void update_selection (Glib::RefPtr<Gtk::ListStore> & list, Gtk::TreeView & view, ColsT & cols);

	/* These are connected to signals from elements, and should only update the gui */

	void change_compatibility_selection (bool select, WeakCompatPtr compat);

	void change_quality_selection (bool select, WeakQualityPtr quality);
	void change_format_selection (bool select, WeakFormatPtr format);
	void change_sample_rate_selection (bool select, WeakSampleRatePtr rate);
	void change_sample_format_selection (bool select, WeakSampleFormatPtr format);
	void change_dither_type_selection (bool select, WeakDitherTypePtr type);

	template<typename T, typename ColsT>
	void change_selection (bool select, boost::weak_ptr<T> w_ptr, Glib::RefPtr<Gtk::ListStore> & list, Gtk::TreeView & view, ColsT & cols);

	void change_quality_compatibility (bool compatibility, WeakQualityPtr quality);
	void change_format_compatibility (bool compatibility, WeakFormatPtr format);
	void change_sample_rate_compatibility (bool compatibility, WeakSampleRatePtr rate);
	void change_sample_format_compatibility (bool compatibility, WeakSampleFormatPtr format);
	void change_dither_type_compatibility (bool compatibility, WeakDitherTypePtr type);

	template<typename T, typename ColsT>
	void change_compatibility (bool compatibility, boost::weak_ptr<T> w_ptr, Glib::RefPtr<Gtk::ListStore> & list, ColsT & cols,
	                           std::string const & c_incompatible = "red", std::string const & c_compatible = "white");

	void update_description();

	uint32_t applying_changes_from_engine;

	/*** Non-interactive selections ***/

	void update_name ();

	void update_trim_start_selection ();
	void update_trim_end_selection ();

	void update_normalize_selection ();
	void update_silence_start_selection ();
	void update_silence_end_selection ();

	void update_clock (AudioClock & clock, ARDOUR::AnyTime const & time);
	void update_time (ARDOUR::AnyTime & time, AudioClock const & clock);

	void update_src_quality_selection ();
	void update_tagging_selection ();

	/*** Encoding options */

	void change_encoding_options (ARDOUR::ExportFormatPtr ptr);

	void empty_encoding_option_table ();
	void remove_widget (Gtk::Widget & to_remove, Gtk::Container * remove_from);

	void show_linear_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatLinear> ptr);
	void show_ogg_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatOggVorbis> ptr);
	void show_flac_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatFLAC> ptr);
	void show_bwf_enconding_options (boost::shared_ptr<ARDOUR::ExportFormatBWF> ptr);

	void fill_sample_format_lists (boost::shared_ptr<ARDOUR::HasSampleFormat> ptr);

	/*** GUI components ***/

	/* Name, new and remove */

	Gtk::HBox  name_hbox;

	Gtk::Label name_label;
	Gtk::Entry name_entry;
	Gtk::Label name_generated_part;

	/* Normalize */

	Gtk::HBox        normalize_hbox;
	Gtk::CheckButton normalize_checkbox;
	Gtk::SpinButton  normalize_spinbutton;
	Gtk::Adjustment  normalize_adjustment;
	Gtk::Label       normalize_db_label;

	/* Silence  */

	Gtk::Table       silence_table;

	Gtk::CheckButton trim_start_checkbox;
	Gtk::CheckButton silence_start_checkbox;
	AudioClock       silence_start_clock;

	Gtk::CheckButton trim_end_checkbox;
	Gtk::CheckButton silence_end_checkbox;
	AudioClock       silence_end_clock;

	/* Upload */
	
	Gtk::CheckButton upload_checkbox;
	Gtk::Label       command_label;
	Gtk::Entry       command_entry;

	/* Format table */

	struct CompatibilityCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::ExportFormatCompatibilityPtr>  ptr;
		Gtk::TreeModelColumn<bool>                                  selected;
		Gtk::TreeModelColumn<std::string>                           label;

		CompatibilityCols () { add(ptr); add(selected); add(label); }
	};
	CompatibilityCols            compatibility_cols;
	Glib::RefPtr<Gtk::ListStore> compatibility_list;

	/* Hack to disallow row selection in compatibilities */
	void prohibit_compatibility_selection ();
	sigc::connection compatibility_select_connection;

	struct QualityCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::ExportFormatManager::QualityPtr>  ptr;
		Gtk::TreeModelColumn<std::string>                            color;
		Gtk::TreeModelColumn<std::string>                            label;

		QualityCols () { add(ptr); add(color); add(label); }
	};
	QualityCols                  quality_cols;
	Glib::RefPtr<Gtk::ListStore> quality_list;

	struct FormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::ExportFormatPtr>  ptr;
		Gtk::TreeModelColumn<std::string>              color;
		Gtk::TreeModelColumn<std::string>              label;

		FormatCols () { add(ptr); add(color); add(label); }
	};
	FormatCols                   format_cols;
	Glib::RefPtr<Gtk::ListStore> format_list;

	struct SampleRateCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::ExportFormatManager::SampleRatePtr>  ptr;
		Gtk::TreeModelColumn<std::string>                               color;
		Gtk::TreeModelColumn<std::string>                               label;

		SampleRateCols () { add(ptr); add(color); add(label); }
	};
	SampleRateCols               sample_rate_cols;
	Glib::RefPtr<Gtk::ListStore> sample_rate_list;

	Gtk::Table       format_table;

	Gtk::Label       compatibility_label;
	Gtk::Label       quality_label;
	Gtk::Label       format_label;
	Gtk::Label       sample_rate_label;

	Gtk::TreeView    compatibility_view;
	Gtk::TreeView    quality_view;
	Gtk::TreeView    format_view;
	Gtk::TreeView    sample_rate_view;

	/* SRC quality combo */

	struct SRCQualityCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::ExportFormatBase::SRCQuality>  id;
		Gtk::TreeModelColumn<std::string>                         label;

		SRCQualityCols () { add(id); add(label); }
	};
	SRCQualityCols               src_quality_cols;
	Glib::RefPtr<Gtk::ListStore> src_quality_list;

	Gtk::Label      src_quality_label;
	Gtk::ComboBox   src_quality_combo;

	/* Common encoding option components */

	Gtk::VBox   encoding_options_vbox;
	Gtk::Label  encoding_options_label;

	Gtk::Table  encoding_options_table;

	/* Other common components */

	Gtk::Button * revert_button;
	Gtk::Button * close_button;

	/*** Changing encoding option stuff ***/

	/* Linear */

	struct SampleFormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::HasSampleFormat::SampleFormatPtr>   ptr;
		Gtk::TreeModelColumn<std::string>                              color;
		Gtk::TreeModelColumn<std::string>                              label;

		SampleFormatCols () { add(ptr); add(color); add(label); }
	};
	SampleFormatCols             sample_format_cols;
	Glib::RefPtr<Gtk::ListStore> sample_format_list;

	struct DitherTypeCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::HasSampleFormat::DitherTypePtr>   ptr;
		Gtk::TreeModelColumn<std::string>                            color;
		Gtk::TreeModelColumn<std::string>                            label;

		DitherTypeCols () { add(ptr); add (color); add(label); }
	};
	DitherTypeCols               dither_type_cols;
	Glib::RefPtr<Gtk::ListStore> dither_type_list;

	Gtk::Label  sample_format_label;
	Gtk::Label  dither_label;

	Gtk::CheckButton with_cue;
	Gtk::CheckButton with_toc;

	Gtk::VBox cue_toc_vbox;

	void update_with_toc ();
	void update_with_cue ();
	void update_upload ();
	void update_command ();

	Gtk::TreeView sample_format_view;
	Gtk::TreeView dither_type_view;

	/* Tagging */

	Gtk::CheckButton  tag_checkbox;

};

#endif /* __export_format_dialog_h__ */
