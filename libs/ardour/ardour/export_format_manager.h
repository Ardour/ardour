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

#ifndef __ardour_export_format_manager_h__
#define __ardour_export_format_manager_h__

#include <list>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <sigc++/signal.h>
#include <sigc++/trackable.h>

#include "ardour/export_formats.h"

using std::string;

namespace ARDOUR
{

class ExportFormat;
class ExportFormatCompatibility;
class ExportFormatSpecification;
class AnyTime;

class ExportFormatManager : public sigc::trackable
{
  public:

	typedef boost::shared_ptr<ExportFormatCompatibility> CompatPtr;
	typedef boost::weak_ptr<ExportFormatCompatibility> WeakCompatPtr;
	typedef std::list<CompatPtr> CompatList;
	
	typedef boost::shared_ptr<ExportFormat> FormatPtr;
	typedef boost::weak_ptr<ExportFormat> WeakFormatPtr;
	typedef std::list<FormatPtr> FormatList;
	
	typedef HasSampleFormat::SampleFormatPtr SampleFormatPtr;
	typedef HasSampleFormat::SampleFormatList SampleFormatList;
	typedef HasSampleFormat::WeakSampleFormatPtr WeakSampleFormatPtr;
	
	typedef HasSampleFormat::DitherTypePtr DitherTypePtr;
	typedef HasSampleFormat::WeakDitherTypePtr WeakDitherTypePtr;
	
	typedef boost::shared_ptr<ExportFormatSpecification> SpecPtr;
	typedef boost::shared_ptr<ExportFormatBase> FormatBasePtr;
	
	/* Quality states */
	
	class QualityState : public ExportFormatBase::SelectableCompatible {
	  public:
		QualityState (ExportFormatBase::Quality quality, Glib::ustring name) :
		  quality (quality) { set_name (name); }
		ExportFormatBase::Quality  quality;
	};
	typedef boost::shared_ptr<QualityState> QualityPtr;
	typedef boost::weak_ptr<QualityState> WeakQualityPtr;
	typedef std::list<QualityPtr> QualityList;
	
	/* Sample rate states */
	
	class SampleRateState : public ExportFormatBase::SelectableCompatible {
	  public:
		SampleRateState (ExportFormatBase::SampleRate rate, Glib::ustring name) :
		  rate (rate) { set_name (name); }
		ExportFormatBase::SampleRate  rate;
	};
	typedef boost::shared_ptr<SampleRateState> SampleRatePtr;
	typedef boost::weak_ptr<SampleRateState> WeakSampleRatePtr;
	typedef std::list<SampleRatePtr> SampleRateList;

  public:

	explicit ExportFormatManager (SpecPtr specification);
	~ExportFormatManager ();

	/* Signals */

	sigc::signal<void, bool> CompleteChanged;

	/* Access to lists */

	CompatList const & get_compatibilities () { return compatibilities; }
	QualityList const & get_qualities () { return qualities; }
	FormatList const & get_formats () { return formats; }
	SampleRateList const & get_sample_rates () { return sample_rates; }
	
	/* Non interactive selections */
	
	void set_name (Glib::ustring name);
	
	void select_src_quality (ExportFormatBase::SRCQuality value);
	void select_trim_beginning (bool value);
	void select_silence_beginning (AnyTime const & time);
	void select_trim_end (bool value);
	void select_silence_end (AnyTime const & time);
	void select_normalize (bool value);
	void select_normalize_target (float value);
	void select_tagging (bool tag);

  private:

	void init_compatibilities ();
	void init_qualities ();
	void init_formats ();
	void init_sample_rates ();

	void add_compatibility (CompatPtr ptr);
	void add_quality (QualityPtr ptr);
	void add_format (FormatPtr ptr);
	void add_sample_rate (SampleRatePtr ptr);

	/* Connected to signals */
	
	void change_compatibility_selection (bool select, WeakCompatPtr const & compat);
	void change_quality_selection (bool select, WeakQualityPtr const & quality);
	void change_format_selection (bool select, WeakFormatPtr const & format);
	void change_sample_rate_selection (bool select, WeakSampleRatePtr const & rate);
	
	void change_sample_format_selection (bool select, WeakSampleFormatPtr const & format);
	void change_dither_type_selection (bool select, WeakDitherTypePtr const & type);
	
	/* Do actual selection */
	
	void select_compatibility (WeakCompatPtr const & compat);
	void select_quality (QualityPtr const & quality);
	void select_format (FormatPtr const & format);
	void select_sample_rate (SampleRatePtr const & rate);
	
	void select_sample_format (SampleFormatPtr const & format);
	void select_dither_type (DitherTypePtr const & type);
	
	bool pending_selection_change;
	void selection_changed ();
	
	/* Formats and compatibilities */
	
	QualityPtr    get_selected_quality ();
	FormatPtr     get_selected_format ();
	SampleRatePtr get_selected_sample_rate ();
	
	SampleFormatPtr get_selected_sample_format ();
	
	FormatBasePtr get_compatibility_intersection ();
	
	FormatBasePtr   universal_set;
	SpecPtr         current_selection;
	
	CompatList      compatibilities;
	QualityList     qualities;
	FormatList      formats;
	SampleRateList  sample_rates;

};

} // namespace ARDOUR

#endif /* __ardour_export_format_manager_h__ */
