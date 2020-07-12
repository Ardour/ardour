/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
#ifndef _ardour_fluidsynth_h_
#define _ardour_fluidsynth_h_

#include <stdint.h>
#include <string.h>
#include <glib.h>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

#include "fluidsynth.h"

namespace ARDOUR {

	class LIBARDOUR_API FluidSynth {
		public:
			/** instantiate a Synth
			 *
			 * @param samplerate samplerate
			 * @param polyphony polyphony
			 */
			FluidSynth (float samplerate, int polyphony = 256);
			~FluidSynth ();

			bool load_sf2 (const std::string& fn);

			bool synth (float* left, float* right, uint32_t n_samples);
			bool midi_event (uint8_t const* const data, size_t len);
			void panic ();

			/* load a preset 
			 * @pgm BankProgram index
			 * @chan midi channel (0..15)
			 * @return true on succcess
			 */
			bool select_program (uint32_t pgm, uint8_t chan);

			uint32_t program_count () const { return _presets.size(); }

			std::string program_name (uint32_t pgm) const {
				if (pgm >= _presets.size()) { return ""; }
				return _presets[pgm].name;
			}

		private:
			fluid_settings_t*   _settings;
			fluid_synth_t*      _synth;
			int                 _synth_id;
			fluid_midi_event_t* _f_midi_event;

			struct BankProgram {
				BankProgram (const std::string& n, int b, int p)
					: name (n)
					, bank (b)
					, program (p)
				{}

				BankProgram (const BankProgram& other)
					: name (other.name)
					, bank (other.bank)
					, program (other.program)
				{}

				std::string name;
				int bank;
				int program;
			};

			std::vector<BankProgram> _presets;
	};

} /* namespace */
#endif
