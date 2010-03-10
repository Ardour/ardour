/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __ardour_monitor_processor_h__
#define __ardour_monitor_processor_h__

#include <vector>

#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/processor.h"

class XMLNode;

namespace ARDOUR {

class Session;

class MonitorProcessor : public Processor
{
  public:
        MonitorProcessor (Session&);
        MonitorProcessor (Session&, const XMLNode& name);

        bool display_to_user() const;

	void run (BufferSet& /*bufs*/, sframes_t /*start_frame*/, sframes_t /*end_frame*/, nframes_t /*nframes*/, bool /*result_required*/);

        XMLNode& state (bool full);
        int set_state (const XMLNode&, int /* version */);

        bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;

        void set_cut_all (bool);
        void set_dim_all (bool);
        void set_polarity (uint32_t, bool invert);
        void set_cut (uint32_t, bool cut);
        void set_dim (uint32_t, bool dim);
        void set_solo (uint32_t, bool);

        void set_dim_level (gain_t);
        void set_solo_boost_level (gain_t);

        gain_t dim_level() const { return _dim_level; }
        gain_t solo_boost_level() const { return _solo_boost_level; }

        bool dimmed (uint32_t chn) const;
        bool soloed (uint32_t chn) const;
        bool inverted (uint32_t chn) const;
        bool cut (uint32_t chn) const;

        PBD::Signal0<void> Changed;
        
  private:
        std::vector<gain_t>  current_gain;
        std::vector<gain_t> _cut;
        std::vector<bool>   _dim;
        std::vector<gain_t> _polarity;
        std::vector<bool>   _soloed;
        uint32_t             solo_cnt;
        bool                _dim_all;
        bool                _cut_all;
        volatile gain_t     _dim_level;
        volatile gain_t     _solo_boost_level;
};

} /* namespace */

#endif /* __ardour_monitor_processor_h__ */
