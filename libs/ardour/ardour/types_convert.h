/*
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef ARDOUR_TYPES_CONVERT_H
#define ARDOUR_TYPES_CONVERT_H

#ifdef COMPILER_MSVC
#pragma warning(disable:4101)
#endif

#include "pbd/enum_convert.h"

#include "ardour/types.h"
#include "ardour/data_type.h"
#include "ardour/mode.h"

/* NOTE: when adding types to this file, you must add four functions:

   std::string to_string (T);
   T string_to (std::string const &);
   bool to_string (T, std::string &);
   bool string_to (std::string const &, T&);
*/

namespace PBD {

DEFINE_ENUM_CONVERT(Timecode::TimecodeFormat)

DEFINE_ENUM_CONVERT(ARDOUR::AnyTime::Type)
DEFINE_ENUM_CONVERT(ARDOUR::SampleFormat)
DEFINE_ENUM_CONVERT(ARDOUR::HeaderFormat)
DEFINE_ENUM_CONVERT(ARDOUR::AutoConnectOption)
DEFINE_ENUM_CONVERT(ARDOUR::TracksAutoNamingRule)
DEFINE_ENUM_CONVERT(ARDOUR::TrackMode)
DEFINE_ENUM_CONVERT(ARDOUR::EditMode)
DEFINE_ENUM_CONVERT(ARDOUR::MonitorModel)
DEFINE_ENUM_CONVERT(ARDOUR::AFLPosition)
DEFINE_ENUM_CONVERT(ARDOUR::PFLPosition)
DEFINE_ENUM_CONVERT(ARDOUR::ListenPosition)
DEFINE_ENUM_CONVERT(ARDOUR::LayerModel)
DEFINE_ENUM_CONVERT(ARDOUR::InsertMergePolicy)
DEFINE_ENUM_CONVERT(ARDOUR::SyncSource)
DEFINE_ENUM_CONVERT(ARDOUR::ShuttleUnits)
DEFINE_ENUM_CONVERT(ARDOUR::ClockDeltaMode)
DEFINE_ENUM_CONVERT(ARDOUR::DenormalModel)
DEFINE_ENUM_CONVERT(ARDOUR::FadeShape)
DEFINE_ENUM_CONVERT(ARDOUR::RegionSelectionAfterSplit)
DEFINE_ENUM_CONVERT(ARDOUR::RangeSelectionAfterSplit)
DEFINE_ENUM_CONVERT(ARDOUR::BufferingPreset)
DEFINE_ENUM_CONVERT(ARDOUR::AutoReturnTarget)
DEFINE_ENUM_CONVERT(ARDOUR::MeterType)
DEFINE_ENUM_CONVERT(ARDOUR::MeterPoint)
DEFINE_ENUM_CONVERT(ARDOUR::DiskIOPoint)
DEFINE_ENUM_CONVERT(ARDOUR::NoteMode)
DEFINE_ENUM_CONVERT(ARDOUR::ChannelMode)
DEFINE_ENUM_CONVERT(ARDOUR::MonitorChoice)
DEFINE_ENUM_CONVERT(ARDOUR::PluginType)
DEFINE_ENUM_CONVERT(ARDOUR::AlignStyle)
DEFINE_ENUM_CONVERT(ARDOUR::AlignChoice)
DEFINE_ENUM_CONVERT(ARDOUR::RegionEquivalence)
DEFINE_ENUM_CONVERT(ARDOUR::WaveformScale)
DEFINE_ENUM_CONVERT(ARDOUR::WaveformShape)
DEFINE_ENUM_CONVERT(ARDOUR::ScreenSaverMode)
DEFINE_ENUM_CONVERT(ARDOUR::VUMeterStandard)
DEFINE_ENUM_CONVERT(ARDOUR::MeterLineUp)
DEFINE_ENUM_CONVERT(ARDOUR::InputMeterLayout)
DEFINE_ENUM_CONVERT(ARDOUR::MidiPortFlags)
DEFINE_ENUM_CONVERT(ARDOUR::TransportRequestType)
DEFINE_ENUM_CONVERT(ARDOUR::LoopFadeChoice)
DEFINE_ENUM_CONVERT(ARDOUR::CueBehavior)

DEFINE_ENUM_CONVERT(MusicalMode::Type)

template <>
inline std::string to_string (ARDOUR::timepos_t val)
{
	return val.str ();
}

template <>
inline ARDOUR::timepos_t string_to (std::string const & str)
{
	ARDOUR::timepos_t tmp (Temporal::AudioTime); /* domain may be changed */
	tmp.string_to (str);
	return tmp;
}

template <>
inline bool to_string (ARDOUR::timepos_t val, std::string & str)
{
	str = val.str ();
	return true;
}

template <>
inline bool string_to (std::string const & str, ARDOUR::timepos_t & val)
{
	return val.string_to (str);
}


template <>
inline std::string to_string (ARDOUR::timecnt_t val)
{
	return val.str ();
}

template <>
inline ARDOUR::timecnt_t string_to (std::string const & str)
{
	ARDOUR::timecnt_t tmp (Temporal::AudioTime); /* domain may change */
	tmp.string_to (str);
	return tmp;
}

template <>
inline bool to_string (ARDOUR::timecnt_t val, std::string & str)
{
	str = val.str ();
	return true;
}

template <>
inline bool string_to (std::string const & str, ARDOUR::timecnt_t & val)
{
	return val.string_to (str);
}

template <>
inline bool to_string (ARDOUR::AutoState val, std::string& str)
{
	str = ARDOUR::auto_state_to_string (val);
	return true;
}

template <>
inline bool string_to (const std::string& str, ARDOUR::AutoState& as)
{
	as = ARDOUR::string_to_auto_state (str);
	return true;
}

template <>
inline bool to_string (ARDOUR::DataType val, std::string& str)
{
	str = val.to_string();
	return true;
}

template <>
inline bool string_to (const std::string& str, ARDOUR::DataType& dt)
{
	dt = ARDOUR::DataType(str);
	return true;
}

template <>
inline bool to_string (ARDOUR::FollowAction fa, std::string& str)
{
	str = fa.to_string();
	return true;
}

template <>
inline bool string_to (const std::string& str, ARDOUR::FollowAction& fa)
{
	fa = ARDOUR::FollowAction (str);
	return true;
}

template<>
inline std::string to_string (ARDOUR::FollowAction fa)
{
	return fa.to_string ();
}

template<>
inline ARDOUR::FollowAction string_to (std::string const & str)
{
	return ARDOUR::FollowAction (str);
}


} // namespace PBD

#endif // ARDOUR_TYPES_CONVERT_H
