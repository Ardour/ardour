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

#ifndef __ardour_export_format_specification_h__
#define __ardour_export_format_specification_h__

#include <string>

#include "pbd/uuid.h"

#include "ardour/types.h"
#include "ardour/export_format_base.h"

class XMLNode;

namespace ARDOUR
{

class ExportFormat;
class ExportFormatCompatibility;
class Session;

class ExportFormatSpecification : public ExportFormatBase {

  private:

	/* Time struct for keeping time formats as close as possible to what was requested */

	struct Time : public AnyTime {
		Time (Session & session) : AnyTime (), session (session) {}
		Time & operator= (AnyTime const & other);

		framecnt_t get_frames_at (framepos_t position, framecnt_t target_rate) const;

		/* Serialization */

		XMLNode & get_state ();
		int set_state (const XMLNode & node);

	  private:
		Session & session;
	};

  private:
	friend class ExportElementFactory;
	explicit ExportFormatSpecification (Session & s);
	ExportFormatSpecification (Session & s, XMLNode const & state);

  public:
	ExportFormatSpecification (ExportFormatSpecification const & other, bool modify_name = true);
	~ExportFormatSpecification ();

	/* compatibility */

	bool is_compatible_with (ExportFormatCompatibility const & compatibility) const;
	bool is_complete () const;

	/* Modifying functions */

	void set_format (boost::shared_ptr<ExportFormat> format);

	void set_name (std::string const & name) { _name = name; }

	void set_type (Type type) { _type = type; }
	void set_format_id (FormatId value) { format_ids.clear(); format_ids.insert (value); }
	void set_endianness (Endianness value) { endiannesses.clear(); endiannesses.insert (value); }
	void set_sample_format (SampleFormat value) { sample_formats.clear(); sample_formats.insert (value); }
	void set_sample_rate (SampleRate value) { sample_rates.clear(); sample_rates.insert (value); }
	void set_quality (Quality value) { qualities.clear(); qualities.insert (value); }

	void set_dither_type (DitherType value) { _dither_type = value; }
	void set_src_quality (SRCQuality value) { _src_quality = value; }
	void set_trim_beginning (bool value) { _trim_beginning = value; }
	void set_trim_end (bool value) { _trim_end = value; }
	void set_normalize (bool value) { _normalize = value; }
	void set_normalize_target (float value) { _normalize_target = value; }

	void set_tag (bool tag_it) { _tag = tag_it; }
	void set_with_cue (bool yn) { _with_cue = yn; }
	void set_with_toc (bool yn) { _with_toc = yn; }

	void set_silence_beginning (AnyTime const & value) { _silence_beginning = value; }
	void set_silence_end (AnyTime const & value) { _silence_end = value; }

	/* Accessing functions */

	PBD::UUID const & id () { return _id; }
	std::string const & name () const { return _name; }
	std::string description (bool include_name = true);

	bool has_broadcast_info () const { return _has_broadcast_info; }
	uint32_t channel_limit () const { return _channel_limit; }
	std::string format_name () const { return _format_name; }

	Type type () const { return _type; }
	FormatId format_id () const { return *format_ids.begin(); }
	Endianness endianness () const { return *endiannesses.begin(); }
	SampleFormat sample_format () const { return *sample_formats.begin(); }
	SampleRate sample_rate () const { return *sample_rates.begin(); }
	Quality quality () const { return *qualities.begin(); }

	DitherType dither_type () const { return _dither_type; }
	SRCQuality src_quality () const { return _src_quality; }
	bool trim_beginning () const { return _trim_beginning; }
	bool trim_end () const { return _trim_end; }
	bool normalize () const { return _normalize; }
	float normalize_target () const { return _normalize_target; }
	bool with_toc() const { return _with_toc; }
	bool with_cue() const { return _with_cue; }

	bool tag () const { return _tag && supports_tagging; }

	framecnt_t silence_beginning_at (framepos_t position, framecnt_t samplerate) const
		{ return _silence_beginning.get_frames_at (position, samplerate); }
	framecnt_t silence_end_at (framepos_t position, framecnt_t samplerate) const
		{ return _silence_end.get_frames_at (position, samplerate); }

	AnyTime silence_beginning_time () const { return _silence_beginning; }
	AnyTime silence_end_time () const { return _silence_end; }

	/* Serialization */

	XMLNode & get_state ();
	int set_state (const XMLNode & root);


  private:

	Session &        session;

	/* The variables below do not have setters (usually set via set_format) */

	std::string  _format_name;
	bool            has_sample_format;
	bool            supports_tagging;
	bool           _has_broadcast_info;
	uint32_t       _channel_limit;

	/* The variables below have getters and setters */

	std::string   _name;
	PBD::UUID       _id;

	Type            _type;
	DitherType      _dither_type;
	SRCQuality      _src_quality;

	bool            _tag;

	bool            _trim_beginning;
	Time            _silence_beginning;
	bool            _trim_end;
	Time            _silence_end;

	bool            _normalize;
	float           _normalize_target;
	bool            _with_toc;
	bool            _with_cue;

	/* serialization helpers */

	void add_option (XMLNode * node, std::string const & name, std::string const & value);
	std::string get_option (XMLNode const * node, std::string const & name);

};

} // namespace ARDOUR

#endif /* __ardour_export_format_specification_h__ */
