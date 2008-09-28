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

#ifndef __ardour_export_formats_h__
#define __ardour_export_formats_h__

#include <ardour/export_format_base.h>
#include <ardour/export_format_compatibility.h>

#include <list>
#include <boost/weak_ptr.hpp>

namespace ARDOUR
{

class HasSampleFormat;

/// Base class for formats
class ExportFormat : public ExportFormatBase, public ExportFormatBase::SelectableCompatible {

  public:
	ExportFormat () {};
	~ExportFormat () {};
	
	virtual bool set_compatibility_state (ExportFormatCompatibility const & compatibility) = 0;
	virtual Type get_type () const = 0;
	
	FormatId get_format_id () const { return *format_ids.begin(); }
	Quality get_quality () const { return *qualities.begin(); }
	
	bool has_sample_format ();
	bool sample_format_is_compatible (SampleFormat format) const;
	
	/* If the format has a specific sample format, this function should be overriden
	 * if the format has a selectable sample format, do not override this!
	 */
	
	virtual SampleFormat get_explicit_sample_format () const { return SF_None; }

	/* If the above is not overriden, this one should be */

	virtual ExportFormat::SampleFormat default_sample_format () const { return SF_None; }
	
	/* If the format has a channel count limit, override this */
	
	virtual uint32_t get_channel_limit () const { return 256; }
	
	/* If the format can be tagged with metadata override this */
	
	virtual bool supports_tagging () const { return false; }
	
	/* If the format contains broadcast info override this */
	
	virtual bool has_broadcast_info () const { return false; }

  protected:
	
	void add_sample_rate (SampleRate rate) { sample_rates.insert (rate); }
	void add_endianness (Endianness endianness) { endiannesses.insert (endianness); }
	
	void set_format_id (FormatId id) { format_ids.clear (); format_ids.insert (id); }
	void set_quality (Quality value) { qualities.clear(); qualities.insert (value); }
};

/// Class to be inherited by export formats that have a selectable sample format
class HasSampleFormat {
  public:

	class SampleFormatState : public ExportFormatBase::SelectableCompatible {
	  public:
		SampleFormatState (ExportFormatBase::SampleFormat format, Glib::ustring name) :
		  format (format) { set_name (name); }
	
		ExportFormatBase::SampleFormat  format;
	};

	class DitherTypeState : public ExportFormatBase::SelectableCompatible {
	  public:
		DitherTypeState (ExportFormatBase::DitherType type, Glib::ustring name) :
		  type (type) { set_name (name); }
	
		ExportFormatBase::DitherType  type;
	};
	
	typedef boost::shared_ptr<SampleFormatState> SampleFormatPtr;
	typedef boost::weak_ptr<SampleFormatState> WeakSampleFormatPtr;
	typedef std::list<SampleFormatPtr> SampleFormatList;
	
	typedef boost::shared_ptr<DitherTypeState> DitherTypePtr;
	typedef boost::weak_ptr<DitherTypeState> WeakDitherTypePtr;
	typedef std::list<DitherTypePtr> DitherTypeList;

  public:

	HasSampleFormat (ExportFormatBase::SampleFormatSet & sample_formats);
	virtual ~HasSampleFormat () {};

	void add_sample_format (ExportFormatBase::SampleFormat format);
	
	SampleFormatList const & get_sample_formats () const { return sample_format_states; }
	DitherTypeList const & get_dither_types () const { return dither_type_states; }
	
	SampleFormatPtr get_selected_sample_format ();
	DitherTypePtr get_selected_dither_type ();
	
	/* Proxies for signals from sample formats and dither types */
	
	sigc::signal<void, bool, WeakSampleFormatPtr> SampleFormatSelectChanged;
	sigc::signal<void, bool, WeakSampleFormatPtr> SampleFormatCompatibleChanged;
	
	sigc::signal<void, bool, WeakDitherTypePtr> DitherTypeSelectChanged;
	sigc::signal<void, bool, WeakDitherTypePtr> DitherTypeCompatibleChanged;
	
	static string get_sample_format_name (ExportFormatBase::SampleFormat format);

  protected:
	/* State lists */

	DitherTypeList    dither_type_states;
	SampleFormatList  sample_format_states;

  private:
	/* Connected to signals */
	
	void add_dither_type (ExportFormatBase::DitherType type, Glib::ustring name);
	void update_sample_format_selection (bool);
	void update_dither_type_selection (bool);
	
	/* Reference to ExportFormatBase::sample_formats */
	ExportFormatBase::SampleFormatSet & _sample_formats;
};

class ExportFormatLinear : public ExportFormat, public HasSampleFormat {
  public:

	ExportFormatLinear (Glib::ustring name, FormatId format_id);
	~ExportFormatLinear () {};
	
	bool set_compatibility_state (ExportFormatCompatibility const & compatibility);
	Type get_type () const { return T_Sndfile; }
	
	void add_endianness (Endianness endianness) { endiannesses.insert (endianness); }
	
	void set_default_sample_format (SampleFormat sf) { _default_sample_format = sf; }
	SampleFormat default_sample_format () const { return _default_sample_format; }
	
  protected:
	SampleFormat _default_sample_format;
};

class ExportFormatOggVorbis : public ExportFormat {
  public:
	ExportFormatOggVorbis ();
	~ExportFormatOggVorbis () {};
	
	static bool check_system_compatibility ();
	
	bool set_compatibility_state (ExportFormatCompatibility const & compatibility);
	Type get_type () const { return T_Sndfile; }
	SampleFormat get_explicit_sample_format () const { return SF_Vorbis; }
	virtual bool supports_tagging () const { return true; }
};

class ExportFormatFLAC : public ExportFormat, public HasSampleFormat {
  public:
	ExportFormatFLAC ();
	~ExportFormatFLAC () {};
	
	static bool check_system_compatibility ();
	
	bool set_compatibility_state (ExportFormatCompatibility const & compatibility);
	Type get_type () const { return T_Sndfile; }
	
	uint32_t get_channel_limit () const { return 8; }
	SampleFormat default_sample_format () const { return SF_16; }
	virtual bool supports_tagging () const { return true; }
};

class ExportFormatBWF : public ExportFormat, public HasSampleFormat {
  public:
	ExportFormatBWF ();
	~ExportFormatBWF () {};
	
	bool set_compatibility_state (ExportFormatCompatibility const & compatibility);
	Type get_type () const { return T_Sndfile; }
	
	SampleFormat default_sample_format () const { return SF_16; }
	virtual bool has_broadcast_info () const { return true; }
};

} // namespace ARDOUR

#endif /* __ardour_export_formats__ */
