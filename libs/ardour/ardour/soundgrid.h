/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __ardour_soundgrid_h__
#define __ardour_soundgrid_h__

#include <vector>
#include <string>

#include <glib.h>

#include <boost/utility.hpp>
#include <boost/function.hpp>

#include <WavesPublicAPI/WTErr.h>
#include <WavesMixerAPI/1.0/WavesMixerAPI.h>

#include "pbd/signals.h"

#include "ardour/ardour.h"

namespace ARDOUR {

class SoundGrid : public boost::noncopyable
{
  public:
	~SoundGrid ();

        int initialize (void* window_handle, uint32_t max_track_inputs, uint32_t max_track_ouputs,
                        uint32_t max_phys_inputs, uint32_t max_phys_outputs);
        int teardown ();

	static SoundGrid& instance();
	static bool available ();
	static std::vector<std::string> lan_port_names();
        static std::string current_lan_port_name();
	static std::string coreaudio_device_name ();
	static uint32_t current_network_buffer_size ();
	static std::vector<uint32_t> possible_network_buffer_sizes ();

        static int set_parameters (const std::string& device, int sr, int bufsize);

	struct InventoryItem {
	    virtual ~InventoryItem() {}; /* force virtual so that we can use dynamic_cast<> */

	    uint32_t    assign;
	    std::string name;
	    std::string mac;
            std::string hostname;
	    uint32_t    channels;
	    std::string device;
	    std::string status;
	};

	struct SGSInventoryItem : public InventoryItem {
	};

	struct IOInventoryItem : public InventoryItem {
                int32_t sample_rate;
                int32_t output_channels;
	};

	typedef std::vector<InventoryItem*> Inventory;
	static void update_inventory (Inventory&);
	static void clear_inventory (Inventory&);

        static void driver_register (const WSDCoreHandle, const WSCoreCallbackTable*, const WSMixerConfig*);

        void finalize (void* ecc, int state);
        void command_status_update (struct WSCommand*);

        static PBD::Signal0<void> Shutdown;
        
        bool add_rack_synchronous (uint32_t clusterType, uint32_t &trackHandle);
        bool add_rack_asynchronous (uint32_t clusterType);
        bool remove_rack_synchronous (uint32_t clusterType, uint32_t trackHandle);
        bool remove_all_racks_synchronous ();
        int connect_input (uint32_t trackHandle, uint32_t chn, uint32_t phys_chn);
        int connect_output (uint32_t trackHandle, uint32_t chn, uint32_t phys_chn);
        int disconnect_input (uint32_t trackHandle, uint32_t chn, uint32_t phys_chn);
        int disconnect_output (uint32_t trackHandle, uint32_t chn, uint32_t phys_chn);
        int set_gain (uint32_t in_clusterType, uint32_t in_trackHandle, double in_gainValue);
        bool get_gain (uint32_t in_clusterType, uint32_t in_trackHandle, double &out_gainValue);

        int configure_driver (uint32_t physical_inputs, uint32_t physical_outputs);
        int connect_input_channel_to_driver (uint32_t physical_input, uint32_t driver_channel);
        int disconnect_input_channel_to_driver (uint32_t physical_input, uint32_t driver_channel);

        std::string map_chainer_input_as_jack_port (uint32_t chainer_id, uint32_t channel);
        void remove_chainer_input (uint32_t chainer_id, uint32_t channel);

        int map_inputs_as_jack_ports (uint32_t ninputs);

  private:
	SoundGrid ();
	static SoundGrid* _instance;
        
	void* dl_handle;
	void* _sg; // handle managed by SoundGrid library
        WSDCoreHandle        _host_handle;
        WSCoreCallbackTable  _callback_table;
        WSMixerConfig        _mixer_config;

	void display_update ();
	static void _display_update ();

        void _finalize (void* ecc, int state);
        void event_completed (int);

        static WTErr _sg_callback (void*, const WSControlID* pControlID);
        WTErr sg_callback (const WSControlID* pControlID);

        WTErr get (WSControlID*, WSControlInfo*);
        WTErr set (WSEvent*, const std::string&);
        WTErr command (WSCommand*);

        struct EventCompletionClosure {
            std::string name;
            boost::function<void(int)> func;
            uint64_t id;

            EventCompletionClosure (const std::string& n, boost::function<void(int)> f) 
            : name (n), func (f), id (g_atomic_int_add (&id_counter, 1) + 1) {}

            static int id_counter; 
        };

        struct CommandStatusClosure {
            std::string name;
            boost::function<void(WSCommand*)> func;
            uint64_t id;
            
            CommandStatusClosure (const std::string& n, boost::function<void(WSCommand*)> f) 
            : name (n), func (f), id (g_atomic_int_add (&id_counter, 1) + 1) {}
            
            static int id_counter; 
        };

        uint32_t          _driver_ports; // how many total channels we tell the SG driver to allocate
        std::vector<bool> _driver_ports_in_use;
};

} // namespace ARDOUR

#endif /* __ardour_soundgrid__ */
