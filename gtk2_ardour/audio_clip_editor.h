/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystem.com>
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

#ifndef __gtk2_ardour_audio_clip_editor_h__
#define __gtk2_ardour_audio_clip_editor_h__

#include <vector>

#include "canvas/canvas.h"

namespace ARDOUR {
class AudioRegion;
}

namespace ArdourWaveView {
class WaveView;
}

class AudioClipEditor : public ArdourCanvas::GtkCanvas
{
   public:
	AudioClipEditor ();
	~AudioClipEditor ();

	void set_region (boost::shared_ptr<ARDOUR::AudioRegion>);

  private:
	std::vector<ArdourWaveView::WaveView *> waves;

	void drop_waves ();
	void size_allocate (Gtk::Allocation const &);
	void set_wave_heights (int);
	void set_waveform_colors ();
};


#endif /* __gtk2_ardour_audio_clip_editor_h__ */
