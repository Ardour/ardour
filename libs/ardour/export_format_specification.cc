/*
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include "ardour/export_format_specification.h"

#include "ardour/export_format_compatibility.h"
#include "ardour/export_formats.h"
#include "ardour/session.h"
#include "ardour/types_convert.h"

#include "pbd/error.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"
#include "pbd/enum_convert.h"
#include "pbd/string_convert.h"
#include "pbd/types_convert.h"

#include "pbd/i18n.h"

namespace PBD {
	DEFINE_ENUM_CONVERT (ARDOUR::ExportFormatBase::FormatId)
	DEFINE_ENUM_CONVERT (ARDOUR::ExportFormatBase::SampleRate)
	DEFINE_ENUM_CONVERT (ARDOUR::ExportFormatBase::SampleFormat)
	DEFINE_ENUM_CONVERT (ARDOUR::ExportFormatBase::DitherType)
	DEFINE_ENUM_CONVERT (ARDOUR::ExportFormatBase::SRCQuality)
	DEFINE_ENUM_CONVERT (ARDOUR::ExportFormatBase::Type)
}

namespace ARDOUR
{

using namespace PBD;
using std::string;
using std::list;

ExportFormatSpecification::Time &
ExportFormatSpecification::Time::operator= (AnyTime const & other)
{
	static_cast<AnyTime &>(*this) = other;
	return *this;
}

samplecnt_t
ExportFormatSpecification::Time::get_samples_at (samplepos_t position, samplecnt_t target_rate) const
{
	samplecnt_t duration = session.any_duration_to_samples (position, *this);
	return ((double) target_rate / session.sample_rate()) * duration + 0.5;
}

XMLNode &
ExportFormatSpecification::Time::get_state ()
{

	XMLNode * node = new XMLNode ("Duration");

	node->set_property ("format", type);

	switch (type) {
	  case Timecode:
		node->set_property ("hours", timecode.hours);
		node->set_property ("minutes", timecode.minutes);
		node->set_property ("seconds", timecode.seconds);
		node->set_property ("frames", timecode.frames);
		break;
	  case BBT:
		node->set_property ("bars", bbt.bars);
		node->set_property ("beats", bbt.beats);
		node->set_property ("ticks", bbt.ticks);
		break;
	  case Samples:
		node->set_property ("samples", samples);
		break;
	  case Seconds:
		node->set_property ("seconds", seconds);
		break;
	}

	return *node;
}

int
ExportFormatSpecification::Time::set_state (const XMLNode & node)
{
	if (!node.get_property ("format", type)) {
		return -1;
	}

	switch (type) {
	case Timecode:
		node.get_property ("hours", timecode.hours);
		node.get_property ("minutes", timecode.minutes);
		node.get_property ("seconds", timecode.seconds);
		node.get_property ("frames", timecode.frames);
		break;

	case BBT:
		node.get_property ("bars", bbt.bars);
		node.get_property ("beats", bbt.beats);
		node.get_property ("ticks", bbt.ticks);
		break;

	case Samples:
		node.get_property ("samples", samples);
		break;

	case Seconds:
		node.get_property ("seconds", seconds);
		break;

	}

	return 0;
}

ExportFormatSpecification::ExportFormatSpecification (Session & s)
	: session (s)
	, _has_sample_format (false)
	, _supports_tagging (false)
	, _has_codec_quality (false)
	, _has_broadcast_info (false)
	, _channel_limit (0)
	, _dither_type (D_None)
	, _src_quality (SRC_SincBest)
	, _tag (true)

	, _trim_beginning (false)
	, _silence_beginning (s)
	, _trim_end (false)
	, _silence_end (s)

	, _normalize (false)
	, _normalize_loudness (false)
	, _use_tp_limiter (true)
	, _normalize_dbfs (GAIN_COEFF_UNITY)
	, _normalize_lufs (-23)
	, _normalize_dbtp (-1)
	, _with_toc (false)
	, _with_cue (false)
	, _with_mp4chaps (false)
	, _soundcloud_upload (false)
	, _demo_noise_level (-20)
	, _demo_noise_duration (0)
	, _demo_noise_interval (0)
	, _command ("")
	, _analyse (true)
	, _codec_quality (0)
{
	format_ids.insert (F_None);
	endiannesses.insert (E_FileDefault);
	sample_formats.insert (SF_None);
	sample_rates.insert (SR_None);
	qualities.insert (Q_None);
}

ExportFormatSpecification::ExportFormatSpecification (Session & s, XMLNode const & state)
	: session (s)
	, _has_sample_format (false)
	, _supports_tagging (false)
	, _has_codec_quality (false)
	, _has_broadcast_info (false)
	, _channel_limit (0)
	, _dither_type (D_None)
	, _src_quality (SRC_SincBest)
	, _tag (true)

	, _trim_beginning (false)
	, _silence_beginning (s)
	, _trim_end (false)
	, _silence_end (s)

	, _normalize (false)
	, _normalize_loudness (false)
	, _use_tp_limiter (true)
	, _normalize_dbfs (GAIN_COEFF_UNITY)
	, _normalize_lufs (-23)
	, _normalize_dbtp (-1)
	, _with_toc (false)
	, _with_cue (false)
	, _with_mp4chaps (false)
	, _soundcloud_upload (false)
	, _demo_noise_level (-20)
	, _demo_noise_duration (0)
	, _demo_noise_interval (0)
	, _command ("")
	, _analyse (true)
	, _codec_quality (0)
{
	_silence_beginning.type = Time::Timecode;
	_silence_end.type = Time::Timecode;

	set_state (state);
}

ExportFormatSpecification::ExportFormatSpecification (ExportFormatSpecification const & other, bool modify_name)
	: ExportFormatBase(other)
	, session (other.session)
	, _silence_beginning (other.session)
	, _silence_end (other.session)
	, _with_toc (other._with_toc)
	, _with_cue (other._with_cue)
	, _with_mp4chaps (other._with_mp4chaps)
	, _soundcloud_upload (false)
	, _demo_noise_level (other._demo_noise_level)
	, _demo_noise_duration (other._demo_noise_duration)
	, _demo_noise_interval (other._demo_noise_interval)
	, _command (other._command)
	, _analyse (other._analyse)
	, _codec_quality (other._codec_quality)
{
	if (modify_name) {
		set_name (other.name() + " (copy)");
	} else {
		set_name (other.name());
	}

	_format_name = other._format_name;
	_has_sample_format = other._has_sample_format;
	_supports_tagging = other._supports_tagging;
	_has_codec_quality = other._has_codec_quality;
	_has_broadcast_info = other._has_broadcast_info;
	_channel_limit = other._channel_limit;

	set_type (other.type());
	set_format_id (other.format_id());
	set_endianness (other.endianness());
	set_sample_format (other.sample_format());
	set_sample_rate (other.sample_rate());
	set_quality (other.quality());

	set_dither_type (other.dither_type());
	set_src_quality (other.src_quality());
	set_trim_beginning (other.trim_beginning());
	set_trim_end (other.trim_end());
	set_normalize (other.normalize());
	set_normalize_loudness (other.normalize_loudness());
	set_use_tp_limiter (other.use_tp_limiter());
	set_normalize_dbfs (other.normalize_dbfs());
	set_normalize_lufs (other.normalize_lufs());
	set_normalize_dbtp (other.normalize_dbtp());

	set_tag (other.tag());

	set_silence_beginning (other.silence_beginning_time());
	set_silence_end (other.silence_end_time());

	set_extension(other.extension());
}

ExportFormatSpecification::~ExportFormatSpecification ()
{
}

XMLNode &
ExportFormatSpecification::get_state ()
{
	XMLNode * node;
	XMLNode * root = new XMLNode ("ExportFormatSpecification");

	root->set_property ("name", _name);
	root->set_property ("id", _id.to_s());
	root->set_property ("with-cue", _with_cue);
	root->set_property ("with-toc", _with_toc);
	root->set_property ("with-mp4chaps", _with_mp4chaps);
	root->set_property ("command", _command);
	root->set_property ("analyse", _analyse);
	root->set_property ("soundcloud-upload", _soundcloud_upload);

	node = root->add_child ("Encoding");
	node->set_property ("id", format_id());
	node->set_property ("type", type());
	node->set_property ("extension", extension());
	node->set_property ("name", _format_name);
	node->set_property ("has-sample-format", _has_sample_format);
	node->set_property ("channel-limit", _channel_limit);

	node = root->add_child ("SampleRate");
	node->set_property ("rate", sample_rate());

	node = root->add_child ("SRCQuality");
	node->set_property ("quality", src_quality());

	node = root->add_child ("Watermark");
	node->set_property ("level",    demo_noise_level ());
	node->set_property ("duration", demo_noise_duration ());
	node->set_property ("interval", demo_noise_interval ());

	if (_has_codec_quality) {
		node = root->add_child ("CodecQuality");
		node->set_property ("quality", codec_quality());
	}

	XMLNode * enc_opts = root->add_child ("EncodingOptions");

	add_option (enc_opts, "sample-format", to_string(sample_format()));
	add_option (enc_opts, "dithering", to_string (dither_type()));
	add_option (enc_opts, "tag-metadata", to_string (_tag));
	add_option (enc_opts, "tag-support", to_string (_supports_tagging));
	add_option (enc_opts, "broadcast-info", to_string (_has_broadcast_info));

	XMLNode * processing = root->add_child ("Processing");

	node = processing->add_child ("Normalize");
	node->set_property ("enabled", normalize());
	node->set_property ("loudness", normalize_loudness());
	node->set_property ("use-tp-limiter", use_tp_limiter());
	node->set_property ("dbfs", normalize_dbfs());
	node->set_property ("lufs", normalize_lufs());
	node->set_property ("dbtp", normalize_dbtp());

	XMLNode * silence = processing->add_child ("Silence");
	XMLNode * start = silence->add_child ("Start");
	XMLNode * end = silence->add_child ("End");

	node = start->add_child ("Trim");
	node->set_property ("enabled", trim_beginning());

	node = start->add_child ("Add");
	node->set_property ("enabled", _silence_beginning.not_zero());
	node->add_child_nocopy (_silence_beginning.get_state());

	node = end->add_child ("Trim");
	node->set_property ("enabled", trim_end());

	node = end->add_child ("Add");
	node->set_property ("enabled", _silence_end.not_zero());
	node->add_child_nocopy (_silence_end.get_state());

	return *root;
}

int
ExportFormatSpecification::set_state (const XMLNode & root)
{
	XMLNode const * child;
	string str;

	root.get_property ("name", _name);

	if (root.get_property ("id", str)) {
		_id = str;
	}

	if (!root.get_property ("with-cue", _with_cue)) {
		_with_cue = false;
	}

	if (!root.get_property ("with-toc", _with_toc)) {
		_with_toc = false;
	}

	if (!root.get_property ("with-mp4chaps", _with_mp4chaps)) {
		_with_mp4chaps = false;
	}

	if (!root.get_property ("command", _command)) {
		_command = "";
	}

	if (!root.get_property ("analyse", _analyse)) {
		_analyse = false;
	}

	if (!root.get_property ("soundcloud-upload", _soundcloud_upload)) {
		_soundcloud_upload = false;
	}

	/* Encoding and SRC */

	if ((child = root.child ("Encoding"))) {
		FormatId fid;
		if (child->get_property ("id", fid)) {
			set_format_id (fid);
		}

		ExportFormatBase::Type type;
		if (child->get_property ("type", type)) {
			set_type (type);
		}

		if (child->get_property ("extension", str)) {
			set_extension (str);
		}

		child->get_property ("name", _format_name);
		child->get_property ("has-sample-format", _has_sample_format);
		child->get_property ("channel-limit", _channel_limit);
	}

	if ((child = root.child ("SampleRate"))) {
		SampleRate rate;
		if (child->get_property ("rate", rate)) {
			set_sample_rate (rate);
		}
	}

	if ((child = root.child ("SRCQuality"))) {
		child->get_property ("quality", _src_quality);
	}

	if ((child = root.child ("Watermark"))) {
		child->get_property ("level", _demo_noise_level);
		child->get_property ("duration", _demo_noise_duration);
		child->get_property ("interval", _demo_noise_interval);
	}

	if ((child = root.child ("CodecQuality"))) {
		child->get_property ("quality", _codec_quality);
		_has_codec_quality = true;
	} else {
		_has_codec_quality = false;
	}

	/* fixup codec quality for old states */
	if (!_has_codec_quality) {
		/* We'd need an instance of ExportFormatManager to look up
		 * defaults for a given type -- in the future there may even be
		 * difference qualities depending on sub-type, so we just
		 * hardcode them here for the time being.
		 */
		if (format_id() == F_FFMPEG) {
			_codec_quality = -2; // ExportFormatOggVorbis::default_codec_quality();
		}
		else if (format_id() == F_Ogg) {
			_codec_quality = 40; // ExportFormatFFMPEG::default_codec_quality();
		}
	}

	/* Encoding options */

	if ((child = root.child ("EncodingOptions"))) {
		set_sample_format ((SampleFormat) string_2_enum (get_option (child, "sample-format"), SampleFormat));
		set_dither_type ((DitherType) string_2_enum (get_option (child, "dithering"), DitherType));
		set_tag (string_to<bool>(get_option (child, "tag-metadata")));
		_supports_tagging = string_to<bool>(get_option (child, "tag-support"));
		_has_broadcast_info = string_to<bool>(get_option (child, "broadcast-info"));
	}

	/* Processing */

	XMLNode const * proc = root.child ("Processing");
	if (!proc) { std::cerr << X_("Could not load processing for export format") << std::endl; return -1; }

	if ((child = proc->child ("Normalize"))) {
		child->get_property ("enabled", _normalize);
		child->get_property ("target", _normalize_dbfs); // old formats before ~ 4.7-930ish
		child->get_property ("loudness", _normalize_loudness);
		child->get_property ("use-tp-limiter", _use_tp_limiter);
		child->get_property ("dbfs", _normalize_dbfs);
		child->get_property ("lufs", _normalize_lufs);
		child->get_property ("dbtp", _normalize_dbtp);
	}

	XMLNode const * silence = proc->child ("Silence");
	if (!silence) { std::cerr << X_("Could not load silence for export format") << std::endl; return -1; }

	XMLNode const * start = silence->child ("Start");
	XMLNode const * end = silence->child ("End");
	if (!start || !end) { std::cerr << X_("Could not load end or start silence for export format") << std::endl; return -1; }

	/* Silence start */

	if ((child = start->child ("Trim"))) {
		child->get_property ("enabled", _trim_beginning);
	}

	bool enabled;
	if ((child = start->child ("Add"))) {
		if (child->get_property ("enabled", enabled) && enabled) {
			if ((child = child->child ("Duration"))) {
				_silence_beginning.set_state (*child);
			}
		} else {
			_silence_beginning.type = Time::Timecode;
		}
	}

	/* Silence end */

	if ((child = end->child ("Trim"))) {
		child->get_property ("enabled", _trim_end);
	}

	if ((child = end->child ("Add"))) {
		if (child->get_property ("enabled", enabled) && enabled) {
			if ((child = child->child ("Duration"))) {
				_silence_end.set_state (*child);
			}
		} else {
				_silence_end.type = Time::Timecode;
		}
	}

	return 0;
}

bool
ExportFormatSpecification::is_compatible_with (ExportFormatCompatibility const & compatibility) const
{
	boost::shared_ptr<ExportFormatBase> intersection = get_intersection (compatibility);

	if (intersection->formats_empty() && format_id() != 0) {
		return false;
	}

	if (intersection->endiannesses_empty() && endianness() != E_FileDefault) {
		return false;
	}

	if (intersection->sample_rates_empty() && sample_rate() != SR_None) {
		return false;
	}

	if (intersection->sample_formats_empty() && sample_format() != SF_None) {
		return false;
	}

	if (intersection->qualities_empty() && quality() != Q_None) {
		return false;
	}

	return true;
}

bool
ExportFormatSpecification::is_complete () const
{
	if (type() == T_None) {
		return false;
	}

	if (!format_id()) {
		return false;
	}

	if (!sample_rate()) {
		return false;
	}

	if (_has_sample_format) {
		if (sample_format() == SF_None) {
			return false;
		}
	}

	return true;
}

void
ExportFormatSpecification::set_format (boost::shared_ptr<ExportFormat> format)
{
	if (format) {
		FormatId new_fmt = format->get_format_id ();
		bool fmt_changed = format_id() != new_fmt;
		set_format_id (new_fmt);

		set_type (format->get_type());
		set_extension (format->extension());

		if (format->get_explicit_sample_format()) {
			set_sample_format (format->get_explicit_sample_format());
		}

		if (format->has_sample_format()) {
			_has_sample_format = true;
		}

		if (format->has_broadcast_info()) {
			_has_broadcast_info = true;
		}

		_has_codec_quality = format->has_codec_quality();
		if (!_has_codec_quality) {
			_codec_quality = 0;
		} else if (fmt_changed) {
			_codec_quality = boost::dynamic_pointer_cast<HasCodecQuality> (format)->default_codec_quality();
		}

		_supports_tagging = format->supports_tagging ();
		_channel_limit = format->get_channel_limit();

		_format_name = format->name();
	} else {
		set_format_id (F_None);
		set_type (T_None);
		set_extension ("");
		_has_broadcast_info = false;
		_has_sample_format = false;
		_supports_tagging = false;
		_channel_limit = 0;
		_codec_quality = 0;
		_format_name = "";
	}
}

string
ExportFormatSpecification::description (bool include_name)
{
	list<string> components;

	if (_normalize) {
		if (_normalize_loudness) {
			components.push_back (_("normalize loudness"));
			if  (_use_tp_limiter) {
				components.push_back (_("limit peak"));
			}
		} else {
			components.push_back (_("normalize peak"));
		}
	}

	if (_trim_beginning && _trim_end) {
		components.push_back ( _("trim"));
	} else if (_trim_beginning) {
		components.push_back (_("trim start"));
	} else if (_trim_end) {
		components.push_back (_("trim end"));
	}

	if (_format_name != "") {
		components.push_back (_format_name);
	}

	if (_has_sample_format) {
		components.push_back (HasSampleFormat::get_sample_format_name (sample_format()));
	}

	switch (sample_rate()) {
	case SR_8:
		components.push_back ("8 kHz");
		break;
	case SR_22_05:
		components.push_back ("22,5 kHz");
		break;
	case SR_44_1:
		components.push_back ("44,1 kHz");
		break;
	case SR_48:
		components.push_back ("48 kHz");
		break;
	case SR_88_2:
		components.push_back ("88,2 kHz");
		break;
	case SR_96:
		components.push_back ("96 kHz");
		break;
	case SR_176_4:
		components.push_back ("176.4 kHz");
		break;
	case SR_192:
		components.push_back ("192 kHz");
		break;
	case SR_Session:
		components.push_back (_("Session rate"));
		break;
	case SR_None:
		break;
	}

	if (_with_toc) {
		components.push_back ("TOC");
	}

	if (_with_cue) {
		components.push_back ("CUE");
	}

	if (_demo_noise_duration > 0 && _demo_noise_interval > 0) {
		components.push_back ("Demo-Noise");
	}

	if (!_command.empty()) {
		components.push_back ("+");
	}

	string desc;
	if (include_name) {
		desc = _name + ": ";
	}

	for (list<string>::const_iterator it = components.begin(); it != components.end(); ++it) {
		if (it != components.begin()) { desc += ", "; }
		desc += *it;
	}
	return desc;
}

void
ExportFormatSpecification::add_option (XMLNode * node, std::string const & name, std::string const & value)
{
	node = node->add_child ("Option");
	node->set_property ("name", name);
	node->set_property ("value", value);
}

std::string
ExportFormatSpecification::get_option (XMLNode const * node, std::string const & name)
{
	XMLNodeList list (node->children ("Option"));

	for (XMLNodeList::iterator it = list.begin(); it != list.end(); ++it) {
		std::string str;
		if ((*it)->get_property ("name", str) && name == str) {
			if ((*it)->get_property ("value", str)) {
				return str;
			}
		}
	}

	std::cerr << "Could not load encoding option \"" << name << "\" for export format" << std::endl;

	return "";
}

}; // namespace ARDOUR
