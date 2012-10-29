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

#include "ardour/export_format_specification.h"

#include <sstream>

#include "ardour/export_format_compatibility.h"
#include "ardour/export_formats.h"
#include "ardour/session.h"

#include "pbd/error.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"
#include "pbd/convert.h"

#include "i18n.h"

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

framecnt_t
ExportFormatSpecification::Time::get_frames_at (framepos_t position, framecnt_t target_rate) const
{
	framecnt_t duration = session.any_duration_to_frames (position, *this);
	return ((double) target_rate / session.frame_rate()) * duration + 0.5;
}

XMLNode &
ExportFormatSpecification::Time::get_state ()
{

	XMLNode * node = new XMLNode ("Duration");

	node->add_property ("format", enum_2_string (type));

	switch (type) {
	  case Timecode:
		node->add_property ("hours", to_string (timecode.hours, std::dec));
		node->add_property ("minutes", to_string (timecode.minutes, std::dec));
		node->add_property ("seconds", to_string (timecode.seconds, std::dec));
		node->add_property ("frames", to_string (timecode.frames, std::dec));
		break;
	  case BBT:
		node->add_property ("bars", to_string (bbt.bars, std::dec));
		node->add_property ("beats", to_string (bbt.beats, std::dec));
		node->add_property ("ticks", to_string (bbt.ticks, std::dec));
		break;
	  case Frames:
		node->add_property ("frames", to_string (frames, std::dec));
		break;
	  case Seconds:
		node->add_property ("seconds", to_string (seconds, std::dec));
		break;
	}

	return *node;
}

int
ExportFormatSpecification::Time::set_state (const XMLNode & node)
{
	XMLProperty const * prop;

	prop = node.property ("format");

	if (!prop) { return -1; }

	type = (Type) string_2_enum (prop->value(), Type);

	switch (type) {
	  case Timecode:
		if ((prop = node.property ("hours"))) {
			timecode.hours = atoi (prop->value());
		}

		if ((prop = node.property ("minutes"))) {
			timecode.minutes = atoi (prop->value());
		}

		if ((prop = node.property ("seconds"))) {
			timecode.seconds = atoi (prop->value());
		}

		if ((prop = node.property ("frames"))) {
			timecode.frames = atoi (prop->value());
		}

		break;

	  case BBT:
		if ((prop = node.property ("bars"))) {
			bbt.bars = atoi (prop->value());
		}

		if ((prop = node.property ("beats"))) {
			bbt.beats = atoi (prop->value());
		}

		if ((prop = node.property ("ticks"))) {
			bbt.ticks = atoi (prop->value());
		}

		break;

	  case Frames:
		if ((prop = node.property ("frames"))) {
			std::istringstream iss (prop->value());
			iss >> frames;
		}

		break;

	  case Seconds:
		if ((prop = node.property ("seconds"))) {
			seconds = atof (prop->value());
		}

		break;
	}

	return 0;
}

ExportFormatSpecification::ExportFormatSpecification (Session & s)
	: session (s)

	, has_sample_format (false)
	, supports_tagging (false)
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
	, _normalize_target (1.0)
	, _with_toc (false)
	, _with_cue (false)
{
	format_ids.insert (F_None);
	endiannesses.insert (E_FileDefault);
	sample_formats.insert (SF_None);
	sample_rates.insert (SR_None);
	qualities.insert (Q_None);
}

ExportFormatSpecification::ExportFormatSpecification (Session & s, XMLNode const & state)
	: session (s)
	, _silence_beginning (s)
	, _silence_end (s)
{
	_silence_beginning.type = Time::Timecode;
	_silence_end.type = Time::Timecode;

	set_state (state);
}

ExportFormatSpecification::ExportFormatSpecification (ExportFormatSpecification const & other)
	: ExportFormatBase(other)
	, session (other.session)
	, _silence_beginning (other.session)
	, _silence_end (other.session)
{
	set_name (other.name() + " (copy)");

	_format_name = other._format_name;
	has_sample_format = other.has_sample_format;

	supports_tagging = other.supports_tagging;
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
	set_normalize_target (other.normalize_target());

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

	root->add_property ("name", _name);
	root->add_property ("id", _id.to_s());
	root->add_property ("with-cue", _with_cue ? "true" : "false");
	root->add_property ("with-toc", _with_toc ? "true" : "false");

	node = root->add_child ("Encoding");
	node->add_property ("id", enum_2_string (format_id()));
	node->add_property ("type", enum_2_string (type()));
	node->add_property ("extension", extension());
	node->add_property ("name", _format_name);
	node->add_property ("has-sample-format", has_sample_format ? "true" : "false");
	node->add_property ("channel-limit", to_string (_channel_limit, std::dec));

	node = root->add_child ("SampleRate");
	node->add_property ("rate", to_string (sample_rate(), std::dec));

	node = root->add_child ("SRCQuality");
	node->add_property ("quality", enum_2_string (src_quality()));

	XMLNode * enc_opts = root->add_child ("EncodingOptions");

	add_option (enc_opts, "sample-format", enum_2_string (sample_format()));
	add_option (enc_opts, "dithering", enum_2_string (dither_type()));
	add_option (enc_opts, "tag-metadata", _tag ? "true" : "false");
	add_option (enc_opts, "tag-support", supports_tagging ? "true" : "false");
	add_option (enc_opts, "broadcast-info", _has_broadcast_info ? "true" : "false");

	XMLNode * processing = root->add_child ("Processing");

	node = processing->add_child ("Normalize");
	node->add_property ("enabled", normalize() ? "true" : "false");
	node->add_property ("target", to_string (normalize_target(), std::dec));

	XMLNode * silence = processing->add_child ("Silence");
	XMLNode * start = silence->add_child ("Start");
	XMLNode * end = silence->add_child ("End");

	node = start->add_child ("Trim");
	node->add_property ("enabled", trim_beginning() ? "true" : "false");

	node = start->add_child ("Add");
	node->add_property ("enabled", _silence_beginning.not_zero() ? "true" : "false");
	node->add_child_nocopy (_silence_beginning.get_state());

	node = end->add_child ("Trim");
	node->add_property ("enabled", trim_end() ? "true" : "false");

	node = end->add_child ("Add");
	node->add_property ("enabled", _silence_end.not_zero() ? "true" : "false");
	node->add_child_nocopy (_silence_end.get_state());

	return *root;
}

int
ExportFormatSpecification::set_state (const XMLNode & root)
{
	XMLProperty const * prop;
	XMLNode const * child;
	string value;

	if ((prop = root.property ("name"))) {
		_name = prop->value();
	}

	if ((prop = root.property ("id"))) {
		_id = prop->value();
	}

	if ((prop = root.property ("with-cue"))) {
		_with_cue = string_is_affirmative (prop->value());
	} else {
		_with_cue = false;
	}
	
	if ((prop = root.property ("with-toc"))) {
		_with_toc = string_is_affirmative (prop->value());
	} else {
		_with_toc = false;
	}
	
	/* Encoding and SRC */

	if ((child = root.child ("Encoding"))) {
		if ((prop = child->property ("id"))) {
			set_format_id ((FormatId) string_2_enum (prop->value(), FormatId));
		}

		if ((prop = child->property ("type"))) {
			set_type ((Type) string_2_enum (prop->value(), Type));
		}

		if ((prop = child->property ("extension"))) {
			set_extension (prop->value());
		}

		if ((prop = child->property ("name"))) {
			_format_name = prop->value();
		}

		if ((prop = child->property ("has-sample-format"))) {
			has_sample_format = string_is_affirmative (prop->value());
		}

		if ((prop = child->property ("has-sample-format"))) {
			has_sample_format = string_is_affirmative (prop->value());
		}

		if ((prop = child->property ("channel-limit"))) {
			_channel_limit = atoi (prop->value());
		}
	}

	if ((child = root.child ("SampleRate"))) {
		if ((prop = child->property ("rate"))) {
			set_sample_rate ( (SampleRate) string_2_enum (prop->value(), SampleRate));
		}
	}

	if ((child = root.child ("SRCQuality"))) {
		if ((prop = child->property ("quality"))) {
			_src_quality = (SRCQuality) string_2_enum (prop->value(), SRCQuality);
		}
	}

	/* Encoding options */

	if ((child = root.child ("EncodingOptions"))) {
		set_sample_format ((SampleFormat) string_2_enum (get_option (child, "sample-format"), SampleFormat));
		set_dither_type ((DitherType) string_2_enum (get_option (child, "dithering"), DitherType));
		set_tag (!(get_option (child, "tag-metadata").compare ("true")));
		supports_tagging = (!(get_option (child, "tag-support").compare ("true")));
		_has_broadcast_info = (!(get_option (child, "broadcast-info").compare ("true")));
	}

	/* Processing */

	XMLNode const * proc = root.child ("Processing");
	if (!proc) { std::cerr << X_("Could not load processing for export format") << std::endl; return -1; }

	if ((child = proc->child ("Normalize"))) {
		if ((prop = child->property ("enabled"))) {
			_normalize = (!prop->value().compare ("true"));
		}

		if ((prop = child->property ("target"))) {
			_normalize_target = atof (prop->value());
		}
	}

	XMLNode const * silence = proc->child ("Silence");
	if (!silence) { std::cerr << X_("Could not load silence for export format") << std::endl; return -1; }

	XMLNode const * start = silence->child ("Start");
	XMLNode const * end = silence->child ("End");
	if (!start || !end) { std::cerr << X_("Could not load end or start silence for export format") << std::endl; return -1; }

	/* Silence start */

	if ((child = start->child ("Trim"))) {
		if ((prop = child->property ("enabled"))) {
			_trim_beginning = (!prop->value().compare ("true"));
		}
	}

	if ((child = start->child ("Add"))) {
		if ((prop = child->property ("enabled"))) {
			if (!prop->value().compare ("true")) {
				if ((child = child->child ("Duration"))) {
					_silence_beginning.set_state (*child);
				}
			} else {
				_silence_beginning.type = Time::Timecode;
			}
		}
	}

	/* Silence end */

	if ((child = end->child ("Trim"))) {
		if ((prop = child->property ("enabled"))) {
			_trim_end = (!prop->value().compare ("true"));
		}
	}

	if ((child = end->child ("Add"))) {
		if ((prop = child->property ("enabled"))) {
			if (!prop->value().compare ("true")) {
				if ((child = child->child ("Duration"))) {
					_silence_end.set_state (*child);
				}
			} else {
				_silence_end.type = Time::Timecode;
			}
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

	if (has_sample_format) {
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
		set_format_id (format->get_format_id ());
		set_type (format->get_type());
		set_extension (format->extension());

		if (format->get_explicit_sample_format()) {
			set_sample_format (format->get_explicit_sample_format());
		}

		if (format->has_sample_format()) {
			has_sample_format = true;
		}

		if (format->has_broadcast_info()) {
			_has_broadcast_info = true;
		}

		supports_tagging = format->supports_tagging ();
		_channel_limit = format->get_channel_limit();

		_format_name = format->name();
	} else {
		set_format_id (F_None);
		set_type (T_None);
		set_extension ("");
		_has_broadcast_info = false;
		has_sample_format = false;
		supports_tagging = false;
		_channel_limit = 0;
		_format_name = "";
	}
}

string
ExportFormatSpecification::description (bool include_name)
{
	list<string> components;

	if (_normalize) {
		components.push_back (_("normalize"));
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

	if (has_sample_format) {
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
	node->add_property ("name", name);
	node->add_property ("value", value);
}

std::string
ExportFormatSpecification::get_option (XMLNode const * node, std::string const & name)
{
	XMLNodeList list (node->children ("Option"));

	for (XMLNodeList::iterator it = list.begin(); it != list.end(); ++it) {
		XMLProperty * prop = (*it)->property ("name");
		if (prop && !name.compare (prop->value())) {
			prop = (*it)->property ("value");
			if (prop) {
				return prop->value();
			}
		}
	}

	std::cerr << "Could not load encoding option \"" << name << "\" for export format" << std::endl;

	return "";
}

}; // namespace ARDOUR
