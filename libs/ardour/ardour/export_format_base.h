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

#ifndef __ardour_export_format_base_h__
#define __ardour_export_format_base_h__

#include <set>
#include <string>

#include <boost/shared_ptr.hpp>

#include <sndfile.h>
#include <samplerate.h>

#include "pbd/signals.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#include "audiographer/general/sample_format_converter.h"

namespace ARDOUR
{

class LIBARDOUR_API ExportFormatBase {
  public:

	enum Type {
		T_None = 0,
		T_Sndfile
	};

	enum FormatId {
		F_None = 0,
		F_WAV = SF_FORMAT_WAV,
		F_W64 = SF_FORMAT_W64,
		F_CAF = SF_FORMAT_CAF,
		F_AIFF = SF_FORMAT_AIFF,
		F_AU = SF_FORMAT_AU,
		F_IRCAM = SF_FORMAT_IRCAM,
		F_RAW = SF_FORMAT_RAW,
		F_FLAC = SF_FORMAT_FLAC,
		F_Ogg = SF_FORMAT_OGG
	};

	enum Endianness {
		E_FileDefault = SF_ENDIAN_FILE, /* Default file endian-ness. */
		E_Little = SF_ENDIAN_LITTLE,    /* Force little endian-ness. */
		E_Big = SF_ENDIAN_BIG,          /* Force big endian-ness. */
		E_Cpu = SF_ENDIAN_CPU           /* Force CPU endian-ness. */
	};

	enum SampleFormat {
		SF_None = 0,
		SF_8 = SF_FORMAT_PCM_S8,
		SF_16 = SF_FORMAT_PCM_16,
		SF_24 = SF_FORMAT_PCM_24,
		SF_32 = SF_FORMAT_PCM_32,
		SF_U8 = SF_FORMAT_PCM_U8,
		SF_Float = SF_FORMAT_FLOAT,
		SF_Double = SF_FORMAT_DOUBLE,
		SF_Vorbis = SF_FORMAT_VORBIS
	};

	enum DitherType {
		D_None = AudioGrapher::D_None,
		D_Rect = AudioGrapher::D_Rect,
		D_Tri = AudioGrapher::D_Tri,
		D_Shaped = AudioGrapher::D_Shaped
	};

	enum Quality {
		Q_None = 0,
		Q_Any,
		Q_LosslessLinear,
		Q_LosslessCompression,
		Q_LossyCompression
	};

	enum SampleRate {
		SR_None = 0,
		SR_Session = 1,
		SR_8 = 8000,
		SR_22_05 = 220500,
		SR_44_1 = 44100,
		SR_48 = 48000,
		SR_88_2 = 88200,
		SR_96 = 96000,
		SR_192 = 192000
	};

	enum SRCQuality {
		SRC_SincBest = SRC_SINC_BEST_QUALITY,
		SRC_SincMedium = SRC_SINC_MEDIUM_QUALITY,
		SRC_SincFast = SRC_SINC_FASTEST,
		SRC_ZeroOrderHold = SRC_ZERO_ORDER_HOLD,
		SRC_Linear = SRC_LINEAR
	};

	/// Class for managing selection and compatibility states
	class LIBARDOUR_API SelectableCompatible {
	  public:
		SelectableCompatible ()
			: _selected (false), _compatible (true) { }
		~SelectableCompatible () {}

		PBD::Signal1<void,bool> SelectChanged;
		PBD::Signal1<void,bool> CompatibleChanged;

		bool selected () const { return _selected; }
		bool compatible () const { return _compatible; }
		std::string name () const { return _name; }

		void set_selected (bool value);
		void set_compatible (bool value);

	  protected:
		void set_name (std::string name) { _name = name; }

	  private:
		bool _selected;
		bool _compatible;

		std::string _name;
	};

  public:

	ExportFormatBase ();
	ExportFormatBase (ExportFormatBase const & other);

	virtual ~ExportFormatBase ();

	boost::shared_ptr<ExportFormatBase> get_intersection (ExportFormatBase const & other) const;
	boost::shared_ptr<ExportFormatBase> get_union (ExportFormatBase const & other) const;

	bool endiannesses_empty () const { return endiannesses.empty (); }
	bool sample_formats_empty () const { return sample_formats.empty (); }
	bool sample_rates_empty () const { return sample_rates.empty (); }
	bool formats_empty () const { return format_ids.empty (); }
	bool qualities_empty () const { return qualities.empty (); }

	bool has_endianness (Endianness endianness) const { return endiannesses.find (endianness) != endiannesses.end() ; }
	bool has_sample_format (SampleFormat format) const { return sample_formats.find (format) != sample_formats.end(); }
	bool has_sample_rate (SampleRate rate) const { return sample_rates.find (rate) != sample_rates.end(); }
	bool has_format (FormatId format) const { return format_ids.find (format) != format_ids.end(); }
	bool has_quality (Quality quality) const { return qualities.find (quality) != qualities.end(); }

	void set_extension (std::string const & extension) { _extension = extension; }
	std::string const & extension () const { return _extension; }

	static SampleRate nearest_sample_rate (framecnt_t sample_rate);

  protected:

	friend class HasSampleFormat;
	typedef std::set<SampleFormat> SampleFormatSet;
	SampleFormatSet sample_formats;

  protected:
	typedef std::set<Endianness> EndianSet;
	typedef std::set<SampleRate> SampleRateSet;
	typedef std::set<FormatId> FormatSet;
	typedef std::set<Quality> QualitySet;

	EndianSet       endiannesses;
	SampleRateSet   sample_rates;
	FormatSet       format_ids;
	QualitySet      qualities;

  private:

	std::string  _extension;

	enum SetOperation {
		SetUnion,
		SetIntersection
	};

	boost::shared_ptr<ExportFormatBase> do_set_operation (ExportFormatBase const & other, SetOperation operation) const;
};

} // namespace ARDOUR

#endif /* __ardour_export_format_base_h__ */
