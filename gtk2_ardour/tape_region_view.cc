/*
    Copyright (C) 2006 Paul Davis

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

#include <cmath>
#include <algorithm>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"

#include "tape_region_view.h"
#include "audio_time_axis.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

const TimeAxisViewItem::Visibility TapeAudioRegionView::default_tape_visibility
	= TimeAxisViewItem::Visibility (
		TimeAxisViewItem::ShowNameHighlight |
		TimeAxisViewItem::ShowNameText |
		TimeAxisViewItem::ShowFrame |
		TimeAxisViewItem::HideFrameRight |
		TimeAxisViewItem::FullWidthNameHighlight);

TapeAudioRegionView::TapeAudioRegionView (ArdourCanvas::Container *parent, RouteTimeAxisView &tv,
					  boost::shared_ptr<AudioRegion> r,
					  double spu,
					  uint32_t basic_color)

	: AudioRegionView (parent, tv, r, spu, basic_color, false,
			   TimeAxisViewItem::Visibility ((r->position() != 0) ? default_tape_visibility :
							 TimeAxisViewItem::Visibility (default_tape_visibility|TimeAxisViewItem::HideFrameLeft)))
{
}

void
TapeAudioRegionView::init (bool /*wfw*/)
{
	/* never wait for data: always just create the waves, connect once and then
	   we'll update whenever we need to.
	*/

	AudioRegionView::init (false);

	/* every time the wave data changes and peaks are ready, redraw */

	for (uint32_t n = 0; n < audio_region()->n_channels(); ++n) {
		audio_region()->audio_source(n)->PeaksReady.connect (*this, invalidator (*this), boost::bind (&TapeAudioRegionView::update, this, n), gui_context());
	}

}

TapeAudioRegionView::~TapeAudioRegionView()
{
}

void
TapeAudioRegionView::update (uint32_t /*n*/)
{
	/* this code doesn't work properly, the WaveViewCache is not updated
	 * when recording over (replacing) an existing part of the tape.
	 *
	 * WaveView probably needs to become aware if the given Region is
	 * tape-track and handle caching.
	 *
	 * explicitly forcing an update here can deadlock if the rendering
	 * request is non-threaded (resize track height or at rec-stop)
	 */

#if 0
	/* deadlock:
#1  0x00007f9570ebd77c in g_mutex_lock_slowpath (mutex=0x7f9575157760 <ArdourWaveView::WaveView::current_image_lock>) at ././glib/gthread-posix.c:1313
#2  0x000055f6f8d1aac0 in Glib::Threads::Mutex::Lock::Lock(Glib::Threads::Mutex&) (this=0x7ffc4d905aa0, mutex=...) at /usr/include/glibmm-2.4/glibmm/threads.h:688
#3  0x00007f9574f05054 in ArdourWaveView::WaveView::invalidate_image_cache() (this=0x55f6fdf341a0) at ../libs/canvas/wave_view.cc:275
#4  0x00007f9574f0b1b0 in ArdourWaveView::WaveView::gain_changed() (this=0x55f6fdf341a0) at ../libs/canvas/wave_view.cc:1329
#5  0x000055f6f96eb1b8 in TapeAudioRegionView::update(unsigned int) (this=0x55f6fdf32640) at ../gtk2_ardour/tape_region_view.cc:102
#6  0x000055f6f96eba9f in boost::_mfi::mf1<void, TapeAudioRegionView, unsigned int>::operator()(TapeAudioRegionView*, unsigned int) const (this=0x55f6fa74ce10, p=0x55f6fdf32640, a1=0)
    at /usr/include/boost/bind/mem_fn_template.hpp:165
#7  0x000055f6f96eb9b3 in boost::_bi::list2<boost::_bi::value<TapeAudioRegionView*>, boost::_bi::value<unsigned int> >::operator()<boost::_mfi::mf1<void, TapeAudioRegionView, unsigned int>, boost::_bi::list0>(boost::_bi::type<void>, boost::_mfi::mf1<void, TapeAudioRegionView, unsigned int>&, boost::_bi::list0&, int) (this=0x55f6fa74ce20, f=..., a=...)
    at /usr/include/boost/bind/bind.hpp:319
#8  0x000055f6f96eb76f in boost::_bi::bind_t<void, boost::_mfi::mf1<void, TapeAudioRegionView, unsigned int>, boost::_bi::list2<boost::_bi::value<TapeAudioRegionView*>, boost::_bi::value<unsigned int> > >::operator()() (this=0x55f6fa74ce10) at /usr/include/boost/bind/bind.hpp:1294
#9  0x000055f6f96eb6b4 in boost::detail::function::void_function_obj_invoker0<boost::_bi::bind_t<void, boost::_mfi::mf1<void, TapeAudioRegionView, unsigned int>, boost::_bi::list2<boost::_bi::value<TapeAudioRegionView*>, boost::_bi::value<unsigned int> > >, void>::invoke(boost::detail::function::function_buffer&) (function_obj_ptr=...)
    at /usr/include/boost/function/function_template.hpp:159
#10 0x000055f6f8d73fc8 in boost::function0<void>::operator()() const (this=0x55f6fefe4f60) at /usr/include/boost/function/function_template.hpp:771
#11 0x000055f6f8da3a00 in boost::_bi::list0::operator()<boost::function<void ()>, boost::_bi::list0>(boost::_bi::type<void>, boost::function<void ()>&, boost::_bi::list0&, int) (this=0x55f6fefe4f80, f=...) at /usr/include/boost/bind/bind.hpp:198
#12 0x000055f6f8d9869f in boost::_bi::bind_t<boost::_bi::unspecified, boost::function<void ()>, boost::_bi::list0>::operator()() (this=0x55f6fefe4f60)
    at /usr/include/boost/bind/bind.hpp:1294
#13 0x000055f6f8d8f068 in boost::detail::function::void_function_obj_invoker0<boost::_bi::bind_t<boost::_bi::unspecified, boost::function<void ()>, boost::_bi::list0>, void>::invoke(boost::detail::function::function_buffer&) (function_obj_ptr=...) at /usr/include/boost/function/function_template.hpp:159
#14 0x000055f6f8d73fc8 in boost::function0<void>::operator()() const (this=0x7ffc4d905f60) at /usr/include/boost/function/function_template.hpp:771
#15 0x00007f9574b11192 in AbstractUI<Gtkmm2ext::UIRequest>::call_slot(PBD::EventLoop::InvalidationRecord*, boost::function<void ()> const&) (this=
    0x55f6fa47dfe0, invalidation=0x55f6fdf375d0, f=...) at /home/rgareus/src/ardour/libs/pbd/pbd/abstract_ui.cc:425
#16 0x000055f6f8d70a9c in PBD::Signal0<void, PBD::OptionalLastValue<void> >::compositor(boost::function<void ()>, PBD::EventLoop*, PBD::EventLoop::InvalidationRecord*) (f=..., event_loop=0x55f6fa47dfe0, ir=0x55f6fdf375d0) at /home/rgareus/src/ardour/build/libs/pbd/pbd/signals_generated.h:216
#17 0x000055f6f8da3c64 in boost::_bi::list3<boost::_bi::value<boost::function<void ()> >, boost::_bi::value<PBD::EventLoop*>, boost::_bi::value<PBD::EventLoop::InvalidationRecord*> >::operator()<void (*)(boost::function<void ()>, PBD::EventLoop*, PBD::EventLoop::InvalidationRecord*), boost::_bi::list0>(boost::_bi::type<void>, void (*&)(boost::function<void ()>, PBD::EventLoop*, PBD::EventLoop::InvalidationRecord*), boost::_bi::list0&, int) (this=0x55f6fa6a9bd8, f=
    @0x55f6fa6a9bd0: 0x55f6f8d70a0f <PBD::Signal0<void, PBD::OptionalLastValue<void> >::compositor(boost::function<void ()>, PBD::EventLoop*, PBD::EventLoop::InvalidationRecord*)>, a=...)
    at /usr/include/boost/bind/bind.hpp:398
#18 0x000055f6f8d98827 in boost::_bi::bind_t<void, void (*)(boost::function<void ()>, PBD::EventLoop*, PBD::EventLoop::InvalidationRecord*), boost::_bi::list3<boost::_bi::value<boost::function<void ()> >, boost::_bi::value<PBD::EventLoop*>, boost::_bi::value<PBD::EventLoop::InvalidationRecord*> > >::operator()() (this=0x55f6fa6a9bd0) at /usr/include/boost/bind/bind.hpp:1294
#19 0x000055f6f8d8f1ee in boost::detail::function::void_function_obj_invoker0<boost::_bi::bind_t<void, void (*)(boost::function<void ()>, PBD::EventLoop*, PBD::EventLoop::InvalidationRecord*), boost::_bi::list3<boost::_bi::value<boost::function<void ()> >, boost::_bi::value<PBD::EventLoop*>, boost::_bi::value<PBD::EventLoop::InvalidationRecord*> > >, void>::invoke(boost::detail::function::function_buffer&) (function_obj_ptr=...) at /usr/include/boost/function/function_template.hpp:159
#20 0x000055f6f8d73fc8 in boost::function0<void>::operator()() const (this=0x55f6ffdb4de0) at /usr/include/boost/function/function_template.hpp:771
#21 0x000055f6f8d70ea8 in PBD::Signal0<void, PBD::OptionalLastValue<void> >::operator()() (this=0x55f6fd359158) at /home/rgareus/src/ardour/build/libs/pbd/pbd/signals_generated.h:325
#22 0x00007f95739515a3 in ARDOUR::AudioSource::done_with_peakfile_writes(bool) (this=0x55f6fd359140, done=true) at ../libs/ardour/audiosource.cc:841
#23 0x00007f9573950fbe in ARDOUR::AudioSource::build_peaks_from_scratch() (this=0x55f6fd359140) at ../libs/ardour/audiosource.cc:779
#24 0x00007f957394ee6b in ARDOUR::AudioSource::read_peaks_with_fpp(ARDOUR::PeakData*, long, long, long, double, long) const (this=0x55f6fd359140, peaks=0x55f700da6b80, npeaks=1356, start=113190, cnt=447480, samples_per_visual_peak=330, samples_per_file_peak=256) at ../libs/ardour/audiosource.cc:391
#25 0x00007f957394eb15 in ARDOUR::AudioSource::read_peaks(ARDOUR::PeakData*, long, long, long, double) const (this=0x55f6fd359140, peaks=0x55f700da6b80, npeaks=1356, start=113190, cnt=447480, samples_per_visual_peak=330) at ../libs/ardour/audiosource.cc:337
#26 0x00007f957393a905 in ARDOUR::AudioRegion::read_peaks(ARDOUR::PeakData*, long, long, long, unsigned int, double) const (this=
    0x55f6fd84c3b0, buf=0x55f700da6b80, npeaks=1356, offset=113190, cnt=447480, chan_n=0, samples_per_pixel=330) at ../libs/ardour/audioregion.cc:431
#27 0x00007f9574f09caa in ArdourWaveView::WaveView::generate_image(boost::shared_ptr<ArdourWaveView::WaveViewThreadRequest>, bool) const (this=0x55f6fdf341a0, req=..., in_render_thread=false)
    at ../libs/canvas/wave_view.cc:1005
#28 0x00007f9574f08e99 in ArdourWaveView::WaveView::get_image(long, long, bool&) const (this=0x55f6fdf341a0, start=225060, end=448800, full_image=@0x7ffc4d9070c7: false)
---Type <return> to continue, or q <return> to quit---
    at ../libs/canvas/wave_view.cc:870
#29 0x00007f9574f0a6e8 in ArdourWaveView::WaveView::render(ArdourCanvas::Rect const&, Cairo::RefPtr<Cairo::Context>) const (this=0x55f6fdf341a0, area=..., context=...)
    at ../libs/canvas/wave_view.cc:1180
*/

	/* check that all waves are build and ready */
	if (!tmp_waves.empty()) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &TapeAudioRegionView::update, n);
	// CAIROCANVAS

	/* this is a quick hack to draw something (abuse gain_changed to force
	 * an image-cache invalidation.
	 *
	 * TODO: ArdourWaveView::WaveView needs an API to look up the specific channel "n"
	 * and a special case to not only invalidate the cache but re-expose the
	 * waveform. e.g.
	 *
	 * waves[m]->rebuild();  // where 'm' corresponds to channel 'n'.
	 */
	for (uint32_t i = 0; i < waves.size(); ++i) {
		waves[i]->gain_changed ();
	}
#endif
}
