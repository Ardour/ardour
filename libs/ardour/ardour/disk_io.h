/*
    Copyright (C) 2009-2016 Paul Davis

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

#ifndef __ardour_disk_io_h__
#define __ardour_disk_io_h__

#include <vector>
#include <string>
#include <exception>

#include "pbd/ringbufferNPT.h"
#include "pbd/rcu.h"

#include "ardour/interpolation.h"
#include "ardour/processor.h"

namespace ARDOUR {

class Session;
class Route;
class Location;

class LIBARDOUR_API DiskIOProcessor : public Processor
{
  public:
	enum Flag {
		Recordable  = 0x1,
		Hidden      = 0x2,
		Destructive = 0x4,
		NonLayered   = 0x8
	};

	static const std::string state_node_name;

	DiskIOProcessor (Session&, const std::string& name, Flag f);

	static void set_buffering_parameters (BufferingPreset bp);

	/** @return A number between 0 and 1, where 0 indicates that the playback buffer
	 *  is dry (ie the disk subsystem could not keep up) and 1 indicates that the
	 *  buffer is full.
	 */
	virtual float playback_buffer_load() const = 0;
	virtual float capture_buffer_load() const = 0;

	void set_flag (Flag f)   { _flags = Flag (_flags | f); }
	void unset_flag (Flag f) { _flags = Flag (_flags & ~f); }

	bool           hidden()      const { return _flags & Hidden; }
	bool           recordable()  const { return _flags & Recordable; }
	bool           non_layered()  const { return _flags & NonLayered; }
	bool           reversed()    const { return _actual_speed < 0.0f; }
	double         speed()       const { return _visible_speed; }

	ChanCount n_channels() { return _n_channels; }

	void non_realtime_set_speed ();
	bool realtime_set_speed (double sp, bool global);

	virtual void punch_in()  {}
	virtual void punch_out() {}

	virtual float buffer_load() const = 0;

	bool slaved() const      { return _slaved; }
	void set_slaved(bool yn) { _slaved = yn; }

	int set_loop (Location *loc);

	PBD::Signal1<void,Location *> LoopSet;
	PBD::Signal0<void>            SpeedChanged;
	PBD::Signal0<void>            ReverseChanged;

	int set_state (const XMLNode&, int version);

	int add_channel (uint32_t how_many);
	int remove_channel (uint32_t how_many);

  protected:
	friend class Auditioner;
	virtual int  seek (framepos_t which_sample, bool complete_refill = false) = 0;

  protected:
	Flag         _flags;
	uint32_t i_am_the_modifier;
	ChanCount    _n_channels;
	double       _visible_speed;
	double       _actual_speed;
	double       _speed;
	double       _target_speed;
	/* items needed for speed change logic */
	bool         _buffer_reallocation_required;
	bool         _seek_required;
	bool         _slaved;
	Location*     loop_location;
	bool          in_set_state;
	framecnt_t    wrap_buffer_size;
	framecnt_t    speed_buffer_size;

	Glib::Threads::Mutex state_lock;

	static bool get_buffering_presets (BufferingPreset bp,
	                                   framecnt_t& read_chunk_size,
	                                   framecnt_t& read_buffer_size,
	                                   framecnt_t& write_chunk_size,
	                                   framecnt_t& write_buffer_size);

	virtual void allocate_temporary_buffers () = 0;

	/** Information about one audio channel, playback or capture
	 * (depending on the derived class)
	 */
	struct ChannelInfo : public boost::noncopyable {

		ChannelInfo (framecnt_t buffer_size,
		             framecnt_t speed_buffer_size,
		             framecnt_t wrap_buffer_size);
		~ChannelInfo ();

		Sample     *wrap_buffer;
		Sample     *speed_buffer;
		Sample     *current_buffer;

		/** A ringbuffer for data to be played back, written to in the
		    butler thread, read from in the process thread.
		*/
		PBD::RingBufferNPT<Sample>* buf;

		Sample* scrub_buffer;
		Sample* scrub_forward_buffer;
		Sample* scrub_reverse_buffer;

		PBD::RingBufferNPT<Sample>::rw_vector read_vector;

		void resize (framecnt_t);
	};

	typedef std::vector<ChannelInfo*> ChannelList;
	SerializedRCUManager<ChannelList> channels;

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);
	int remove_channel_from (boost::shared_ptr<ChannelList>, uint32_t how_many);

	CubicInterpolation interpolation;

};

} // namespace ARDOUR

#endif /* __ardour_disk_io_h__ */
