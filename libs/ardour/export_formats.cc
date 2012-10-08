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

#include "ardour/export_formats.h"

#include "i18n.h"

using namespace std;

namespace ARDOUR
{

bool
ExportFormat::has_sample_format ()
{
	return dynamic_cast<HasSampleFormat *> (this);
}

bool
ExportFormat::sample_format_is_compatible (SampleFormat format) const
{
	return (sample_formats.find (format) != sample_formats.end());
}

/*** HasSampleFormat ***/

HasSampleFormat::HasSampleFormat (ExportFormatBase::SampleFormatSet & sample_formats) :
  _sample_formats (sample_formats)
{
	/* Dither Types */

	add_dither_type (ExportFormatBase::D_Shaped, _("Shaped Noise"));
	add_dither_type (ExportFormatBase::D_Tri, _("Triangular"));
	add_dither_type (ExportFormatBase::D_Rect, _("Rectangular"));
	add_dither_type (ExportFormatBase::D_None,  _("None"));
}

void
HasSampleFormat::add_sample_format (ExportFormatBase::SampleFormat format)
{
	_sample_formats.insert (format);

	SampleFormatPtr ptr (new SampleFormatState (format, get_sample_format_name (format)));
	sample_format_states.push_back (ptr);
	ptr->SelectChanged.connect_same_thread (*this, boost::bind (&HasSampleFormat::update_sample_format_selection, this, _1));
	// BOOST SIGNALS Could this be made any uglier?
	ptr->SelectChanged.connect_same_thread (*this,
		boost::bind (boost::type<void> (), boost::ref (SampleFormatSelectChanged), _1, WeakSampleFormatPtr (ptr)));
	ptr->CompatibleChanged.connect_same_thread (*this,
		boost::bind (boost::type<void> (), boost::ref (SampleFormatCompatibleChanged), _1, WeakSampleFormatPtr (ptr)));
}

void
HasSampleFormat::add_dither_type (ExportFormatBase::DitherType type, string name)
{
	DitherTypePtr ptr (new DitherTypeState (type, name));
	dither_type_states.push_back (ptr);
	ptr->SelectChanged.connect_same_thread (*this, boost::bind (&HasSampleFormat::update_dither_type_selection, this, _1));
	// BOOST SIGNALS Could this be made any uglier?
	ptr->SelectChanged.connect_same_thread (*this,
		boost::bind (boost::type<void> (), boost::ref (DitherTypeSelectChanged), _1, WeakDitherTypePtr (ptr)));
	ptr->CompatibleChanged.connect_same_thread (*this,
		boost::bind (boost::type<void> (),boost::ref ( DitherTypeCompatibleChanged), _1, WeakDitherTypePtr (ptr)));
}

HasSampleFormat::SampleFormatPtr
HasSampleFormat::get_selected_sample_format ()
{
	for (SampleFormatList::iterator it = sample_format_states.begin(); it != sample_format_states.end(); ++it) {
		if ((*it)->selected()) {
			return *it;
		}
	}

	return SampleFormatPtr();
}

HasSampleFormat::DitherTypePtr
HasSampleFormat::get_selected_dither_type ()
{
	for (DitherTypeList::iterator it = dither_type_states.begin(); it != dither_type_states.end(); ++it) {
		if ((*it)->selected()) {
			return *it;
		}
	}

	return DitherTypePtr();
}

void
HasSampleFormat::update_sample_format_selection (bool)
{
	SampleFormatPtr format = get_selected_sample_format();
	if (!format) {
		return;
	}

	if (format->format == ExportFormatBase::SF_24 ||
	    format->format == ExportFormatBase::SF_32 ||
	    format->format == ExportFormatBase::SF_Float ||
	    format->format == ExportFormatBase::SF_Double) {
		for (DitherTypeList::iterator it = dither_type_states.begin(); it != dither_type_states.end(); ++it) {
			if ((*it)->type == ExportFormatBase::D_None) {
				(*it)->set_selected (true);
			} else {
				(*it)->set_compatible (false);
			}
		}

	} else {
		for (DitherTypeList::iterator it = dither_type_states.begin(); it != dither_type_states.end(); ++it) {
			(*it)->set_compatible (true);
		}
	}
}

void
HasSampleFormat::update_dither_type_selection (bool)
{
	DitherTypePtr type = get_selected_dither_type();
	if (!type) {
		return;
	}

	if (!type->compatible()) {
		SampleFormatPtr format = get_selected_sample_format();
		if (format) {
			format->set_selected (false);
		}

		for (DitherTypeList::iterator it = dither_type_states.begin(); it != dither_type_states.end(); ++it) {
			(*it)->set_compatible (true);
		}
	}
}

string
HasSampleFormat::get_sample_format_name (ExportFormatBase::SampleFormat format)
{
	switch (format) {
	  case ExportFormatBase::SF_8:
		return _("8bit");
	  case ExportFormatBase::SF_16:
		return _("16bit");
	  case ExportFormatBase::SF_24:
		return _("24bit");
	  case ExportFormatBase::SF_32:
		return _("32bit");
	  case ExportFormatBase::SF_Float:
		return _("float");
	  case ExportFormatBase::SF_Double:
		return _("double");
	  case ExportFormatBase::SF_U8:
		return _("8bit unsigned");
	  case ExportFormatBase::SF_Vorbis:
		return _("Vorbis sample format");
	  case ExportFormatBase::SF_None:
		return _("No sample format");
	}
	return "";
}

/*** Linear ***/

ExportFormatLinear::ExportFormatLinear (string name, FormatId format_id) :
  HasSampleFormat (sample_formats),
  _default_sample_format (SF_None)
{
	set_name (name);
	set_format_id (format_id);

	add_sample_rate (SR_8);
	add_sample_rate (SR_22_05);
	add_sample_rate (SR_44_1);
	add_sample_rate (SR_48);
	add_sample_rate (SR_88_2);
	add_sample_rate (SR_96);
	add_sample_rate (SR_192);
	add_sample_rate (SR_Session);

	add_endianness (E_FileDefault);

	set_quality (Q_LosslessLinear);
}

bool
ExportFormatLinear::set_compatibility_state (ExportFormatCompatibility const & compatibility)
{
	/* Global state */

	bool compatible = true;

	if (!compatibility.has_quality (Q_LosslessLinear)) {
		compatible = false;
	}

	if (!compatibility.has_format (get_format_id())) {
		compatible = false;
	}

	boost::shared_ptr<ExportFormatBase> intersection = get_intersection (compatibility);

	if (intersection->endiannesses_empty()) {
		compatible = false;
	}

	if (intersection->sample_rates_empty()) {
		compatible = false;
	}

	if (intersection->sample_formats_empty()) {
		compatible = false;
	}

	set_compatible (compatible);

	/* Sample Formats */

	for (SampleFormatList::iterator it = sample_format_states.begin(); it != sample_format_states.end(); ++it) {
		(*it)->set_compatible (compatibility.has_sample_format ((*it)->format));
	}

	return compatible;
}

/*** Ogg Vorbis ***/

ExportFormatOggVorbis::ExportFormatOggVorbis ()
{
	/* Check system compatibility */

	SF_INFO sf_info;
	sf_info.channels = 2;
	sf_info.samplerate = SR_44_1;
	sf_info.format = F_Ogg | SF_Vorbis;
	if (sf_format_check (&sf_info) != SF_TRUE) {
		throw ExportFormatIncompatible();
	}

	set_name ("Ogg Vorbis");
	set_format_id (F_Ogg);
	sample_formats.insert (SF_Vorbis);

	add_sample_rate (SR_22_05);
	add_sample_rate (SR_44_1);
	add_sample_rate (SR_48);
	add_sample_rate (SR_88_2);
	add_sample_rate (SR_96);
	add_sample_rate (SR_192);
	add_sample_rate (SR_Session);

	add_endianness (E_FileDefault);

	set_extension ("ogg");
	set_quality (Q_LossyCompression);
}

bool
ExportFormatOggVorbis::set_compatibility_state (ExportFormatCompatibility const & compatibility)
{
	bool compatible = compatibility.has_format (F_Ogg);
	set_compatible (compatible);
	return compatible;
}

/*** FLAC ***/

ExportFormatFLAC::ExportFormatFLAC () :
  HasSampleFormat (sample_formats)
{
	/* Check system compatibility */

	SF_INFO sf_info;
	sf_info.channels = 2;
	sf_info.samplerate = SR_44_1;
	sf_info.format = F_FLAC | SF_16;
	if (sf_format_check (&sf_info) != SF_TRUE) {
		throw ExportFormatIncompatible();
	}

	set_name ("FLAC");
	set_format_id (F_FLAC);

	add_sample_rate (SR_22_05);
	add_sample_rate (SR_44_1);
	add_sample_rate (SR_48);
	add_sample_rate (SR_88_2);
	add_sample_rate (SR_96);
	add_sample_rate (SR_192);
	add_sample_rate (SR_Session);

	add_sample_format (SF_8);
	add_sample_format (SF_16);
	add_sample_format (SF_24);

	add_endianness (E_FileDefault);

	set_extension ("flac");
	set_quality (Q_LosslessCompression);
}

bool
ExportFormatFLAC::set_compatibility_state (ExportFormatCompatibility const & compatibility)
{
	bool compatible = compatibility.has_format (F_FLAC);
	set_compatible (compatible);
	return compatible;
}

/*** BWF ***/

ExportFormatBWF::ExportFormatBWF () :
  HasSampleFormat (sample_formats)
{
	set_name ("BWF");
	set_format_id (F_WAV);

	add_sample_rate (SR_22_05);
	add_sample_rate (SR_44_1);
	add_sample_rate (SR_48);
	add_sample_rate (SR_88_2);
	add_sample_rate (SR_96);
	add_sample_rate (SR_192);
	add_sample_rate (SR_Session);

	add_sample_format (SF_U8);
	add_sample_format (SF_16);
	add_sample_format (SF_24);
	add_sample_format (SF_32);
	add_sample_format (SF_Float);
	add_sample_format (SF_Double);

	add_endianness (E_FileDefault);

	set_extension ("wav");
	set_quality (Q_LosslessLinear);
}

bool
ExportFormatBWF::set_compatibility_state (ExportFormatCompatibility const & compatibility)
{
	bool compatible = compatibility.has_format (F_WAV);
	set_compatible (compatible);
	return compatible;
}

}; // namespace ARDOUR
