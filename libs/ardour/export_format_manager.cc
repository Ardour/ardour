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

#include "ardour/export_format_manager.h"

#include "ardour/export_format_specification.h"
#include "ardour/export_format_compatibility.h"

#include "i18n.h"

using std::string;

namespace ARDOUR
{

ExportFormatManager::ExportFormatManager (ExportFormatSpecPtr specification) :
  pending_selection_change (false),
  universal_set (new ExportFormatBase ())
{
	current_selection = specification;

	init_compatibilities ();
	init_qualities ();
	init_formats ();
	init_sample_rates ();

	prev_description = current_selection->description();
}

ExportFormatManager::~ExportFormatManager ()
{

}

void
ExportFormatManager::init_compatibilities ()
{
	ExportFormatCompatibilityPtr c_ptr;

	c_ptr.reset (new ExportFormatCompatibility (_("CD")));
	c_ptr->add_sample_rate (ExportFormatBase::SR_44_1);
	c_ptr->add_format_id (ExportFormatBase::F_WAV);
	c_ptr->add_format_id (ExportFormatBase::F_AIFF);
	c_ptr->add_quality (ExportFormatBase::Q_LosslessLinear);
	c_ptr->add_sample_format (ExportFormatBase::SF_16);
	c_ptr->add_endianness (ExportFormatBase::E_FileDefault);
	add_compatibility (c_ptr);

	c_ptr.reset (new ExportFormatCompatibility (_("DVD-A")));
	c_ptr->add_sample_rate (ExportFormatBase::SR_44_1);
	c_ptr->add_sample_rate (ExportFormatBase::SR_48);
	c_ptr->add_sample_rate (ExportFormatBase::SR_88_2);
	c_ptr->add_sample_rate (ExportFormatBase::SR_96);
	c_ptr->add_sample_rate (ExportFormatBase::SR_192);
	c_ptr->add_format_id (ExportFormatBase::F_WAV);
	c_ptr->add_format_id (ExportFormatBase::F_AIFF);
	c_ptr->add_quality (ExportFormatBase::Q_LosslessLinear);
	c_ptr->add_sample_format (ExportFormatBase::SF_16);
	c_ptr->add_sample_format (ExportFormatBase::SF_24);
	c_ptr->add_endianness (ExportFormatBase::E_FileDefault);
	add_compatibility (c_ptr);

	c_ptr.reset (new ExportFormatCompatibility (_("iPod")));
	c_ptr->add_sample_rate (ExportFormatBase::SR_44_1);
	c_ptr->add_sample_rate (ExportFormatBase::SR_48);
	c_ptr->add_format_id (ExportFormatBase::F_WAV);
	c_ptr->add_format_id (ExportFormatBase::F_AIFF);
	c_ptr->add_quality (ExportFormatBase::Q_LosslessLinear);
	c_ptr->add_sample_format (ExportFormatBase::SF_16);
	c_ptr->add_sample_format (ExportFormatBase::SF_24);
	c_ptr->add_endianness (ExportFormatBase::E_FileDefault);
	add_compatibility (c_ptr);

	c_ptr.reset (new ExportFormatCompatibility (_("Something else")));
	c_ptr->add_sample_rate (ExportFormatBase::SR_44_1);
	c_ptr->add_sample_rate (ExportFormatBase::SR_48);
	c_ptr->add_format_id (ExportFormatBase::F_WAV);
	c_ptr->add_format_id (ExportFormatBase::F_AIFF);
	c_ptr->add_format_id (ExportFormatBase::F_AU);
	c_ptr->add_format_id (ExportFormatBase::F_FLAC);
	c_ptr->add_quality (ExportFormatBase::Q_LosslessLinear);
	c_ptr->add_quality (ExportFormatBase::Q_LosslessCompression);
	c_ptr->add_sample_format (ExportFormatBase::SF_16);
	c_ptr->add_sample_format (ExportFormatBase::SF_24);
	c_ptr->add_sample_format (ExportFormatBase::SF_32);
	c_ptr->add_endianness (ExportFormatBase::E_FileDefault);
	add_compatibility (c_ptr);
}

void
ExportFormatManager::init_qualities ()
{
	add_quality (QualityPtr (new QualityState (ExportFormatBase::Q_Any, _("Any"))));
	add_quality (QualityPtr (new QualityState (ExportFormatBase::Q_LosslessLinear, _("Lossless (linear PCM)"))));
	add_quality (QualityPtr (new QualityState (ExportFormatBase::Q_LossyCompression, _("Lossy compression"))));
	add_quality (QualityPtr (new QualityState (ExportFormatBase::Q_LosslessCompression, _("Lossless compression"))));
}

void
ExportFormatManager::init_formats ()
{
	ExportFormatPtr f_ptr;
	ExportFormatLinear * fl_ptr;

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("AIFF", ExportFormatBase::F_AIFF));
	fl_ptr->add_sample_format (ExportFormatBase::SF_U8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->add_endianness (ExportFormatBase::E_Big);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_16);
	fl_ptr->set_extension ("aiff");
	add_format (f_ptr);

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("AU", ExportFormatBase::F_AU));
	fl_ptr->add_sample_format (ExportFormatBase::SF_8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_16);
	fl_ptr->set_extension ("au");
	add_format (f_ptr);

	f_ptr.reset (new ExportFormatBWF ());
	add_format (f_ptr);

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("IRCAM", ExportFormatBase::F_IRCAM));
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_24);
	fl_ptr->set_extension ("sf");
	add_format (f_ptr);

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("WAV", ExportFormatBase::F_WAV));
	fl_ptr->add_sample_format (ExportFormatBase::SF_U8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->add_endianness (ExportFormatBase::E_Little);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_16);
	fl_ptr->set_extension ("wav");
	add_format (f_ptr);

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("W64", ExportFormatBase::F_W64));
	fl_ptr->add_sample_format (ExportFormatBase::SF_U8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->set_extension ("w64");
	add_format (f_ptr);

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("CAF", ExportFormatBase::F_CAF));
	fl_ptr->add_sample_format (ExportFormatBase::SF_U8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->set_extension ("caf");
	add_format (f_ptr);

	f_ptr.reset (fl_ptr = new ExportFormatLinear ("RAW", ExportFormatBase::F_RAW));
	fl_ptr->add_sample_format (ExportFormatBase::SF_U8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_8);
	fl_ptr->add_sample_format (ExportFormatBase::SF_16);
	fl_ptr->add_sample_format (ExportFormatBase::SF_24);
	fl_ptr->add_sample_format (ExportFormatBase::SF_32);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->add_sample_format (ExportFormatBase::SF_Double);
	fl_ptr->set_default_sample_format (ExportFormatBase::SF_Float);
	fl_ptr->set_extension ("raw");
	add_format (f_ptr);

	try {
		f_ptr.reset (new ExportFormatOggVorbis ());
		add_format (f_ptr);
	} catch (ExportFormatIncompatible & e) {}

	try {
		f_ptr.reset (new ExportFormatFLAC ());
		add_format (f_ptr);
	} catch (ExportFormatIncompatible & e) {}
}

void
ExportFormatManager::init_sample_rates ()
{
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_Session, _("Session rate"))));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_8, "8 kHz")));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_22_05, "22,05 kHz")));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_44_1, "44,1 kHz")));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_48, "48 kHz")));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_88_2, "88,2 kHz")));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_96, "96 kHz")));
	add_sample_rate (SampleRatePtr (new SampleRateState (ExportFormatBase::SR_192, "192 kHz")));
}

void
ExportFormatManager::add_compatibility (ExportFormatCompatibilityPtr ptr)
{
	compatibilities.push_back (ptr);
	ptr->SelectChanged.connect_same_thread (*this,
	                                        boost::bind (&ExportFormatManager::change_compatibility_selection,
	                                                     this, _1, WeakExportFormatCompatibilityPtr (ptr)));
}

void
ExportFormatManager::add_quality (QualityPtr ptr)
{
	ptr->SelectChanged.connect_same_thread (*this, boost::bind (&ExportFormatManager::change_quality_selection, this, _1, WeakQualityPtr (ptr)));
	qualities.push_back (ptr);
}

void
ExportFormatManager::add_format (ExportFormatPtr ptr)
{
	formats.push_back (ptr);
	ptr->SelectChanged.connect_same_thread (*this, boost::bind (&ExportFormatManager::change_format_selection, this, _1, WeakExportFormatPtr (ptr)));
	universal_set = universal_set->get_union (*ptr);

	/* Encoding options */

	boost::shared_ptr<HasSampleFormat> hsf;

	if ((hsf = boost::dynamic_pointer_cast<HasSampleFormat> (ptr))) {
		hsf->SampleFormatSelectChanged.connect_same_thread (*this, boost::bind (&ExportFormatManager::change_sample_format_selection, this, _1, _2));
		hsf->DitherTypeSelectChanged.connect_same_thread (*this, boost::bind (&ExportFormatManager::change_dither_type_selection, this, _1, _2));
	}
}

void
ExportFormatManager::add_sample_rate (SampleRatePtr ptr)
{
	ptr->SelectChanged.connect_same_thread (*this, boost::bind (&ExportFormatManager::change_sample_rate_selection, this, _1, WeakSampleRatePtr (ptr)));
	sample_rates.push_back (ptr);
}

void
ExportFormatManager::set_name (string name)
{
	current_selection->set_name (name);
	check_for_description_change ();
}

void
ExportFormatManager::select_src_quality (ExportFormatBase::SRCQuality value)
{
	current_selection->set_src_quality (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_with_cue (bool value)
{
	current_selection->set_with_cue (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_with_toc (bool value)
{
	current_selection->set_with_toc (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_upload (bool value)
{
	current_selection->set_upload (value);
	check_for_description_change ();
}

void
ExportFormatManager::set_command (std::string command)
{
	current_selection->set_command (command);
	check_for_description_change ();
}

void
ExportFormatManager::select_trim_beginning (bool value)
{
	current_selection->set_trim_beginning (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_silence_beginning (AnyTime const & time)
{
	current_selection->set_silence_beginning (time);
	check_for_description_change ();
}

void
ExportFormatManager::select_trim_end (bool value)
{
	current_selection->set_trim_end (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_silence_end (AnyTime const & time)
{
	current_selection->set_silence_end (time);
	check_for_description_change ();
}

void
ExportFormatManager::select_normalize (bool value)
{
	current_selection->set_normalize (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_normalize_target (float value)
{
	current_selection->set_normalize_target (value);
	check_for_description_change ();
}

void
ExportFormatManager::select_tagging (bool tag)
{
	current_selection->set_tag (tag);
	check_for_description_change ();
}

void
ExportFormatManager::change_compatibility_selection (bool select, WeakExportFormatCompatibilityPtr const & compat)
{
	bool do_selection_changed = !pending_selection_change;
	if (!pending_selection_change) {
		pending_selection_change = true;
	}

	ExportFormatCompatibilityPtr ptr = compat.lock();

	if (ptr && select) {
		select_compatibility (ptr);
	}

	if (do_selection_changed) {
		selection_changed ();
	}
}

void
ExportFormatManager::change_quality_selection (bool select, WeakQualityPtr const & quality)
{
	QualityPtr ptr = quality.lock ();

	if (!ptr) {
		return;
	}

	if (select) {
		select_quality (ptr);
	} else if (ptr->quality == current_selection->quality()) {
		ptr.reset();
		select_quality (ptr);
	}
}

void
ExportFormatManager::change_format_selection (bool select, WeakExportFormatPtr const & format)
{
	ExportFormatPtr ptr = format.lock();

	if (!ptr) {
		return;
	}

	if (select) {
		select_format (ptr);
	} else if (ptr->get_format_id() == current_selection->format_id()) {
		ptr.reset();
		select_format (ptr);
	}
}

void
ExportFormatManager::change_sample_rate_selection (bool select, WeakSampleRatePtr const & rate)
{
	SampleRatePtr ptr = rate.lock();

	if (!ptr) {
		return;
	}

	if (select) {
		select_sample_rate (ptr);
	} else if (ptr->rate == current_selection->sample_rate()) {
		ptr.reset();
		select_sample_rate (ptr);
	}
}

void
ExportFormatManager::change_sample_format_selection (bool select, WeakSampleFormatPtr const & format)
{
	SampleFormatPtr ptr = format.lock();

	if (!ptr) {
		return;
	}

	if (select) {
		select_sample_format (ptr);
	} else if (ptr->format == current_selection->sample_format()) {
		ptr.reset();
		select_sample_format (ptr);
	}
}

void
ExportFormatManager::change_dither_type_selection (bool select, WeakDitherTypePtr const & type)
{
	DitherTypePtr ptr = type.lock();

	if (!ptr) {
		return;
	}

	if (select) {
		select_dither_type (ptr);
	} else if (ptr->type == current_selection->dither_type()) {
		ptr.reset();
		select_dither_type (ptr);
	}
}

void
ExportFormatManager::select_compatibility (WeakExportFormatCompatibilityPtr const & /*compat*/)
{
	/* Calculate compatibility intersection for the selection */

	ExportFormatBasePtr compat_intersect = get_compatibility_intersection ();

	/* Unselect incompatible items */

	boost::shared_ptr<ExportFormatBase> select_intersect;

	select_intersect = compat_intersect->get_intersection (*current_selection);
	if (select_intersect->qualities_empty()) {
		select_quality (QualityPtr());
	}

	select_intersect = compat_intersect->get_intersection (*current_selection);
	if (select_intersect->formats_empty()) {
		select_format (ExportFormatPtr());
	}

	select_intersect = compat_intersect->get_intersection (*current_selection);
	if (select_intersect->sample_rates_empty()) {
		select_sample_rate (SampleRatePtr());
	}

	select_intersect = compat_intersect->get_intersection (*current_selection);
	if (select_intersect->sample_formats_empty()) {
		select_sample_format (SampleFormatPtr());
	}
}

void
ExportFormatManager::select_quality (QualityPtr const & quality)
{
	bool do_selection_changed = !pending_selection_change;
	if (!pending_selection_change) {
		pending_selection_change = true;
	}

	if (quality) {
		current_selection->set_quality (quality->quality);

		/* Deselect format if it is incompatible */

		ExportFormatPtr format = get_selected_format();
		if (format && !format->has_quality (quality->quality)) {
			format->set_selected (false);
		}

	} else {
		current_selection->set_quality (ExportFormatBase::Q_None);

		QualityPtr current_quality = get_selected_quality();
		if (current_quality) {
			current_quality->set_selected (false);
		}

		/* Note:
		 * A quality is never explicitly deselected without also deselecting the format
		 * so we don't need to deselect the format here.
		 * doing so causes extra complications
		 */
	}

	if (do_selection_changed) {
		selection_changed ();
	}
}

void
ExportFormatManager::select_format (ExportFormatPtr const & format)
{
	bool do_selection_changed = !pending_selection_change;
	if (!pending_selection_change) {
		pending_selection_change = true;
	}

	current_selection->set_format (format);

	if (format) {

		/* Slect right quality for format */

		ExportFormatBase::Quality quality = format->get_quality();
		for (QualityList::iterator it = qualities.begin (); it != qualities.end (); ++it) {
			if ((*it)->quality == quality) {
				(*it)->set_selected (true);
			} else {
				(*it)->set_selected (false);
			}
		}

		/* Handle sample formats */

		ExportFormatBase::SampleFormat format_to_select;
		if (format->sample_format_is_compatible (current_selection->sample_format())) {
			format_to_select = current_selection->sample_format();
		} else {
			format_to_select = format->default_sample_format();
		}

		boost::shared_ptr<HasSampleFormat> hsf;
		if ((hsf = boost::dynamic_pointer_cast<HasSampleFormat> (format))) {
			SampleFormatList sample_formats = hsf->get_sample_formats();
			for (SampleFormatList::iterator it = sample_formats.begin (); it != sample_formats.end (); ++it) {
				if ((*it)->format == format_to_select) {
					(*it)->set_selected (true);
				} else {
					(*it)->set_selected (false);
				}
			}
		}

		current_selection->set_sample_format (format_to_select);

	} else {
		ExportFormatPtr current_format = get_selected_format ();
		if (current_format) {
			current_format->set_selected (false);
		}
	}

	if (do_selection_changed) {
		selection_changed ();
	}
}

void
ExportFormatManager::select_sample_rate (SampleRatePtr const & rate)
{

	bool do_selection_changed = !pending_selection_change;
	if (!pending_selection_change) {
		pending_selection_change = true;
	}

	if (rate) {
		current_selection->set_sample_rate (rate->rate);
	} else {
		current_selection->set_sample_rate (ExportFormatBase::SR_None);

		SampleRatePtr current_rate = get_selected_sample_rate();
		if (current_rate) {
			current_rate->set_selected (false);
		}
	}

	if (do_selection_changed) {
		selection_changed ();
	}
}

void
ExportFormatManager::select_sample_format (SampleFormatPtr const & format)
{

	bool do_selection_changed = !pending_selection_change;
	if (!pending_selection_change) {
		pending_selection_change = true;
	}

	if (format) {
		current_selection->set_sample_format (format->format);
	} else {
		current_selection->set_sample_format (ExportFormatBase::SF_None);

		SampleFormatPtr current_format = get_selected_sample_format();
		if (current_format) {
			current_format->set_selected (false);
		}
	}

	if (do_selection_changed) {
		selection_changed ();
	}
}

void
ExportFormatManager::select_dither_type (DitherTypePtr const & type)
{

	bool do_selection_changed = !pending_selection_change;
	if (!pending_selection_change) {
		pending_selection_change = true;
	}

	if (type) {
		current_selection->set_dither_type (type->type);
	} else {
		current_selection->set_dither_type (ExportFormatBase::D_None);
	}

	if (do_selection_changed) {
		selection_changed ();
	}
}

void
ExportFormatManager::selection_changed ()
{
	/* Get a list of incompatible compatibility selections */

	CompatList incompatibles;
	for (CompatList::iterator it = compatibilities.begin(); it != compatibilities.end(); ++it) {
		if (!current_selection->is_compatible_with (**it)) {
			incompatibles.push_back (*it);
		}
	}

	/* Deselect them */

	for (CompatList::iterator it = incompatibles.begin(); it != incompatibles.end(); ++it) {
		(*it)->set_selected (false);
	}

	/* Mark compatibility for everything necessary */

	std::set<ExportFormatBase::Quality> compatible_qualities;
	ExportFormatBasePtr compat_intersect = get_compatibility_intersection ();
	ExportFormatCompatibility global_compat (*compat_intersect);

	for (FormatList::iterator it = formats.begin(); it != formats.end(); ++it) {
		if ((*it)->set_compatibility_state (global_compat)) {
			compatible_qualities.insert ((*it)->get_quality());
		}
	}

	bool any_quality_compatible = true;
	for (QualityList::iterator it = qualities.begin(); it != qualities.end(); ++it) {
		if (compatible_qualities.find((*it)->quality) != compatible_qualities.end()) {
			(*it)->set_compatible (true);

		} else {
			(*it)->set_compatible (false);

			if ((*it)->quality != ExportFormatBase::Q_Any) {
				any_quality_compatible = false;
			}
		}
	}

	if (any_quality_compatible) {
		for (QualityList::iterator it = qualities.begin(); it != qualities.end(); ++it) {
			if ((*it)->quality == ExportFormatBase::Q_Any) {
				(*it)->set_compatible (true);
				break;
			}
		}
	}

	for (SampleRateList::iterator it = sample_rates.begin(); it != sample_rates.end(); ++it) {
		if (compat_intersect->has_sample_rate ((*it)->rate)) {
			(*it)->set_compatible (true);
		} else {
			(*it)->set_compatible (false);
		}
	}

	boost::shared_ptr<HasSampleFormat> hsf;
	if ((hsf = boost::dynamic_pointer_cast<HasSampleFormat> (get_selected_format()))) {

		SampleFormatList sf_list = hsf->get_sample_formats();
		for (SampleFormatList::iterator it = sf_list.begin(); it != sf_list.end(); ++it) {
			if (compat_intersect->has_sample_format ((*it)->format)) {
				(*it)->set_compatible (true);
			} else {
				(*it)->set_compatible (false);
			}
		}

	}

	/* Signal completeness and possible description change */

	CompleteChanged (current_selection->is_complete());
	check_for_description_change ();

	/* Reset pending state */

	pending_selection_change = false;
}

void
ExportFormatManager::check_for_description_change ()
{
	std::string new_description = current_selection->description();
	if (new_description == prev_description) { return; }

	prev_description = new_description;
	DescriptionChanged();
}

ExportFormatManager::QualityPtr
ExportFormatManager::get_selected_quality ()
{
	for (QualityList::iterator it = qualities.begin(); it != qualities.end(); ++it) {
		if ((*it)->selected()) {
			return *it;
		}
	}

	return QualityPtr();
}

ExportFormatPtr
ExportFormatManager::get_selected_format ()
{
	ExportFormatPtr format;

	for (FormatList::iterator it = formats.begin(); it != formats.end(); ++it) {
		if ((*it)->selected()) {
			return *it;
		}
	}

	return format;
}

ExportFormatManager::SampleRatePtr
ExportFormatManager::get_selected_sample_rate ()
{
	for (SampleRateList::iterator it = sample_rates.begin(); it != sample_rates.end(); ++it) {
		if ((*it)->selected()) {
			return *it;
		}
	}

	return SampleRatePtr();
}

ExportFormatManager::SampleFormatPtr
ExportFormatManager::get_selected_sample_format ()
{
	boost::shared_ptr<HasSampleFormat> hsf;

	if ((hsf = boost::dynamic_pointer_cast<HasSampleFormat> (get_selected_format()))) {
		return hsf->get_selected_sample_format ();
	} else {
		return SampleFormatPtr ();
	}
}


ExportFormatBasePtr
ExportFormatManager::get_compatibility_intersection ()
{
	ExportFormatBasePtr compat_intersect = universal_set;

	for (CompatList::iterator it = compatibilities.begin(); it != compatibilities.end(); ++it) {
		if ((*it)->selected ()) {
			compat_intersect = compat_intersect->get_intersection (**it);
		}
	}

	return compat_intersect;
}

}; // namespace ARDOUR
