/*
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_export_format_specification_h__
#define __ardour_export_format_specification_h__

#include <string>

#include "pbd/uuid.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/export_format_base.h"

class XMLNode;

namespace ARDOUR
{

class ExportFormat;
class ExportFormatCompatibility;
class Session;

class LIBARDOUR_API ExportFormatSpecification : public ExportFormatBase {

  private:

	/* Time struct for keeping time formats as close as possible to what was requested */

	struct Time : public AnyTime {
		Time (Session & session) : AnyTime (), session (session) {}
		Time & operator= (AnyTime const & other);

		samplecnt_t get_samples_at (samplepos_t position, samplecnt_t target_rate) const;

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
	void set_normalize_loudness (bool value) { _normalize_loudness = value; }
	void set_use_tp_limiter (bool value) { _use_tp_limiter = value; }
	void set_normalize_dbfs (float value) { _normalize_dbfs = value; }
	void set_normalize_lufs (float value) { _normalize_lufs = value; }
	void set_normalize_dbtp (float value) { _normalize_dbtp = value; }

	void set_demo_noise_level    (float db) { _demo_noise_level = db; }
	void set_demo_noise_duration (int msec) { _demo_noise_duration = msec; }
	void set_demo_noise_interval (int msec) { _demo_noise_interval = msec; }

	void set_tag (bool tag_it) { _tag = tag_it; }
	void set_with_cue (bool yn) { _with_cue = yn; }
	void set_with_toc (bool yn) { _with_toc = yn; }
	void set_with_mp4chaps (bool yn) { _with_mp4chaps = yn; }
	void set_soundcloud_upload (bool yn) { _soundcloud_upload = yn; }
	void set_command (std::string command) { _command = command; }
	void set_analyse (bool yn) { _analyse = yn; }
	void set_codec_quality (int q) { _codec_quality = q; }

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

	FormatId format_id () const {
		if (!format_ids.empty() )
			return *format_ids.begin();
		else
			return FormatId(0);
	}

	Endianness endianness () const {
		if (!endiannesses.empty() )
			return *endiannesses.begin();
		else
			return Endianness(0);
	}

	SampleFormat sample_format () const {
		if (!sample_formats.empty() )
			return *sample_formats.begin();
		else
			return SampleFormat(0);
	}

	SampleRate sample_rate () const {
		if (!sample_rates.empty() )
			return *sample_rates.begin();
		else
			return SampleRate(0);

	}

	Quality quality () const {
		if (!qualities.empty() )
			return *qualities.begin();
		else
			return Quality(0);
	}

	DitherType dither_type () const { return _dither_type; }
	SRCQuality src_quality () const { return _src_quality; }
	bool trim_beginning () const { return _trim_beginning; }
	bool trim_end () const { return _trim_end; }
	bool normalize () const { return _normalize; }
	bool normalize_loudness () const { return _normalize_loudness; }
	bool use_tp_limiter () const { return _use_tp_limiter; }
	float normalize_dbfs () const { return _normalize_dbfs; }
	float normalize_lufs () const { return _normalize_lufs; }
	float normalize_dbtp () const { return _normalize_dbtp; }
	bool with_toc() const { return _with_toc; }
	bool with_cue() const { return _with_cue; }
	bool with_mp4chaps() const { return _with_mp4chaps; }

	float demo_noise_level () const    { return _demo_noise_level; }
	int   demo_noise_duration () const { return _demo_noise_duration; }
	int   demo_noise_interval () const { return _demo_noise_interval; }

	bool soundcloud_upload() const { return _soundcloud_upload; }
	std::string command() const { return _command; }
	bool analyse() const { return _analyse; }
	int  codec_quality() const { return _codec_quality; }

	bool tag () const { return _tag && _supports_tagging; }

	samplecnt_t silence_beginning_at (samplepos_t position, samplecnt_t samplerate) const
		{ return _silence_beginning.get_samples_at (position, samplerate); }
	samplecnt_t silence_end_at (samplepos_t position, samplecnt_t samplerate) const
		{ return _silence_end.get_samples_at (position, samplerate); }

	AnyTime silence_beginning_time () const { return _silence_beginning; }
	AnyTime silence_end_time () const { return _silence_end; }

	/* Serialization */

	XMLNode & get_state ();
	int set_state (const XMLNode & root);


  private:

	Session &        session;

	/* The variables below do not have setters (usually set via set_format) */

	std::string  _format_name;
	bool         _has_sample_format;
	bool         _supports_tagging;
	bool         _has_codec_quality;
	bool         _has_broadcast_info;
	uint32_t     _channel_limit;

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
	bool            _normalize_loudness;
	bool            _use_tp_limiter;
	float           _normalize_dbfs;
	float           _normalize_lufs;
	float           _normalize_dbtp;
	bool            _with_toc;
	bool            _with_cue;
	bool            _with_mp4chaps;
	bool            _soundcloud_upload;

	float           _demo_noise_level;
	int             _demo_noise_duration;
	int             _demo_noise_interval;

	std::string     _command;
	bool            _analyse;
	int             _codec_quality;

	/* serialization helpers */

	void add_option (XMLNode * node, std::string const & name, std::string const & value);
	std::string get_option (XMLNode const * node, std::string const & name);

};

} // namespace ARDOUR

#endif /* __ardour_export_format_specification_h__ */
